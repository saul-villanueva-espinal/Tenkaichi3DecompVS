"""Real six-process C transport through the Python UDP relay (loopback only)."""
from pathlib import Path
import os
import shutil
import socket
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
subprocess.run([sys.executable, "port/tests/room_directory_test.py"], cwd=root, check=True)
with tempfile.TemporaryDirectory() as tmp:
    exe = str(Path(tmp) / ("room_peer.exe" if os.name == "nt" else "room_peer"))
    tools = root.parent / "local-toolchain/ucrt64/bin"
    os.environ["PATH"] = str(tools) + os.pathsep + os.environ.get("PATH", "")
    cc = shutil.which("cc") or shutil.which("gcc") or str(tools / "gcc.exe")
    subprocess.run([cc, "-std=c11", "-D_POSIX_C_SOURCE=200809L",
                    "-Wall", "-Wextra", "port/tests/room_relay_peer_test.c", "port/src/gs/coop_rooms.c",
                    "-lpthread", "-o", exe] + (["-lws2_32"] if os.name == "nt" else []), cwd=root, check=True)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]
    processes = []
    try:
        service = subprocess.Popen([sys.executable, "port/raid_rooms/server.py", "--port", str(port)],
                                   cwd=root, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        processes.append(service)
        assert "listening" in service.stdout.readline()
        env = os.environ | {"BT3_ROOM_SERVER": f"127.0.0.1:{port}"}
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as unavailable:
            unavailable.bind(("127.0.0.1", 0))
            dead_port = unavailable.getsockname()[1]
            # Keep a bound socket which deliberately sends no response.
            for mode in ("--async-cancel", "--async-timeout"):
                subprocess.run([exe, mode], env=env | {"BT3_ROOM_SERVER": f"127.0.0.1:{dead_port}"},
                               check=True, timeout=12)
        host = subprocess.Popen([exe], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        processes.append(host)
        line = host.stdout.readline().strip()
        assert line.startswith("CODE="), line + host.stderr.read()
        code = line[5:]
        bad = subprocess.run([exe, code, "bad-mods"], env=env, capture_output=True, text=True, timeout=12)
        assert bad.returncode == 0 and "mods" in bad.stderr, bad
        for _ in range(5):
            processes.append(subprocess.Popen([exe, "BT3-" + code], env=env, stdout=subprocess.PIPE,
                                              stderr=subprocess.PIPE, text=True))
        for process in processes[1:]:
            out, err = process.communicate(timeout=15)
            assert process.returncode == 0 and "PASS" in out, out + err
            print(out.strip())
        print("room_relay_test: six C clients, real UDP service, invitation and mismatched-mod rejection PASS (local only)")
    finally:
        for process in processes:
            if process.poll() is None:
                process.terminate()
            process.communicate(timeout=5)

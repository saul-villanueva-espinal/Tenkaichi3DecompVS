"""Real lobby transport -> process restart -> co-op input sync through one room.
Loopback test only. This does not prove reachability across the public Internet.
"""
from pathlib import Path
import os
import shutil
import socket
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
tools = root.parent / "local-toolchain/ucrt64/bin"
os.environ["PATH"] = str(tools) + os.pathsep + os.environ.get("PATH", "")
with tempfile.TemporaryDirectory() as tmp:
    exe = str(Path(tmp) / ("restart_peer.exe" if os.name == "nt" else "restart_peer"))
    subprocess.run([shutil.which("cc") or shutil.which("gcc"), "-std=c11", "-D_POSIX_C_SOURCE=200809L",
                    "-Iport/include", "port/tests/coop_peer_test.c", "port/src/gs/coop_net.c",
                    "port/src/gs/coop_rooms.c", "port/src/plat_pc_battle.c", "port/src/plat_tournament.c",
                    "-lpthread", "-o", exe] + (["-lws2_32"] if os.name == "nt" else []), cwd=root, check=True)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]
    service = subprocess.Popen([sys.executable, "port/raid_rooms/server.py", "--port", str(port)],
                               cwd=root, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        assert "listening" in service.stdout.readline()
        for count in (2, 6):
            base = {k: v for k, v in os.environ.items() if not k.startswith(("BT3_COOP", "BT3_ROOM", "FIXTURE"))}
            base.update(BT3_ROOM_SERVER=f"127.0.0.1:{port}", FIXTURE_RESTART="1")
            config = [3, count, 0, count, 1] + sum(
                ([i * 2 if side == 0 else 4, 0] for side in range(2) for i in range(10)), [])
            config += [125, 150, 150, 2, 25, 162500, 200, 50, 30, 14, 134, 2]
            base["BT3_COOP_CONFIG"] = ",".join(map(str, config))
            processes = []
            try:
                host = subprocess.Popen([exe, "--relay-setup"], env=base | {"FIXTURE_HOST": "1"},
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                processes.append(host)
                line = host.stdout.readline().strip()
                assert line.startswith("CODE="), line
                code = line[5:]
                for i in range(1, count):
                    guest = subprocess.Popen([exe, "--relay-setup", code],
                                     env=base | {"FIXTURE_HERO": str(i * 2)},
                                     stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                    processes.append(guest)
                    assert guest.stdout.readline().strip() == f"JOINED={i}"
                for p in processes:
                    out, err = p.communicate(timeout=35)
                    assert p.returncode == 0 and f"all {count} distinct pads" in out, out + err
                print(f"room_restart_test: {count} peers, SAME invitation after real exec, owned heroes and 300 input ticks PASS")
            finally:
                for p in processes:
                    if p.poll() is None:
                        if os.name == "nt":
                            subprocess.run(["taskkill", "/PID", str(p.pid), "/T", "/F"],
                                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                        else:
                            p.terminate()
                    p.communicate(timeout=5)
    finally:
        service.terminate()
        service.communicate(timeout=5)

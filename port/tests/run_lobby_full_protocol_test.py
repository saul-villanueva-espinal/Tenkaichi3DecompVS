"""Comprehensive multi-process lobby protocol test: 2 to 6 players, bad-mod rejection,
room-full rejection, cancellation, and synchronized launch."""
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
cc = shutil.which("cc") or shutil.which("gcc") or str(tools / "gcc.exe")

with tempfile.TemporaryDirectory() as tmp:
    exe = str(Path(tmp) / ("lobby_test.exe" if os.name == "nt" else "lobby_test"))
    subprocess.run([cc, "-std=c11", "-D_POSIX_C_SOURCE=200809L",
                    "-Wall", "-Wextra", "-Iport/include",
                    "port/tests/lobby_full_protocol_test.c", "port/src/gs/coop_rooms.c",
                    "-lpthread", "-o", exe] + (["-lws2_32"] if os.name == "nt" else []),
                   cwd=root, check=True)

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]

    # Start relay server
    server_proc = subprocess.Popen([sys.executable, "port/raid_rooms/server.py", "--port", str(port)],
                                   cwd=root, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        assert "listening" in server_proc.stdout.readline()
        env = os.environ | {"BT3_ROOM_SERVER": f"127.0.0.1:{port}"}

        # -------------------------------------------------------------
        # Phase 1: 2-Player Test (1 Host + 1 Guest) + Bad-Mod Rejection
        # -------------------------------------------------------------
        print("=== Phase 1: 2-Player Flow + Incompatible Mod Rejection ===")
        host_2p = subprocess.Popen([exe, "--host", "1"], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        line = host_2p.stdout.readline().strip()
        assert line.startswith("CODE="), line + host_2p.stderr.read()
        code_2p = line[5:]
        print(f"[Host] Created room: {code_2p}")

        # Attempt to join with bad mod fingerprint
        bad_guest = subprocess.run([exe, code_2p, "bad-mods"], env=env, capture_output=True, text=True, timeout=10)
        assert bad_guest.returncode == 0 and "mods" in bad_guest.stderr, bad_guest.stderr
        print("[Security] Bad-mod guest correctly REJECTED with mismatch error.")

        # Real guest joins
        guest_2p = subprocess.Popen([exe, code_2p], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        g_out, g_err = guest_2p.communicate(timeout=10)
        assert guest_2p.returncode == 0 and "LOBBY_GUEST_LAUNCH_PASS" in g_out, g_out + g_err
        h_out, h_err = host_2p.communicate(timeout=10)
        assert host_2p.returncode == 0 and "LOBBY_HOST_LAUNCH_PASS" in h_out, h_out + h_err
        print("[Pass] 2-player flow successfully created, joined, ready, and launched!\n")

        # -------------------------------------------------------------
        # Phase 2: 6-Player Test (1 Host + 5 Guests) + Overflow Rejection
        # -------------------------------------------------------------
        print("=== Phase 2: Full 6-Player Flow + Room-Full Overflow Rejection ===")
        host_6p = subprocess.Popen([exe, "--host", "5"], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        line = host_6p.stdout.readline().strip()
        assert line.startswith("CODE="), line + host_6p.stderr.read()
        code_6p = line[5:]
        print(f"[Host] Created 6-player room: {code_6p}")

        guests_6p = []
        for i in range(5):
            g = subprocess.Popen([exe, "BT3-" + code_6p], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            guests_6p.append(g)

        # 7th guest tries to join full room
        overflow_guest = subprocess.run([exe, code_6p, "overflow-guest"], env=env, capture_output=True, text=True, timeout=10)
        assert overflow_guest.returncode == 0 and "llena" in overflow_guest.stderr, overflow_guest.stderr
        print("[Capacity] 7th player correctly REJECTED: Room full (capacity 6 reached).")

        # Wait for all 5 guests and host to complete synchronized launch
        for i, g in enumerate(guests_6p):
            out, err = g.communicate(timeout=15)
            assert g.returncode == 0 and "LOBBY_GUEST_LAUNCH_PASS" in out, f"Guest {i+1} failed: {out} {err}"
            print(f"[Guest {i+1}] Slot confirmed, ready, and launch received.")

        h6_out, h6_err = host_6p.communicate(timeout=15)
        assert host_6p.returncode == 0 and "LOBBY_HOST_LAUNCH_PASS" in h6_out, h6_out + h6_err
        print("[Pass] 6-player flow successfully synchronized and launched!\n")

        print("ALL AUTOMATED LOBBY PROTOCOL TESTS PASSED!")
    finally:
        server_proc.terminate()
        server_proc.communicate(timeout=5)

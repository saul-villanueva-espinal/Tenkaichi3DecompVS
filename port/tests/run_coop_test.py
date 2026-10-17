"""Exercise the actual UDP host/clients in four separate native processes."""
from pathlib import Path
import os
import socket
import subprocess
import tempfile
import time

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as tmp:
    exe = str(Path(tmp) / 'peer')
    subprocess.run(['cc', '-std=c11', '-D_POSIX_C_SOURCE=200809L', '-Wall', '-Wextra',
                    '-Iport/include', 'port/tests/coop_peer_test.c', 'port/src/gs/coop_net.c',
                    'port/src/plat_pc_battle.c', '-lpthread', '-o', exe], cwd=root, check=True)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.bind(('127.0.0.1', 0))
        port = s.getsockname()[1]
    env = {k: v for k, v in os.environ.items() if not k.startswith('BT3_COOP')}
    peers = []
    try:
        peers.append(subprocess.Popen([exe], env=env | {'BT3_COOP_HOST': str(port)}, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True))
        time.sleep(.1)
        bad = subprocess.run([exe], env=env | {'BT3_COOP_JOIN': f'127.0.0.1:{port}', 'FIXTURE_BAD_ASSET': '1'}, capture_output=True, text=True, timeout=10)
        assert bad.returncode == 2 and 'rechazo' in bad.stderr, bad
        for _ in range(3):
            peers.append(subprocess.Popen([exe], env=env | {'BT3_COOP_JOIN': f'127.0.0.1:{port}'}, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True))
        for p in peers:
            out, err = p.communicate(timeout=30)
            assert p.returncode == 0, out + err
            assert 'all four distinct pads' in out, out + err
        print('coop_test: real four-process UDP session, distinct inputs and mismatched-package rejection pass')
    finally:
        for p in peers:
            if p.poll() is None:
                p.kill()
                p.communicate()

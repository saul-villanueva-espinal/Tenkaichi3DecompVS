from pathlib import Path
import os, subprocess, tempfile

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as tmp:
    exe = Path(tmp) / 'visual_rng_test.exe'
    subprocess.run(['cc', '-O2', '-Iport/include', '-Iinclude',
                    'port/tests/visual_rng_test.c', 'port/src/plat_sys.c',
                    '-o', str(exe)], cwd=root, check=True)
    subprocess.run([str(exe)], cwd=root, check=True)

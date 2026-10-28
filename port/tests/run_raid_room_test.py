"""Run the actual native lobby input/profile logic against the real validator."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'src/menu/port_raid.c').read_text(encoding='utf-8')
state = source[source.index('enum { RD_LOBBY'):source.index('/* Original metallic')]
input_code = source[source.index('static s32 Rd_Check'):source.index('#include "port_versus_room.inc"')]
buttons = '\n'.join(re.findall(r'^#define PADG_.*$', (root / 'include/sys/pad.h').read_text(encoding='utf-8'), re.M))
with tempfile.TemporaryDirectory() as tmp:
    cfile, exe = Path(tmp) / 'raid.c', Path(tmp) / 'raid_test'
    fixture = (root / 'port/tests/raid_room_test.c').read_text(encoding='utf-8')
    cfile.write_text(fixture.replace('/* BUTTONS */', buttons).replace('/* ACTUAL LOBBY */', state + input_code), encoding='utf-8')
    tools = root.parent / "local-toolchain/ucrt64/bin"
    os.environ["PATH"] = str(tools) + os.pathsep + os.environ.get("PATH", "")
    cc = shutil.which("cc") or shutil.which("gcc") or str(tools / "gcc.exe")
    subprocess.run([cc, '-std=c11', '-D_POSIX_C_SOURCE=200809L', '-Wall', '-Wextra', '-Iport/include',
                    str(cfile), 'port/src/gs/coop_rooms.c', 'port/src/plat_pc_battle.c', '-lpthread'] + (['-lws2_32'] if os.name == 'nt' else []) + ['-o', str(exe)], cwd=root, check=True)
    subprocess.run([str(exe)], check=True)

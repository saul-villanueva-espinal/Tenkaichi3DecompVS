"""Run the actual native lobby input/profile logic against the real validator."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'src/menu/port_raid.c').read_text(encoding='utf-8')
state = source[source.index('enum { RD_LOBBY'):source.index('/* Original metallic')]
input_code = source[source.index('static s32 Rd_Check'):source.index('s32 PortRaid_Run')]
buttons = '\n'.join(re.findall(r'^#define PADG_.*$', (root / 'include/sys/pad.h').read_text(encoding='utf-8'), re.M))
with tempfile.TemporaryDirectory() as tmp:
    cfile, exe = Path(tmp) / 'raid.c', Path(tmp) / 'raid_test'
    fixture = (root / 'port/tests/raid_room_test.c').read_text(encoding='utf-8')
    cfile.write_text(fixture.replace('/* BUTTONS */', buttons).replace('/* ACTUAL LOBBY */', state + input_code), encoding='utf-8')
    subprocess.run(['cc', '-std=c11', '-D_POSIX_C_SOURCE=200809L', '-Wall', '-Wextra', '-Iport/include',
                    str(cfile), 'port/src/plat_pc_battle.c', '-lpthread', '-o', str(exe)], cwd=root, check=True)
    subprocess.run([str(exe)], check=True)

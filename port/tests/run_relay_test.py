from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
def function(path, declaration):
    source = (root / path).read_text()
    start = source.index(declaration)
    return source[start:source.index('\n}', start)+2]
actual = '\n'.join([
    function('src/battle/btl_char_member.c', 's32 BtlMember_Replace('),
    function('src/battle/btl_char_member.c', 's32 BtlMember_CountAlive('),
    function('src/menu/port_pc_battle.c', 'void Port_PcRelayTick('),
])
with tempfile.TemporaryDirectory() as tmp:
    src, exe = Path(tmp) / 'relay.c', Path(tmp) / 'relay'
    src.write_text((root / 'port/tests/relay_test.c').read_text().replace('/* IMPLEMENTATION */', actual))
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Iport/include',str(src),'-o',str(exe)],cwd=root,check=True)
    subprocess.run([str(exe)],check=True)

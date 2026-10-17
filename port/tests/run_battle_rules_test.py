from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as tmp:
    exe=str(Path(tmp)/'rules_test')
    subprocess.run(['cc','-std=c11','-DPORT','-Iinclude','-Iport/include',
                    'port/tests/battle_rules_test.c','src/battle/port_battle_rules.c',
                    '-o',exe],cwd=root,check=True)
    subprocess.run([exe],check=True)

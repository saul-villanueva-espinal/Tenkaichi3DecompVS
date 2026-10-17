from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as tmp:
    exe=str(Path(tmp)/'native_roster_test')
    subprocess.run(['cc','-std=c11','-D_POSIX_C_SOURCE=200809L','-Iport/include','port/tests/native_roster_test.c','port/src/plat_pc_battle.c','port/src/plat_native_roster.c','port/src/plat_settings.c','-lpthread','-o',exe],cwd=root,check=True)
    env=dict(os.environ,BT3_SETTINGS=str(Path(tmp)/'settings.txt'))
    for phase in ['write','read']:
        subprocess.run([exe,phase],env=env,check=True)

"""Compile the real grid-builder C code against tiny menu/save fixtures, without disc data."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'src/battle/view_a_e.c').read_text()
macro_start = source.index('#define CHARA_UNLOCKED')
macro_end = source.index('#if 0', macro_start)
start = source.index('void ChrGrid_Build(')
end = source.index('\n#else\nINCLUDE_ASM("asm/nonmatchings/battle/view_a_e", ChrGrid_Build);', start)
wrapper_start = source.index('s32 ChrGrid_PortSaveIndex(')
wrapper_end = source.index('\n#endif', wrapper_start)
implementation = source[macro_start:macro_end] + source[start:end] + '\n' + source[wrapper_start:wrapper_end]
fixture = (root / 'port/tests/grid_test.c').read_text()
with tempfile.TemporaryDirectory() as tmp:
    c = Path(tmp) / 'grid.c'
    exe = Path(tmp) / 'grid_test'
    c.write_text(fixture.replace('/* IMPLEMENTATION */', implementation))
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Iinclude', str(c), '-o', str(exe)], cwd=root, check=True)
    subprocess.run([str(exe)], cwd=root, check=True)
    # The roster can be increased at one build setting without overlapping file namespaces.
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-DPORT_TOTAL_CHARAS=512', '-Iinclude', str(c), '-o', str(exe)], cwd=root, check=True)
    subprocess.run([str(exe)], cwd=root, check=True)
    selection = (root / 'src/menu/menu_e_b.c').read_text()
    start = selection.index('s32 TeamSel_IsCharaFree(')
    end = selection.index('\n}', start) + 2
    duplicates = (root / 'port/tests/duplicates_test.c').read_text().replace('/* IMPLEMENTATION */', selection[start:end])
    c.write_text(duplicates)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', str(c), '-o', str(exe)], cwd=root, check=True)
    subprocess.run([str(exe)], cwd=root, check=True)

"""Check the actual grid movement, hit regions and all sixteen destinations."""
from pathlib import Path
import tempfile,subprocess
root=Path(__file__).resolve().parents[2];src=(root/'src/menu/port_main_manga.inc').read_text()
def function(name):
 at=src.index(name+'(');start=src.rfind('\n',0,at)+1;end=src.index('{',at)+1;depth=1
 while depth:depth+=(src[end]=='{')-(src[end]=='}');end+=1
 return src[start:end]
start=src.index('static const s16 sMangaBounds');end=src.index('};',start)+2
test='''#include <assert.h>
#include <stdio.h>
typedef int s32;typedef short s16;typedef unsigned u32;typedef float f32;
enum {PADG_LEFT=1,PADG_RIGHT=2,PADG_DOWN=4,PADG_UP=8,PADG_LEFT2=0x200000,PADG_RIGHT2=0x400000,PADG_DOWN2=0x800000,PADG_UP2=0x1000000};
'''+src[start:end]+'\n'+'\n'.join(function(n) for n in ('Mm_X','Mm_Y','Mm_Move','Mm_Hit','Mm_Target'))+'''
int main(void){
 int i,j,x,y;int expected[16]={6,13,33,38,-1,80,48,44,53,60,62,38,40,40,80,80};
 for(i=0;i<16;i++){
  assert(Mm_Target(i)==expected[i]);
  assert(Mm_Move(Mm_Move(i,PADG_LEFT),PADG_RIGHT)==i);
  assert(Mm_Move(Mm_Move(i,PADG_UP),PADG_DOWN)==i);
  j=i;for(x=0;x<4;x++)j=Mm_Move(j,PADG_RIGHT);assert(j==i);
  x=(int)Mm_X((sMangaBounds[i][0]+sMangaBounds[i][2])/2);
  y=(int)Mm_Y((sMangaBounds[i][1]+sMangaBounds[i][3])/2);assert(Mm_Hit(x,y)==i);
 }
 assert(Mm_Hit(0,0)==-1&&Mm_Hit(511,447)==-1);
 puts("PASS: 16 real destinations, 16 hit regions, 4x4 wrap navigation");return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
 f=Path(tmp)/'main.c';f.write_text(test);exe=Path(tmp)/'main.exe'
 subprocess.run(['cc','-std=c11',str(f),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)

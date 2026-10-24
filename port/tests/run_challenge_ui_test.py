"""Exercise real menu hit regions and the limits displayed for each format."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
src=(root/'src/menu/port_challenge_ui.inc').read_text()
def function(name):
    at=src.index(name+'(');start=src.rfind('\n',0,at)+1;end=src.index('{',at)+1;depth=1
    while depth:
        depth+=(src[end]=='{')-(src[end]=='}');end+=1
    return src[start:end]
test='''#include <assert.h>
#include <stdio.h>
typedef int s32;typedef unsigned u32;typedef float f32;
enum {PADG_CROSS=0x200,PADG_CIRCLE=0x100,PADG_LEFT=1,PADG_RIGHT=2,PADG_R1=0x10000};
static int sCustomFormat,sCustomEnemyNum,sCustomCur;
static float Ps_X(float x){return x;}static float Ps_Y(float y){return y;}
static void Snd_PlaySe(int a,int b){(void)a;(void)b;}
'''
test+='\n'.join(function(n) for n in ('Ch_PlayerCount','Ch_EnemyCount','Challenge_Pointer'))
test+='''
int main(void){int tab,floor,i,n;unsigned p;
 for(sCustomFormat=0;sCustomFormat<3;sCustomFormat++)for(n=1;n<=10;n++){
  sCustomEnemyNum=n;assert(Ch_PlayerCount()==(sCustomFormat==0?10:sCustomFormat==1?5:1));
  assert(Ch_EnemyCount()==(sCustomFormat==2?1:sCustomFormat==1&&n>5?5:n));
 }
 for(i=0;i<5;i++){tab=0;floor=-1;p=0;Challenge_Pointer(0,60,90+(4-i)*64,&tab,&floor,&p);assert(floor==i&&!p);}
 for(i=0;i<3;i++){tab=0;floor=0;p=0;Challenge_Pointer(0,245+i*90,30,&tab,&floor,&p);assert(tab==i);}
 for(i=0;i<7;i++){
  tab=2;floor=0;p=0;Challenge_Pointer(2,172,100+i*34.8f,&tab,&floor,&p);assert(sCustomCur==i&&(p&PADG_RIGHT));
  p=0;Challenge_Pointer(2,86,100+i*34.8f,&tab,&floor,&p);assert(sCustomCur==i&&(p&PADG_LEFT));
 }
 for(i=0;i<3;i++){tab=i;floor=0;p=0;Challenge_Pointer(i,450,370,&tab,&floor,&p);assert(p&PADG_CROSS);}
 puts("PASS: five floors, three tabs, seven editors, launch buttons and accurate 1/5/10 team limits");return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp)/'challenge.c';p.write_text(test);exe=Path(tmp)/'challenge.exe'
    subprocess.run(['cc','-std=c11',str(p),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)

"""Exercise the native carousel's real list, page and portrait selection code."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[2]
source=(root/'src/menu/port_roster.c').read_text()
def function(name):
    a=source.index(name)
    return source[a:source.index('\n}',a)+2]
actual='\n'.join(function(n) for n in ['static s32 Nr_GetSaga(', 'static void Nr_List(', 'static s32 Nr_Id(', 'static void Nr_PageShift(', 'static void Nr_Face('])
fixture=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "port_pc_battle.h"
typedef int s32;
#define NR_PAGE 7
#define NR_SAGA_ALL 0
#define NR_SAGA_Z 1
#define NR_SAGA_SUPER 2
#define NR_SAGA_GT 3
#define NR_SAGA_MOVIES 4
#define NR_SAGA_CLASSIC 5
#define NR_SAGA_FAV 6
#define PORT_CHARA_ADDED(id) ((id)>=PORT_CHARA_FIRST)
static PcBattle sNr;
static int sNrKind,sNrSlot[2],sNrFilled[2][10],sNrCursor[2],sNrSaga,sNrCostume[2];
static int sNrList[PORT_TOTAL_CHARAS],sNrCount,sNrFavorite[PORT_CHARA_ID_LIMIT];
static char sNrMessage[160];
struct Side {int chara,cur;struct {int color;} member[1];};
static struct Side sides[2];
static struct {struct Side *side[2];} team;
#define gTeamSel (&team)
static int loads[2];
int Port_PcRosterId(int i) {return i<0||i>=169?-1:i<161?i:PORT_CHARA_FIRST+i-161;}
int Ps_Wrap(int v,int lo,int hi) {return v<lo?hi:v>hi?lo:v;}
void TeamSel_RequestFace(int s) {loads[s]++;}
/* IMPLEMENTATION */
int main(void) {
    int i,seen[PORT_TOTAL_CHARAS]={0};
    team.side[0]=&sides[0];team.side[1]=&sides[1];
    Nr_List();assert(sNrCount==PORT_TOTAL_CHARAS);
    for(i=0;i<PORT_TOTAL_CHARAS;i++){assert(sNrList[i]==i);sNrCursor[0]=i;seen[sNrList[sNrCursor[0]]]++;}
    for(i=0;i<PORT_TOTAL_CHARAS;i++)assert(seen[i]==1);
    sNrCursor[0]=0;sNrCursor[1]=4;
    Nr_PageShift(0,-1);assert(sNrCursor[0]==287&&sNrCursor[1]==4);
    sNrCursor[0]=289;assert(Nr_Id(0)==-1);
    Nr_PageShift(0,1);assert(sNrCursor[0]==2&&sNrCursor[1]==4);
    Nr_PageShift(1,1);assert(sNrCursor[1]==11&&sNrCursor[0]==2);
    sNrFilled[0][0]=1;sNr.fighter[0][0].chara=100;
    sNrCostume[0]=3;Nr_Face();assert(sides[0].chara==2&&sides[0].member[0].color==3);
    assert(loads[0]==1);Nr_Face();assert(loads[0]==1);
    sNrCursor[0]=289;Nr_Face();assert(sides[0].chara==-1);
    sNrCursor[0]=0;Nr_Face();assert(sides[0].chara==0);
    sNrKind=3;Nr_Face();assert(sides[1].chara==-1);
    puts("classic_roster: all 290 positions reachable; independent carousels wrap; live portrait follows cursor even in a saved team; empty slots never load a fighter");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    c=Path(tmp)/'classic.c';exe=Path(tmp)/'classic'
    c.write_text(fixture.replace('/* IMPLEMENTATION */',actual))
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Iport/include',str(c),'-o',str(exe)],cwd=root,check=True)
    subprocess.run([str(exe)],check=True)

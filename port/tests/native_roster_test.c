#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "port_pc_battle.h"
int Port_CharaCount(void) {return 8;}
const char *Port_FighterName(int c) {(void)c;return "Cell Junior";}
int Port_ExtraCostumeOk(int c,int t,char *why,int n) {(void)c;(void)why;(void)n;return t<2;}
int Port_NetActive(void) {return 0;}
int Port_NetSession(void) {return 0;}
int Port_ModesFight(void) {return 0;}
int ChrTbl_GetCost(int c) {(void)c;return 2;}
extern void Port_RosterName(char *,int,int);
int main(int argc,char **argv) {
    PcBattle b,got;int s,i;char name[80];
    assert(argc==2);Port_PcBattleDefault(&b);b.mode=PC_RELAY;b.count[0]=b.count[1]=10;
    for(s=0;s<2;s++)for(i=0;i<10;i++){b.fighter[s][i].chara=s?79:109;b.fighter[s][i].costume=i%2;}
    if(!strcmp(argv[1],"write")) {
        Port_RosterRemember(&b);
        for(i=0;i<Port_PcRosterCount();i++)assert(Port_RosterFavorite(Port_PcRosterId(i),1));
    } else {
        assert(Port_RosterRecall(PC_RELAY,&got)&&!memcmp(&b,&got,sizeof(b)));
        for(i=0;i<Port_PcRosterCount();i++)assert(Port_RosterFavorite(Port_PcRosterId(i),0));
        assert(!Port_RosterFavorite(109,1)&&!Port_RosterFavorite(109,0));
        assert(Port_RosterFavorite(79,0));
    }
    Port_RosterName(name,sizeof(name),109);assert(!strcmp(name,"Cell Junior"));
    puts("native_roster_test: complete rosters and 169 favorites survive process restart");return 0;
}

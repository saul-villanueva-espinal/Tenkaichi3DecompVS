#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "port_pc_battle.h"
extern volatile int gPortPcHubReady;
int Port_CharaCount(void) { return 8; }
const char *Port_FighterName(int c) { (void)c; return "fixture"; }
int Port_ExtraCostumeOk(int c,int costume,char *why,int size) { (void)c;(void)why;(void)size;return costume<2; }
int Port_NetActive(void) { return 0; }
int Port_NetSession(void) { return 0; }
int main(void) {
    PcBattle b,got;char why[160];int s,i;
    Port_PcBattleDefault(&b);
    assert(Port_PcBattleValidate(&b,why,sizeof(why)));
    assert(!Port_PcBattleRequest(&b));
    gPortPcHubReady=1;
    b.mode=PC_RELAY;b.count[0]=b.count[1]=10;
    for(s=0;s<2;s++) for(i=0;i<10;i++) {b.fighter[s][i].chara=109;b.fighter[s][i].costume=i%2;}
    assert(Port_PcBattleValidate(&b,why,sizeof(why)));
    assert(Port_PcBattleRequest(&b)&&Port_PcBattlePending());
    assert(Port_PcBattleTake(&got)&&memcmp(&b,&got,sizeof(b))==0&&!Port_PcBattlePending());
    b.count[0]=11;assert(!Port_PcBattleValidate(&b,why,sizeof(why)));b.count[0]=10;
    b.fighter[0][9].chara=PORT_CHARA_FIRST+8;assert(!Port_PcBattleValidate(&b,why,sizeof(why)));
    b.fighter[0][9].chara=PORT_CHARA_FIRST+7;assert(Port_PcBattleValidate(&b,why,sizeof(why)));
    b.fighter[0][9].costume=7;assert(!Port_PcBattleValidate(&b,why,sizeof(why)));
    b.mode=PC_RAID;b.players=4;b.count[0]=4;b.count[1]=1;
    assert(Port_PcBattleValidate(&b,why,sizeof(why)));
    b.count[0]=3;assert(!Port_PcBattleValidate(&b,why,sizeof(why)));
    assert(Port_PcRosterCount()==169&&Port_PcRosterId(161)==198&&Port_PcRosterId(169)==-1);
    puts("pc_battle_test: ten-member teams, duplicates, costumes, four allies and request isolation pass");
}

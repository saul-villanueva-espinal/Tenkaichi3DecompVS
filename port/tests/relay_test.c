#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "port_pc_battle.h"
typedef int s32;
typedef float f32;
#define BTL_MEMBER_SLOTS 5
#define BtlUtil_Max(a,b) ((a)>(b)?(a):(b))
typedef struct { int health; } Gauge;
typedef struct { int chara,costume,present,bonus[7],ability[4],cpuLevel,aiType; Gauge gauge; } BtlMember;
typedef struct { int active,memberCount; BtlMember members[5]; } BtlMemberChr;
typedef struct { int chara,costume,variant,cpuLevel;float health;int bonus[8],ability[4],aiType; } BattleMember;
typedef BattleMember BtlMemberSetup;
static PcBattle sBattle;
static int sRelay,sNext[2],sRemaining[2],loaded[2];
static BtlMemberChr fighters[2];
static BattleMember setup[2][5];
BtlMemberChr *BtlChar_FindByObjId(int s) { return &fighters[s]; }
BattleMember *BattleSide_GetMember(int s,int slot) { return &setup[s][slot]; }
BtlMember *BtlMember_Get(BtlMemberChr *c,int n) { return &c->members[n]; }
Gauge *BtlMember_GetGauge(BtlMemberChr *c,int n) { return &c->members[n].gauge; }
void BtlMember_LoadParams(BtlMemberChr *c,int n,int init,float pct,int variant) { (void)init;(void)variant;c->members[n].gauge.health=(int)pct; }
void BtlMember_Init(BattleMember *m,int chara,int costume,int variant,int level,float hp,void *items) {
    (void)items;memset(m,0,sizeof(*m));m->chara=chara;m->costume=costume;m->variant=variant;m->cpuLevel=level;m->health=hp;
}
void Port_PcRelayLog(int s,int entered,int chara) { (void)entered;(void)chara;loaded[s]++; }
/* IMPLEMENTATION */
int main(void) {
    int s,i,defeated,seen[2][10]={{0}};
    sRelay=1;
    for(s=0;s<2;s++) {
        sBattle.count[s]=10;sNext[s]=5;fighters[s].memberCount=5;
        for(i=0;i<10;i++) {sBattle.fighter[s][i].chara=s*20+i;sBattle.fighter[s][i].costume=i%2;}
        for(i=0;i<5;i++) {fighters[s].members[i].chara=s*20+i;fighters[s].members[i].present=1;fighters[s].members[i].gauge.health=100;}
    }
    Port_PcRelayTick();assert(sRemaining[0]==10&&sRemaining[1]==10);
    for(s=0;s<2;s++) for(defeated=0;defeated<10;defeated++) {
        BtlMemberChr *c=&fighters[s];BtlMember *m=&c->members[c->active];
        assert(m->gauge.health==100);seen[s][m->chara-s*20]++;
        m->gauge.health=0;Port_PcRelayTick();
        assert(sRemaining[s]==9-defeated);
        for(i=0;i<5;i++) if(c->members[i].gauge.health>0) break;
        if(defeated<9) {assert(i<5);c->active=i;Port_PcRelayTick();assert(sRemaining[s]==9-defeated);}
        else assert(i==5);
    }
    assert(loaded[0]==5&&loaded[1]==5);
    for(s=0;s<2;s++) for(i=0;i<10;i++) assert(seen[s][i]==1);
    puts("relay_test: real bench replacement consumes all ten members on both sides, preserves active fighter, ends after ten KOs");
}

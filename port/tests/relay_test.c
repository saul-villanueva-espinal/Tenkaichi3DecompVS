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
typedef struct { int chara,costume,variant,cpuLevel;float health;int bonus[8],ability[4],aiType;void *data,*buf[2]; } BattleMember;
typedef struct { int chara,costume,variant; } Form;
typedef struct { BattleMember members[5];int memberCount,lead;Form form,startForm; } BattleSide;
typedef struct { BattleSide sides[2]; } BattleSetup;
typedef BattleMember BtlMemberSetup;
static PcBattle sBattle;
static int sRelay,sRelayMask=3,sNext[2],sRemaining[2],loaded[2],sLevel[2]={13,13};
static BtlMemberChr fighters[2];
static BattleSetup setup;
static char buffers[2][5][2];
BattleSetup *Battle_GetSetup(void) { return &setup; }
BtlMemberChr *BtlChar_FindByObjId(int s) { return &fighters[s]; }
BattleMember *BattleSide_GetMember(int s,int slot) { return &setup.sides[s].members[slot]; }
BtlMember *BtlMember_Get(BtlMemberChr *c,int n) { return &c->members[n]; }
Gauge *BtlMember_GetGauge(BtlMemberChr *c,int n) { return &c->members[n].gauge; }
void BtlMember_LoadParams(BtlMemberChr *c,int n,int init,float pct,int variant) { (void)init;(void)variant;c->members[n].gauge.health=(int)pct; }
void BtlMember_Init(BattleMember *m,int chara,int costume,int variant,int level,float hp,void *items) {
    (void)items;memset(m,0,sizeof(*m));m->chara=chara;m->costume=costume;m->variant=variant;m->cpuLevel=level;m->health=hp;
}
int Port_PcTestHealth(void) { return 100; }
void Port_PcRelayLog(int s,int entered,int chara) { (void)entered;(void)chara;loaded[s]++; }
static void check_buffers(void) {
    int s,i;
    for(s=0;s<2;s++) for(i=0;i<5;i++) {
        BattleMember *m=BattleSide_GetMember(s,i);
        assert(m->buf[0]==&buffers[s][i][0]);
        assert(m->buf[1]==&buffers[s][i][1]);
        assert(m->data==m->buf[1]);
    }
}
/* IMPLEMENTATION */
int main(void) {
    int s,i,defeated,seen[2][10]={{0}};
    sRelay=1;
    for(s=0;s<2;s++) {
        sBattle.count[s]=10;sNext[s]=5;fighters[s].memberCount=5;
        for(i=0;i<10;i++) {sBattle.fighter[s][i].chara=s*20+i;sBattle.fighter[s][i].costume=i%2;}
        for(i=0;i<5;i++) {fighters[s].members[i].chara=s*20+i;fighters[s].members[i].present=1;fighters[s].members[i].gauge.health=100;}
        for(i=0;i<5;i++) {BattleMember *m=BattleSide_GetMember(s,i);m->buf[0]=&buffers[s][i][0];m->buf[1]=&buffers[s][i][1];m->data=m->buf[1];}
    }
    Port_PcRelayTick();assert(sRemaining[0]==10&&sRemaining[1]==10);
    for(s=0;s<2;s++) for(defeated=0;defeated<10;defeated++) {
        BtlMemberChr *c=&fighters[s];BtlMember *m=&c->members[c->active];
        assert(m->gauge.health==100);seen[s][m->chara-s*20]++;
        m->gauge.health=0;Port_PcRelayTick();
        check_buffers();
        assert(sRemaining[s]==9-defeated);
        for(i=0;i<5;i++) if(c->members[i].gauge.health>0) break;
        if(defeated<9) {assert(i<5);c->active=i;Port_PcRelayTick();assert(sRemaining[s]==9-defeated);}
        else assert(i==5);
    }
    assert(loaded[0]==5&&loaded[1]==5);
    for(s=0;s<2;s++) for(i=0;i<10;i++) assert(seen[s][i]==1);
    setup.sides[0].form.chara=99;setup.sides[1].form.chara=88;
    Port_PcRelayRewind();check_buffers();
    assert(setup.sides[0].form.chara==99&&setup.sides[1].form.chara==88);
    for(s=0;s<2;s++) {assert(sRemaining[s]==10&&sNext[s]==5);assert(setup.sides[s].startForm.chara==s*20);for(i=0;i<5;i++)assert(BattleSide_GetMember(s,i)->chara==s*20+i);}
    sRelayMask=1;setup.sides[1].members[0].chara=123;
    Port_PcRelayRewind();check_buffers();assert(setup.sides[1].members[0].chara==123);
    puts("relay_test: twenty KOs and rematch preserve all loader buffers and loaded-form detection; player-only rewind preserves CPU team");
}

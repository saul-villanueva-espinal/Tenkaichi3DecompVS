#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "port_pc_battle.h"
typedef int s32;
typedef float f32;
#define BTL_MEMBER_SLOTS 5
#define BtlUtil_Max(a,b) ((a)>(b)?(a):(b))
typedef struct { int health,ki,variant; } Gauge;
typedef struct { int chara,costume,present,bonus[7],ability[4],cpuLevel,aiType; Gauge gauge; } BtlMember;
typedef struct { int side,active,memberCount,switchTarget; BtlMember members[5]; } BtlMemberChr;
typedef struct { int chara,costume,variant,cpuLevel;float health;int bonus[8],ability[4],aiType;void *data,*buf[2]; } BattleMember;
typedef struct { int chara,costume,variant; } Form;
typedef struct { BattleMember members[5];int memberCount,lead;Form form,startForm; } BattleSide;
typedef struct { BattleSide sides[2]; } BattleSetup;
typedef BattleMember BtlMemberSetup;
static PcBattle sBattle;
static int sRelay,sRelayMask=3,sNext[2],sRemaining[2],loaded[2],sLevel[2]={13,13};
static BtlMember sReserve[2][PORT_RELAY_MAX];
static BattleMember sReserveSetup[2][PORT_RELAY_MAX];
static int sReserveValid[2][PORT_RELAY_MAX],sSlotIndex[2][5],sBankReady[2];
static BtlMemberChr fighters[2];
static BattleSetup setup;
static int buffers[2][5][2],portraitLoads;
#define CHARA_MEMBER_FILE_COSTUME(c,k,s) ((c)*100+(k)*2+(s))
void *File_LoadSync(int file,void *buf,int unused) { (void)unused;*(int *)buf=file;portraitLoads++;return buf; }
void Res_RelocateOffsets(void *out,void *base,void *hdr) { (void)out;(void)base;(void)hdr; }
BattleSetup *Battle_GetSetup(void) { return &setup; }
BtlMemberChr *BtlChar_FindByObjId(int s) { return &fighters[s]; }
BattleMember *BattleSide_GetMember(int s,int slot) { return &setup.sides[s].members[slot]; }
BtlMember *BtlMember_Get(BtlMemberChr *c,int n) { return &c->members[n]; }
Gauge *BtlMember_GetGauge(BtlMemberChr *c,int n) { return &c->members[n].gauge; }
void BtlMember_LoadParams(BtlMemberChr *c,int n,int init,float pct,int variant) { (void)init;c->members[n].gauge.health=(int)pct;c->members[n].gauge.variant=variant;c->members[n].gauge.ki=100; }
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
        assert(m->data==m->buf[0] || m->data==m->buf[1]);
    }
}
/* IMPLEMENTATION */
static void init_teams(void) {
    int s,i;
    memset(fighters,0,sizeof(fighters));memset(setup.sides,0,sizeof(setup.sides));
    sBankReady[0]=sBankReady[1]=loaded[0]=loaded[1]=0;
    sRelay=1;
    for(s=0;s<2;s++) {
        sBattle.count[s]=10;sNext[s]=5;fighters[s].memberCount=5;fighters[s].side=s;fighters[s].switchTarget=1;
        for(i=0;i<10;i++) {sBattle.fighter[s][i].chara=s*20+i;sBattle.fighter[s][i].costume=i%2;}
        for(i=0;i<5;i++) {fighters[s].members[i].chara=s*20+i;fighters[s].members[i].present=1;fighters[s].members[i].gauge.health=100;}
        for(i=0;i<5;i++) {BattleMember *m=BattleSide_GetMember(s,i);m->buf[0]=&buffers[s][i][0];m->buf[1]=&buffers[s][i][1];m->data=m->buf[1];}
        for(i=0;i<5;i++) {BattleMember *m=BattleSide_GetMember(s,i);m->chara=s*20+i;m->costume=i%2;}
    }
}
int main(void) {
    int s,i,defeated,seen[2][10]={{0}};
    init_teams();
    /* Both pads can browse the entire team before the first KO. Each side is
       independent and slot identity is independent of duplicate character IDs. */
    Port_PcRelayTick();
    for(s=0;s<2;s++) {
        BtlMemberChr *c=&fighters[s];int visited[10]={0},reverse[10]={0},k,slot;
        for(k=0;k<9;k++) {
            BtlMember_NextSwitchTarget(c);i=sSlotIndex[s][c->switchTarget];visited[i]++;
            assert(i!=0);assert(c->members[c->active].chara==s*20);
            assert(*(int *)BattleSide_GetMember(s,c->switchTarget)->data==CHARA_MEMBER_FILE_COSTUME(s*20+i,i%2,s));
        }
        for(i=1;i<10;i++) assert(visited[i]==1);
        while(sSlotIndex[s][c->switchTarget]!=6) BtlMember_NextSwitchTarget(c);
        slot=c->switchTarget;c->members[slot].gauge.health=37;c->members[slot].gauge.ki=81;c->members[slot].gauge.variant=2;
        for(k=0;k<9;k++) BtlMember_NextSwitchTarget(c);
        slot=c->switchTarget;assert(sSlotIndex[s][slot]==6);
        assert(c->members[slot].gauge.health==37&&c->members[slot].gauge.ki==81&&c->members[slot].gauge.variant==2);
        c->active=slot;
        for(k=0;k<9;k++) {
            BtlMember_PrevSwitchTarget(c);i=sSlotIndex[s][c->switchTarget];reverse[i]++;
            assert(sSlotIndex[s][c->active]==6&&c->members[c->active].gauge.health==37);
        }
        for(i=0;i<10;i++) assert(reverse[i]==(i!=6));
        Port_PcRelayTick();assert(sRemaining[s]==10);check_buffers();
    }
    init_teams();
    for(s=0;s<2;s++) for(i=0;i<10;i++) sBattle.fighter[s][i].chara=s*20;
    for(s=0;s<2;s++) for(i=0;i<5;i++) fighters[s].members[i].chara=s*20;
    for(s=0;s<2;s++) {
        BtlMemberChr *c=&fighters[s];int visited[10]={0},k;
        for(k=0;k<9;k++) {BtlMember_NextSwitchTarget(c);visited[sSlotIndex[s][c->switchTarget]]++;}
        for(i=1;i<10;i++) assert(visited[i]==1);
    }
    init_teams();
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
    assert(portraitLoads>10);
    puts("relay_test: both pads browse all ten before any KO; duplicate IDs, state, portraits, twenty KOs, rematch and loader buffers preserved");
}

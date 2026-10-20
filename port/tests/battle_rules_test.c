#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "battle/battle.h"
#include "battle/btl_char_member.h"
#include "port_pc_battle.h"
static BattleWork work;
static BtlMemberChr fighters[11];
static int fighterCount=3,attacker=1;
static int fighting=1,net;
int BtlSeq_IsFighting(void){return fighting;}
int BtlChar_GetCount(void){return fighterCount;}
int BtlOpp_GetObjId(BtlMemberChr *c){(void)c;return attacker;}
BtlMemberChr *BtlChar_Get(int i){return &fighters[i];}
BtlMemberChr *BtlChar_FindByObjId(int id){int i;for(i=0;i<fighterCount;i++)if(fighters[i].objId==id)return fighters+i;return NULL;}
BtlMemberGauge *BtlMember_GetActiveGauge(BtlMemberChr *c){return &c->members[c->active].gauge;}
BattleWork *Battle_GetWork(void){return &work;}
int Port_NetActive(void){return net;}
int Port_NetSession(void){return 0;}
int Port_ModesFight(void){return 0;}
extern void Port_BattleRulesSet(const PcBattle *),Port_BattleRulesReset(void),Port_BattleRulesTick(void);
extern void Port_BattleRulesGauge(BtlMemberChr *,BtlMemberGauge *,int);
extern int Port_BattleRulesDamage(BtlMemberChr *,int),Port_BattleRulesKi(BtlMemberChr *,int),Port_BattleRulesArmor(int);
static void ticks(int n){while(n--)Port_BattleRulesTick();}
int main(void){
    PcBattle b={0};int i;
    for(i=0;i<3;i++){
        BtlMemberGauge *g;
        fighters[i].objId=i;fighters[i].side=i==0?0:1;
        fighters[i].members[0].chara=i==1?117:0;
        g=BtlMember_GetActiveGauge(&fighters[i]);g->healthMax=30000;g->health=10000;g->kiMax=100000;
    }
    b.mode=PC_RELAY;b.rules.damagePct=125;b.rules.kiPct=150;
    b.rules.regenPerSec=150;b.rules.regenSides=2;b.rules.infiniteKiChara1=118;b.rules.armorSides=2;
    Port_BattleRulesSet(&b);
    assert(Port_BattleRulesDamage(fighters,1000)==1250);
    assert(Port_BattleRulesDamage(fighters,0)==0 && Port_BattleRulesDamage(fighters,-5)==-5);
    assert(Port_BattleRulesKi(fighters,1000)==1500);
    assert(Port_BattleRulesArmor(1)==1 && Port_BattleRulesArmor(0)==0);
    ticks(30);assert(fighters[1].members[0].gauge.health==10150);
    assert(fighters[1].members[0].gauge.ki==100000 && fighters[0].members[0].gauge.ki==0);
    assert(fighters[0].members[0].gauge.health==10000);
    work.flags=BATTLE_FLAG_PAUSE;ticks(60);assert(fighters[1].members[0].gauge.health==10150);
    work.flags=0;fighting=0;ticks(60);assert(fighters[1].members[0].gauge.health==10150);fighting=1;
    fighters[1].members[0].gauge.health=0;ticks(30);assert(!fighters[1].members[0].gauge.health);
    fighters[1].members[0].gauge.health=29999;ticks(30);assert(fighters[1].members[0].gauge.health==30000);
    memset(&b,0,sizeof(b));b.mode=PC_RAID;b.rules.bossHp=162500;b.rules.bossDamagePct=200;b.rules.enragePct=50;
    b.rules.healthPct=25;b.rules.armorSides=2;
    Port_BattleRulesSet(&b);
    for(i=0;i<3;i++){fighters[i].members[0].gauge.health=10000;Port_BattleRulesGauge(fighters+i,BtlMember_GetActiveGauge(fighters+i),1);}
    assert(fighters[1].members[0].gauge.health==162500);
    assert(fighters[2].members[0].gauge.healthMax==30000 && fighters[2].members[0].gauge.health==2500);
    assert(Port_BattleRulesArmor(2)==0 && Port_BattleRulesArmor(1)==1);
    assert(Port_BattleRulesDamage(fighters,1000)==2000);
    fighters[1].members[0].gauge.health=81250;
    assert(Port_BattleRulesDamage(fighters,1000)==3000);
    assert(Port_BattleRulesDamage(fighters+1,1000)==1000);
    assert(Port_BattleRulesKi(fighters+1,1000)==1500);
    fighters[1].members[0].gauge.healthMax=30000;
    Port_BattleRulesGauge(fighters+1,BtlMember_GetActiveGauge(fighters+1),0);
    assert(fighters[1].members[0].gauge.healthMax==162500 && fighters[1].members[0].gauge.health==81250);
    Port_BattleRulesSet(NULL);assert(Port_BattleRulesDamage(fighters,1000)==1000 && !Port_BattleRulesArmor(1));
    b.rules.damagePct=400;b.rules.bossDamagePct=400;Port_BattleRulesSet(&b);
    assert(Port_BattleRulesDamage(fighters,1000000)==24000000);
    memset(&b,0,sizeof(b));b.mode=PC_RAID;b.count[0]=6;b.count[1]=5;b.rules.bossHp=150000;
    b.rules.bossDamagePct=200;b.rules.enragePct=50;
    for(i=0;i<4;i++) {b.bossRules[i].bossHp=100000+i*10000;b.bossRules[i].bossDamagePct=125+i*25;b.bossRules[i].enragePct=50;}
    fighterCount=11;Port_BattleRulesSet(&b);
    for(i=0;i<11;i++) {fighters[i].objId=i;Port_BattleRulesGauge(fighters+i,BtlMember_GetActiveGauge(fighters+i),1);}
    assert(fighters[1].members[0].gauge.healthMax==150000);
    for(i=7;i<11;i++) assert(fighters[i].members[0].gauge.healthMax==100000+(i-7)*10000);
    attacker=10;assert(Port_BattleRulesDamage(fighters,1000)==2000);
    fighters[10].members[0].gauge.health=65000;
    assert(Port_BattleRulesDamage(fighters,1000)==3000);
    attacker=7;assert(Port_BattleRulesDamage(fighters,1000)==1250);
    net=1;Port_BattleRulesSet(&b);assert(Port_BattleRulesDamage(fighters,1000)==1000);
    puts("battle_rules_test: damage, ki, regeneration, pause, KO, boss identity, fury, transformations and reset pass");
}

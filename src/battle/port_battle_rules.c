#include "common.h"
#ifdef PORT
#include "battle/battle.h"
#include "battle/btl_char_member.h"
#include "port_pc_battle.h"
extern s32 BtlSeq_IsFighting(void),BtlChar_GetCount(void),BtlOpp_GetObjId(BtlMemberChr *);
extern BtlMemberChr *BtlChar_Get(s32),*BtlChar_FindByObjId(s32);
extern s32 Port_NetActive(void),Port_NetSession(void),Port_ModesFight(void);
static PcBattleRules sRules,sBossRules[PORT_RAID_BOSSES];
static s32 sRaid,sTicks,sAllies,sBossCount;
void Port_BattleRulesReset(void) {sTicks=0;}
void Port_BattleRulesSet(const PcBattle *battle) {
    static const PcBattleRules normal={0};s32 i;
    sRules=normal;sRaid=0;sTicks=0;sAllies=2;sBossCount=1;
    if(battle && !Port_NetActive() && !Port_NetSession()) {
        sRules=battle->rules;sRaid=battle->mode==PC_RAID;
        if(battle->count[0]>0) sAllies=battle->count[0];
        if(battle->count[1]>0 && battle->count[1]<=PORT_RAID_BOSSES) sBossCount=battle->count[1];
    }
    for(i=0;i<PORT_RAID_BOSSES;i++) {
        sBossRules[i]=sRules;
        if(i && battle && battle->bossRules[i-1].bossHp) sBossRules[i]=battle->bossRules[i-1];
    }
}
static s32 RulesBoss(s32 objId) {
    s32 n=objId==1?0:objId-sAllies;
    return sRaid && (objId==1 || n>=1) && n<sBossCount ? n:-1;
}
s32 Port_BattleRulesIsBoss(s32 objId) {return RulesBoss(objId)>=0;}
static s32 RulesSide(s32 objId,s32 side) {return sRaid?RulesBoss(objId)>=0:side;}
static s32 RulesEnraged(s32 index) {
    BtlMemberChr *boss;BtlMemberGauge *g;s32 id=index? sAllies+index:1;
    if(!sRaid || index<0 || !sBossRules[index].enragePct) return 0;
    boss=BtlChar_FindByObjId(id);if(!boss) return 0;g=BtlMember_GetActiveGauge(boss);
    return g->health>0 && g->health*100<=g->healthMax*sBossRules[index].enragePct;
}
s32 Port_BattleRulesEnraged(void) {s32 i;for(i=0;i<sBossCount;i++) if(RulesEnraged(i)) return 1;return 0;}
void Port_BattleRulesGauge(BtlMemberChr *chr,BtlMemberGauge *g,s32 init) {
    s32 n=RulesBoss(chr->objId);
    if(n>=0) {
        g->healthMax=sBossRules[n].bossHp?sBossRules[n].bossHp:60000;
        if(init) {g->health=g->healthMax;g->ki=g->kiMax;g->blast=g->blastMax;}
    } else if(init && sRules.healthPct && !RulesSide(chr->objId,chr->side)) {
        g->health=g->health*sRules.healthPct/100;if(g->health<1) g->health=1;
    }
}
s32 Port_BattleRulesDamage(BtlMemberChr *victim,s32 amount) {
    s32 pct=sRules.damagePct?sRules.damagePct:100,n;
    if(amount<=0) return amount;
    if(sRaid && RulesBoss(victim->objId)<0) {
        /* The combat opponent is bound to the attacker by the collision code;
           it is independent from the local player's camera lock-on. */
        n=sBossCount==1?0:RulesBoss(BtlOpp_GetObjId(victim));
        if(n>=0) {
            pct=pct*(sBossRules[n].bossDamagePct?sBossRules[n].bossDamagePct:100)/100;
            if(RulesEnraged(n)) pct=pct*150/100;
        }
    }
    return amount>1000000?amount:(amount/100)*pct+(amount%100)*pct/100;
}
s32 Port_BattleRulesKi(BtlMemberChr *chr,s32 amount) {
    s32 pct=sRules.kiPct?sRules.kiPct:100,n=RulesBoss(chr->objId);
    if(n>=0 && RulesEnraged(n)) pct=pct*150/100;
    return amount>1000000?amount:(amount/100)*pct+(amount%100)*pct/100;
}
s32 Port_BattleRulesArmor(s32 objId) {
    BtlMemberChr *chr=BtlChar_FindByObjId(objId);s32 n=RulesBoss(objId);
    return chr && ((n>=0?sBossRules[n].armorSides:sRules.armorSides)&(1<<RulesSide(objId,chr->side)))?1:0;
}
void Port_BattleRulesTick(void) {
    s32 i;
    if(!BtlSeq_IsFighting() || (Battle_GetWork()->flags&(BATTLE_FLAG_PAUSE|BATTLE_FLAG_LOAD_CHARA))) return;
    sTicks=(sTicks+1)%30;
    for(i=0;i<BtlChar_GetCount();i++) {
        BtlMemberChr *chr=BtlChar_Get(i);BtlMemberGauge *g;PcBattleRules *r;s32 side,n;
        if(!chr || chr->active<0 || chr->active>=BTL_MEMBER_SLOTS) continue;
        g=BtlMember_GetActiveGauge(chr);if(g->health<=0) continue;
        n=RulesBoss(chr->objId);r=n>=0?&sBossRules[n]:&sRules;side=RulesSide(chr->objId,chr->side);
        if(r->infiniteKiChara1 && side==1 && chr->members[chr->active].chara==r->infiniteKiChara1-1) g->ki=g->kiMax;
        if(!sTicks && (r->regenSides&(1<<side))) {g->health+=r->regenPerSec;if(g->health>g->healthMax) g->health=g->healthMax;}
    }
}
#endif

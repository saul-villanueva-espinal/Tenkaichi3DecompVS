#include "common.h"
#ifdef PORT
#include "battle/battle.h"
#include "battle/btl_char_member.h"
#include "port_pc_battle.h"

extern s32 BtlSeq_IsFighting(void), BtlChar_GetCount(void);
extern BtlMemberChr *BtlChar_Get(s32), *BtlChar_FindByObjId(s32);
extern s32 Port_NetActive(void), Port_NetSession(void), Port_ModesFight(void);
static PcBattleRules sRules;
static s32 sRaid, sTicks;

void Port_BattleRulesReset(void) { sTicks=0; }
void Port_BattleRulesSet(const PcBattle *battle) {
    static const PcBattleRules normal={0};
    sRules=normal;sRaid=0;sTicks=0;
    if(battle && !Port_NetActive() && !Port_NetSession()) {
        sRules=battle->rules;sRaid=battle->mode==PC_RAID;
    }
}
static s32 RulesSide(s32 objId,s32 side) {
    /* Extra raid allies can have the native side bit 1. Only object 1 is boss. */
    return sRaid ? objId==1 : side;
}
static s32 RulesEnraged(void) {
    BtlMemberChr *boss;
    BtlMemberGauge *g;
    if(!sRaid || !sRules.enragePct) return 0;
    boss=BtlChar_FindByObjId(1);
    if(!boss) return 0;
    g=BtlMember_GetActiveGauge(boss);
    return g->health>0 && g->health*100<=g->healthMax*sRules.enragePct;
}
void Port_BattleRulesGauge(BtlMemberChr *chr,BtlMemberGauge *g,s32 init) {
    if(sRaid && chr->objId==1) {
        g->healthMax=sRules.bossHp?sRules.bossHp:60000;
        if(init) {g->health=g->healthMax;g->ki=g->kiMax;g->blast=g->blastMax;}
    } else if(init && sRules.healthPct && !RulesSide(chr->objId,chr->side)) {
        g->health=g->health*sRules.healthPct/100;
        if(g->health<1) g->health=1;
    }
}
s32 Port_BattleRulesDamage(BtlMemberChr *victim,s32 amount) {
    s32 pct=sRules.damagePct?sRules.damagePct:100;
    if(amount<=0) return amount;
    if(sRaid && victim->objId!=1) {
        pct=pct*(sRules.bossDamagePct?sRules.bossDamagePct:100)/100;
        if(RulesEnraged()) pct=pct*150/100;
    }
    return amount>1000000 ? amount : amount*pct/100;
}
s32 Port_BattleRulesKi(BtlMemberChr *chr,s32 amount) {
    s32 pct=sRules.kiPct?sRules.kiPct:100;
    if(sRaid && chr->objId==1 && RulesEnraged()) pct=pct*150/100;
    return amount>1000000 ? amount : amount*pct/100;
}
s32 Port_BattleRulesArmor(s32 objId) {
    BtlMemberChr *chr=BtlChar_FindByObjId(objId);
    return chr && (sRules.armorSides & (1<<RulesSide(objId,chr->side))) ? 1:0;
}
void Port_BattleRulesTick(void) {
    s32 i;
    if(!BtlSeq_IsFighting() || (Battle_GetWork()->flags & (BATTLE_FLAG_PAUSE|BATTLE_FLAG_LOAD_CHARA))) return;
    sTicks=(sTicks+1)%30;
    for(i=0;i<BtlChar_GetCount();i++) {
        BtlMemberChr *chr=BtlChar_Get(i);
        BtlMemberGauge *g;
        s32 side;
        if(!chr || chr->active<0 || chr->active>=BTL_MEMBER_SLOTS) continue;
        g=BtlMember_GetActiveGauge(chr);if(g->health<=0) continue;
        side=RulesSide(chr->objId,chr->side);
        if(sRules.infiniteKiChara1 && side==1 &&
           chr->members[chr->active].chara==sRules.infiniteKiChara1-1) g->ki=g->kiMax;
        if(!sTicks && (sRules.regenSides & (1<<side))) {
            g->health+=sRules.regenPerSec;
            if(g->health>g->healthMax) g->health=g->healthMax;
        }
    }
}
#endif

#include "common.h"
#ifdef PORT
#include "battle/battle_setup.h"
#include "battle/battle_work.h"
#include "battle/btl_char_member.h"
#include "menu/menu_a.h"
#include "port_pc_battle.h"
#include "port_fighters.h"
extern void Port_SettingSave(const char *, int);
extern void Port_LocalPlayersSet(int);
extern void *BtlChar_FindByObjId(s32);
extern int Port_CoopEnabled(void), Port_CoopConfig(PcBattle *);
static PcBattle sBattle;
static s32 sRelay, sNext[2];
static volatile s32 sRemaining[2];

/* The same ten-member builder can supply any legacy team selector. Its five
   visible native slots are the first bank; the rest enter as defeated benches
   become free. Scripts and simultaneous extra modes never consume this queue. */
void Port_PcApplyTeams(void) {
    PcBattle b;
    s32 s, i;
    if ((Battle_GetMode() != 0 && Battle_GetMode() != 2) || Port_ModesFight() != PORT_MODE_NONE || Port_CoopEnabled()) return;
    if (!Port_PcTeamsTake(&b)) return;
    sBattle = b; sRelay = 1;
    for (s = 0; s < 2; s++) {
        BattleSide *side = &Battle_GetSetup()->sides[s];
        s32 level = side->members[0].cpuLevel;
        side->memberCount = b.count[s] > 5 ? 5 : b.count[s]; side->lead = 0;
        sNext[s] = side->memberCount; sRemaining[s] = b.count[s];
        for (i = 0; i < side->memberCount; i++) {
            PcFighter *f = &b.fighter[s][i];
            BattleSetup_SetMember(s, i, f->chara, f->costume, 0, level, 100.0f, NULL);
        }
    }
}

void Port_PcRelayStop(void) { sRelay = 0; sRemaining[0] = sRemaining[1] = 0; }
int Port_PcRelayRemaining(int side) { return side >= 0 && side < 2 ? sRemaining[side] : 0; }

void Port_PcRelayRewind(void) {
    s32 s, i;
    if (!sRelay) return;
    for (s = 0; s < 2; s++) {
        BattleSide *side = &Battle_GetSetup()->sides[s];
        side->memberCount = sBattle.count[s] > 5 ? 5 : sBattle.count[s];
        side->lead = 0;
        for (i = 0; i < side->memberCount; i++) {
            PcFighter *f = &sBattle.fighter[s][i];
            BtlMember_Init(&side->members[i], f->chara, f->costume, 0, 13, 100.0f, NULL);
        }
        side->form.chara = side->startForm.chara = sBattle.fighter[s][0].chara;
        side->form.costume = side->startForm.costume = sBattle.fighter[s][0].costume;
        side->form.variant = side->startForm.variant = 0;
        sNext[s] = side->memberCount; sRemaining[s] = sBattle.count[s];
    }
}

/* The engine's five native member slots remain intact. Defeated bench slots
   are replenished from the ordered PC queue before the next-member checks. */
void Port_PcRelayTick(void) {
    s32 s;
    if (!sRelay) return;
    for (s = 0; s < 2; s++) {
        BtlMemberChr *chr = BtlChar_FindByObjId(s);
        if (!chr) continue;
        while (sNext[s] < sBattle.count[s]) {
            PcFighter *f = &sBattle.fighter[s][sNext[s]];
            BattleMember m;
            s32 slot;
            BtlMember_Init(&m, f->chara, f->costume, 0, 13, 100.0f, NULL);
            slot = BtlMember_Replace(chr, (BtlMemberSetup *)&m);
            if (slot < 0) break;
            *BattleSide_GetMember(s, slot) = m;
            sNext[s]++;
            { extern void Port_PcRelayLog(int, int, int); Port_PcRelayLog(s, sNext[s], f->chara); }
        }
        sRemaining[s] = BtlMember_CountAlive(chr) + sBattle.count[s] - sNext[s];
    }
}

int Port_PcBattleBuild(void) {
    PcBattle b;
    s32 s, i, mode;
    if (Port_CoopEnabled()) {
        if (!Port_CoopConfig(&b)) return 0;
    } else if (!Port_PcBattleTake(&b)) return 0;
    Battle_ClearWork();
    sBattle = b;
    sRelay = b.mode == PC_RELAY || (b.mode == PC_DUEL && (b.count[0] > 5 || b.count[1] > 5));
    mode = b.mode == PC_TAG ? PORT_MODE_TEAM : b.mode == PC_RAID ? PORT_MODE_RAID : PORT_MODE_NONE;
    Port_SettingSave("modes_fight", mode);
    Port_SettingSave("modes_extras", 0);
    Port_LocalPlayersSet(b.players);
    BattleSetup_SetRule(b.players > 1, b.mode == PC_TRAINING ? 6 : 0, 0, sRelay || b.mode == PC_RAID ? 4 : 2, 0, b.stage, 0);
    for (s = 0; s < 2; s++) {
        s32 count = b.count[s] > 5 ? 5 : b.count[s];
        s32 cpu = s == 1 && (b.players == 1 || b.mode == PC_RAID || b.mode == PC_TRAINING);
        sNext[s] = count;
        sRemaining[s] = b.count[s];
        BattleSetup_SetSide(s, cpu ? BTL_CONTROL_CPU : BTL_CONTROL_PAD, s, count, 1, 1, 0, NULL);
        for (i = 0; i < count; i++) {
            PcFighter *f = &b.fighter[s][i];
            BattleSetup_SetMember(s, i, f->chara, f->costume, 0, b.mode == PC_TRAINING && s ? -1 : 13, 100.0f, NULL);
        }
    }
    BattleSetup_Finish();
    /* Return to the main menu after a PC battle; there is no legacy selector
       with only five plates standing behind a ten-member team. */
    gProgress->mode = 4;
    return 1;
}
#endif

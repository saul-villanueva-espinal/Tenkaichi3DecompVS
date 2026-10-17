#include "common.h"
#ifdef PORT
#include "battle/battle_setup.h"
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

void Port_PcRelayStop(void) { sRelay = 0; sRemaining[0] = sRemaining[1] = 0; }
int Port_PcRelayRemaining(int side) { return side >= 0 && side < 2 ? sRemaining[side] : 0; }

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
    sRelay = b.mode == PC_RELAY;
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

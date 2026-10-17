#include "common.h"
#ifdef PORT
#include "battle/battle_setup.h"
#include "battle/battle_work.h"
#include "battle/btl_char_member.h"
#include "menu/menu_a.h"
#include "port_pc_battle.h"
#include "port_fighters.h"
#include "sys/save.h"
extern s32 CpuLevel_FromSetting(s32);
extern void Port_SettingSave(const char *, int);
extern void Port_LocalPlayersSet(int);
extern int Port_PcTestHealth(void);
extern void *BtlChar_FindByObjId(s32);
extern int Port_CoopEnabled(void), Port_CoopConfig(PcBattle *);
extern unsigned Port_CoopSeed(void);
extern int Port_NetActive(void), Port_NetSession(void);
static PcBattle sBattle;
static s32 sRelay, sNext[2], sLevel[2], sRelayMask = 3;
static volatile s32 sRemaining[2];
static s32 sNativeRules,sNativeCpuCpu,sNativeTraining,sNativeBgm;
void Port_PcNativeRules(s32 players,s32 training,s32 bgm) {sNativeRules=1;sNativeCpuCpu=players==2;sNativeTraining=training;sNativeBgm=bgm;}

/* Keep the window responsive while verifying assets or waiting for the party.
   This advances presentation only; battle objects and clocks do not exist yet. */
void Port_CoopWaitFrame(void) {
    extern void Gfx_BeginFrame(void), Gfx_EndFrame(s32), Gfx_ClearScreen(s32, u32), Dma_Flush(void);
    Gfx_BeginFrame(); Gfx_ClearScreen(0, 0x80120B08); Gfx_EndFrame(1); Dma_Flush();
}

/* The same ten-member builder can supply any legacy team selector. Its five
   visible native slots are the first bank; the rest enter as defeated benches
   become free. Scripts and simultaneous extra modes never consume this queue. */
void Port_PcApplyTeams(void) {
    PcBattle b;
    s32 s, i;
    if ((Battle_GetMode() != 0 && Battle_GetMode() != 2) || Port_ModesFight() != PORT_MODE_NONE || Port_CoopEnabled() || Port_NetActive() || Port_NetSession()) return;
    if (!Port_PcTeamsTake(&b)) return;
    if (Port_PcTeamsMask() == 1) {
        BattleSide *original = &Battle_GetSetup()->sides[1];
        b.count[1] = original->memberCount;
        for (i=0;i<b.count[1] && i<5;i++) { b.fighter[1][i].chara=original->members[i].chara; b.fighter[1][i].costume=original->members[i].costume; }
    }
    sBattle = b; sRelay = 1; sRelayMask = Port_PcTeamsMask();
    for (s = 0; s < 2; s++) {
        BattleSide *side = &Battle_GetSetup()->sides[s];
        s32 level = side->members[0].cpuLevel;
        sLevel[s] = level;
        if (!(sRelayMask & (1 << s))) { sNext[s] = side->memberCount; sRemaining[s] = side->memberCount; continue; }
        side->memberCount = b.count[s] > 5 ? 5 : b.count[s]; side->lead = 0;
        sNext[s] = side->memberCount; sRemaining[s] = b.count[s];
        for (i = 0; i < side->memberCount; i++) {
            PcFighter *f = &b.fighter[s][i];
            BattleSetup_SetMember(s, i, f->chara, f->costume, 0, level, (f32)Port_PcTestHealth(), NULL);
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
        if (!(sRelayMask & (1 << s))) continue;
        side->memberCount = sBattle.count[s] > 5 ? 5 : sBattle.count[s];
        side->lead = 0;
        for (i = 0; i < side->memberCount; i++) {
            PcFighter *f = &sBattle.fighter[s][i];
            BtlMember_Init(&side->members[i], f->chara, f->costume, 0, sLevel[s], (f32)Port_PcTestHealth(), NULL);
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
            BtlMember_Init(&m, f->chara, f->costume, 0, sLevel[s], (f32)Port_PcTestHealth(), NULL);
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
        { extern void Port_Srand(unsigned); extern void Rand_Seed(unsigned);
          Port_Srand(Port_CoopSeed()); Rand_Seed(Port_CoopSeed()); }
    } else if (!Port_PcBattleTake(&b)) return 0;
    Battle_ClearWork();
    sBattle = b; sRelayMask = 3;
    sRelay = b.mode == PC_RELAY || (b.mode == PC_DUEL && (b.count[0] > 5 || b.count[1] > 5));
    mode = b.mode == PC_FFA ? PORT_MODE_FFA : b.mode == PC_TAG ? PORT_MODE_TEAM : b.mode == PC_RAID ? PORT_MODE_RAID : PORT_MODE_NONE;
    Port_SettingSave("modes_fight", sNativeRules && sNativeTraining ? PORT_MODE_TRAINING : mode);
    Port_SettingSave("modes_extras", sNativeRules && sNativeTraining ? 1 : 0);
    Port_LocalPlayersSet(b.players);
    BattleSetup_SetRule(b.players > 1, b.mode == PC_TRAINING ? 6 : 0, 0, sRelay || b.mode == PC_RAID || b.mode == PC_TRAINING ? 0 : 3, 0, b.stage, 0);
    if(sNativeRules) BattleSetup_SetRule(b.players>1,b.mode==PC_TRAINING?6:0,sNativeBgm,b.mode==PC_TRAINING||b.mode==PC_RAID?0:gSaveData->rule[0]==4?0:gSaveData->rule[0]+1,gSaveData->rule[2],b.stage,gSaveData->rule[5]^1);
    for (s = 0; s < 2; s++) {
        s32 count = b.count[s] > 5 ? 5 : b.count[s];
        s32 cpu = (s == 1 && (b.players == 1 || b.mode == PC_RAID || b.mode == PC_TRAINING)) || (sNativeRules && sNativeCpuCpu && mode==PORT_MODE_NONE && b.mode!=PC_TRAINING);
        sLevel[s] = b.mode == PC_TRAINING && s ? -1 : sNativeRules ? CpuLevel_FromSetting(gSaveData->rule[1]) : 13;
        sNext[s] = count;
        sRemaining[s] = b.count[s];
        BattleSetup_SetSide(s, cpu ? BTL_CONTROL_CPU : BTL_CONTROL_PAD, s, count, 1, 1, 0, NULL);
        for (i = 0; i < count; i++) {
            PcFighter *f = &b.fighter[s][i];
            BattleSetup_SetMember(s, i, f->chara, f->costume, 0, sLevel[s], (f32)Port_PcTestHealth(), NULL);
        }
    }
    BattleSetup_Finish();
    sNativeRules=sNativeCpuCpu=sNativeTraining=0;
    /* Return to the main menu after a PC battle; there is no legacy selector
       with only five plates standing behind a ten-member team. */
    gProgress->mode = 4;
    return 1;
}
#endif

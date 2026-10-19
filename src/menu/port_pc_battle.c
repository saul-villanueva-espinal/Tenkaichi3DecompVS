#include "common.h"
#ifdef PORT
#include "battle/battle_setup.h"
#include "battle/battle_work.h"
#include "battle/btl_char_member.h"
#include "menu/menu_a.h"
#include "port_pc_battle.h"
#include "port_fighters.h"
#include "sys/save.h"
#include "port_chara.h"
#include "menu/port_screen.h"
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
/* The original fighter/HUD ABI has five slots. Page the selected reserves into
   those slots without changing the ABI or discarding their combat state. */
static BtlMember sReserve[2][PORT_RELAY_MAX];
static BattleMember sReserveSetup[2][PORT_RELAY_MAX];
static s32 sReserveValid[2][PORT_RELAY_MAX], sSlotIndex[2][BTL_MEMBER_SLOTS];
static s32 sBankReady[2];
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
    sBankReady[0] = sBankReady[1] = 0;
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

void Port_PcRelayStop(void) { sRelay = 0; sRemaining[0] = sRemaining[1] = 0; sBankReady[0] = sBankReady[1] = 0; }
int Port_PcRelayRemaining(int side) { return side >= 0 && side < 2 ? sRemaining[side] : 0; }

/* Draw with the disc's glyph atlas on the game's HUD layer, so photo mode,
   widescreen anchoring and the original font all behave like the native bars. */
void Port_PcRelayDraw(void) {
    extern void Font_PushStyle(void),Font_PopStyle(void),Font_SetScale(f32);
    extern void Font_SetScaleXY(f32,f32),Font_SetColor(u32),Font_SetShadowColor(u32);
    extern void Font_SetAlign(s32),Font_SetShadowMode(s32),Font_SetSpacing(s32,s32);
    extern void Font_PrintAsciiAt(s32,s32,char *),Font_FlushAll(void),Port_GsMarker(s32);
    extern s32 Port_TourneyEnabled(void);
    static char labels[2][4],counts[2][16];
    s32 s;
    f32 x,y=89.0f,w=76.0f,h=17.0f;
    u32 silver=Ps_Rgba(180,195,210,128),black=Ps_Rgba(5,9,20,128);
    if (!sRelay || Port_CoopEnabled() || Port_TourneyEnabled() || (sBattle.count[0]<=5 && sBattle.count[1]<=5)) return;
    Font_PushStyle(); Font_SetScale(1.0f); Font_SetScaleXY(.56f,.51f);
    Font_SetAlign(0); Font_SetSpacing(0,0);Font_SetShadowMode(2);Font_SetShadowColor(black);
    for(s=0;s<2;s++) {
        u32 top=s?Ps_Rgba(140,25,60,128):Ps_Rgba(25,80,150,128);
        x=s?422.0f:14.0f;
        Port_GsMarker(s?0x11:0x10);
        Ps_ScreenQuad(x+5,y,x+w-5,y,x,y+h*.5f,x+w,y+h*.5f,silver,silver,silver,silver);
        Ps_ScreenQuad(x,y+h*.5f,x+w,y+h*.5f,x+5,y+h,x+w-5,y+h,silver,silver,silver,silver);
        Ps_ScreenQuad(x+6,y+2,x+w-6,y+2,x+2,y+h*.5f,x+w-2,y+h*.5f,top,top,black,black);
        Ps_ScreenQuad(x+2,y+h*.5f,x+w-2,y+h*.5f,x+6,y+h-2,x+w-6,y+h-2,black,black,top,top);
        sprintf(labels[s],"J%d",s+1);sprintf(counts[s],"%d/%d",sRemaining[s],sBattle.count[s]);
        Font_SetColor(s?Ps_Rgba(255,110,130,128):Ps_Rgba(110,200,255,128));
        Font_PrintAsciiAt((s32)x+9,(s32)y+2,labels[s]);
        Font_SetColor(Ps_Rgba(255,235,100,128));
        Font_PrintAsciiAt((s32)x+31,(s32)y+2,counts[s]);
        Font_FlushAll();
    }
    Font_PopStyle();Port_GsMarker(0x12);
}

/* Slots own both loader buffers until Battle_Unload. A fresh member setup has
   no buffers: replacing the whole slot would leak them and later free NULL. */
static void PcRelay_StoreMember(BattleMember *slot, BattleMember *fresh) {
    fresh->data = slot->data;
    fresh->buf[0] = slot->buf[0];
    fresh->buf[1] = slot->buf[1];
    *slot = *fresh;
}

static void PcRelay_Portrait(s32 s, s32 slot) {
    BattleMember *m = BattleSide_GetMember(s, slot);
    if (m->buf[0] && File_LoadSync(CHARA_MEMBER_FILE_COSTUME(m->chara, m->costume, s), m->buf[0], 0)) {
        Res_RelocateOffsets(&m->buf[0], m->buf[0], m->buf[0]);
        m->data = m->buf[0];
    }
}

static void PcRelay_Bank(s32 s, BtlMemberChr *chr) {
    s32 i;
    if (sBankReady[s]) return;
    for (i = 0; i < PORT_RELAY_MAX; i++) sReserveValid[s][i] = 0;
    for (i = 0; i < BTL_MEMBER_SLOTS; i++) {
        sSlotIndex[s][i] = i < chr->memberCount ? i : -1;
        /* A rematch may reuse a portrait buffer that held a later reserve. */
        if (i < chr->memberCount) PcRelay_Portrait(s, i);
    }
    sBankReady[s] = 1;
}

static s32 PcRelay_FindSlot(s32 s, BtlMemberChr *chr, s32 index) {
    s32 i;
    for (i = 0; i < chr->memberCount; i++) if (sSlotIndex[s][i] == index) return i;
    return -1;
}

static s32 PcRelay_Alive(s32 s, BtlMemberChr *chr, s32 index) {
    s32 slot = PcRelay_FindSlot(s, chr, index);
    if (slot >= 0) return chr->members[slot].present && chr->members[slot].gauge.health > 0;
    return !sReserveValid[s][index] || (sReserve[s][index].present && sReserve[s][index].gauge.health > 0);
}

static void PcRelay_Page(s32 s, BtlMemberChr *chr, s32 slot, s32 index) {
    s32 old = sSlotIndex[s][slot], i;
    BattleMember fresh;
    BtlMember *member = &chr->members[slot];
    if (old >= 0) {
        sReserve[s][old] = *member;
        sReserveSetup[s][old] = *BattleSide_GetMember(s, slot);
        sReserveValid[s][old] = 1;
    }
    if (sReserveValid[s][index]) {
        *member = sReserve[s][index];
        fresh = sReserveSetup[s][index];
        fresh.chara = member->chara; fresh.costume = member->costume;
        fresh.variant = member->gauge.variant;
    } else {
        BtlMember empty = {0};
        PcFighter *f = &sBattle.fighter[s][index];
        BtlMember_Init(&fresh, f->chara, f->costume, 0, sLevel[s], (f32)Port_PcTestHealth(), NULL);
        *member = empty;
        member->present = 1; member->chara = fresh.chara; member->costume = fresh.costume;
        member->cpuLevel = fresh.cpuLevel; member->aiType = fresh.aiType;
        for (i = 0; i < 4; i++) member->ability[i] = fresh.ability[i];
        BtlMember_LoadParams(chr, slot, 1, fresh.health, fresh.variant);
        for (i = 0; i < 7; i++) member->bonus[i] = fresh.bonus[i + 1];
        if (index >= BTL_MEMBER_SLOTS) {
            extern void Port_PcRelayLog(int, int, int);
            sNext[s]++;
            Port_PcRelayLog(s, index + 1, f->chara);
        }
    }
    PcRelay_StoreMember(BattleSide_GetMember(s, slot), &fresh);
    sSlotIndex[s][slot] = index;
    PcRelay_Portrait(s, slot);
}

/* Called by the original right-stick navigation in both directions. The same
   controls can now reach all ten choices before any teammate is defeated. */
s32 Port_PcRelayCycle(BtlMemberChr *chr, s32 direction) {
    s32 s, start, active, step, index, slot;
    if (!sRelay || !chr) return 0;
    s = chr->side;
    if (s < 0 || s > 1 || !(sRelayMask & (1 << s)) || sBattle.count[s] <= BTL_MEMBER_SLOTS) return 0;
    PcRelay_Bank(s, chr);
    active = sSlotIndex[s][chr->active];
    start = chr->switchTarget >= 0 && chr->switchTarget < chr->memberCount ? sSlotIndex[s][chr->switchTarget] : active;
    for (step = 1; step <= sBattle.count[s]; step++) {
        index = (start + direction * step + sBattle.count[s]) % sBattle.count[s];
        if (index == active || !PcRelay_Alive(s, chr, index)) continue;
        slot = PcRelay_FindSlot(s, chr, index);
        if (slot < 0) {
            slot = chr->switchTarget;
            if (slot < 0 || slot >= chr->memberCount || slot == chr->active) slot = (chr->active + 1) % chr->memberCount;
            PcRelay_Page(s, chr, slot, index);
        }
        chr->switchTarget = slot;
        return 1;
    }
    return 1;
}

void Port_PcRelayRewind(void) {
    s32 s, i;
    if (!sRelay) return;
    sBankReady[0] = sBankReady[1] = 0;
    for (s = 0; s < 2; s++) {
        BattleSide *side = &Battle_GetSetup()->sides[s];
        if (!(sRelayMask & (1 << s))) continue;
        side->memberCount = sBattle.count[s] > 5 ? 5 : sBattle.count[s];
        side->lead = 0;
        for (i = 0; i < side->memberCount; i++) {
            PcFighter *f = &sBattle.fighter[s][i];
            BattleMember fresh;
            BtlMember_Init(&fresh, f->chara, f->costume, 0, sLevel[s], (f32)Port_PcTestHealth(), NULL);
            PcRelay_StoreMember(&side->members[i], &fresh);
        }
        /* Keep the loaded form: BtlLoad_Reload must detect and reload it. */
        side->startForm.chara = sBattle.fighter[s][0].chara;
        side->startForm.costume = sBattle.fighter[s][0].costume;
        side->startForm.variant = 0;
        sNext[s] = side->memberCount; sRemaining[s] = sBattle.count[s];
    }
}

/* Keep the native bank supplied, including reserves that were paged out while
   browsing. Defeated members are never recreated as fresh fighters. */
void Port_PcRelayTick(void) {
    s32 s, i, slot;
    if (!sRelay) return;
    for (s = 0; s < 2; s++) {
        BtlMemberChr *chr = BtlChar_FindByObjId(s);
        if (!chr) continue;
        if (!(sRelayMask & (1 << s))) { sRemaining[s] = BtlMember_CountAlive(chr); continue; }
        PcRelay_Bank(s, chr);
        for (slot = 0; slot < chr->memberCount; slot++) {
            if (slot == chr->active || chr->members[slot].gauge.health > 0) continue;
            for (i = 0; i < sBattle.count[s]; i++) {
                if (PcRelay_FindSlot(s, chr, i) < 0 && PcRelay_Alive(s, chr, i)) {
                    PcRelay_Page(s, chr, slot, i);
                    break;
                }
            }
        }
        sRemaining[s] = 0;
        for (i = 0; i < sBattle.count[s]; i++) sRemaining[s] += PcRelay_Alive(s, chr, i) != 0;
    }
}

/* Read-only overflow accessor: never index BattleSide.members[5]. */
int Port_PcRaidOverflow(s32 *chara,s32 *costume) {
    if(sBattle.mode!=PC_RAID || sBattle.count[0]<=5) return 0;
    *chara=sBattle.fighter[0][5].chara;*costume=sBattle.fighter[0][5].costume;return 1;
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
    {extern s32 Port_TourneyTestBots(void);if(Port_TourneyTestBots()) {sNativeRules=1;sNativeCpuCpu=1;}}
    { extern void Port_BattleRulesSet(const PcBattle *); Port_BattleRulesSet(&b); }
    sBattle = b; sRelayMask = 3;
    sBankReady[0] = sBankReady[1] = 0;
    sRelay = b.mode == PC_RELAY || (b.mode == PC_DUEL && (b.count[0] > 5 || b.count[1] > 5));
    mode = b.mode == PC_FFA ? PORT_MODE_FFA : b.mode == PC_TAG ? PORT_MODE_TEAM : b.mode == PC_RAID ? PORT_MODE_RAID : PORT_MODE_NONE;
    Port_SettingSave("modes_fight", sNativeRules && sNativeTraining ? PORT_MODE_TRAINING : mode);
    Port_SettingSave("modes_extras", sNativeRules && sNativeTraining ? 1 : 0);
    Port_LocalPlayersSet(b.players);
    BattleSetup_SetRule(b.players > 1, b.mode == PC_TRAINING ? 6 : 0, 0, sRelay || b.mode == PC_RAID || b.mode == PC_TRAINING ? 0 : 3, 0, b.stage, 0);
    if(sNativeRules) BattleSetup_SetRule(b.players>1,b.mode==PC_TRAINING?6:0,sNativeBgm,b.mode==PC_TRAINING||b.mode==PC_RAID?0:gSaveData->rule[0]==4?0:gSaveData->rule[0]+1,gSaveData->rule[2],b.stage,gSaveData->rule[5]^1);
    if(b.rules.bgm1) Battle_GetSetup()->rule.bgm=b.rules.bgm1-1;
    {extern s32 Port_TourneyEnabled(void);if(Port_TourneyEnabled()) {
        Battle_GetSetup()->rule.timeLimit=2; /* 90 s; draws replay the match */
        Battle_GetSetup()->rule.screenMode=0; /* spectators share one arena camera */
    }}
    for (s = 0; s < 2; s++) {
        s32 count = b.count[s] > 5 ? 5 : b.count[s];
        s32 cpu = (s == 1 && (b.players == 1 || b.mode == PC_RAID || b.mode == PC_TRAINING)) || (sNativeRules && sNativeCpuCpu && mode==PORT_MODE_NONE && b.mode!=PC_TRAINING);
        sLevel[s] = b.mode == PC_TRAINING && s ? -1 : sNativeRules ? CpuLevel_FromSetting(gSaveData->rule[1]) : 13;
        if(b.rules.cpuLevel1 && cpu) sLevel[s]=b.rules.cpuLevel1-1;
        sNext[s] = count;
        sRemaining[s] = b.count[s];
        BattleSetup_SetSide(s, cpu ? BTL_CONTROL_CPU : BTL_CONTROL_PAD, s, count, 1, 1, 0, NULL);
        for (i = 0; i < count; i++) {
            PcFighter *f = &b.fighter[s][i];
            BattleSetup_SetMember(s, i, f->chara, f->costume, 0, sLevel[s], (f32)Port_PcTestHealth(), NULL);
        }
    }
    if(Port_CoopEnabled()) Battle_GetSetup()->rule.screenMode=0;
    BattleSetup_Finish();
    sNativeRules=sNativeCpuCpu=sNativeTraining=0;
    /* Return to the main menu after a PC battle; there is no legacy selector
       with only five plates standing behind a ten-member team. */
    gProgress->mode = 4;
    return 1;
}
#endif

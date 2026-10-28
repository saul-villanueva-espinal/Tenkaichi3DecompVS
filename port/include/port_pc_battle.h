#ifndef PORT_PC_BATTLE_H
#define PORT_PC_BATTLE_H
#include "port_limits.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { PC_DUEL, PC_TRAINING, PC_TAG, PC_RAID, PC_RELAY, PC_FFA, PC_MODE_COUNT };
typedef struct PcFighter { int chara, costume; } PcFighter;
/* Zero values preserve the normal game's rules. Sent by the raid host. */
typedef struct PcBattleRules {
    int damagePct, kiPct, regenPerSec, regenSides, healthPct;
    int bossHp, bossDamagePct, enragePct, cpuLevel1, bgm1;
    int infiniteKiChara1, armorSides;
} PcBattleRules;
typedef struct PcBattle {
    int mode, players, stage, count[2];
    PcFighter fighter[2][PORT_RELAY_MAX];
    PcBattleRules rules;
    /* Additional simultaneous bosses. The first boss retains rules above. */
    PcBattleRules bossRules[PORT_RAID_BOSSES - 1];
} PcBattle;
void Port_PcBattleDefault(PcBattle *b);
int Port_PcBattleParse(PcBattle *b, const char *text);
int Port_PcBattleValidate(const PcBattle *b, char *why, int size);
int Port_PcBattleRequest(const PcBattle *b);
int Port_PcBattlePending(void);
int Port_RaidRoomActive(void);
int Port_PcRosterQueue(const PcBattle *b);
int Port_PcRosterPlayerQueue(const PcBattle *b, int dp);
int Port_PcTeamsMask(void);
void Port_GetLocalIp(int ip[4]);

void Port_RosterRemember(const PcBattle *b);
int Port_RosterRecall(int mode, PcBattle *b);
int Port_RosterFavorite(int chara, int toggle);
int Port_RosterTestMode(void);
unsigned Port_RosterTestInput(unsigned age);
int Port_RosterPointerRead(int *x, int *y);
void Port_RosterPointerSet(int x, int y);
extern volatile int gPortNativeRosterActive;
int Port_NativeRosterPick(PcBattle *out, int kind, int dp);
int Port_NativeRosterBattle(int kind);
int Port_PcBattleTake(PcBattle *b);
int Port_PcTeamsRequest(const PcBattle *b);
int Port_PcTeamsTake(PcBattle *b);
int Port_PcTeamsPreview(PcBattle *b);
void Port_PcApplyTeams(void);
void Port_PcRelayRewind(void);
int Port_PcBattleBuild(void); /* game thread, after leaving main menu */
void Port_PcRelayTick(void);  /* game thread, before the KO/next-member checks */
int Port_PcRelayRemaining(int side);
int Port_PcRosterCount(void);
int Port_PcRosterId(int index);
const char *Port_PcRosterName(int chara);
int Port_MartialMode(void);
void Port_MartialModeSet(int on);
#ifdef __cplusplus
}
#endif
#endif

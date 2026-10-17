#ifndef PORT_PC_BATTLE_H
#define PORT_PC_BATTLE_H
#include "port_limits.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { PC_DUEL, PC_TRAINING, PC_TAG, PC_RAID, PC_RELAY, PC_MODE_COUNT };
typedef struct PcFighter { int chara, costume; } PcFighter;
typedef struct PcBattle {
    int mode, players, stage, count[2];
    PcFighter fighter[2][PORT_RELAY_MAX];
} PcBattle;
void Port_PcBattleDefault(PcBattle *b);
int Port_PcBattleValidate(const PcBattle *b, char *why, int size);
int Port_PcBattleRequest(const PcBattle *b);
int Port_PcBattlePending(void);
int Port_PcBattleTake(PcBattle *b);
int Port_PcBattleBuild(void); /* game thread, after leaving main menu */
void Port_PcRelayTick(void);  /* game thread, before the KO/next-member checks */
int Port_PcRelayRemaining(int side);
int Port_PcRosterCount(void);
int Port_PcRosterId(int index);
const char *Port_PcRosterName(int chara);
#ifdef __cplusplus
}
#endif
#endif

#ifndef PORT_COOP_H
#define PORT_COOP_H
#include "port_pc_battle.h"
#ifdef __cplusplus
extern "C" {
#endif
int Port_CoopEnabled(void);
int Port_CoopConfig(PcBattle *b);
int Port_CoopMe(void);
int Port_CoopInputDelay(void);
int Port_CoopPlayerDropped(int player);
int Port_CoopConnected(void);
unsigned Port_CoopSeed(void);
int Port_CoopLobbyStatus(int *known, int *total);
int Port_CoopLobbyCopy(PcBattle *out); /* game thread during the lobby pump */
void Port_CoopBeginTick(unsigned tick);
void Port_CoopPauseSimulation(int paused); /* local scene teardown is outside the shared timeline */
void Port_CoopBeginLoad(void);
void Port_CoopBattleReady(void); /* shared tick zero follows every participant's completed battle load */
int Port_CoopFailed(void);
int Port_CoopTestInput(void);
void Port_CoopErrorText(char *out,int size);
void Port_CoopLeave(void);
void Port_CoopInput(int player, unsigned char *data);
int Port_CoopLaunch(int host, const char *address, int port, const PcBattle *b);
#ifdef __cplusplus
}
#endif
#endif

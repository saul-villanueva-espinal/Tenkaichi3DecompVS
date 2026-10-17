#ifndef PORT_COOP_H
#define PORT_COOP_H
#include "port_pc_battle.h"
#ifdef __cplusplus
extern "C" {
#endif
int Port_CoopEnabled(void);
int Port_CoopConfig(PcBattle *b);
int Port_CoopMe(void);
unsigned Port_CoopSeed(void);
int Port_CoopLobbyStatus(int *known, int *total);
void Port_CoopBeginTick(unsigned tick);
void Port_CoopInput(int player, unsigned char *data);
int Port_CoopLaunch(int host, const char *address, int port, const PcBattle *b);
#ifdef __cplusplus
}
#endif
#endif

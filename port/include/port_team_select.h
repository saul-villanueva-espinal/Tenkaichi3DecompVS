#ifndef PORT_TEAM_SELECT_H
#define PORT_TEAM_SELECT_H
#include "port_pc_battle.h"
int Port_TeamSelectLimit(void);
void Port_TeamSelectSetLimit(int limit);
void Port_TeamSelectDraw(void);
void Port_TeamSelectBegin(void);
void Port_TeamSelectEnd(void);
void Port_TeamSelectCapture(void);
int Port_TeamSelectCopy(PcBattle *out);
void Port_TeamChip(int chara, float x, float y, float w, float h, int alpha);
int Port_PcNativeTeamsQueue(const PcBattle *b);
#endif

#ifndef PORT_SELECT_MODELS_H
#define PORT_SELECT_MODELS_H
int Port_SelectModelsActive(void);
void Port_SelectModelsBegin(void);
void Port_SelectModelsSet(int side, int chara, int costume);
void Port_SelectModelsDraw(void);
void Port_SelectModelsDrawRaid(void);
void Port_SelectModelsDrawRaidEx(int layout);
void Port_SelectModelsEnd(void);
#endif

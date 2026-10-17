#ifndef PORT_TOURNAMENT_H
#define PORT_TOURNAMENT_H
#include "port_pc_battle.h"
#ifdef __cplusplus
extern "C" {
#endif
#define PORT_TOURNEY_MAX 16
typedef struct PortTourney {
    int count, current, champion;
    PcFighter fighters[PORT_TOURNEY_MAX];
    int winners[PORT_TOURNEY_MAX-1]; /* seed index; -1 is pending */
} PortTourney;
int Port_TourneyInit(PortTourney *t,int count);
int Port_TourneyPair(const PortTourney *t,int match,int *a,int *b);
int Port_TourneyAdvance(PortTourney *t,int winnerSide);
int Port_TourneyEnabled(void);
int Port_TourneyCopy(PortTourney *out);
int Port_TourneyLaunch(int host,const char *address,int port,int count,const PcFighter *hero);
void Port_TourneyResult(int resultBits);
void Port_TourneyLeave(void);
int Port_TourneyFailed(void);
void Port_TourneyErrorText(char *out,int size);
void Port_TourneyCancel(void);
int Port_TourneySpectating(void);
void Port_TourneyActive(int *a,int *b);
#ifdef __cplusplus
}
#endif
#endif

#include "../include/port_tournament.h"
int Port_TourneyInit(PortTourney *t,int count) {
    int i;
    if(!t || (count!=4 && count!=8 && count!=16)) return 0;
    t->count=count;t->current=0;t->champion=-1;
    for(i=0;i<PORT_TOURNEY_MAX;i++) {t->fighters[i].chara=0;t->fighters[i].costume=0;}
    for(i=0;i<PORT_TOURNEY_MAX-1;i++) t->winners[i]=-1;
    return 1;
}
int Port_TourneyPair(const PortTourney *t,int match,int *a,int *b) {
    int width,base=0,previous=-1;
    if(!t || !a || !b || match<0 || match>=t->count-1) return 0;
    for(width=t->count/2;width;previous=base,base+=width,width/=2) {
        if(match<base+width) {
            int k=(match-base)*2;
            *a=previous<0?k:t->winners[previous+k];
            *b=previous<0?k+1:t->winners[previous+k+1];
            return *a>=0 && *b>=0;
        }
    }
    return 0;
}
int Port_TourneyAdvance(PortTourney *t,int winnerSide) {
    int a,b,i;
    if(!t || winnerSide<0 || winnerSide>1 || t->champion>=0 ||
       !Port_TourneyPair(t,t->current,&a,&b) || t->winners[t->current]>=0) return 0;
    t->winners[t->current]=winnerSide?b:a;
    if(t->current==t->count-2) {t->champion=t->winners[t->current];t->current=-1;return 1;}
    for(i=0;i<t->count-1;i++) if(t->winners[i]<0 && Port_TourneyPair(t,i,&a,&b)) {t->current=i;return 1;}
    return 0;
}

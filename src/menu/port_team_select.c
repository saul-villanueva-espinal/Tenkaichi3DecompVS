#include "common.h"
#ifdef PORT
#include "menu/menu_e.h"
#include "menu/menu_f.h"
#include "menu/port_screen.h"
#include "port_team_select.h"
#include "port_fighters.h"
extern int Port_NetActive(void),Port_NetSession(void);
static PcBattle sChosen;
static int sChosenReady,sLimitOverride;
void Port_TeamSelectSetLimit(int limit) { sLimitOverride=limit; }
int Port_TeamSelectLimit(void) {
    if(sLimitOverride>0 && !Port_NetActive() && !Port_NetSession()) return sLimitOverride;
    return Port_NetActive() || Port_NetSession() || Port_ModesFight()!=PORT_MODE_NONE ? TS_MEMBER_MAX : PORT_RELAY_MAX;
}
void Port_TeamSelectCapture(void) {
    int s,k,bgm;
    Port_PcBattleDefault(&sChosen);
    sChosen.mode=PC_RELAY;sChosen.players=gTeamSel->players==TEAMSEL_PLAYERS_TWO?2:1;
    sChosen.stage=gTeamSel->stage->stage;
    bgm=gTeamSel->bgmIds[gTeamSel->stage->bgm];
    /* PC presets encode a music index plus one; native/random tracks outside
       that range stay in the native battle setup and never invalidate a team. */
    sChosen.rules.bgm1=bgm>=0 && bgm<20 ? bgm+1 : 0;
    for(s=0;s<2;s++) {
        sChosen.count[s]=gTeamSel->side[s]->memberCount;
        for(k=0;k<sChosen.count[s];k++) {
            sChosen.fighter[s][k].chara=gTeamSel->side[s]->member[k].chara;
            sChosen.fighter[s][k].costume=gTeamSel->side[s]->member[k].color;
        }
    }
    sChosenReady=1;
    if(Port_TeamSelectLimit()>TS_MEMBER_MAX && (sChosen.count[0]>TS_MEMBER_MAX || sChosen.count[1]>TS_MEMBER_MAX))
        Port_PcNativeTeamsQueue(&sChosen);
}
int Port_TeamSelectCopy(PcBattle *out) {
    if(!sChosenReady || !out) return 0;
    *out=sChosen;return 1;
}
/* A row uses the same top/bottom positions as the original team strip, so
   character reels and item/costume panels retain their existing controls. */
void Port_TeamSelectDraw(void) {
    int s,k;
    char text[48];
    if(Port_TeamSelectLimit()<=TS_MEMBER_MAX || (gTeamSel->flags&TEAMSEL_STAGE)) return;
    Ps_PageSet();Ps_Align(0);
    for(s=0;s<2;s++) {
        TeamSelSide *side=gTeamSel->side[s];
        float y=s?369.0f:17.0f;
        u32 gold=Ps_Rgba(255,215,40,128),dark=Ps_Rgba(6,10,23,128);
        u32 color=s?Ps_Rgba(190,30,70,128):Ps_Rgba(20,75,195,128);
        if(side->state!=TEAMSEL_ST_TEAM && side->state!=TEAMSEL_ST_DONE) continue;
        Ps_Rect(4,y-3,508,y+52,dark,dark);
        Ps_Rect(6,y-1,506,y+1,gold,gold);
        Ps_Rect(7,y+8,40,y+48,color,dark);
        sprintf(text,"J%d",s+1);Ps_Text(11,y+15,text,.67f,28,1);
        for(k=0;k<PORT_RELAY_MAX;k++) {
            float x=45.0f+k*38.0f;
            int selected=side->state==TEAMSEL_ST_TEAM && side->cur==k;
            Ps_Rect(x-1,y+13,x+35,y+48,selected?gold:color,selected?gold:color);
            Ps_Rect(x+1,y+15,x+33,y+46,dark,dark);
            sprintf(text,"%d",k+1);Ps_Text(x+11,y-1,text,.43f,24,1);
            if(k<side->memberCount && side->member[k].chara>=0)
                Port_TeamChip(side->member[k].chara,x+2,y+16,30,29,128);
            else Ps_Text(x+12,y+21,"+",.54f,20,selected?1:0);
        }
        Ps_Rect(430,y+13,503,y+48,side->cur==TS_CUR_MENU?gold:color,gold);
        Ps_Rect(432,y+15,501,y+46,dark,dark);
        Ps_Text(441,y+22,side->state==TEAMSEL_ST_DONE?"LISTO":"FIN",.57f,58,1);
        sprintf(text,"%d/10",side->memberCount);Ps_Text(446,y-1,text,.43f,52,1);
    }
}
#endif

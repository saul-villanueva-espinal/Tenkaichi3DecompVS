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
static MFlash sFinishMovie;
static int sFinishReady;
void Port_TeamSelectEnd(void) {
    if(sFinishReady) Flash_Destroy(&sFinishMovie);
    sFinishReady=0;
}
void Port_TeamSelectBegin(void) {
    int i;
    MFlashRef ref;
    Port_TeamSelectEnd();
    if(Port_TeamSelectLimit()<=TS_MEMBER_MAX) return;
    /* Keep an independent original FIN animation. The player's own movie is
       removed when that team is confirmed; it cannot supply a persistent button. */
    Flash_Create(&sFinishMovie,MPACK_AT(gTeamSel->res,37),gTeamSel->teamTex[0]);
    Flash_Play(&sFinishMovie,1);Flash_GotoLabel(&sFinishMovie,"fl_fast_in",1);
    for(i=0;i<32;i++) Flash_Advance(&sFinishMovie);
    Flash_FindLabel(&sFinishMovie,NULL,"mc_menu_plate",&ref);
    Flash_ClipGotoLabel(&sFinishMovie,&ref,"fl_on_start");
    for(i=0;i<16;i++) Flash_Advance(&sFinishMovie);
    sFinishReady=1;
}
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
/* Original plate atlas and FIN movie, laid out across the same full-width
   canvas as the original reels. Only the count grows from five to ten. */
extern void Port_FlashImage(void *,float,float,float,float,int,int,int,int);
extern void Port_FlashRegion(MFlash *,float,float,float,float,float,float,float,float);
extern void Font_PushStyle(void),Font_PopStyle(void),Font_SetScale(float);
extern void Font_SetScaleXY(float,float),Font_SetAlign(int),Font_SetShadowMode(int);
extern void Font_SetColor(unsigned),Font_SetSpacing(int,int);
extern void Font_PrintAsciiAt(int,int,char *);
void Port_TeamSelectDraw(void) {
    int s,k;
    char labels[2][PORT_RELAY_MAX][4];
    unsigned edge=Ps_Rgba(12,24,35,128),light=Ps_Rgba(180,195,190,128);
    unsigned grey=Ps_Rgba(118,143,143,128),gold=Ps_Rgba(255,233,30,128);
    if(Port_TeamSelectLimit()<=TS_MEMBER_MAX || (gTeamSel->flags&TEAMSEL_STAGE)) return;
    Ps_PageFull();
    Font_PushStyle();Font_SetScale(1.0f);Font_SetScaleXY(.62f,.56f);
    Font_SetAlign(1);Font_SetSpacing(0,0);Font_SetShadowMode(0);
    Font_SetColor(Ps_Rgba(8,15,31,128));
    for(s=0;s<2;s++) {
        TeamSelSide *side=gTeamSel->side[s];
        float y=s?363.0f:17.0f;
        int tile=s?5:0;
        if(side->state!=TEAMSEL_ST_TEAM && side->state!=TEAMSEL_ST_DONE) continue;
        for(k=0;k<PORT_RELAY_MAX;k++) {
            float x=14.0f+k*41.0f;
            int selected=side->state==TEAMSEL_ST_TEAM && side->cur==k;
            /* The lower 56 texels contain the native bevel and blue well.
               The tab above is extended for two-digit slot numbers. */
            Port_FlashImage(gTeamSel->teamTex[s][0],x,y+14,40,36,(tile%4)*64,(tile/4)*72+16,64,56);
            Ps_Quad(x+12,y,x+40,y,x,y+15,x+40,y+15,edge,edge,edge,edge);
            Ps_Quad(x+13,y+2,x+37,y+2,x+2,y+14,x+37,y+14,light,light,grey,grey);
            if(selected) Ps_Rect(x+3,y+19,x+37,y+48,gold,gold);
            if(k<side->memberCount && side->member[k].chara>=0)
                Port_TeamChip(side->member[k].chara,x+5,y+21,30,25,128);
            else if(selected)
                Ps_Rect(x+5,y+21,x+35,y+46,Ps_Rgba(20,75,180,128),Ps_Rgba(0,0,75,128));
            sprintf(labels[s][k],"%d",k+1);
            Font_PrintAsciiAt((int)(x+20),(int)(y+1),labels[s][k]);
        }
        if(sFinishReady) Port_FlashRegion(&sFinishMovie,336,36,164,44,430,y+18,74,27);
        if(side->cur==TS_CUR_MENU && side->state==TEAMSEL_ST_TEAM) {
            Ps_Quad(430,y+22,435,y+18,430,y+41,435,y+45,gold,gold,gold,gold);
            Ps_Quad(499,y+18,504,y+22,499,y+45,504,y+41,gold,gold,gold,gold);
        }
    }
    Font_FlushAll();Font_PopStyle();
}
#endif

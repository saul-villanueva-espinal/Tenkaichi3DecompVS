/* Approved artwork is UI chrome only. Settings, progress, opponents and stage
   previews are drawn from the same state used to launch the actual battle. */
#include "sys/load_sprite.h"
#include "port_team_select.h"
extern void TeamSel_Init(s32),TeamSel_Term(void),TeamSel_UpdateFaceLoad(void),TeamSel_UpdateStageLoad(void),TeamSel_RequestStage(void);
extern void Sprite_DrawList(void *,s32,s32,LoadSprite *);
extern void Port_GsDrain(void);
static s32 sChallengeResources,sChallengeStage=-1;
static TsTeam sChallengeSavedTeams[2];
static s32 sChallengeSavedPlayers,sChallengeSavedType,sChallengeSavedDp,sChallengeSavedStage,sChallengeSavedBgm;

static void Challenge_ResourcesBegin(void) {
    s32 side,i;if(sChallengeResources)return;
    sChallengeSavedTeams[0]=gTsProgress->team[0];sChallengeSavedTeams[1]=gTsProgress->team[1];
    sChallengeSavedPlayers=gTsProgress->players;sChallengeSavedType=gTsProgress->battleType;
    sChallengeSavedDp=gTsProgress->dpLevel;sChallengeSavedStage=gTsProgress->stageCell;sChallengeSavedBgm=gTsProgress->bgm;
    memset(gTsProgress->team,0,sizeof(gTsProgress->team));gTsProgress->players=0;gTsProgress->battleType=1;
    gTsProgress->dpLevel=gTsProgress->stageCell=gTsProgress->bgm=0;
    for(side=0;side<2;side++)for(i=0;i<5;i++)gTsProgress->team[side].member[i].chara=i?-1:0;
    if(!gMenuArc5)gMenuArc5=File_LoadSync(gProgress->baseFile,NULL,0);
    TeamSel_Init(2);sChallengeResources=1;sChallengeStage=-1;
}
static void Challenge_ResourcesEnd(void) {
    if(!sChallengeResources)return;
    while(gTeamSel->faceState!=TEAMSEL_LOAD_IDLE || gTeamSel->stageState!=TEAMSEL_LOAD_IDLE) {
        Gfx_BeginFrame();TeamSel_UpdateFaceLoad();TeamSel_UpdateStageLoad();Gfx_EndFrame(1);Dma_Flush();File_Stub264D90();
    }
    Port_GsDrain();TeamSel_Term();Dma_ResetBuffers();sChallengeResources=0;sChallengeStage=-1;
    gTsProgress->team[0]=sChallengeSavedTeams[0];gTsProgress->team[1]=sChallengeSavedTeams[1];
    gTsProgress->players=sChallengeSavedPlayers;gTsProgress->battleType=sChallengeSavedType;
    gTsProgress->dpLevel=sChallengeSavedDp;gTsProgress->stageCell=sChallengeSavedStage;gTsProgress->bgm=sChallengeSavedBgm;
}
static s32 Challenge_Pick(PcBattle *battle,s32 floor) {
    extern s32 Port_TowerRosterPick(PcBattle *,s32);
    s32 chosen;
    /* The native picker owns TeamSel too: never keep a second copy alive. */
    Challenge_ResourcesEnd();chosen=floor>=0?Port_TowerRosterPick(battle,floor):Port_NativeRosterPick(battle,3,0);
    /* Cancellation returns into this very frame, before the loop's next
       ResourcesBegin: chips and stage buffers must exist before drawing. */
    if(!chosen){Challenge_ResourcesBegin();gPortNativeRosterActive=1;}
    return chosen;
}
static void Ch_Text(f32 x,f32 y,const char *s,f32 scale,f32 width,s32 gold) {
    f32 measured=Ps_TextWidth((char *)s,scale),sx=scale;
    if(width>0 && measured>width)sx*=width/measured;
    Ps_Align(0);Ps_SetTextKind(3);
    if(gold<0) {
        Ps_TextExOuterXY(x,y,(char *)s,sx,scale,0,Ps_Rgba(7,12,20,128),Ps_Rgba(7,12,20,128),
            Ps_Rgba(255,224,63,128),Ps_Rgba(255,224,63,128),0);Ps_SetTextKind(0);return;
    }
    Ps_TextExOuterXY(x,y,(char *)s,sx,scale,0,Ps_Rgba(255,255,gold?90:255,128),
        gold?Ps_Rgba(255,187,8,128):Ps_Rgba(205,226,255,128),Ps_Rgba(0,0,0,128),Ps_Rgba(0,0,0,128),scale*.18f);
    Ps_SetTextKind(0);
}
static void Ch_Fill(f32 x,f32 y,f32 w,f32 h) {
    Ps_Rect(x,y,x+w,y+h,Ps_Rgba(3,22,48,128),Ps_Rgba(1,10,27,128));
}
static void Ch_Edge(f32 x,f32 y,f32 w,f32 h,s32 selected) {
    u32 col=selected?Ps_Rgba(255,227,32,128):Ps_Rgba(25,176,255,128);
    Ps_Rect(x,y,x+w,y+1,col,col);Ps_Rect(x,y+h-1,x+w,y+h,col,col);
    Ps_Rect(x,y,x+1,y+h,col,col);Ps_Rect(x+w-1,y,x+w,y+h,col,col);
}
/* A patch of the approved frame, without resizing or rasterizing its text. */
static void Ch_Art(s32 id,f32 x,f32 y,f32 w,f32 h,f32 u,f32 v,f32 uw,f32 vh) {
    Ps_Sprite(id,x,y,x+w,y+h,u/512,v/448,(u+uw)/512,(v+vh)/448,PS_PLAIN,128);
}
static void Ch_Stage(s32 stage,f32 x,f32 y,f32 w,f32 h) {
    LoadSprite list[9];s32 row,col,i;void *res;
    if(sChallengeStage!=stage){sChallengeStage=stage;gTeamSel->stage->stage=stage;TeamSel_RequestStage();}
    Ch_Fill(x,y,w,h);
    /* A pending picture belongs to the previous selection: don't mislabel it. */
    if(!(gTeamSel->flags&TEAMSEL_STAGE_READY))return;
    res=gTeamSel->bg[0];if(!res)return;
    memset(list,0,sizeof(list));list[8].flags=LOAD_SPR_END;
    for(row=0;row<2;row++)for(col=0;col<4;col++) {
        i=row*4+col;list[i].flags=LOAD_SPR_DRAW;list[i].tex=i;
        list[i].x0=(s32)Ps_X(x+w*col/4);list[i].x1=(s32)Ps_X(x+w*(col+1)/4);
        list[i].y0=(s32)Ps_Y(y+h*row/2);list[i].y1=(s32)Ps_Y(y+h*(row+1)/2);
        list[i].u1=128;list[i].v1=256;list[i].r=list[i].g=list[i].b=list[i].a=128;
    }Sprite_DrawList(res,0,0,list);
}
static s32 Ch_PlayerCount(void){return sCustomFormat==2?1:sCustomFormat==1?5:10;}
static s32 Ch_EnemyCount(void){return sCustomFormat==2?1:sCustomFormat==1 && sCustomEnemyNum>5?5:sCustomEnemyNum;}
static void Ch_Footer(s32 tab) {
    f32 x=tab==0?316:130;
    /* Remove the presentation-only annotation and show current controls. */
    Ch_Fill(424,0,88,17);
    Ch_Fill(tab==0?314:0,418,tab==0?198:512,30);
    if(tab==0) {
        Ps_Button(0,327,433,13);Ch_Text(337,427,"Confirmar",.48f,38,0);
        Ps_Button(4,388,433,13);Ch_Text(398,427,"Volver",.48f,28,0);
        Ch_Text(430,427,"L1 / R1",.43f,30,0);Ch_Text(463,427,"Cambiar modo",.41f,45,0);return;
    }
    Ps_Button(0,x+10,433,14);Ch_Text(x+19,427,tab==2?"Elegir equipo":"Confirmar",.50f,70,0);
    Ps_Button(4,x+87,433,14);Ch_Text(x+97,427,"Volver",.50f,40,0);
    Ch_Text(x+143,427,"L1 / R1",.47f,32,0);Ch_Text(x+176,427,"Cambiar modo",.43f,65,0);
}
/* Manga tower: all selectable content is drawn from battle definitions. */
static void Ch_MangaText(f32 x,f32 y,const char *s,f32 scale,f32 width,s32 white) {
    f32 measured=Ps_TextWidth((char *)s,scale),sx=scale;u32 ink=white?Ps_Rgba(255,248,230,128):Ps_Rgba(17,10,8,128);
    if(measured>width&&width>0)sx*=width/measured;
    Ps_Align(0);Ps_SetTextKind(3);Ps_TextExOuterXY(x,y,(char *)s,sx,scale,0,ink,ink,white?Ps_Rgba(79,4,10,128):Ps_Rgba(239,221,188,128),0,0);Ps_SetTextKind(0);
}
static void Ch_Tower(s32 floorCur,s32 clears) {
    static const char *const names[5]={"ASALTO SAIYAN","SAGA FREEZER","SAGA CELL","SAGA MAJIN","SAGA DIVINA"};
    static const char *const shortNames[5]={"SAIYAN","FREEZER","CELL","MAJIN","DIVINA"};
    static const s32 leaders[5]={29,95,108,111,133};
    static const char *const rules[5]={"Ki +50%","Danio x1.25","HP +150 / s","Ki infinito","Super armadura"};
    const ChallengeFloorDef *fl=&sFloors[floorCur];s32 i;char buf[96];
    for(i=0;i<CHALLENGE_FLOORS;i++) {
        f32 y=70+(4-i)*67; s32 sel=i==floorCur;
        u32 ink=sel?Ps_Rgba(175,7,19,128):Ps_Rgba(225,209,177,124);
        Ps_Rect(39,y+1,154,y+51,ink,ink);
        Port_TeamChip(leaders[i],112,y+3,39,45,128);
        if(sel){u32 red=Ps_Rgba(235,13,25,128);Ps_Rect(36,y,157,y+2,red,red);Ps_Rect(36,y+50,157,y+52,red,red);}
        sprintf(buf,"%d",i+1);Ch_MangaText(18,y+13,buf,1.65f,15,1);
        Ch_MangaText(43,y+9,i?"SAGA":"ASALTO",.95f,65,sel);
        Ch_MangaText(43,y+25,shortNames[i],.95f,65,sel);
        if(clears&(1<<i))Ch_MangaText(43,y+43,"COMPLETADO",.35f,62,sel);
    }
    sprintf(buf,"PISO %d - ",floorCur+1);Ps_Cat(buf,names[floorCur],sizeof(buf));Ch_MangaText(326,70,buf,1.27f,169,0);
    Ch_MangaText(327,102,"Derrota al escuadron rival.",.65f,166,0);
    Ch_MangaText(327,126,"RIVALES",.76f,166,0);
    for(i=0;i<fl->enemyCount;i++) {
        f32 x=327+(i%5)*34,y=143+(i/5)*35;
        Ps_Rect(x,y,x+32,y+28,Ps_Rgba(12,9,7,128),Ps_Rgba(12,9,7,128));
        Port_TeamChip(fl->enemies[i],x+1,y+1,30,26,128);
        Ch_MangaText(x,y+28,Challenge_CharaName(fl->enemies[i]),.31f,32,0);
    }
    Ch_MangaText(328,219,"ESCENARIO",.69f,101,0);Ch_MangaText(432,219,"REGLAS",.69f,65,0);
    Ch_Stage(fl->stage,328,237,96,57);Ch_MangaText(328,297,sStageNames[fl->stage],.51f,96,0);
    Ch_MangaText(432,240,rules[floorCur],.65f,65,0);
    Ch_MangaText(432,263,"Sin limite",.60f,65,0);Ch_MangaText(432,277,"de tiempo",.60f,65,0);
    Ch_MangaText(328,316,"DIFICULTAD",.69f,74,0);
    for(i=0;i<5;i++)Challenge_Star(414+i*17,325,6.0f,i<fl->difficulty?Ps_Rgba(180,8,20,128):Ps_Rgba(121,113,100,128));
}
static void Ch_Survival(s32 best) {
    char buf[48];Ch_Fill(52,201,92,25);sprintf(buf,"%d OLEADAS",best);Ch_Text(57,199,buf,1.43f,85,0);
    /* The remaining rules and ten empty roster slots are the approved artwork.
       X enters the native ten-member picker, where the actual team is chosen. */
    Ch_Text(25,309,"TRIANGULO: reiniciar record",.36f,130,0);
}
static void Ch_Custom(void) {
    static const char *const label[7]={"Formato","Rivales","Equipo rival","Dificultad","Escenario","Musica","Danio"};
    static const char *const formats[3]={"Relevos 10 vs 10","Tag Team 5 vs 5","Duelo 1 vs 1"};
    char num[32],buf[40];const char *v[7];s32 i;
    sprintf(num,"%d luchadores",Ch_EnemyCount());v[0]=formats[sCustomFormat];v[1]=num;
    v[2]=sArchetypeNames[sCustomArchetype];v[3]=sAiNames[sCustomAi];v[4]=Ch_MapName(sCustomStage);v[5]=sBgmNames[sCustomBgm];v[6]=sModNames[sCustomMod];
    for(i=0;i<7;i++) {
        f32 y=89+i*34.8f; s32 sel=i==sCustomCur;
        Ch_Art(TBO_CHALLENGE_CUSTOM,9,y,181,34,9,sel?89:124,181,34);
        if(sel)Ps_Rect(18,y+5,176,y+30,Ps_Rgba(255,231,36,128),Ps_Rgba(255,194,13,128));
        else Ch_Fill(18,y+5,158,25);
        Ch_Text(20,y+8,label[i],.90f,65,sel?-1:0);
        Ch_Text(85,y+8,"<",.84f,6,sel?-1:0);Ch_Text(104,y+9,v[i],.79f,63,sel?-1:0);Ch_Text(171,y+8,">",.84f,6,sel?-1:0);
    }
    Ch_Stage(sCustomStage,330,105,169,114);Ch_Fill(330,220,169,17);Ch_Text(384,220,Ch_MapName(sCustomStage),.84f,113,0);
    Ch_Fill(350,255,31,32);Ch_Text(351,254,sCustomFormat==0?"RELEVOS":sCustomFormat==1?"TAG TEAM":"DUELO",.64f,30,1);
    sprintf(buf,"%d vs %d",Ch_PlayerCount(),Ch_PlayerCount());Ch_Text(352,270,buf,.58f,27,0);
    Ch_Fill(422,252,17,23);sprintf(buf,"%d",Ch_EnemyCount());Ch_Text(423,249,buf,1.40f,16,1);
    Ch_Fill(480,272,18,11);Ch_Text(480,271,sCustomMod==0?"x1.0":sCustomMod==1?"x1.25":sCustomMod==2?"x1.5":"x2.0",.61f,18,0);
    Ch_Fill(348,306,151,18);Ch_Text(350,305,sBgmNames[sCustomBgm],.85f,146,0);
    /* Formats with fewer slots must not advertise a team of ten. */
    if(Ch_PlayerCount()!=10)for(i=Ch_PlayerCount();i<10;i++) {
        f32 x=12+i*31.5f;Ch_Fill(x,363,30,38);Ch_Text(x+9,374,"--",.75f,15,0);
    }
}
static void Challenge_DrawReference(s32 tab,s32 floorCur,s32 clears,s32 best) {
    static const s32 art[3]={TBO_TOWER_MANGA,TBO_CHALLENGE_SURVIVAL,TBO_CHALLENGE_CUSTOM};
    Ps_PageFull();Ps_SetShift(0);Ps_Sprite(art[tab],0,0,512,448,0,0,1,1,PS_PLAIN,128);
    if(tab==0)Ch_Tower(floorCur,clears);else if(tab==1)Ch_Survival(best);else Ch_Custom();
    if(tab)Ch_Footer(tab);
}
static void Challenge_Pointer(s32 tab,s32 mx,s32 my,s32 *active,s32 *floor,u32 *p) {
    s32 i;f32 tx,ty,tw;
    if(tab==0) {
        if(my>=Ps_Y(20)&&my<=Ps_Y(52)&&mx>=Ps_X(259)) {
            *active=mx<Ps_X(337)?0:mx<Ps_X(411)?1:2;Snd_PlaySe(1,0);return;
        }
        if(mx>=Ps_X(12)&&mx<=Ps_X(160))for(i=0;i<5;i++) {
            f32 y=70+(4-i)*67;if(my>=Ps_Y(y)&&my<=Ps_Y(y+52)){*floor=i;Snd_PlaySe(1,0);return;}
        }
        if(mx>=Ps_X(320)&&my>=Ps_Y(346)&&my<=Ps_Y(405)){*p|=PADG_CROSS;return;}
        if(my>=Ps_Y(417)) {
            if(mx>=Ps_X(325)&&mx<Ps_X(375))*p|=PADG_CROSS;
            else if(mx>=Ps_X(375)&&mx<Ps_X(425))*p|=PADG_CIRCLE;
            else if(mx>=Ps_X(425))*p|=PADG_R1;
        }
        return;
    }
    tx=tab==0?237:tab==1?93:10,ty=tab==0?18:tab==1?43:4,tw=tab==0?90:tab==1?108:90;
    if(my>=Ps_Y(ty)&&my<=Ps_Y(ty+29))for(i=0;i<3;i++)if(mx>=Ps_X(tx+i*tw)&&mx<=Ps_X(tx+(i+1)*tw)) {
        *active=i;Snd_PlaySe(1,0);return;
    }
    if(tab==0 && mx>=Ps_X(27)&&mx<=Ps_X(133))for(i=0;i<5;i++) {
        f32 y=81+(4-i)*64;if(my>=Ps_Y(y)&&my<=Ps_Y(y+49)){*floor=i;Snd_PlaySe(1,0);return;}
    }
    if(tab==2 && mx>=Ps_X(9)&&mx<=Ps_X(190))for(i=0;i<7;i++) {
        f32 y=89+i*34.8f;if(my>=Ps_Y(y)&&my<=Ps_Y(y+34)) {
            sCustomCur=i;*p|=mx<Ps_X(97)?PADG_LEFT:PADG_RIGHT;return;
        }
    }
    if((tab==0&&mx>=Ps_X(321)&&my>=Ps_Y(342)&&my<=Ps_Y(391)) ||
       (tab==1&&mx>=Ps_X(378)&&my>=Ps_Y(326)&&my<=Ps_Y(407)) ||
       (tab==2&&mx>=Ps_X(367)&&my>=Ps_Y(357)&&my<=Ps_Y(407))) {*p|=PADG_CROSS;return;}
    if(my>=Ps_Y(418)) {
        f32 x=tab==0?316:130;
        if(mx>=Ps_X(x)&&mx<Ps_X(x+75))*p|=PADG_CROSS;
        else if(mx>=Ps_X(x+75)&&mx<Ps_X(x+140))*p|=PADG_CIRCLE;
        else if(mx>=Ps_X(x+140))*p|=PADG_R1;
    }
}

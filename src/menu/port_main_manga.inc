/* Unified sixteen-entry hub. The approved illustration remains the actual
   full-resolution UI artwork, not a re-created approximation of its panels. */
#include "menu/port_screen.h"
#include "menu/tbo_art.h"
extern int Port_RosterPointerRead(int *,int *),Port_RosterTestMode(void);
extern unsigned Port_RosterTestInput(unsigned);
extern volatile int gPortPcHubReady;
extern int Port_PcBattlePending(void);
static s32 sMangaCursor=14,sMangaPlayers,sMangaPlayerCursor,sMangaAge;
static s32 sMangaOnline;
static const char *const sMangaNames[16]={
    "Modo Historia","Ultimate Battle","Torneo Mundial","Duelo",
    "Duelo Marcial","Online","Evolution Z","Entrenamiento",
    "Data Center","Referencia de Personaje","Opciones","Mod Manager",
    "Tag Team","Todos vs Todos","Jefe Final","Torre de Desafios"
};
static const char *const sMangaHelp[16]={
    "Revive las grandes sagas de Dragon Ball.","Supera los retos de Ultimate Battle.",
    "Compite por el titulo del torneo mundial.","Elige tus guerreros y prepara un duelo.",
    "Combate cuerpo a cuerpo en tierra. Sin vuelo ni poderes; puro combate marcial.","Entra al centro de modos online.",
    "Potencia y personaliza a tus guerreros.","Practica tus tecnicas y movimientos.",
    "Consulta tus datos y registros de juego.","Consulta la informacion de los personajes.",
    "Ajusta las opciones del juego.","Administra tus paquetes de personajes.",
    "Combate junto a tu equipo.","Batalla campal: todos contra todos.",
    "Enfrenta a los jefes del multiverso.","Supera pisos y batallas del multiverso."
};
/* Rectangular interiors of the slightly slanted printed comic panels, in
   original 1672 x 941 artwork pixels. Hit testing and highlight share these. */
static const s16 sMangaBounds[16][4]={
    {238,282,535,409},{545,282,827,409},{840,282,1124,409},{1140,282,1425,409},
    {220,425,529,553},{540,425,829,553},{839,425,1137,553},{1146,425,1453,553},
    {205,571,525,690},{539,571,829,690},{841,571,1148,690},{1160,571,1464,690},
    {196,708,520,831},{539,708,829,831},{842,708,1142,831},{1154,708,1467,831}
};
static f32 Mm_X(s32 x){return x*(512.0f/1672);}
static f32 Mm_Y(s32 y){return y*(448.0f/941);}
static s32 Mm_Move(s32 cur,u32 repeat) {
    s32 row=cur/4,col=cur%4;
    if(repeat&(PADG_LEFT|PADG_LEFT2))col=(col+3)%4;
    else if(repeat&(PADG_RIGHT|PADG_RIGHT2))col=(col+1)%4;
    else if(repeat&(PADG_UP|PADG_UP2))row=(row+3)%4;
    else if(repeat&(PADG_DOWN|PADG_DOWN2))row=(row+1)%4;
    return row*4+col;
}
static s32 Mm_Hit(s32 x,s32 y) {
    s32 i;for(i=0;i<16;i++)if(x>=Mm_X(sMangaBounds[i][0])&&x<=Mm_X(sMangaBounds[i][2])&&
        y>=Mm_Y(sMangaBounds[i][1])&&y<=Mm_Y(sMangaBounds[i][3]))return i;
    return -1;
}
static void Mm_Text(f32 x,f32 y,const char *str,f32 size,f32 width,s32 red) {
    f32 w=Ps_TextWidth((char *)str,size),sx=size;if(width>0&&w>width)sx*=width/w;
    Ps_Align(0);Ps_SetTextKind(3);
    Ps_TextExOuterXY(x,y,(char *)str,sx,size,0,red?Ps_Rgba(255,46,36,128):Ps_Rgba(255,255,245,128),
        red?Ps_Rgba(207,10,12,128):Ps_Rgba(235,235,222,128),Ps_Rgba(0,0,0,128),0,0);
    Ps_SetTextKind(0);
}
static void Mm_Draw(void) {
    s32 i=sMangaCursor;const s16 *b=sMangaBounds[i];
    Ps_PageFull();Ps_SetShift(0);
    /* At the approved initial selection, show the user's exact image. */
    Ps_Sprite(TBO_MAIN_MANGA,0,0,512,448,0,0,1,1,PS_PLAIN,128);
    /* Always overlay card 4 ribbon to display Duelo Marcial cleanly over background artwork */
    {
        f32 x4=Mm_X(sMangaBounds[4][0]),y4=Mm_Y(sMangaBounds[4][1]);
        f32 w4=Mm_X(sMangaBounds[4][2]-sMangaBounds[4][0]),h4=Mm_Y(sMangaBounds[4][3]-sMangaBounds[4][1]);
        Ps_Rect(x4+1,y4+h4-17,x4+w4-1,y4+h4-1,Ps_Rgba(14,14,16,230),Ps_Rgba(8,8,10,230));
        Mm_Text(x4+4,y4+h4-18,sMangaNames[4],.88f,w4-8,0);
    }
    if(i!=14) {
        f32 x=Mm_X(b[0]),y=Mm_Y(b[1]),w=Mm_X(b[2]-b[0]),h=Mm_Y(b[3]-b[1]);
        u32 red=Ps_Rgba(255,18,20,128),white=Ps_Rgba(255,195,180,128);
        /* Only the originally selected card and context band are neutralized.
           Every other pixel comes directly from the user's exact attachment. */
        Ps_Sprite(TBO_MAIN_MANGA_NEUTRAL,Mm_X(834),Mm_Y(696),Mm_X(1152),Mm_Y(842),
            834/1672.0f,696/941.0f,1152/1672.0f,842/941.0f,PS_PLAIN,128);
        Ps_Sprite(TBO_MAIN_MANGA_NEUTRAL,Mm_X(545),Mm_Y(844),Mm_X(1320),Mm_Y(887),
            545/1672.0f,844/941.0f,1320/1672.0f,887/941.0f,PS_PLAIN,128);
        /* Redraw only the selected panel with red ink and a lit border. */
        Ps_Sprite(TBO_MAIN_MANGA,x,y,x+w,y+h,b[0]/1672.0f,b[1]/941.0f,b[2]/1672.0f,b[3]/941.0f,0x151580,128);
        Ps_Rect(x-1,y-1,x+w+1,y,red,red);Ps_Rect(x-1,y+h,x+w+1,y+h+1,red,red);
        Ps_Rect(x-1,y,x,y+h,red,red);Ps_Rect(x+w,y,x+w+1,y+h,red,red);
        Ps_Rect(x,y,x+w,y+.45f,white,white);Ps_Rect(x,y+h-.45f,x+w,y+h,white,white);
        Ps_Rect(x+1,y+h-17,x+w-1,y+h-1,Ps_Rgba(178,2,9,128),Ps_Rgba(130,0,5,128));
        Mm_Text(x+4,y+h-18,sMangaNames[i],.88f,w-8,0);
        /* The neutral artwork has an empty brushstroke context band. */
        Mm_Text(Mm_X(552),Mm_Y(849),sMangaNames[i],.86f,98,1);
        Mm_Text(Mm_X(884),Mm_Y(850),sMangaHelp[i],.62f,175,0);
    }
    if(sMangaPlayers) {
        static const char *const players[5]={"1 jugador contra CPU","2 jugadores - pantalla dividida","3 jugadores - cuatro cuadrantes","4 jugadores - cuatro cuadrantes","ONLINE - SALAS ORACLE"};
        Ps_Rect(130,116,383,386,Ps_Rgba(245,228,191,128),Ps_Rgba(211,191,153,128));
        Ps_Rect(132,118,381,384,Ps_Rgba(8,7,7,128),Ps_Rgba(20,10,10,128));
        Mm_Text(147,124,"NUMERO DE JUGADORES",1.05f,221,0);
        for(i=0;i<5;i++) {
            f32 y=155+i*29;if(i==sMangaPlayerCursor)Ps_Rect(144,y,370,y+26,Ps_Rgba(197,8,15,128),Ps_Rgba(113,2,6,128));
            Mm_Text(153,y+2,players[i],.68f,210,0);
        }
        Mm_Text(148,365,"X: confirmar   O: volver",.52f,210,0);
    }
    Font_FlushAll();
}
/* Return a real progress destination, -1 for the native network overlay. */
static s32 Mm_Target(s32 item) {
    static const s32 modes[16]={6,13,33,38,38,80,48,44,53,60,62,38,40,40,80,80};
    return item>=0&&item<16?modes[item]:0;
}
static s32 Mm_Choose(s32 item,s32 players) {
    s32 next=Mm_Target(item);
    if(next<0){Port_NetMenuOpen();return 0;}
    if(item==4)Port_MartialModeSet(1);
    else Port_MartialModeSet(0);
    if(item==5)gPortScreen=PORT_SCREEN_TBO;
    else if(item==11)gPortDuelRole=2;
    else if(item==12 || item==13) {
        next=Port_ModesChoosePlayers(item-12,players);
        if(next==40) {
            sPortModesDuel[0]=*(s32 *)((u8 *)gProgress+0x620);sPortModesDuel[1]=*(s32 *)((u8 *)gProgress+0x624);
            *(s32 *)((u8 *)gProgress+0x620)=players>=2?1:0;*(s32 *)((u8 *)gProgress+0x624)=1;
        }
    }else if(item==14)gPortScreen=PORT_SCREEN_RAID;
    else if(item==15)gPortScreen=PORT_SCREEN_CHALLENGE;
    return next;
}
static s32 MainManga_Run(void) {
    s32 result=1,leaving=0,mx,my,click,old,next,hit,savedPointer=gPortNativeRosterActive;
    u32 p,r;PcBattle discard;
    Port_MartialModeSet(0);
    gPortDuelRole=0;sMangaPlayers=0;sMangaAge=0;sMangaOnline=0;sPortModes=0;
    Port_ModesShow(NULL);Port_PcTeamsTake(&discard);gPortPcHubReady=1;gPortNativeRosterActive=1;
    ColorFade_StartIn(0,0,0,20);
    while(1) {
        Gfx_BeginFrame();Pad_Update();Snd_Update();ColorFade_Update();sMangaAge++;
        p=gPad[0].gamePressed|(Port_RosterTestMode()==14?Port_RosterTestInput(sMangaAge):0);r=gPad[0].gameRepeat|p;
        if(gProgress->flags&MPROG_FREEZE)p=r=0;
        Ps_PageFull();Ps_SetShift(0);click=Port_RosterPointerRead(&mx,&my);
        if(!leaving) {
            if(sMangaPlayers) {
                old=sMangaPlayerCursor;
                if(r&(PADG_UP|PADG_UP2))sMangaPlayerCursor=Ps_Wrap(sMangaPlayerCursor-1,0,4);
                else if(r&(PADG_DOWN|PADG_DOWN2))sMangaPlayerCursor=Ps_Wrap(sMangaPlayerCursor+1,0,4);
                if(click&&mx>=144&&mx<=370&&my>=155&&my<300){sMangaPlayerCursor=(my-155)/29;p|=PADG_CROSS;}
                if(old!=sMangaPlayerCursor)Snd_PlaySe(1,0);
                if(p&(PADG_CIRCLE|PADG_TRIANGLE)){sMangaPlayers=0;Snd_PlaySe(1,2);}
                else if(p&PADG_CROSS) {
                    sMangaOnline=sMangaPlayerCursor==4?(sMangaCursor==12?PC_TAG:PC_FFA):0;
                    next=Mm_Choose(sMangaCursor,sMangaOnline?2:sMangaPlayerCursor+1);
                    if(next>0){gProgress->mode=next;leaving=1;MAINMENU_COUNT();Snd_PlaySe(1,1);ColorFade_StartOut(0,0,0,15);}
                }
            }else {
                old=sMangaCursor;sMangaCursor=Mm_Move(sMangaCursor,r);
                if(click) {
                    hit=Mm_Hit(mx,my);if(hit>=0){sMangaCursor=hit;p|=PADG_CROSS;}
                    else if(my>=425&&mx>=438&&mx<=467)p|=PADG_CIRCLE;
                }
                if(old!=sMangaCursor)Snd_PlaySe(1,0);
                if(p&PADG_CROSS) {
                    if(sMangaCursor==12||sMangaCursor==13){sMangaPlayers=1;sMangaPlayerCursor=0;Snd_PlaySe(1,1);}
                    else {next=Mm_Choose(sMangaCursor,1);if(next>0){gProgress->mode=next;leaving=1;MAINMENU_COUNT();Snd_PlaySe(1,1);ColorFade_StartOut(0,0,0,15);}}
                }else if(p&(PADG_CIRCLE|PADG_TRIANGLE)){leaving=1;result=0;Snd_PlaySe(1,2);ColorFade_StartOut(0,0,0,15);}
            }
            if(Port_PcBattlePending()){gPortScreen=PORT_SCREEN_PC_BATTLE;gProgress->mode=MPROG_MODE_PORT_SCREEN;result=1;leaving=1;ColorFade_StartOut(0,0,0,15);}
        }
        Mm_Draw();ColorFade_Draw();Gfx_EndFrame(1);Dma_Flush();File_Stub264D90();
        if(leaving&&ColorFade_IsOutDone())break;
    }
    sceGsSyncPath(0,0);gPortPcHubReady=0;gPortNativeRosterActive=savedPointer;Dma_ResetBuffers();
    if(sMangaOnline) {
        extern s32 PortVersusRoom_Run(s32);
        gPortPcHubReady=1;
        if(PortVersusRoom_Run(sMangaOnline)){gPortScreen=PORT_SCREEN_PC_BATTLE;gProgress->mode=MPROG_MODE_PORT_SCREEN;}
        else {gProgress->mode=4;result=1;}
        gPortPcHubReady=0;
    }
    return result;
}

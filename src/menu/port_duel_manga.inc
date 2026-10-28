#include "menu/port_screen.h"
#include "menu/tbo_art.h"
#include "port_manga_widgets.inc"
static s32 sDuelMangaPointer;
static void Dm_MangaDraw(void) {
    static const s32 rect[4][4]={{40,148,875,177},{38,343,839,158},{25,523,830,150},{24,693,790,85}};
    static const char names[4][32]={"JUGADOR VS CPU","JUGADOR VS JUGADOR","CPU VS CPU","AJUSTES DE BATALLA"};
    static const char help[4][80]={"Selecciona ambos equipos y prepara el enfrentamiento.","Elige los guerreros de los dos jugadores.","Configura y observa un combate entre las CPU.","Ajusta las reglas antes de combatir."};
    s32 cur=DM->sel[0];Ps_PageFull();Ps_SetShift(0);
    Ps_Sprite(cur?TBO_MANGA_DUEL_NEUTRAL:TBO_MANGA_DUEL,0,0,512,448,0,0,1,1,PS_PLAIN,128);
    Mg_Paper(1410,0,262,27); /* Presentation annotation is absent in the game. */
    if(cur){Mg_Outline(rect[cur][0],rect[cur][1],rect[cur][2],rect[cur][3],1);Mg_Rect(rect[cur][0],rect[cur][1],7,rect[cur][3],Ps_Rgba(217,10,21,128));}
    Mg_Ink(179,821,522,71);Mg_Text(190,826,names[cur],30,495,1);Mg_Text(191,866,help[cur],18,495,1);
    if((DM->flags&DUELMENU_NO_PAD2)&&cur==1){Mg_Ink(756,819,393,64);Mg_Text(771,837,"CONECTA EL SEGUNDO MANDO",25,366,1);}
    Font_FlushAll();
}
static void Dm_MangaPointer(void) {
    s32 x,y,hit=-1,clicked;
    extern volatile s32 gPortNativeRosterActive;
    extern s32 Port_RosterPointerRead(s32 *,s32 *);
    if(!sDuelMangaPointer){sDuelMangaPointer=1;}
    gPortNativeRosterActive=1;clicked=Port_RosterPointerRead(&x,&y);
    if(DM->level!=DUEL_LEVEL_TOP)return;
    if(clicked) {
        if(Mg_Hit(x,y,40,148,875,177))hit=0;
        else if(Mg_Hit(x,y,38,343,839,158))hit=1;
        else if(Mg_Hit(x,y,25,523,830,150))hit=2;
        else if(Mg_Hit(x,y,24,693,790,85))hit=3;
        else if(Mg_Hit(x,y,757,816,395,76))hit=DM->sel[0];
        if(hit>=0){DM->sel[0]=hit;gPad[0].gamePressed|=PADG_CROSS;}
    }
    /* Match the artwork's O/Volver prompt; retain the native state machine. */
    if(gPad[0].gamePressed&PADG_CIRCLE)gPad[0].gamePressed|=PADG_TRIANGLE;
}

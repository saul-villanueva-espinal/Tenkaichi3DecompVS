#include "common.h"
#ifdef PORT
#include "menu/menu_e.h"
#include "menu/menu_f.h"
#include "menu/port_screen.h"
#include "sys/pad.h"
#include "sys/save.h"
#include "sys/loading.h"
#include "port_pc_battle.h"
#include "port_coop.h"
#include "port_fighters.h"

/* Game-thread roster. Every panel, portrait and tile is submitted through the
   game's sprite/font/DMA path; this screen never opens the settings overlay. */
extern s32 Port_ModesFight(void), Port_LocalPlayers(void), Port_NetActive(void), Port_NetSession(void);
extern s32 Port_ExtraCostumeOk(s32,s32,char *,s32), ChrTbl_GetCost(s32);
extern void Port_LocalPlayersSet(s32);
extern void Port_SettingSave(const char *,s32);
extern void Port_RosterLog(s32,s32,s32,s32,s32);
extern void Port_RosterName(char *,s32,s32);
extern void Port_PcNativeRules(s32,s32);
extern void TeamSel_Init(s32), TeamSel_Term(void), TeamSel_UpdateFaceLoad(void), TeamSel_UpdateStageLoad(void);
extern void TeamSel_RequestFace(s32), TeamSel_RequestStage(void);
extern void Sprite_DrawList(void *,s32,s32,LoadSprite *);
extern f32 sinf(f32),cosf(f32);
extern void Ps_PageSet(void), Font_FlushAll(void);

#define NR_COLS 12
#define NR_PAGE 36
#define NR_CX 16
#define NR_CY 250
#define NR_DX 40
#define NR_DY 38
static PcBattle sNr;
static s32 sNrKind,sNrSide,sNrSlot[2],sNrFilled[2][10],sNrCursor[2],sNrPhase,sNrFavoriteOnly;
static s32 sNrFavorite[PORT_CHARA_ID_LIMIT],sNrList[PORT_TOTAL_CHARAS],sNrCount,sNrAge,sNrDp;
static s32 sNrConnection,sNrField,sNrIp[5]={127,0,0,1,29740};
static char sNrMessage[160];
static s32 sNrCostume[2];

static void Nr_Text(f32 x,f32 y,char *text,f32 size,f32 width,s32 style) {
    Ps_Align(0);Ps_Text(x,y,text,size,width,style);
}
static void Nr_Box(f32 x,f32 y,f32 w,f32 h,s32 selected,s32 side) {
    u32 gold=Ps_Rgba(255,196,66,128),edge=Ps_Rgba(96,123,156,128);
    Ps_Rect(x-1,y-1,x+w+1,y+h+1,selected?gold:edge,selected?gold:edge);
    Ps_Rect(x,y,x+w,y+h,selected?Ps_Rgba(122,62,16,128):Ps_Rgba(side?24:12,side?35:32,side?66:54,128),Ps_Rgba(7,12,24,128));
}
/* Texture size is in GS TEX0. Portrait packages keep the same resource format. */
static void Nr_Image(void *resource,s32 tex,f32 x,f32 y,f32 w,f32 h,s32 flip,s32 alpha) {
    MTexRes *res=resource;u64 tex0;LoadSprite list[2];s32 tw,th;
    if(!res || !res->tex) return;
    tex0=*(u64 *)(res->tex+tex*0x40+0x30);
    tw=1<<((tex0>>26)&15);th=1<<((tex0>>30)&15);
    if(tw<1||tw>1024||th<1||th>1024) return;
    memset(list,0,sizeof(list));list[0].flags=LOAD_SPR_DRAW;list[1].flags=LOAD_SPR_END;
    list[0].x0=(s32)Ps_X(x);list[0].y0=(s32)Ps_Y(y);list[0].x1=(s32)Ps_X(x+w);list[0].y1=(s32)Ps_Y(y+h);
    list[0].u0=flip?tw:0;list[0].u1=flip?0:tw;list[0].v1=th;list[0].tex=tex;
    list[0].r=list[0].g=list[0].b=128;list[0].a=alpha;
    Sprite_DrawList(res,0,0,list);
}
static void Nr_Chip(s32 chara,f32 x,f32 y,f32 w,f32 h,s32 alpha) {
    if(chara>=0) Nr_Image(PORT_CHIP(gTeamSel->chipPack,chara),0,x,y,w,h,0,alpha);
}
static void Nr_List(void) {
    s32 i,id;sNrCount=0;
    for(i=0;i<PORT_TOTAL_CHARAS;i++) {
        id=Port_PcRosterId(i);
        if(!sNrFavoriteOnly || (id>=0 && sNrFavorite[id])) sNrList[sNrCount++]=i;
    }
    if(sNrCursor[sNrSide]>=sNrCount) sNrCursor[sNrSide]=sNrCount?sNrCount-1:0;
}
static s32 Nr_Id(void) {return sNrCount?Port_PcRosterId(sNrList[sNrCursor[sNrSide]]):-1;}
static void Nr_Face(void) {
    s32 s,id=Nr_Id();
    for(s=0;s<2;s++) {
        s32 target=s==sNrSide?id:sNr.fighter[s][sNrSlot[s]].chara;
        s32 color=s==sNrSide?sNrCostume[s]:sNr.fighter[s][sNrSlot[s]].costume;
        s32 changed=target!=gTeamSel->side[s]->chara || color!=gTeamSel->side[s]->member[0].color;
        if(sNrKind==3 && s==1) target=-1;
        gTeamSel->side[s]->cur=0;
        gTeamSel->side[s]->member[0].color=color;
        if(changed) {gTeamSel->side[s]->chara=target;TeamSel_RequestFace(s);}
    }
}
static s32 Nr_Cost(s32 s) {
    s32 i,n=0;for(i=0;i<sNr.count[s];i++) if(sNrFilled[s][i]) n+=ChrTbl_GetCost(sNr.fighter[s][i].chara);return n;
}
static s32 Nr_Complete(void) {
    s32 s,i;for(s=0;s<(sNrKind==3?1:2);s++) for(i=0;i<sNr.count[s];i++) if(!sNrFilled[s][i]) return 0;
    if(sNrDp>0 && (Nr_Cost(0)>sNrDp || (sNrKind!=3 && Nr_Cost(1)>sNrDp)))return 0;
    return 1;
}
static void Nr_Selected(void) {
    s32 id=Nr_Id(),side=sNrSide,slot=sNrSlot[side],next;char why[160];
    if(id<0) {Ps_Copy(sNrMessage,"Casilla reservada: instala un personaje para usarla.",160);Snd_PlaySe(1,7);return;}
    if(sNrDp>0 && Nr_Cost(side)-(sNrFilled[side][slot]?ChrTbl_GetCost(sNr.fighter[side][slot].chara):0)+ChrTbl_GetCost(id)>sNrDp) {
        Ps_Copy(sNrMessage,"El personaje supera los puntos disponibles del equipo.",160);Snd_PlaySe(1,7);return;
    }

    if(!Port_ExtraCostumeOk(id,sNrCostume[side],why,sizeof(why))) {Ps_Copy(sNrMessage,"Ese traje no tiene todos sus archivos. Elige otro traje.",160);return;}
    sNr.fighter[side][slot].chara=id;sNr.fighter[side][slot].costume=sNrCostume[side];sNrFilled[side][slot]=1;sNrMessage[0]=0;
    Port_RosterLog(1,side,slot,id,sNr.fighter[side][slot].costume);Snd_PlaySe(1,1);
    next=slot+1;
    if(next<sNr.count[side]) sNrSlot[side]=next;
    else if(side==0 && sNrKind!=3) {sNrSide=1;Nr_List();}
}
static void Nr_StageRequest(void) {
    gTeamSel->stage->stage=sNr.stage;TeamSel_RequestStage();
}
static void Nr_Star(f32 x,f32 y,f32 r) {
    s32 k;for(k=0;k<5;k++) {f32 a=-1.5707963f+k*1.2566371f,b=a+0.6283185f,c=a+1.2566371f;u32 red=Ps_Rgba(180,40,10,128);
        Ps_Quad(x,y,x+cosf(a)*r,y+sinf(a)*r,x+cosf(b)*r*.4f,y+sinf(b)*r*.4f,x+cosf(c)*r,y+sinf(c)*r,red,red,red,red);
    }
}
static void Nr_Background(void) {
    s32 i,j;
    if(gTeamSel->bg[0] && (gTeamSel->flags&TEAMSEL_STAGE_READY)) Sprite_DrawPicture(gTeamSel->bg[0],0,0,96);
    else if(gTeamSel->bg[1]) Sprite_DrawPicture(gTeamSel->bg[1],0,0,96);
    Ps_Rect(0,0,512,448,Ps_Rgba(8,24,54,104),Ps_Rgba(5,10,24,124));
    Ps_Quad(0,0,210,0,0,210,110,130,Ps_Rgba(255,120,12,44),Ps_Rgba(255,120,12,0),Ps_Rgba(255,120,12,0),Ps_Rgba(255,120,12,0));
    for(i=0;i<7;i++) {f32 x=356+i*22.0f;Ps_Disc(x,16,7,Ps_Rgba(255,214,84,128),Ps_Rgba(228,100,4,128));
        for(j=0;j<=i;j++){f32 a=j*6.2831853f/(i+1);Nr_Star(x+(i?cosf(a)*3.0f:0),16+(i?sinf(a)*3.0f:0),1.6f);}
    }
    Ps_Rect(12,33,500,35,Ps_Rgba(255,196,56,128),Ps_Rgba(255,120,20,128));
}
static void Nr_Portraits(void) {
    s32 s,i;char line[160];
    for(s=0;s<2;s++) {
        f32 x=s?266.0f:16.0f; s32 active=sNrSide==s;
        Nr_Box(x,42,230,149,active,s);
        if(sNrKind==3 && s==1) {
            Nr_Text(x+12,82,"RIVAL DEL MODO",.67f,205,1);
            Nr_Text(x+12,110,"Las reglas y el rival se conservan.",.43f,205,0);
        } else {
            if((gTeamSel->side[s]->flags&(TEAMSEL_SIDE_FACE_READY|TEAMSEL_SIDE_NO_FACE))==TEAMSEL_SIDE_FACE_READY)
                Nr_Image(gTeamSel->faceRes[s],0,x+18,43,194,147,s,128);
            Ps_Rect(x,155,x+230,191,Ps_Rgba(12,16,27,116),Ps_Rgba(14,19,34,128));
            if(active && Nr_Id()<0) Ps_Copy(line,"CASILLA RESERVADA",160);
            else Port_RosterName(line,160,gTeamSel->side[s]->chara>=0?gTeamSel->side[s]->chara:sNr.fighter[s][sNrSlot[s]].chara);
            Nr_Text(x+8,157,line,.55f,214,1);
            sprintf(line,"Traje %d  |  Integrante %d de %d",(active?sNrCostume[s]:sNr.fighter[s][sNrSlot[s]].costume)+1,sNrSlot[s]+1,sNr.count[s]);
            Nr_Text(x+8,176,line,.37f,214,0);
        }
        if(sNrKind==3 && s==1) continue;
        sprintf(line,sNr.mode==PC_RAID?(s?"JEFE CPU":"ALIADOS"):s?"EQUIPO 2":"EQUIPO 1");
        Nr_Text(x,193,line,.35f,112,s?2:1);
        if(sNrDp>0) {sprintf(line,"PD %d / %d",Nr_Cost(s),sNrDp);Nr_Text(x+141,193,line,.35f,88,0);}
        for(i=0;i<sNr.count[s];i++) {
            f32 xx=x+i*23.0f;Nr_Box(xx,207,21,22,active&&sNrSlot[s]==i,s);
            if(sNrFilled[s][i]) Nr_Chip(sNr.fighter[s][i].chara,xx+1,208,19,20,128);
            else {sprintf(line,"%d",i+1);Nr_Text(xx+5,211,line,.42f,16,3);}
        }
    }
}
static void Nr_DrawGrid(void) {
    s32 page=sNrCursor[sNrSide]/NR_PAGE,k,idx,id;char text[100];
    if(sNrFavoriteOnly)sprintf(text,"FAVORITOS  %d",sNrCount);else sprintf(text,"PERSONAJES Y FORMAS  %d / %d",Port_PcRosterCount(),PORT_TOTAL_CHARAS);
    Nr_Text(16,233,text,.4f,310,0);sprintf(text,"L1  %d / %d  R1",page+1,(sNrCount+NR_PAGE-1)/NR_PAGE);Nr_Text(388,233,text,.4f,110,1);
    for(k=0;k<NR_PAGE;k++) {
        f32 x=NR_CX+(k%NR_COLS)*NR_DX,y=NR_CY+(k/NR_COLS)*NR_DY;
        idx=page*NR_PAGE+k;if(idx>=sNrCount) break;id=Port_PcRosterId(sNrList[idx]);
        Nr_Box(x,y,37,34,idx==sNrCursor[sNrSide],sNrSide);
        if(id>=0) Nr_Chip(id,x+2,y+1,33,30,128);
        else {Ps_Rect(x+2,y+2,x+35,y+32,Ps_Rgba(14,19,30,128),Ps_Rgba(6,9,17,128));Nr_Text(x+13,y+9,"+",.55f,20,3);}
        Ps_Rect(x+1,y+23,x+36,y+33,Ps_Rgba(7,10,18,98),Ps_Rgba(7,10,18,118));
        sprintf(text,"%03d",sNrList[idx]+1);Nr_Text(x+3,y+23,text,.3f,28,idx==sNrCursor[sNrSide]?1:0);
        if(id>=0 && sNrFavorite[id]) Nr_Star(x+30,y+6,3);
    }
    if(!sNrCount) Nr_Text(46,295,"Todavía no tienes favoritos. Pulsa R3 para ver todos.",.54f,420,0);
    Nr_Text(16,370,"Cruceta: elegir   X: añadir   Círculo: deshacer   Triángulo: equipo",.38f,480,0);
    Nr_Text(16,384,"Cuadrado: traje   L2/R2: posición   SELECT: tamaño   L3: favorito   R3: filtrar",.34f,480,3);
    Nr_Box(16,403,206,24,0,0);Nr_Text(25,406,"X  CONFIRMAR PERSONAJE",.46f,189,0);
    Nr_Box(280,403,216,24,Nr_Complete(),1);Nr_Text(290,406,sNrKind==3?"START  CONFIRMAR EQUIPO":"START  ELEGIR ESCENARIO",.46f,198,1);
}
static void Nr_DrawStage(void) {
    s32 i;char text[100];
    Nr_Text(16,46,"ELIGE EL CAMPO DE BATALLA",.8f,470,1);
    for(i=0;i<35;i++) {
        f32 x=16+(i%7)*69.0f,y=83+(i/7)*54.0f;void *res=MPACK_AT(gTeamSel->stagePack,i+1);
        Nr_Box(x,y,64,48,sNr.stage==i,0);Nr_Image(res,0,x+2,y+2,60,44,0,128);
        if(sNr.stage==i) {sprintf(text,"%d",i+1);Nr_Text(x+3,y+32,text,.38f,52,1);}
    }
    Nr_Text(16,363,"Cruceta: escenario   X / START: continuar   Círculo: personajes",.4f,480,0);
    Nr_Box(280,403,216,24,1,1);Nr_Text(290,406,sNr.mode==PC_RAID&&sNr.players>1?"X  SALA COOPERATIVA":"X  COMENZAR COMBATE",.46f,198,1);
}
static void Nr_DrawNetwork(void) {
    s32 i;char line[100];static char *choices[]={"COMBATE LOCAL","CREAR SALA ONLINE","UNIRSE A UNA SALA"};
    Nr_Text(26,57,"RAID COOPERATIVO",1.0f,460,1);
    sprintf(line,"%d aliados humanos contra un jefe CPU",sNr.players);Nr_Text(26,94,line,.55f,460,0);
    for(i=0;i<3;i++) {Nr_Box(30,135+i*43,450,34,sNrConnection==i,1);Nr_Text(45,141+i*43,choices[i],.68f,420,sNrConnection==i?1:0);}
    if(sNrConnection) {
        Nr_Text(30,278,"DIRECCIÓN DEL ANFITRIÓN Y PUERTO",.48f,440,0);
        for(i=0;i<5;i++) {f32 x=30+i*91.0f;Nr_Box(x,306,84,38,sNrField==i,0);sprintf(line,"%d",sNrIp[i]);Nr_Text(x+9,314,line,.65f,70,1);}
        Nr_Text(30,354,"L1/R1: campo   Arriba/abajo: valor   Cuadrado: +10",.43f,450,0);
        Nr_Text(30,374,"Mismos datos y paquetes. El puerto UDP debe ser accesible.",.4f,450,3);
    }
    Nr_Box(280,403,216,24,1,1);Nr_Text(290,406,"START  CONFIRMAR",.5f,198,1);
}
static void Nr_Draw(void) {
    char title[100];static char *titles[]={"DUELO","ENTRENAMIENTO","TAG TEAM SIMULTÁNEO","RAID COOPERATIVO","RELEVOS 10 CONTRA 10","TODOS CONTRA TODOS"};
    Ps_PageSet();Nr_Background();
    Ps_Copy(title,sNrKind==3?"BATALLA DEFINITIVA":titles[sNr.mode],100);Nr_Text(16,6,title,.83f,325,1);
    if(sNrPhase==0) {Nr_Portraits();Nr_DrawGrid();}
    else if(sNrPhase==1) Nr_DrawStage();else Nr_DrawNetwork();
    if(sNrMessage[0]) {Ps_Rect(12,430,500,447,Ps_Rgba(63,17,8,128),Ps_Rgba(25,8,7,128));Nr_Text(17,431,sNrMessage,.36f,476,1);}
    else Nr_Text(16,432,"PLANTILLA PC  |  Formas independientes  |  Puedes repetir luchadores",.33f,480,3);
}
/* kind: 0 individual duel, 1 relay teams, 2 training, 3 the mode's player-only team. */
s32 Port_NativeRosterPick(PcBattle *out,s32 kind,s32 dp) {
    TsTeam saved[2];s32 savedPlayers,savedType,savedDp,savedStage,savedBgm;
    s32 s,i,mode=Port_ModesFight(),result=0,remembered,max,done=0;
    PcBattle last;char why[160];
    if(Port_NetActive()||Port_NetSession()) return 0;
    if(!gMenuArc5) gMenuArc5=File_LoadSync(gProgress->baseFile,NULL,0);
    saved[0]=gTsProgress->team[0];saved[1]=gTsProgress->team[1];savedPlayers=gTsProgress->players;
    savedType=gTsProgress->battleType;savedDp=gTsProgress->dpLevel;savedStage=gTsProgress->stageCell;savedBgm=gTsProgress->bgm;
    memset(gTsProgress->team,0,sizeof(gTsProgress->team));gTsProgress->players=0;gTsProgress->battleType=1;gTsProgress->dpLevel=0;gTsProgress->stageCell=gTsProgress->bgm=0;
    for(s=0;s<2;s++) for(i=0;i<5;i++) {gTsProgress->team[s].member[i].chara=i?-1:s?4:0;}
    Port_PcBattleDefault(&sNr);sNr.mode=kind==2?PC_TRAINING:kind==1||kind==3?PC_RELAY:PC_DUEL;
    if(kind==1) {if(mode==PORT_MODE_TEAM) sNr.mode=PC_TAG;else if(mode==PORT_MODE_FFA) sNr.mode=PC_FFA;else if(mode==PORT_MODE_RAID) sNr.mode=PC_RAID;}
    sNr.players=(kind==2||kind==3)?1:mode?Port_LocalPlayers():savedPlayers==1?2:1;
    max=sNr.mode==PC_RELAY?10:(sNr.mode==PC_TAG||sNr.mode==PC_FFA)?5:sNr.mode==PC_RAID?4:1;
    sNr.count[0]=sNr.count[1]=max;if(sNr.mode==PC_RAID) {sNr.count[0]=sNr.players;sNr.count[1]=1;}
    if(kind==3) sNr.count[1]=1;
    remembered=Port_RosterRecall(sNr.mode,&last);
    if(remembered && last.count[0]<=max && last.count[1]<=max && last.players==sNr.players) sNr=last;else remembered=0;
    if(kind==3) sNr.count[1]=1;
    sNrCostume[0]=sNr.fighter[0][0].costume;sNrCostume[1]=sNr.fighter[1][0].costume;
    memset(sNrFilled,0,sizeof(sNrFilled));if(remembered) memset(sNrFilled,1,sizeof(sNrFilled));
    memset(sNrSlot,0,sizeof(sNrSlot));sNrCursor[0]=0;sNrCursor[1]=4;sNrSide=0;sNrKind=kind;sNrDp=dp;sNrPhase=sNrAge=sNrFavoriteOnly=sNrConnection=sNrField=0;sNrMessage[0]=0;
    if(kind==3) sNrFilled[1][0]=1;
    for(i=0;i<PORT_CHARA_ID_LIMIT;i++) sNrFavorite[i]=0;
    for(i=0;i<Port_PcRosterCount();i++) {s32 id=Port_PcRosterId(i);sNrFavorite[id]=Port_RosterFavorite(id,0);}
    TeamSel_Init(2);gPortNativeRosterActive=1;
    {extern volatile s32 gPortPcTeamReady;gPortPcTeamReady=0;}
    Nr_List();Nr_Face();Nr_StageRequest();Port_RosterLog(0,kind,sNr.mode,Port_PcRosterCount(),PORT_TOTAL_CHARAS);
    ColorFade_StartIn(0,0,0,20);
    while(1) {
        u32 p,r; s32 x,y,click;
        Gfx_BeginFrame();Pad_Update();Snd_Update();ColorFade_Update();TeamSel_UpdateFaceLoad();TeamSel_UpdateStageLoad();
        p=gPad[0].gamePressed|Port_RosterTestInput((unsigned)++sNrAge);r=gPad[0].gameRepeat|p;
        if(sNrSide==1 && sNr.players==2) {p|=gPad[1].gamePressed;r|=gPad[1].gameRepeat|p;}
        if(gProgress->flags&MPROG_FREEZE) p=r=0;
        Ps_PageSet();click=Port_RosterPointerRead(&x,&y);
        if(click) {
            if(sNrPhase==0) {
                s32 page=sNrCursor[sNrSide]/NR_PAGE;
                for(i=0;i<NR_PAGE;i++) {f32 xx=NR_CX+(i%NR_COLS)*NR_DX,yy=NR_CY+(i/NR_COLS)*NR_DY;
                    if(x>=Ps_X(xx)&&x<Ps_X(xx+37)&&y>=Ps_Y(yy)&&y<Ps_Y(yy+34)&&page*NR_PAGE+i<sNrCount){if(sNrCursor[sNrSide]!=page*NR_PAGE+i)sNrCostume[sNrSide]=0;sNrCursor[sNrSide]=page*NR_PAGE+i;p|=PADG_CROSS;}}
                for(s=0;s<(kind==3?1:2);s++) for(i=0;i<sNr.count[s];i++) {f32 xx=(s?266:16)+i*23.0f;if(x>=Ps_X(xx)&&x<Ps_X(xx+21)&&y>=Ps_Y(207)&&y<Ps_Y(229)){sNrSide=s;sNrSlot[s]=i;Nr_List();}}
                if(y>=Ps_Y(403)&&y<Ps_Y(427)) {if(x<Ps_X(230)) p|=PADG_CROSS;else p|=PADG_START;}
            } else if(sNrPhase==1) {
                for(i=0;i<35;i++){f32 xx=16+(i%7)*69.0f,yy=83+(i/7)*54.0f;if(x>=Ps_X(xx)&&x<Ps_X(xx+64)&&y>=Ps_Y(yy)&&y<Ps_Y(yy+48)){sNr.stage=i;Nr_StageRequest();p|=PADG_CROSS;}}
            }
        }
        if(sNrPhase==0) {
            s32 old=sNrCursor[sNrSide],cur=old,id;
            if(r&(PADG_LEFT|PADG_LEFT2)) cur--;else if(r&(PADG_RIGHT|PADG_RIGHT2))cur++;else if(r&(PADG_UP|PADG_UP2))cur-=NR_COLS;else if(r&(PADG_DOWN|PADG_DOWN2))cur+=NR_COLS;
            if(sNrCount && (p&(PADG_L1|PADG_R1))) {s32 page=Ps_Wrap(cur/NR_PAGE+((p&PADG_L1)?-1:1),0,(sNrCount-1)/NR_PAGE);cur=page*NR_PAGE+cur%NR_PAGE;if(cur>=sNrCount)cur=sNrCount-1;}
            if(sNrCount){cur=Ps_Wrap(cur,0,sNrCount-1);sNrCursor[sNrSide]=cur;}
            if(cur!=old) {sNrCostume[sNrSide]=0;Snd_PlaySe(1,0);sNrMessage[0]=0;}
            if(p&PADG_TRIANGLE && kind!=3) {sNrSide^=1;Nr_List();Snd_PlaySe(1,0);}
            if(p&PADG_L2)sNrSlot[sNrSide]=Ps_Wrap(sNrSlot[sNrSide]-1,0,sNr.count[sNrSide]-1);
            if(p&PADG_R2)sNrSlot[sNrSide]=Ps_Wrap(sNrSlot[sNrSide]+1,0,sNr.count[sNrSide]-1);
            if(p&(PADG_L2|PADG_R2)) {sNrCostume[sNrSide]=sNr.fighter[sNrSide][sNrSlot[sNrSide]].chara==Nr_Id()?sNr.fighter[sNrSide][sNrSlot[sNrSide]].costume:0;gTeamSel->side[sNrSide]->chara=-1;}
            id=Nr_Id();
            if((p&PADG_SQUARE)&&id>=0) {s32 c=sNrCostume[sNrSide],n;for(n=0;n<8;n++){c=(c+1)%8;if(Port_ExtraCostumeOk(id,c,why,sizeof(why)))break;}sNrCostume[sNrSide]=c;gTeamSel->side[sNrSide]->chara=-1;Snd_PlaySe(1,0);}
            if((p&PADG_L3)&&id>=0) {sNrFavorite[id]=Port_RosterFavorite(id,1);Snd_PlaySe(1,1);}
            if(p&PADG_R3) {sNrFavoriteOnly^=1;sNrCursor[sNrSide]=0;Nr_List();}
            if((p&PADG_SELECT)&&max>1&&sNr.mode!=PC_RAID) {static s32 counts[]={1,3,5,10};s32 n=0;while(n<4&&counts[n]<=sNr.count[sNrSide])n++;sNr.count[sNrSide]=n<4&&counts[n]<=max?counts[n]:1;if(sNrSlot[sNrSide]>=sNr.count[sNrSide])sNrSlot[sNrSide]=0;Snd_PlaySe(1,0);}
            if(p&PADG_CROSS)Nr_Selected();
            if(p&PADG_CIRCLE) {s32 slot=sNrSlot[sNrSide];if(sNrFilled[sNrSide][slot])sNrFilled[sNrSide][slot]=0;else if(slot>0){sNrSlot[sNrSide]--;sNrFilled[sNrSide][slot-1]=0;}else if(sNrSide){sNrSide=0;Nr_List();}else {result=0;done=1;}Snd_PlaySe(1,2);}
            if(p&PADG_START) {if(Nr_Complete()){if(kind==3){result=1;done=1;}sNrPhase=1;Nr_StageRequest();sNrMessage[0]=0;Snd_PlaySe(1,1);}else{Ps_Copy(sNrMessage,"Completa las posiciones de ambos equipos antes de continuar.",160);Snd_PlaySe(1,7);}}
            Nr_Face();
        } else if(sNrPhase==1) {
            s32 old=sNr.stage;
            if(r&(PADG_LEFT|PADG_LEFT2))sNr.stage--;else if(r&(PADG_RIGHT|PADG_RIGHT2))sNr.stage++;else if(r&(PADG_UP|PADG_UP2))sNr.stage-=7;else if(r&(PADG_DOWN|PADG_DOWN2))sNr.stage+=7;
            sNr.stage=Ps_Wrap(sNr.stage,0,34);if(old!=sNr.stage){Nr_StageRequest();Snd_PlaySe(1,0);}
            if(p&PADG_CIRCLE){sNrPhase=0;Snd_PlaySe(1,2);}
            if(p&(PADG_CROSS|PADG_START)){if(sNr.mode==PC_RAID&&sNr.players>1)sNrPhase=2;else {result=1;done=1;}}
        } else {
            if(sNrConnection==0) {if(r&PADG_UP)sNrConnection=2;else if(r&PADG_DOWN)sNrConnection=1;}
            else {if(p&PADG_TRIANGLE)sNrConnection=sNrConnection==1?2:1;
                if(p&PADG_L1)sNrField=Ps_Wrap(sNrField-1,0,4);if(p&PADG_R1)sNrField=Ps_Wrap(sNrField+1,0,4);
                if(r&PADG_UP)sNrIp[sNrField]++;if(r&PADG_DOWN)sNrIp[sNrField]--;if(p&PADG_SQUARE)sNrIp[sNrField]+=10;
                sNrIp[sNrField]=Ps_Wrap(sNrIp[sNrField],sNrField==4?1:0,sNrField==4?65535:255);
            }
            if(p&PADG_CIRCLE){sNrPhase=1;sNrConnection=0;}
            if(p&PADG_SELECT)sNrConnection=(sNrConnection+1)%3;
            if(p&(PADG_START|PADG_CROSS)){if(!sNrConnection){result=1;done=1;}else {char addr[80];sprintf(addr,"%d.%d.%d.%d",sNrIp[0],sNrIp[1],sNrIp[2],sNrIp[3]);if(!Port_CoopLaunch(sNrConnection==1,addr,sNrIp[4],&sNr))Ps_Copy(sNrMessage,"No se pudo abrir la sala. Revisa los personajes y el puerto.",160);}}
        }
        Nr_Draw();Font_FlushAll();ColorFade_Draw();Gfx_EndFrame(1);Dma_Flush();File_Stub264D90();
        if(done)break;
    }
    /* Finish outstanding native file work before freeing its resource buffers. */
    while(gTeamSel->faceState!=TEAMSEL_LOAD_IDLE || gTeamSel->stageState!=TEAMSEL_LOAD_IDLE) {
        Gfx_BeginFrame();TeamSel_UpdateFaceLoad();TeamSel_UpdateStageLoad();Nr_Draw();Font_FlushAll();Gfx_EndFrame(1);Dma_Flush();File_Stub264D90();
    }
    if(result) {*out=sNr;Port_RosterRemember(&sNr);Port_RosterLog(2,kind,sNr.count[0],sNr.count[1],sNr.stage);}
    gPortNativeRosterActive=0;TeamSel_Term();Dma_ResetBuffers();
    gTsProgress->team[0]=saved[0];gTsProgress->team[1]=saved[1];gTsProgress->players=savedPlayers;gTsProgress->battleType=savedType;gTsProgress->dpLevel=savedDp;gTsProgress->stageCell=savedStage;gTsProgress->bgm=savedBgm;
    return result;
}
s32 Port_NativeRosterBattle(s32 kind) {
    PcBattle chosen;s32 mode=gProgress->mode,dp=0;
    if(kind==1&&gTsProgress->battleType==2) dp=(gTsProgress->dpLevel+2)*5;
    if(!Port_NativeRosterPick(&chosen,kind,dp))return 0;
    if(!Port_PcRosterQueue(&chosen))return 0;
    Port_PcNativeRules(gTsProgress->players,kind==2 && Port_ModesFight()==PORT_MODE_TRAINING);
    if(!Port_PcBattleBuild())return 0;
    gProgress->mode=mode;return 1;
}
#endif

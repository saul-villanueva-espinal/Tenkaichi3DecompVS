#include "common.h"
#ifdef PORT
#include "menu/menu_d.h"
#include "menu/menu_e.h"
#include "menu/menu_f.h"
#include "menu/port_screen.h"
#include "sys/pad.h"
#include "sys/save.h"
#include "sys/load_sprite.h"
#include "port_pc_battle.h"
#include "port_coop.h"
#include "port_fighters.h"
#include "port_chara.h"
#include "plat_songs.h"

/* Native classic selection: two large portraits and independent seven-card
   carousels, with 290 browsable character/form positions and ten relay slots. */
extern s32 Port_ModesFight(void), Port_LocalPlayers(void), Port_NetActive(void), Port_NetSession(void);
extern s32 Port_ExtraCostumeOk(s32,s32,char *,s32), ChrTbl_GetCost(s32);
extern void Port_LocalPlayersSet(s32);
extern void Port_SettingSave(const char *,s32);
extern void Port_RosterLog(s32,s32,s32,s32,s32);
extern void Port_RosterName(char *,s32,s32);
extern void Port_PcNativeRules(s32,s32,s32);
extern void TeamSel_Init(s32), TeamSel_Term(void), TeamSel_UpdateFaceLoad(void), TeamSel_UpdateStageLoad(void);
extern void TeamSel_RequestFace(s32), TeamSel_RequestStage(void);
extern void Sprite_DrawList(void *,s32,s32,LoadSprite *);
extern f32 sinf(f32),cosf(f32);
extern void Ps_PageSet(void), Font_FlushAll(void);

#define NR_COLS 7
#define NR_ROWS 1
#define NR_PAGE 7
#define NR_CX 70
#define NR_CY 27
#define NR_CARD_W 56
#define NR_CARD_H 44
#define NR_DX 61
#define NR_DY 46

enum {
    NR_SAGA_ALL = 0,
    NR_SAGA_Z,
    NR_SAGA_SUPER,
    NR_SAGA_GT,
    NR_SAGA_MOVIES,
    NR_SAGA_CLASSIC,
    NR_SAGA_FAV,
    NR_SAGA_COUNT
};

static const char *sSagaTitles[NR_SAGA_COUNT] = {
    "TODOS LOS PERSONAJES",
    "DRAGON BALL Z",
    "DRAGON BALL SUPER",
    "DRAGON BALL GT",
    "PEL\303\215CULAS Y ESPECIALES",
    "DRAGON BALL CL\303\201SICO",
    "PERSONAJES FAVORITOS"
};

static const char *sSagaTabs[NR_SAGA_COUNT] = {
    "TODOS", "DRAGON BALL Z", "SUPER", "GT", "PEL\303\215CULAS", "CL\303\201SICO", "\342\230\205 FAVORITOS"
};

static PcBattle sNr;
static s32 sNrKind,sNrSide,sNrSlot[2],sNrFilled[2][10],sNrCursor[2],sNrPhase,sNrSaga;
static s32 sNrFavorite[PORT_CHARA_ID_LIMIT],sNrList[PORT_TOTAL_CHARAS],sNrCount,sNrAge,sNrDp;
static s32 sNrConnection,sNrField,sNrIp[5]={127,0,0,1,29740};
static char sNrMessage[160];
static s32 sNrCostume[2];
static s32 sNrBgm;

static void Nr_Text(f32 x,f32 y,char *text,f32 size,f32 width,s32 style) {
    Ps_Align(0);Ps_Text(x,y,text,size,width,style);
}
static void Nr_Box(f32 x,f32 y,f32 w,f32 h,s32 selected,s32 side) {
    u32 gold=Ps_Rgba(255,204,50,240),edge=Ps_Rgba(60,85,125,180);
    Ps_Rect(x-1,y-1,x+w+1,y+h+1,selected?gold:edge,selected?gold:edge);
    Ps_Rect(x,y,x+w,y+h,selected?Ps_Rgba(110,65,12,220):Ps_Rgba(side?28:14,side?24:30,side?54:48,220),Ps_Rgba(6,10,20,240));
}
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
    MTexRes *res;
    if(chara<0)return;
    if(PORT_CHARA_ADDED(chara)) {
        res=(MTexRes *)PortChips_Get(chara);
        if(res) {Nr_Image(res,0,x,y,w,h,0,alpha);return;}
        chara=Port_CharaBase(chara);
    }
    res=(MTexRes *)MPACK_AT(gTeamSel->chipPack,chara+1);
    Nr_Image(res,0,x,y,w,h,0,alpha);
}
static s32 Nr_GetSaga(s32 chara) {
    if(chara<0) return NR_SAGA_ALL;
    if(PORT_CHARA_ADDED(chara) || chara>=161) return NR_SAGA_SUPER;
    if(chara==11 || chara==12 || (chara>=66 && chara<=70) || (chara>=148 && chara<=157)) return NR_SAGA_CLASSIC;
    if((chara>=7 && chara<=10) || chara==38 || chara==54 || chara==63 || chara==64 || chara==65 || (chara>=140 && chara<=147)) return NR_SAGA_GT;
    if(chara==53 || chara==61 || chara==62 || (chara>=71 && chara<=76) || (chara>=118 && chara<=139) || chara==159 || chara==160) return NR_SAGA_MOVIES;
    return NR_SAGA_Z;
}
static void Nr_List(void) {
    s32 i,id,saga;sNrCount=0;
    for(i=0;i<PORT_TOTAL_CHARAS;i++) {
        id=Port_PcRosterId(i);
        if(id<0) {
            if(sNrSaga==NR_SAGA_ALL) sNrList[sNrCount++]=i;
            continue;
        }
        if(sNrSaga==NR_SAGA_FAV) {
            if(sNrFavorite[id]) sNrList[sNrCount++]=i;
            continue;
        }
        saga=Nr_GetSaga(id);
        if(sNrSaga==NR_SAGA_ALL || sNrSaga==saga) sNrList[sNrCount++]=i;
    }
    if(sNrCursor[0]>=sNrCount) sNrCursor[0]=sNrCount?sNrCount-1:0;
    if(sNrCursor[1]>=sNrCount) sNrCursor[1]=sNrCount?sNrCount-1:0;
}
static s32 Nr_Id(s32 side) {
    if(!sNrCount || side<0 || side>1) return -1;
    if(sNrCursor[side]>=sNrCount) sNrCursor[side]=sNrCount-1;
    return Port_PcRosterId(sNrList[sNrCursor[side]]);
}
static void Nr_PageShift(s32 side,s32 direction) {
    s32 cur,page;
    if(side<0 || side>1 || sNrCount<=NR_PAGE)return;
    cur=sNrCursor[side];
    page=Ps_Wrap(cur/NR_PAGE+direction,0,(sNrCount-1)/NR_PAGE);
    cur=page*NR_PAGE+cur%NR_PAGE;
    if(cur>=sNrCount)cur=sNrCount-1;
    sNrCursor[side]=cur;sNrCostume[side]=0;sNrMessage[0]=0;
}
static void Nr_Face(void) {
    s32 s;
    for(s=0;s<2;s++) {
        s32 curId=Nr_Id(s);
        s32 target=curId;
        s32 color=sNrCostume[s];
        s32 changed;
        if(sNrKind==3 && s==1) target=-1;
        changed=target!=gTeamSel->side[s]->chara || color!=gTeamSel->side[s]->member[0].color;
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
static void Nr_Selected(s32 side) {
    s32 slot=sNrSlot[side],id=Nr_Id(side),next;char why[160];
    if(id<0) {Ps_Copy(sNrMessage,"Casilla disponible: falta instalar el personaje y sus archivos.",160);Snd_PlaySe(1,7);return;}
    if(sNrDp>0 && Nr_Cost(side)-(sNrFilled[side][slot]?ChrTbl_GetCost(sNr.fighter[side][slot].chara):0)+ChrTbl_GetCost(id)>sNrDp) {
        Ps_Copy(sNrMessage,"El personaje supera los puntos disponibles del equipo.",160);Snd_PlaySe(1,7);return;
    }
    if(!Port_ExtraCostumeOk(id,sNrCostume[side],why,sizeof(why))) {Ps_Copy(sNrMessage,"Ese traje no tiene todos sus archivos. Elige otro traje.",160);return;}
    sNr.fighter[side][slot].chara=id;sNr.fighter[side][slot].costume=sNrCostume[side];sNrFilled[side][slot]=1;sNrMessage[0]=0;
    Port_RosterLog(1,side,slot,id,sNr.fighter[side][slot].costume);Snd_PlaySe(1,1);
    next=slot+1;
    if(next<sNr.count[side]) sNrSlot[side]=next;
    else if(side==0 && sNrKind!=3 && sNr.players==1) sNrSide=1;
}
static void Nr_StageRequest(void) {
    gTeamSel->stage->stage=sNr.stage;TeamSel_RequestStage();
}
static void Nr_Star(f32 x,f32 y,f32 r) {
    s32 k;for(k=0;k<5;k++) {f32 a=-1.5707963f+k*1.2566371f,b=a+0.6283185f,c=a+1.2566371f;u32 red=Ps_Rgba(255,160,20,128);
        Ps_Quad(x,y,x+cosf(a)*r,y+sinf(a)*r,x+cosf(b)*r*.4f,y+sinf(b)*r*.4f,x+cosf(c)*r,y+sinf(c)*r,red,red,red,red);
    }
}
static s32 Nr_GetForms(s32 chara,s32 *forms,s32 max) {
    s32 count=0,i,k,j;
    if(chara<0 || max<1) return 0;
    if(PORT_CHARA_ADDED(chara)) {
        s32 n=Port_CharaForms(chara,forms,max);
        if(n>0) return n;
        forms[0]=chara;return 1;
    }
    if(gTeamSel && gTeamSel->cells[0]) {
        TsCell *cells=gTeamSel->cells[0];
        s32 masterCount=gTeamSel->masterCount[0];
        for(i=0;i<masterCount;i++) {
            if(cells[i].id==chara) {
                count=cells[i].formCount;if(count>max)count=max;
                for(k=0;k<count;k++) forms[k]=cells[i].form[k];
                return count;
            }
            for(k=0;k<cells[i].formCount;k++) {
                if(cells[i].form[k]==chara) {
                    count=cells[i].formCount;if(count>max)count=max;
                    for(j=0;j<count;j++) forms[j]=cells[i].form[j];
                    return count;
                }
            }
        }
    }
    forms[0]=chara;return 1;
}
static void Nr_SelectForm(s32 side,s32 targetId) {
    s32 i;
    if(targetId<0 || side<0 || side>1) return;
    for(i=0;i<sNrCount;i++) {
        if(Port_PcRosterId(sNrList[i])==targetId) {
            sNrCursor[side]=i;sNrCostume[side]=0;gTeamSel->side[side]->chara=-1;
            Nr_Face();Snd_PlaySe(1,0);return;
        }
    }
    sNrSaga=NR_SAGA_ALL;Nr_List();
    for(i=0;i<sNrCount;i++) {
        if(Port_PcRosterId(sNrList[i])==targetId) {
            sNrCursor[side]=i;sNrCostume[side]=0;gTeamSel->side[side]->chara=-1;
            Nr_Face();Snd_PlaySe(1,0);return;
        }
    }
}
static void Nr_Background(void) {
    if(gTeamSel->bg[0] && (gTeamSel->flags&TEAMSEL_STAGE_READY)) Sprite_DrawPicture(gTeamSel->bg[0],0,0,128);
    else if(gTeamSel->bg[1]) Sprite_DrawPicture(gTeamSel->bg[1],0,0,128);
    else Ps_Rect(0,0,512,448,Ps_Rgba(70,168,219,128),Ps_Rgba(45,95,48,128));
    Ps_Rect(0,0,512,23,Ps_Rgba(8,12,18,100),Ps_Rgba(8,12,18,100));
    Ps_Rect(252,23,260,403,Ps_Rgba(118,64,4,128),Ps_Rgba(118,64,4,128));
    Ps_Rect(254,23,258,403,Ps_Rgba(255,245,86,128),Ps_Rgba(255,196,10,128));
}

static void Nr_YellowBand(f32 x,f32 y,f32 w,f32 h) {
    u32 black=Ps_Rgba(6,7,3,128),gold=Ps_Rgba(236,175,0,128);
    Ps_Rect(x,y,x+w,y+h,black,black);
    Ps_Rect(x+2,y+2,x+w-2,y+h-2,gold,gold);
    Ps_Rect(x+4,y+4,x+w-4,y+h-4,Ps_Rgba(255,255,81,128),Ps_Rgba(160,160,0,128));
    Ps_Quad(x+3,y+3,x+27,y+3,x+3,y+h-3,x+14,y+h-3,Ps_Rgba(255,67,14,128),Ps_Rgba(255,178,0,128),Ps_Rgba(255,67,14,128),Ps_Rgba(255,178,0,128));
}

static void Nr_DrawFighterPanels(void) {
    s32 side,i,id;char line[160];
    for(side=0;side<2;side++) {
        f32 x=side?261.0f:0.0f,w=251.0f;
        id=Nr_Id(side);
        if(sNrKind==3 && side==1) {
            Nr_Text(x+18,155,"RIVAL DEL DESAFIO",.70f,w-34,1);
            Nr_Text(x+18,185,"Equipo y reglas del modo",.43f,w-34,0);
        } else if(id>=0 && (gTeamSel->side[side]->flags&(TEAMSEL_SIDE_FACE_READY|TEAMSEL_SIDE_NO_FACE))==TEAMSEL_SIDE_FACE_READY) {
            Nr_Image(gTeamSel->faceRes[side],0,x+1,76,w-2,210,side,128);
        } else {
            Nr_Text(x+108,142,"+",1.8f,40,1);
            Nr_Text(x+24,202,"CASILLA DISPONIBLE",.57f,w-44,1);
            Nr_Text(x+24,225,"Instala aqui un nuevo personaje",.39f,w-44,0);
        }
        /* The original two portraits remain the focus. Small team tiles keep
           all ten relay positions visible without replacing the carousel. */
        if(sNr.count[side]>1 && !(sNrKind==3&&side==1)) for(i=0;i<sNr.count[side];i++) {
            f32 sx=x+9+i*23.0f;
            Nr_Box(sx,263,20,19,i==sNrSlot[side],side);
            if(sNrFilled[side][i]) Nr_Chip(sNr.fighter[side][i].chara,sx+1,264,18,17,128);
            else {sprintf(line,"%d",i+1);Nr_Text(sx+4,265,line,.31f,15,1);}
        }
        Nr_YellowBand(x,290,w,25);
        if(sNrKind==3&&side==1) Ps_Copy(line,"Rival del modo",160);
        else if(id>=0) Port_RosterName(line,160,id);
        else sprintf(line,"Casilla %03d",sNrCount?sNrList[sNrCursor[side]]+1:1);
        Nr_Text(x+31,293,line,.62f,w-39,0);
        Nr_YellowBand(x,320,w,23);
        if(sNrKind==3&&side==1) Ps_Copy(line,"Reglas conservadas",160);
        else if(id<0) Ps_Copy(line,"Disponible para ampliar el roster",160);
        else if(sNrDp>0) sprintf(line,"Traje %d   PD %d / %d",sNrCostume[side]+1,Nr_Cost(side),sNrDp);
        else sprintf(line,"Traje %d   Posicion %d / %d",sNrCostume[side]+1,sNrSlot[side]+1,sNr.count[side]);
        Nr_Text(x+31,323,line,.41f,w-39,0);
    }
    Ps_Rect(224,180,288,215,Ps_Rgba(8,7,2,110),Ps_Rgba(8,7,2,110));
    Nr_Text(230,182,"DRAGON BALL",.42f,54,1);
    Nr_Text(246,198,"VS",.58f,22,1);
}

static void Nr_DrawGrid(void) {
    s32 side,k,idx,id,page;char line[100];
    for(side=0;side<2;side++) {
        f32 y=side?355.0f:27.0f;
        s32 active=sNr.players==2 || sNrSide==side;
        Nr_YellowBand(1,y,63,NR_CARD_H);
        Nr_Text(12,y+9,sNr.mode==PC_RAID?(side?"JEFE":"1P"):(side?(sNr.players==2?"2P":"COM"):"1P"),.84f,48,1);
        page=sNrCursor[side]/NR_PAGE;
        if(sNrKind==3&&side==1) continue;
        Ps_Rect(65,y+18,510,y+27,Ps_Rgba(9,8,2,128),Ps_Rgba(9,8,2,128));
        for(k=0;k<NR_PAGE;k++) {
            f32 x=NR_CX+k*NR_DX;
            idx=page*NR_PAGE+k;
            if(idx>=sNrCount)break;
            id=Port_PcRosterId(sNrList[idx]);
            if(idx==sNrCursor[side]) {
                Ps_Rect(x-3,y-3,x+NR_CARD_W+3,y+NR_CARD_H+3,active?Ps_Rgba(255,255,90,128):Ps_Rgba(255,181,20,128),Ps_Rgba(255,203,30,128));
            }
            Ps_Rect(x,y,x+NR_CARD_W,y+NR_CARD_H,Ps_Rgba(4,5,1,128),Ps_Rgba(4,5,1,128));
            Ps_Rect(x+2,y+2,x+NR_CARD_W-2,y+NR_CARD_H-2,Ps_Rgba(255,218,15,128),Ps_Rgba(169,157,0,128));
            if(id>=0) Nr_Chip(id,x+3,y+3,NR_CARD_W-6,NR_CARD_H-6,128);
            else {sprintf(line,"%03d",sNrList[idx]+1);Nr_Text(x+9,y+5,line,.45f,42,0);Nr_Text(x+23,y+21,"+",.60f,18,0);}
            if(id>=0&&sNrFavorite[id])Nr_Star(x+NR_CARD_W-7,y+7,3);
        }
    }
    sprintf(line,"%03d / %03d   %s",sNrCount?sNrList[sNrCursor[sNrSide]]+1:0,PORT_TOTAL_CHARAS,sSagaTabs[sNrSaga]);
    Nr_Text(10,403,line,.34f,266,1);
    Nr_Box(284,402,218,21,Nr_Complete(),1);
    Nr_Text(293,404,sNrKind==3?"START: CONFIRMAR EQUIPO":"START: ELEGIR ESCENARIO",.37f,202,1);
}

static void Nr_DrawStage(void) {
    s32 i;char text[100];
    Nr_Text(16,46,"ELIGE EL CAMPO DE BATALLA",.8f,470,1);
    for(i=0;i<35;i++) {
        f32 x=16+(i%7)*69.0f,y=83+(i/7)*54.0f;void *res=MPACK_AT(gTeamSel->stagePack,i+1);
        Nr_Box(x,y,64,48,sNr.stage==i,0);Nr_Image(res,0,x+2,y+2,60,44,0,128);
        if(sNr.stage==i) {sprintf(text,"%d",i+1);Nr_Text(x+3,y+32,text,.38f,52,1);}
    }
    Nr_Text(16,363,"Cruceta: escenario   X / START: continuar   C\303\255rculo: personajes",.4f,480,0);
    sprintf(text,"L1 / R1: musica  %d / %d",gTeamSel->stage->bgm+1,gTeamSel->bgmCount);Nr_Text(16,382,text,.42f,480,1);
    Nr_Box(280,403,216,24,1,1);Nr_Text(290,406,sNr.mode==PC_RAID&&sNr.players>1?"X  SALA COOPERATIVA":"X  COMENZAR COMBATE",.46f,198,1);
}

static void Nr_DrawNetwork(void) {
    s32 i;char line[100];static char *choices[]={"COMBATE LOCAL","CREAR SALA ONLINE","UNIRSE A UNA SALA"};
    Nr_Text(26,57,"RAID COOPERATIVO",1.0f,460,1);
    sprintf(line,"%d aliados humanos contra un jefe CPU",sNr.players);Nr_Text(26,94,line,.55f,460,0);
    for(i=0;i<3;i++) {Nr_Box(30,135+i*43,450,34,sNrConnection==i,1);Nr_Text(45,141+i*43,choices[i],.68f,420,sNrConnection==i?1:0);}
    if(sNrConnection) {
        Nr_Text(30,278,"DIRECCI\303\223N DEL ANFITRI\303\223N Y PUERTO",.48f,440,0);
        for(i=0;i<5;i++) {f32 x=30+i*91.0f;Nr_Box(x,306,84,38,sNrField==i,0);sprintf(line,"%d",sNrIp[i]);Nr_Text(x+9,314,line,.65f,70,1);}
        Nr_Text(30,354,"L1/R1: campo   Arriba/abajo: valor   Cuadrado: +10",.43f,450,0);
        Nr_Text(30,374,"Mismos datos y paquetes. El puerto UDP debe ser accesible.",.4f,450,3);
    }
    Nr_Text(30,386,"SELECT: conexion   Triangulo: crear / unirse",.34f,450,3);
    Nr_Box(280,403,216,24,1,1);Nr_Text(290,406,"START  CONFIRMAR",.5f,198,1);
}

static void Nr_Draw(void) {
    char title[120];static char *titles[]={"DUELO","ENTRENAMIENTO","TAG TEAM SIMULT\303\201NEO","RAID COOPERATIVO","RELEVOS 10 CONTRA 10","TODOS CONTRA TODOS"};
    Ps_PageSet();Nr_Background();
    sprintf(title,"%s  |  SELECCI\303\223N DE PERSONAJES",sNrKind==3?"BATALLA DEFINITIVA":titles[sNr.mode]);
    Nr_Text(10,3,title,0.43f,490,1);
    if(sNrPhase==0) {Nr_DrawFighterPanels();Nr_DrawGrid();}
    else if(sNrPhase==1) Nr_DrawStage();else Nr_DrawNetwork();
    if(sNrMessage[0]) {Ps_Rect(12,430,500,447,Ps_Rgba(63,17,8,128),Ps_Rgba(25,8,7,128));Nr_Text(17,431,sNrMessage,.36f,476,1);}
    else Nr_Text(10,430,"Cruceta: personaje  X: elegir  O: volver  L1/R1: paginas  L2/R2: posicion",.33f,480,3);
}

/* kind: 0 individual duel, 1 relay teams, 2 training, 3 the mode's player-only team. */
s32 Port_NativeRosterPick(PcBattle *out,s32 kind,s32 dp) {
    extern int Port_Setting(const char *, int);
    extern int Port_RosterTestMode(void);
    if (!Port_Setting("native_roster", 0) && !Port_RosterTestMode()) {
        s32 ok = 0;
        if (!gMenuArc5) gMenuArc5 = File_LoadSync(gProgress->baseFile, NULL, 0);
        if (out != NULL && out->count[0] == 1) {
            extern s32 CharSel_Run(s32);
            ok = CharSel_Run(2);
            if (ok) {
                out->fighter[0][0].chara = CSPROG->side[0].pick.chara;
                out->fighter[0][0].costume = CSPROG->side[0].pick.colorPlate;
                out->fighter[0][0].items = CSPROG->side[0].pick.items;
                out->stage = CSPROG->stage;
                if (CSPROG->bgm > 0) out->rules.bgm1 = CSPROG->bgm;
            }
        } else {
            extern s32 TeamSel_Run(s32);
            if (out != NULL && out->count[1] > 0) {
                s32 k;
                for (k = 0; k < 5 && k < out->count[1]; k++) {
                    gTsProgress->team[1].member[k].chara = out->fighter[1][k].chara;
                    gTsProgress->team[1].member[k].color = out->fighter[1][k].costume;
                }
            }
            ok = TeamSel_Run(2);
            if (ok && out != NULL) {
                s32 k, c = 0;
                for (k = 0; k < 10; k++) {
                    if (k < 5 && gTsProgress->team[0].member[k].chara >= 0) {
                        out->fighter[0][k].chara = gTsProgress->team[0].member[k].chara;
                        out->fighter[0][k].costume = gTsProgress->team[0].member[k].color;
                        out->fighter[0][k].items = gTsProgress->team[0].member[k].items;
                        c++;
                    }
                }
                if (c > 0) out->count[0] = c;
                out->stage = gTsProgress->stageCell;
                if (gTsProgress->bgm > 0) out->rules.bgm1 = gTsProgress->bgm;
            }
        }
        return ok;
    }
    TsTeam saved[2];s32 savedPlayers,savedType,savedDp,savedStage,savedBgm;
    s32 s,i,mode=Port_ModesFight(),result=0,remembered,max,done=0;
    PcBattle last;
    if(Port_NetActive()||Port_NetSession()) return 0;
    if(!gMenuArc5) gMenuArc5=File_LoadSync(gProgress->baseFile,NULL,0);
    saved[0]=gTsProgress->team[0];saved[1]=gTsProgress->team[1];savedPlayers=gTsProgress->players;
    savedType=gTsProgress->battleType;savedDp=gTsProgress->dpLevel;savedStage=gTsProgress->stageCell;savedBgm=gTsProgress->bgm;
    memset(gTsProgress->team,0,sizeof(gTsProgress->team));gTsProgress->players=0;gTsProgress->battleType=1;gTsProgress->dpLevel=0;gTsProgress->stageCell=gTsProgress->bgm=0;
    for(s=0;s<2;s++) for(i=0;i<5;i++) {gTsProgress->team[s].member[i].chara=i?-1:s?4:0;}
    Port_PcBattleDefault(&sNr);sNr.mode=kind==2?PC_TRAINING:kind==1||kind==3?PC_RELAY:PC_DUEL;
    if(kind==1) {if(mode==PORT_MODE_TEAM) sNr.mode=PC_TAG;else if(mode==PORT_MODE_FFA) sNr.mode=PC_FFA;else if(mode==PORT_MODE_RAID) sNr.mode=PC_RAID;}
    sNr.players=(kind==2||kind==3)?1:mode?Port_LocalPlayers():savedPlayers==1?2:1;
    if(kind==3 && out!=NULL && out->count[1]>0) {
        sNr.mode=out->mode;sNr.players=out->players;
    }
    max=sNr.mode==PC_RELAY?10:(sNr.mode==PC_TAG||sNr.mode==PC_FFA)?5:sNr.mode==PC_RAID?4:1;
    sNr.count[0]=sNr.count[1]=max;if(sNr.mode==PC_RAID) {sNr.count[0]=sNr.players;sNr.count[1]=1;}
    remembered=Port_RosterRecall(sNr.mode,&last);
    if(remembered && last.count[0]<=max && last.count[1]<=max && last.players==sNr.players) sNr=last;else remembered=0;
    memset(&sNr.rules,0,sizeof(sNr.rules));
    memset(sNrFilled,0,sizeof(sNrFilled));if(remembered) memset(sNrFilled,1,sizeof(sNrFilled));
    if(kind==3 && out!=NULL && out->count[1]>0) {
        s32 k;
        sNr.rules=out->rules;
        sNr.count[1] = out->count[1] > 10 ? 10 : out->count[1];
        for(k=0; k<sNr.count[1]; k++) {
            sNr.fighter[1][k] = out->fighter[1][k];
            sNrFilled[1][k] = 1;
        }
        if(out->count[0]>0) sNr.count[0] = out->count[0] > 10 ? 10 : out->count[0];
        if(out->stage>=0 && out->stage<35) sNr.stage = out->stage;
    } else if(kind==3) {
        sNr.count[1]=1;
        sNrFilled[1][0]=1;
    }
    sNrCostume[0]=sNr.fighter[0][0].costume;sNrCostume[1]=sNr.fighter[1][0].costume;
    memset(sNrSlot,0,sizeof(sNrSlot));sNrCursor[0]=0;sNrCursor[1]=0;sNrSide=0;sNrKind=kind;sNrDp=dp;sNrPhase=sNrAge=sNrConnection=sNrField=sNrSaga=0;sNrMessage[0]=0;
    for(i=0;i<PORT_CHARA_ID_LIMIT;i++) sNrFavorite[i]=0;
    for(i=0;i<Port_PcRosterCount();i++) {s32 id=Port_PcRosterId(i);sNrFavorite[id]=Port_RosterFavorite(id,0);}
    TeamSel_Init(2);gPortNativeRosterActive=1;
    {extern volatile s32 gPortPcTeamReady;gPortPcTeamReady=0;}
    Nr_List();sNrCursor[1]=sNrCount>4?4:0;Nr_Face();Nr_StageRequest();Port_RosterLog(0,kind,sNr.mode,Port_PcRosterCount(),PORT_TOTAL_CHARAS);
    ColorFade_StartIn(0,0,0,20);
    while(1) {
        u32 p0,r0,p1=0,r1=0; s32 x,y,click;
        Gfx_BeginFrame();Pad_Update();Snd_Update();ColorFade_Update();TeamSel_UpdateFaceLoad();TeamSel_UpdateStageLoad();
        p0=gPad[0].gamePressed|Port_RosterTestInput((unsigned)++sNrAge);r0=gPad[0].gameRepeat|p0;
        if(sNr.players==2 && kind!=3) {
            p1=gPad[1].gamePressed;r1=gPad[1].gameRepeat|p1;
        }
        if(gProgress->flags&MPROG_FREEZE) p0=r0=p1=r1=0;
        Ps_PageSet();click=Port_RosterPointerRead(&x,&y);

        /* Mouse / Pointer Interaction */
        if(click) {
            if(sNrPhase==0) {
                for(s=0;s<2;s++) {
                    f32 yy=s?355.0f:27.0f;
                    s32 page=sNrCursor[s]/NR_PAGE;
                    if(kind==3&&s==1)continue;
                    if(y>=Ps_Y(yy)&&y<Ps_Y(yy+NR_CARD_H)) {
                        sNrSide=s;
                        for(i=0;i<NR_PAGE;i++) {
                            f32 xx=NR_CX+i*NR_DX;
                            if(x>=Ps_X(xx)&&x<Ps_X(xx+NR_CARD_W)&&page*NR_PAGE+i<sNrCount) {
                                if(sNrCursor[s]==page*NR_PAGE+i)Nr_Selected(s);
                                else {sNrCostume[s]=0;sNrCursor[s]=page*NR_PAGE+i;Snd_PlaySe(1,0);}
                            }
                        }
                    }
                    for(i=0;i<sNr.count[s];i++) {
                        f32 xx=(s?261.0f:0.0f)+9+i*23.0f;
                        if(y>=Ps_Y(263)&&y<Ps_Y(282)&&x>=Ps_X(xx)&&x<Ps_X(xx+20)){sNrSide=s;sNrSlot[s]=i;}
                    }
                }
                if(y>=Ps_Y(402)&&y<Ps_Y(423)&&x>=Ps_X(284))p0|=PADG_START;

            } else if(sNrPhase==1) {
                for(i=0;i<35;i++){f32 xx=16+(i%7)*69.0f,yy=83+(i/7)*54.0f;if(x>=Ps_X(xx)&&x<Ps_X(xx+64)&&y>=Ps_Y(yy)&&y<Ps_Y(yy+48)){sNr.stage=i;Nr_StageRequest();p0|=PADG_CROSS;}}
                if(y>=Ps_Y(403)&&y<Ps_Y(427)&&x>=Ps_X(280))p0|=PADG_CROSS;
            } else {
                for(i=0;i<3;i++)if(x>=Ps_X(30)&&x<Ps_X(480)&&y>=Ps_Y(135+i*43)&&y<Ps_Y(169+i*43))sNrConnection=i;
                if(y>=Ps_Y(403)&&y<Ps_Y(427)&&x>=Ps_X(280))p0|=PADG_START;
            }
        }

        if(sNrPhase==0) {
            /* Input processing for Player 0 (Pad 0) */
            s32 p0Side=(sNr.players==2 && kind!=3)?0:sNrSide;
            s32 old0=sNrCursor[p0Side],cur0=old0;
            if(r0&(PADG_LEFT|PADG_LEFT2)) cur0--;
            else if(r0&(PADG_RIGHT|PADG_RIGHT2)) cur0++;
            else if(r0&(PADG_UP|PADG_UP2)) cur0-=NR_COLS;
            else if(r0&(PADG_DOWN|PADG_DOWN2)) cur0+=NR_COLS;

            if(sNrCount) {
                cur0=Ps_Wrap(cur0,0,sNrCount-1);
                sNrCursor[p0Side]=cur0;
            }
            if(cur0!=old0) {sNrCostume[p0Side]=0;Snd_PlaySe(1,0);sNrMessage[0]=0;}

            /* Page cycling via L2 / R2 */
            if(sNrCount>NR_PAGE && (p0&(PADG_L1|PADG_R1))) {
                Nr_PageShift(p0Side,(p0&PADG_L1)?-1:1);Snd_PlaySe(1,0);
            }

            if(p0&PADG_L2)sNrSlot[p0Side]=Ps_Wrap(sNrSlot[p0Side]-1,0,sNr.count[p0Side]-1);
            if(p0&PADG_R2)sNrSlot[p0Side]=Ps_Wrap(sNrSlot[p0Side]+1,0,sNr.count[p0Side]-1);
            if((p0&PADG_SELECT)&&max>1&&sNr.mode!=PC_RAID) {
                static s32 counts[]={1,3,5,10};s32 n=0;
                while(n<4&&counts[n]<=sNr.count[p0Side])n++;
                sNr.count[p0Side]=n<4&&counts[n]<=max?counts[n]:1;
                if(sNrSlot[p0Side]>=sNr.count[p0Side])sNrSlot[p0Side]=0;
            }

            /* Costume Cycling for Player 0 */
            if(p0&PADG_SQUARE) {
                s32 id0=Nr_Id(p0Side),c=sNrCostume[p0Side],n;char why[160];
                if(id0>=0) {
                    for(n=0;n<8;n++){c=(c+1)%8;if(Port_ExtraCostumeOk(id0,c,why,sizeof(why)))break;}
                    sNrCostume[p0Side]=c;gTeamSel->side[p0Side]->chara=-1;Snd_PlaySe(1,0);
                }
            }
            /* Form Cycling for Player 0 via R3 */
            if(p0&PADG_R3) {
                s32 id0=Nr_Id(p0Side),forms[8],fCount=Nr_GetForms(id0,forms,8),k,nextForm=id0;
                if(fCount>1) {
                    for(k=0;k<fCount;k++){if(forms[k]==id0){nextForm=forms[(k+1)%fCount];break;}}
                    Nr_SelectForm(p0Side,nextForm);
                }
            }
            /* Favorite toggle via L3 */
            if(p0&PADG_L3) {
                s32 id0=Nr_Id(p0Side);
                if(id0>=0) {
                    sNrFavorite[id0]=Port_RosterFavorite(id0,1);
                    if(sNrSaga==NR_SAGA_FAV) Nr_List();
                    Snd_PlaySe(1,1);
                }
            }

            if(p0&PADG_CROSS) Nr_Selected(p0Side);
            if(p0&PADG_CIRCLE) {
                s32 slot=sNrSlot[p0Side];
                if(sNrFilled[p0Side][slot]) sNrFilled[p0Side][slot]=0;
                else if(slot>0){sNrSlot[p0Side]--;sNrFilled[p0Side][slot-1]=0;}
                else if(p0Side&&sNr.players==1){sNrSide=0;Nr_List();}
                else if(!p0Side){result=0;done=1;}
                Snd_PlaySe(1,2);
            }
            if(sNr.players==1 && (p0&PADG_TRIANGLE) && kind!=3) {
                sNrSide^=1;Snd_PlaySe(1,0);
            }

            /* Input processing for Player 1 (Pad 1) when in 2-Player mode */
            if(sNr.players==2 && kind!=3) {
                s32 old1=sNrCursor[1],cur1=old1;
                if(r1&(PADG_LEFT|PADG_LEFT2)) cur1--;
                else if(r1&(PADG_RIGHT|PADG_RIGHT2)) cur1++;
                else if(r1&(PADG_UP|PADG_UP2)) cur1-=NR_COLS;
                else if(r1&(PADG_DOWN|PADG_DOWN2)) cur1+=NR_COLS;

                if(sNrCount) {
                    cur1=Ps_Wrap(cur1,0,sNrCount-1);
                    sNrCursor[1]=cur1;
                }
                if(cur1!=old1) {sNrCostume[1]=0;Snd_PlaySe(1,0);sNrMessage[0]=0;}

                if(sNrCount>NR_PAGE && (p1&(PADG_L1|PADG_R1))) {
                    Nr_PageShift(1,(p1&PADG_L1)?-1:1);Snd_PlaySe(1,0);
                }
                if(p1&PADG_L2)sNrSlot[1]=Ps_Wrap(sNrSlot[1]-1,0,sNr.count[1]-1);
                if(p1&PADG_R2)sNrSlot[1]=Ps_Wrap(sNrSlot[1]+1,0,sNr.count[1]-1);
                if(p1&PADG_SQUARE) {
                    s32 id1=Nr_Id(1),c=sNrCostume[1],n;char why[160];
                    if(id1>=0) {
                        for(n=0;n<8;n++){c=(c+1)%8;if(Port_ExtraCostumeOk(id1,c,why,sizeof(why)))break;}
                        sNrCostume[1]=c;gTeamSel->side[1]->chara=-1;Snd_PlaySe(1,0);
                    }
                }
                if(p1&PADG_R3) {
                    s32 id1=Nr_Id(1),forms[8],fCount=Nr_GetForms(id1,forms,8),k,nextForm=id1;
                    if(fCount>1) {
                        for(k=0;k<fCount;k++){if(forms[k]==id1){nextForm=forms[(k+1)%fCount];break;}}
                        Nr_SelectForm(1,nextForm);
                    }
                }
                if(p1&PADG_L3) {
                    s32 id1=Nr_Id(1);
                    if(id1>=0) {
                        sNrFavorite[id1]=Port_RosterFavorite(id1,1);
                        if(sNrSaga==NR_SAGA_FAV) Nr_List();
                        Snd_PlaySe(1,1);
                    }
                }
                if(p1&PADG_CROSS) Nr_Selected(1);
                if(p1&PADG_CIRCLE) {
                    s32 slot=sNrSlot[1];
                    if(sNrFilled[1][slot]) sNrFilled[1][slot]=0;
                    else if(slot>0){sNrSlot[1]--;sNrFilled[1][slot-1]=0;}
                    Snd_PlaySe(1,2);
                }
            }

            /* START: Confirm and continue */
            if((p0|p1)&PADG_START) {
                if(Nr_Complete()){if(kind==3){result=1;done=1;}sNrPhase=1;Nr_StageRequest();sNrMessage[0]=0;Snd_PlaySe(1,1);}
                else{Ps_Copy(sNrMessage,"Completa las posiciones de ambos equipos antes de continuar.",160);Snd_PlaySe(1,7);}
            }
            Nr_Face();
        } else if(sNrPhase==1) {
            s32 old=sNr.stage;
            if((p0|p1)&(PADG_L1|PADG_R1)) {s32 song;gTeamSel->stage->bgm=Ps_Wrap(gTeamSel->stage->bgm+(((p0|p1)&PADG_L1)?-1:1),0,gTeamSel->bgmCount-1);song=gTeamSel->bgmIds[gTeamSel->stage->bgm];if(song==TS_BGM_RANDOM)song=Rand_Range(9)+8;Bgm_Play(PORT_BGM_FILE(song));Snd_PlaySe(1,0);}
            if((r0|r1)&(PADG_LEFT|PADG_LEFT2))sNr.stage--;else if((r0|r1)&(PADG_RIGHT|PADG_RIGHT2))sNr.stage++;else if((r0|r1)&(PADG_UP|PADG_UP2))sNr.stage-=7;else if((r0|r1)&(PADG_DOWN|PADG_DOWN2))sNr.stage+=7;
            sNr.stage=Ps_Wrap(sNr.stage,0,34);if(old!=sNr.stage){Nr_StageRequest();Snd_PlaySe(1,0);}
            if((p0|p1)&PADG_CIRCLE){sNrPhase=0;Snd_PlaySe(1,2);}
            if((p0|p1)&(PADG_CROSS|PADG_START)){if(sNr.mode==PC_RAID&&sNr.players>1)sNrPhase=2;else {result=1;done=1;}}
        } else {
            if(sNrConnection==0) {if((r0|r1)&PADG_UP)sNrConnection=2;else if((r0|r1)&PADG_DOWN)sNrConnection=1;}
            else {if((p0|p1)&PADG_TRIANGLE)sNrConnection=sNrConnection==1?2:1;
                if((p0|p1)&PADG_L1)sNrField=Ps_Wrap(sNrField-1,0,4);if((p0|p1)&PADG_R1)sNrField=Ps_Wrap(sNrField+1,0,4);
                if((r0|r1)&PADG_UP)sNrIp[sNrField]++;if((r0|r1)&PADG_DOWN)sNrIp[sNrField]--;if((p0|p1)&PADG_SQUARE)sNrIp[sNrField]+=10;
                sNrIp[sNrField]=Ps_Wrap(sNrIp[sNrField],sNrField==4?1:0,sNrField==4?65535:255);
            }
            if((p0|p1)&PADG_CIRCLE){sNrPhase=1;sNrConnection=0;}
            if((p0|p1)&PADG_SELECT)sNrConnection=(sNrConnection+1)%3;
            if((p0|p1)&(PADG_START|PADG_CROSS)){if(!sNrConnection){result=1;done=1;}else {char addr[80];sprintf(addr,"%d.%d.%d.%d",sNrIp[0],sNrIp[1],sNrIp[2],sNrIp[3]);if(!Port_CoopLaunch(sNrConnection==1,addr,sNrIp[4],&sNr))Ps_Copy(sNrMessage,"No se pudo abrir la sala. Revisa los personajes y el puerto.",160);}}
        }
        Nr_Draw();Font_FlushAll();ColorFade_Draw();Gfx_EndFrame(1);Dma_Flush();File_Stub264D90();
        if(done)break;
    }
    /* Finish outstanding native file work before freeing its resource buffers. */
    while(gTeamSel->faceState!=TEAMSEL_LOAD_IDLE || gTeamSel->stageState!=TEAMSEL_LOAD_IDLE) {
        Gfx_BeginFrame();TeamSel_UpdateFaceLoad();TeamSel_UpdateStageLoad();Nr_Draw();Font_FlushAll();Gfx_EndFrame(1);Dma_Flush();File_Stub264D90();
    }
    if(result) {*out=sNr;sNrBgm=gTeamSel->bgmIds[gTeamSel->stage->bgm];if(sNrBgm==TS_BGM_RANDOM)sNrBgm=Rand_Range(9)+8;Port_RosterRemember(&sNr);Port_RosterLog(2,kind,sNr.count[0],sNr.count[1],sNr.stage);}
    gPortNativeRosterActive=0;TeamSel_Term();Dma_ResetBuffers();
    gTsProgress->team[0]=saved[0];gTsProgress->team[1]=saved[1];gTsProgress->players=savedPlayers;gTsProgress->battleType=savedType;gTsProgress->dpLevel=savedDp;gTsProgress->stageCell=savedStage;gTsProgress->bgm=savedBgm;
    return result;
}
s32 Port_NativeRosterBattle(s32 kind) {
    PcBattle chosen;s32 mode=gProgress->mode,dp=0;
    if(kind==1&&gTsProgress->battleType==2) dp=(gTsProgress->dpLevel+2)*5;
    if(!Port_NativeRosterPick(&chosen,kind,dp))return 0;
    if(!Port_PcRosterQueue(&chosen))return 0;
    Port_PcNativeRules(gTsProgress->players,kind==2 && Port_ModesFight()==PORT_MODE_TRAINING,sNrBgm);
    if(!Port_PcBattleBuild())return 0;
    gProgress->mode=mode;return 1;
}
#endif

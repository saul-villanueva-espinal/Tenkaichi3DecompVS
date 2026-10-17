#include "common.h"
#ifdef PORT
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

/* Authentic HD Budokai Tenkaichi 3 Roster with simultaneous 2-Player (J1 vs J2) selection,
   290 character grid, sagas tabs, and interactive transformation forms. */
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

#define NR_COLS 12
#define NR_ROWS 3
#define NR_PAGE 36
#define NR_CX 16
#define NR_CY 228
#define NR_CARD_W 37
#define NR_CARD_H 42
#define NR_DX 40
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
static void Nr_Face(void) {
    s32 s;
    for(s=0;s<2;s++) {
        s32 curId=Nr_Id(s);
        s32 slot=sNrSlot[s];
        s32 target=sNrFilled[s][slot]?sNr.fighter[s][slot].chara:curId;
        s32 color=sNrFilled[s][slot]?sNr.fighter[s][slot].costume:sNrCostume[s];
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
    if(id<0) {Ps_Copy(sNrMessage,"Casilla reservada: instala un personaje para usarla.",160);Snd_PlaySe(1,7);return;}
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
    s32 i;
    if(gTeamSel->bg[0] && (gTeamSel->flags&TEAMSEL_STAGE_READY)) Sprite_DrawPicture(gTeamSel->bg[0],0,0,96);
    else if(gTeamSel->bg[1]) Sprite_DrawPicture(gTeamSel->bg[1],0,0,96);
    /* Clean dark cinematic background */
    Ps_Rect(0,0,512,448,Ps_Rgba(8,16,32,180),Ps_Rgba(4,8,18,215));
    /* Ambient diagonal beams */
    Ps_Quad(0,0,240,0,0,240,120,120,Ps_Rgba(0,180,255,20),Ps_Rgba(0,180,255,0),Ps_Rgba(0,180,255,0),Ps_Rgba(0,180,255,0));
    Ps_Quad(272,448,512,250,512,448,390,448,Ps_Rgba(255,100,20,18),Ps_Rgba(255,100,20,0),Ps_Rgba(255,100,20,0),Ps_Rgba(255,100,20,0));
    /* Top sleek golden accent */
    Ps_Rect(14,33,498,35,Ps_Rgba(255,196,56,180),Ps_Rgba(255,120,20,180));
}

static void Nr_DrawFighterPanels(void) {
    s32 s,i,k,forms[8],fCount;char line[160];
    u32 gold=Ps_Rgba(255,204,50,255);

    /* Dual Fighter Panels: Player 1 (Left: 14..248) and Player 2 (Right: 264..498) */
    for(s=0;s<2;s++) {
        f32 x=s?264.0f:14.0f,w=234.0f;
        s32 active=(sNr.players==2)?1:(sNrSide==s);
        s32 charaId=gTeamSel->side[s]->chara>=0?gTeamSel->side[s]->chara:sNr.fighter[s][sNrSlot[s]].chara;
        u32 teamCol=s?Ps_Rgba(255,60,60,240):Ps_Rgba(0,195,255,240);

        /* Panel Glass Frame */
        Ps_Rect(x-1,37,x+w+1,199,active?gold:Ps_Rgba(50,75,115,160),active?gold:Ps_Rgba(30,45,75,170));
        Ps_Rect(x,38,x+w,198,Ps_Rgba(8,16,30,225),Ps_Rgba(4,8,18,240));
        Ps_Rect(x,38,x+w,40,teamCol,teamCol);

        if(sNrKind==3 && s==1) {
            Nr_Text(x+14,80,"RIVAL DEL MODO",.68f,206,1);
            Nr_Text(x+14,108,"Las reglas y el rival se conservan.",.42f,206,0);
            continue;
        }

        /* Player Badge Header */
        Ps_Rect(x+6,42,x+82,55,teamCol,teamCol);
        sprintf(line,sNr.mode==PC_RAID?(s?"JEFE CPU":"ALIADOS"):s?"JUGADOR 2":"JUGADOR 1");
        Nr_Text(x+9,43,line,0.34f,70,1);

        /* Costume & DP badges */
        sprintf(line,"Traje %d/8",(sNrFilled[s][sNrSlot[s]]?sNr.fighter[s][sNrSlot[s]].costume:sNrCostume[s])+1);
        Nr_Text(x+88,43,line,0.32f,64,0);
        sprintf(line,"PD: %d%s",sNrDp>0?Nr_Cost(s):ChrTbl_GetCost(charaId),sNrDp>0?"/20":"");
        Nr_Text(x+156,43,line,0.32f,72,1);

        /* Full Character Portrait */
        if((gTeamSel->side[s]->flags&(TEAMSEL_SIDE_FACE_READY|TEAMSEL_SIDE_NO_FACE))==TEAMSEL_SIDE_FACE_READY) {
            f32 px=s?(x+92.0f):(x+6.0f);
            Nr_Image(gTeamSel->faceRes[s],0,px,56,136,104,s,128);
        }

        /* Available Transformation Forms Track */
        fCount=Nr_GetForms(charaId,forms,6);
        for(k=0;k<fCount;k++) {
            f32 fx=s?(x+18.0f+(k%3)*25.0f):(x+154.0f+(k%3)*25.0f);
            f32 fy=72.0f+(k/3)*26.0f;
            s32 activeForm=(forms[k]==charaId);
            if(activeForm) Ps_Disc(fx,fy,11,gold,Ps_Rgba(255,140,0,255));
            else Ps_Disc(fx,fy,9,Ps_Rgba(50,70,100,180),Ps_Rgba(25,35,55,190));
            Ps_Disc(fx,fy,8,Ps_Rgba(8,14,24,255),Ps_Rgba(4,7,14,255));
            Nr_Chip(forms[k],fx-7,fy-7,14,14,128);
        }

        /* Name Banner */
        Ps_Rect(x,160,x+w,178,Ps_Rgba(6,10,20,220),Ps_Rgba(4,7,14,240));
        if(charaId<0) Ps_Copy(line,"CASILLA RESERVADA",160);
        else Port_RosterName(line,160,charaId);
        Nr_Text(x+8,162,line,0.48f,w-16,1);

        /* Dragon Stars */
        for(k=0;k<5;k++) Nr_Star(x+w-58+k*11.0f,169,2.5f);

        /* Team Member Slot Capsules */
        for(i=0;i<sNr.count[s];i++) {
            f32 sw=sNr.count[s]<=5?24.0f:18.0f;
            f32 sx=x+6.0f+i*(sw+3.0f);
            s32 isSlotActive=(sNrSlot[s]==i);
            Nr_Box(sx,181,sw,15,isSlotActive,s);
            if(sNrFilled[s][i]) Nr_Chip(sNr.fighter[s][i].chara,sx+1,182,sw-2,13,128);
            else {sprintf(line,"%d",i+1);Nr_Text(sx+sw*0.5f-3.0f,182,line,0.30f,12,isSlotActive?1:0);}
        }
    }

    /* Central Iconic VS Emblem */
    Ps_Disc(256,116,14,gold,Ps_Rgba(230,100,10,240));
    Ps_Disc(256,116,11,Ps_Rgba(16,24,44,255),Ps_Rgba(8,12,24,255));
    Nr_Text(249,110,"VS",0.42f,16,1);
}

static void Nr_DrawGrid(void) {
    s32 page=sNrCursor[sNrSide]/NR_PAGE,k,idx,id,i;char text[100];
    u32 gold=Ps_Rgba(255,204,50,255);
    u32 p1Col=Ps_Rgba(0,210,255,255),p2Col=Ps_Rgba(255,60,60,255);

    /* 1. Sagas Filter Bar (x: 14..498, y: 202..222) */
    {
        f32 tabW=484.0f/NR_SAGA_COUNT;
        for(i=0;i<NR_SAGA_COUNT;i++) {
            f32 tx=14+i*tabW;
            s32 activeTab=(i==sNrSaga);
            if(activeTab) {
                Ps_Rect(tx,202,tx+tabW-2,222,gold,Ps_Rgba(220,120,10,240));
                Nr_Text(tx+2,205,(char *)sSagaTabs[i],0.30f,tabW-4,1);
            } else {
                Ps_Rect(tx,203,tx+tabW-2,221,Ps_Rgba(20,30,50,160),Ps_Rgba(10,16,28,190));
                Nr_Text(tx+2,205,(char *)sSagaTabs[i],0.28f,tabW-4,0);
            }
        }
    }

    /* 2. Character Grid: 12 Columns x 3 Rows = 36 cards per page */
    for(k=0;k<NR_PAGE;k++) {
        f32 x=NR_CX+(k%NR_COLS)*NR_DX,y=NR_CY+(k/NR_COLS)*NR_DY;
        idx=page*NR_PAGE+k;if(idx>=sNrCount) break;id=Port_PcRosterId(sNrList[idx]);
        s32 isP1=(idx==sNrCursor[0]);
        s32 isP2=(sNrKind!=3 && idx==sNrCursor[1]);

        if(isP1 && isP2) {
            /* Both players hovering on same card */
            Ps_Rect(x-2,y-2,x+NR_CARD_W+2,y+NR_CARD_H+2,p1Col,p2Col);
            Ps_Rect(x,y,x+NR_CARD_W,y+NR_CARD_H,Ps_Rgba(45,30,60,230),Ps_Rgba(20,10,30,240));
        } else if(isP1) {
            /* Player 1 cursor */
            Ps_Rect(x-2,y-2,x+NR_CARD_W+2,y+NR_CARD_H+2,p1Col,Ps_Rgba(0,140,230,255));
            Ps_Rect(x,y,x+NR_CARD_W,y+NR_CARD_H,Ps_Rgba(10,45,75,220),Ps_Rgba(5,20,35,240));
        } else if(isP2) {
            /* Player 2 cursor */
            Ps_Rect(x-2,y-2,x+NR_CARD_W+2,y+NR_CARD_H+2,p2Col,Ps_Rgba(200,30,30,255));
            Ps_Rect(x,y,x+NR_CARD_W,y+NR_CARD_H,Ps_Rgba(75,15,15,220),Ps_Rgba(35,8,8,240));
        } else {
            /* Standard card */
            Ps_Rect(x-1,y-1,x+NR_CARD_W+1,y+NR_CARD_H+1,Ps_Rgba(35,50,75,150),Ps_Rgba(18,28,45,160));
            Ps_Rect(x,y,x+NR_CARD_W,y+NR_CARD_H,Ps_Rgba(12,18,30,210),Ps_Rgba(6,10,18,230));
        }

        if(id>=0) Nr_Chip(id,x+2,y+2,33,28,128);
        else {Ps_Rect(x+2,y+2,x+35,y+30,Ps_Rgba(16,22,34,160),Ps_Rgba(8,12,20,180));Nr_Text(x+13,y+9,"+",.55f,20,3);}

        /* Card bottom number tag */
        Ps_Rect(x,y+31,x+NR_CARD_W,y+NR_CARD_H,Ps_Rgba(5,8,16,220),Ps_Rgba(5,8,16,240));
        sprintf(text,"%03d",sNrList[idx]+1);
        Nr_Text(x+3,y+31,text,0.28f,30,(isP1||isP2)?1:0);

        /* Dual Cursor badges */
        if(isP1) Nr_Text(x+2,y+2,"1P",0.28f,14,1);
        if(isP2) Nr_Text(x+22,y+2,"2P",0.28f,14,2);

        if(id>=0 && sNrFavorite[id]) Nr_Star(x+28,y+6,2.5f);
    }
    if(!sNrCount) Nr_Text(140,280,"No hay luchadores en esta saga.",0.52f,240,0);

    /* Page Navigation Bar */
    sprintf(text,"Pagina %d / %d   (%d personajes en %s)",page+1,(sNrCount+NR_PAGE-1)/NR_PAGE,sNrCount,sSagaTitles[sNrSaga]);
    Nr_Text(16,370,text,0.34f,480,0);

    /* Bottom Control Bar */
    Nr_Box(14,404,180,26,0,0);
    Nr_Text(22,408,"X  CONFIRMAR LUCHADOR",0.42f,168,0);

    Nr_Box(200,404,70,26,0,0);
    Nr_Text(208,408,"O  VOLVER",0.42f,56,0);

    Nr_Box(276,404,222,26,Nr_Complete(),1);
    Nr_Text(284,408,sNrKind==3?"START  CONFIRMAR EQUIPO":"START  ELEGIR ESCENARIO",0.42f,206,1);
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
    Nr_Text(16,8,title,0.72f,400,1);
    if(sNrPhase==0) {Nr_DrawFighterPanels();Nr_DrawGrid();}
    else if(sNrPhase==1) Nr_DrawStage();else Nr_DrawNetwork();
    if(sNrMessage[0]) {Ps_Rect(12,430,500,447,Ps_Rgba(63,17,8,128),Ps_Rgba(25,8,7,128));Nr_Text(17,431,sNrMessage,.36f,476,1);}
    else Nr_Text(16,432,"Cruceta: elegir  |  X: a\303\261adir  |  O: deshacer  |  Cuadrado: traje  |  R3: forma  |  L1/R1: sagas  |  L2/R2: paginas",.33f,480,3);
}

/* kind: 0 individual duel, 1 relay teams, 2 training, 3 the mode's player-only team. */
s32 Port_NativeRosterPick(PcBattle *out,s32 kind,s32 dp) {
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
    max=sNr.mode==PC_RELAY?10:(sNr.mode==PC_TAG||sNr.mode==PC_FFA)?5:sNr.mode==PC_RAID?4:1;
    sNr.count[0]=sNr.count[1]=max;if(sNr.mode==PC_RAID) {sNr.count[0]=sNr.players;sNr.count[1]=1;}
    remembered=Port_RosterRecall(sNr.mode,&last);
    if(remembered && last.count[0]<=max && last.count[1]<=max && last.players==sNr.players) sNr=last;else remembered=0;
    memset(sNrFilled,0,sizeof(sNrFilled));if(remembered) memset(sNrFilled,1,sizeof(sNrFilled));
    if(kind==3 && out!=NULL && out->count[1]>0) {
        s32 k;
        sNr.count[1] = out->count[1] > 10 ? 10 : out->count[1];
        for(k=0; k<sNr.count[1]; k++) {
            sNr.fighter[1][k] = out->fighter[1][k];
            sNrFilled[1][k] = 1;
        }
        if(out->count[0]>0) sNr.count[0] = out->count[0] > 10 ? 10 : out->count[0];
        if(out->stage>=0 && out->stage<35) sNr.stage = out->stage;
        if(out->mode>0) sNr.mode = out->mode;
    } else if(kind==3) {
        sNr.count[1]=1;
        sNrFilled[1][0]=1;
    }
    sNrCostume[0]=sNr.fighter[0][0].costume;sNrCostume[1]=sNr.fighter[1][0].costume;
    memset(sNrSlot,0,sizeof(sNrSlot));sNrCursor[0]=0;sNrCursor[1]=sNrCount>1?1:0;sNrSide=0;sNrKind=kind;sNrDp=dp;sNrPhase=sNrAge=sNrConnection=sNrField=sNrSaga=0;sNrMessage[0]=0;
    for(i=0;i<PORT_CHARA_ID_LIMIT;i++) sNrFavorite[i]=0;
    for(i=0;i<Port_PcRosterCount();i++) {s32 id=Port_PcRosterId(i);sNrFavorite[id]=Port_RosterFavorite(id,0);}
    TeamSel_Init(2);gPortNativeRosterActive=1;
    {extern volatile s32 gPortPcTeamReady;gPortPcTeamReady=0;}
    Nr_List();Nr_Face();Nr_StageRequest();Port_RosterLog(0,kind,sNr.mode,Port_PcRosterCount(),PORT_TOTAL_CHARAS);
    ColorFade_StartIn(0,0,0,20);
    while(1) {
        u32 p0,r0,p1=0,r1=0; s32 x,y,click;
        Gfx_BeginFrame();Pad_Update();Snd_Update();ColorFade_Update();TeamSel_UpdateFaceLoad();TeamSel_UpdateStageLoad();
        p0=gPad[0].gamePressed|Port_RosterTestInput((unsigned)++sNrAge);r0=gPad[0].gameRepeat|p0;
        if(sNr.players==2) {
            p1=gPad[1].gamePressed;r1=gPad[1].gameRepeat|p1;
        }
        if(gProgress->flags&MPROG_FREEZE) p0=r0=p1=r1=0;
        Ps_PageSet();click=Port_RosterPointerRead(&x,&y);

        /* Mouse / Pointer Interaction */
        if(click) {
            if(sNrPhase==0) {
                s32 page=sNrCursor[sNrSide]/NR_PAGE;
                /* Click on Sagas Tabs (y: 202..222) */
                if(y>=Ps_Y(202)&&y<Ps_Y(222)&&x>=Ps_X(14)&&x<Ps_X(498)) {
                    s32 tab=(s32)((x-Ps_X(14))*NR_SAGA_COUNT/(Ps_X(498)-Ps_X(14)));
                    if(tab>=0&&tab<NR_SAGA_COUNT) {sNrSaga=tab;sNrCursor[0]=0;sNrCursor[1]=0;Nr_List();Nr_Face();Snd_PlaySe(1,0);}
                }
                /* Click on Panels to switch active side */
                if(y>=Ps_Y(38)&&y<Ps_Y(198)) {
                    if(x>=Ps_X(14)&&x<Ps_X(248)) sNrSide=0;
                    else if(x>=Ps_X(264)&&x<Ps_X(498)&&kind!=3) sNrSide=1;
                }
                /* Click on Form Nodes */
                for(s=0;s<2;s++) {
                    s32 fId=Nr_Id(s),forms[8],fCount=Nr_GetForms(fId,forms,6),k;
                    f32 px=s?264.0f:14.0f;
                    for(k=0;k<fCount;k++) {
                        f32 fx=s?(px+18.0f+(k%3)*25.0f):(px+154.0f+(k%3)*25.0f),fy=72.0f+(k/3)*26.0f;
                        if(x>=Ps_X(fx-10)&&x<Ps_X(fx+10)&&y>=Ps_Y(fy-10)&&y<Ps_Y(fy+10)) {
                            Nr_SelectForm(s,forms[k]);
                        }
                    }
                }
                /* Click on Grid Cards */
                for(i=0;i<NR_PAGE;i++) {
                    f32 xx=NR_CX+(i%NR_COLS)*NR_DX,yy=NR_CY+(i/NR_COLS)*NR_DY;
                    if(x>=Ps_X(xx)&&x<Ps_X(xx+NR_CARD_W)&&y>=Ps_Y(yy)&&y<Ps_Y(yy+NR_CARD_H)&&page*NR_PAGE+i<sNrCount) {
                        if(sNrCursor[sNrSide]==page*NR_PAGE+i) Nr_Selected(sNrSide);
                        else {sNrCostume[sNrSide]=0;sNrCursor[sNrSide]=page*NR_PAGE+i;Snd_PlaySe(1,0);}
                    }
                }
                /* Click on Bottom Action Buttons */
                if(y>=Ps_Y(404)&&y<Ps_Y(430)) {
                    if(x<Ps_X(194)) Nr_Selected(sNrSide);
                    else if(x<Ps_X(270)) p0|=PADG_CIRCLE;
                    else p0|=PADG_START;
                }
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
            s32 p0Side=(sNr.players==2)?0:sNrSide;
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
            if(sNrCount>NR_PAGE && (p0&(PADG_L2|PADG_R2))) {
                s32 pg=Ps_Wrap(cur0/NR_PAGE+((p0&PADG_L2)?-1:1),0,(sNrCount-1)/NR_PAGE);
                cur0=pg*NR_PAGE+cur0%NR_PAGE;if(cur0>=sNrCount)cur0=sNrCount-1;
                sNrCursor[p0Side]=cur0;Snd_PlaySe(1,0);
            }

            /* Sagas tab cycling via L1 / R1 */
            if(p0&(PADG_L1|PADG_R1)) {
                sNrSaga=Ps_Wrap(sNrSaga+((p0&PADG_L1)?-1:1),0,NR_SAGA_COUNT-1);
                sNrCursor[0]=0;sNrCursor[1]=0;Nr_List();Snd_PlaySe(1,0);
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
            if(sNr.players==2) {
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

                if(sNrCount>NR_PAGE && (p1&(PADG_L2|PADG_R2))) {
                    s32 pg=Ps_Wrap(cur1/NR_PAGE+((p1&PADG_L2)?-1:1),0,(sNrCount-1)/NR_PAGE);
                    cur1=pg*NR_PAGE+cur1%NR_PAGE;if(cur1>=sNrCount)cur1=sNrCount-1;
                    sNrCursor[1]=cur1;Snd_PlaySe(1,0);
                }
                if(p1&(PADG_L1|PADG_R1)) {
                    sNrSaga=Ps_Wrap(sNrSaga+((p1&PADG_L1)?-1:1),0,NR_SAGA_COUNT-1);
                    sNrCursor[0]=0;sNrCursor[1]=0;Nr_List();Snd_PlaySe(1,0);
                }
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

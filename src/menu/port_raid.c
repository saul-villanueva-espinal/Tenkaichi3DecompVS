#include "common.h"
#include "menu/menu_e.h"
#include "menu/menu_f.h"
#include "menu/port_screen.h"
#include "sys/load_sprite.h"
#include "sys/pad.h"
#include "port_coop.h"
#include "port_pc_battle.h"
#include "port_team_select.h"
#include "port_select_models.h"
#ifdef PORT
extern void TeamSel_Init(s32), TeamSel_Term(void), TeamSel_UpdateFaceLoad(void), TeamSel_UpdateStageLoad(void), TeamSel_RequestStage(void);
extern void Gfx_BeginFrame(void), Gfx_EndFrame(s32), Dma_Flush(void), Dma_ResetBuffers(void), File_Stub264D90(void), Font_FlushAll(void);
extern void ColorFade_StartIn(s32,s32,s32,s32), ColorFade_StartOut(s32,s32,s32,s32), ColorFade_Update(void), ColorFade_Draw(void);
extern s32 ColorFade_IsInDone(void), ColorFade_IsOutDone(void);
extern void Bgm_Play(s32), Port_PcNativeRules(s32,s32,s32);
extern s32 Port_Setting(const char *,s32), Port_ExtraCostumeOk(s32,s32,char *,s32);
extern void Port_SettingSave(const char *,s32), Port_SettingsWrite(void);
enum { RD_LOBBY, RD_CUSTOM, RD_ALLY, RD_NETWORK };
typedef struct RaidRoom {
    PcBattle battle;
    s32 page,cursor,ally,connection,difficulty,count,frames,leaving,launch,ip[5];
    char message[160];
} RaidRoom;
static RaidRoom sRoom;
#define R (&sRoom)
static s32 Rd_Clamp(s32 v,s32 lo,s32 hi) {return v<lo?lo:v>hi?hi:v;}
static s32 Rd_Wrap(s32 v,s32 n) {return (v%n+n)%n;}
static PcFighter *Rd_Boss(void) {return &R->battle.fighter[1][0];}
static PcFighter *Rd_Shown(void) {return R->page==RD_ALLY?&R->battle.fighter[0][R->ally]:Rd_Boss();}
static s32 Rd_Index(s32 id) {
    s32 i;for(i=0;i<Port_PcRosterCount();i++) if(Port_PcRosterId(i)==id) return i;return 0;
}
/* Each mod costume owns a separate profile. Never rewrite character assets. */
static s32 Rd_Profile(s32 field,s32 value,s32 save) {
    char key[48];PcFighter *f=Rd_Boss();sprintf(key,"raid_%d_%d_%d",f->chara,f->costume,field);
    if(save) {Port_SettingSave(key,value);return value;}return Port_Setting(key,value);
}
static void Rd_Preset(void) {
    PcBattleRules *r=&R->battle.rules;s32 d=R->difficulty;memset(r,0,sizeof(*r));
    r->bossHp=d==0?60000:d==1?120000:200000;r->bossDamagePct=d==0?100:d==1?150:200;
    r->enragePct=50;r->cpuLevel1=d==0?14:d==1?22:30;r->armorSides=d==2?2:0;r->bgm1=14;
}
static void Rd_ProfileLoad(void) {
    PcBattleRules *r=&R->battle.rules;Rd_Preset();
    r->bossHp=Rd_Clamp(Rd_Profile(0,r->bossHp,0),10000,250000);
    r->bossDamagePct=Rd_Clamp(Rd_Profile(1,r->bossDamagePct,0),50,400);
    r->cpuLevel1=Rd_Clamp(Rd_Profile(2,r->cpuLevel1,0),1,30);
    r->enragePct=Rd_Clamp(Rd_Profile(3,r->enragePct,0),0,90);
    r->armorSides=Rd_Profile(4,r->armorSides,0)?2:0;
    r->infiniteKiChara1=Rd_Profile(5,0,0)?Rd_Boss()->chara+1:0;
}
static void Rd_ProfileSave(void) {
    PcBattleRules *r=&R->battle.rules;
    Rd_Profile(0,r->bossHp,1);Rd_Profile(1,r->bossDamagePct,1);Rd_Profile(2,r->cpuLevel1,1);
    Rd_Profile(3,r->enragePct,1);Rd_Profile(4,r->armorSides,1);Rd_Profile(5,r->infiniteKiChara1!=0,1);
    Port_SettingSave("raid_boss",Rd_Boss()->chara);Port_SettingSave("raid_costume",Rd_Boss()->costume);
    Port_SettingSave("raid_stage",R->battle.stage);Port_SettingsWrite();Ps_Copy(R->message,"Perfil del jefe guardado.",sizeof(R->message));
}
static void Rd_Character(PcFighter *f,s32 dir) {
    f->chara=Port_PcRosterId(Rd_Wrap(Rd_Index(f->chara)+dir,Port_PcRosterCount()));f->costume=0;if(f==Rd_Boss()) Rd_ProfileLoad();
}
static void Rd_Costume(PcFighter *f,s32 dir) {
    s32 i;char why[160];for(i=1;i<=8;i++) {
        s32 c=Rd_Wrap(f->costume+i*dir,8);if(Port_ExtraCostumeOk(f->chara,c,why,sizeof(why))) {f->costume=c;break;}
    }if(f==Rd_Boss()) Rd_ProfileLoad();
}
static void Rd_Init(void) {
    static const s32 heroes[PORT_COOP_MAX]={3,31,21,23,3,35};s32 i,id;char why[160];
    memset(R,0,sizeof(*R));Port_PcBattleDefault(&R->battle);R->battle.mode=PC_RAID;R->battle.count[1]=1;
    R->count=PORT_COOP_MAX;R->connection=1;R->difficulty=1;R->ip[0]=127;R->ip[3]=1;R->ip[4]=29740;
    id=Port_Setting("raid_boss",133);Rd_Boss()->chara=Port_PcRosterId(Rd_Index(id));
    Rd_Boss()->costume=Rd_Clamp(Port_Setting("raid_costume",0),0,7);
    if(!Port_ExtraCostumeOk(Rd_Boss()->chara,Rd_Boss()->costume,why,sizeof(why))) Rd_Boss()->costume=0;
    for(i=0;i<PORT_COOP_MAX;i++) R->battle.fighter[0][i].chara=heroes[i];
    R->battle.stage=Rd_Clamp(Port_Setting("raid_stage",0),0,34);Rd_ProfileLoad();
}
/* Original metallic/blue hierarchy, full-width 512x448 native canvas. */
static void Rd_Plate(f32 x,f32 y,f32 w,f32 h,s32 selected,s32 gold) {
    u32 silver=Ps_Rgba(184,203,211,128),black=Ps_Rgba(7,13,24,128);
    Ps_Quad(x+5,y,x+w,y,x+w-5,y+h,x,y+h,silver,silver,Ps_Rgba(58,73,85,128),silver);
    Ps_Quad(x+6,y+2,x+w-2,y+2,x+w-6,y+h-2,x+2,y+h-2,black,black,black,black);
    Ps_Quad(x+8,y+4,x+w-4,y+4,x+w-8,y+h-4,x+4,y+h-4,
        gold?Ps_Rgba(255,242,80,128):Ps_Rgba(selected?33:15,selected?117:60,selected?210:144,128),
        gold?Ps_Rgba(255,185,12,128):Ps_Rgba(12,31,71,128),gold?Ps_Rgba(185,99,0,128):Ps_Rgba(3,9,24,128),
        gold?Ps_Rgba(249,189,13,128):Ps_Rgba(4,20,55,128));
    if(selected) Ps_Rect(x+7,y+h-3,x+w-7,y+h-1,Ps_Rgba(255,234,55,128),Ps_Rgba(255,234,55,128));
}
static void Rd_Text(f32 x,f32 y,char *text,f32 scale,f32 width,s32 gold) {
    Ps_TextExOuter(x,y,text,scale,width,Ps_Rgba(255,255,gold?105:255,128),gold?Ps_Rgba(255,204,12,128):Ps_Rgba(195,210,231,128),
        Ps_Rgba(0,0,0,128),Ps_Rgba(0,0,0,128),1.0f);
}
static void Rd_Row(s32 row,char *label,char *value) {
    f32 y=102+row*25;Rd_Plate(16,y,239,24,R->cursor==row,0);
    Rd_Text(25,y+4,label,.48f,87,0);Rd_Text(117,y+4,value,.48f,127,1);
}
static void Rd_Draw(void) {
    s32 i,index=Rd_Index(Rd_Shown()->chara),n=Port_PcRosterCount();char buf[96];PcFighter *f=Rd_Shown();PcBattleRules *rules=&R->battle.rules;
    Ps_PageFull();
    if(gTeamSel->flags&TEAMSEL_STAGE_READY) Sprite_DrawPicture(gTeamSel->bg[0],0,0,128);
    else if(gTeamSel->bg[1]) Sprite_DrawPicture(gTeamSel->bg[1],0,0,128);
    else Ps_Rect(0,0,512,448,Ps_Rgba(90,177,222,128),Ps_Rgba(60,118,46,128));
    Port_SelectModelsSet(0,f->chara,f->costume);Port_SelectModelsSet(1,-1,0);Port_SelectModelsDrawRaid();Ps_PageFull();
    Rd_Plate(-8,3,350,32,0,0);Rd_Text(23,9,R->page==RD_CUSTOM?"PERSONALIZAR JEFE":"JEFE FINAL - SALA ONLINE",.83f,307,0);
    for(i=0;i<8;i++) {
        f32 x=18+i*30.0f;s32 id=Port_PcRosterId(Rd_Wrap(index+i-3,n));Rd_Plate(x,42,29,43,i==3,0);Port_TeamChip(id,x+4,47,21,31,128);
    }
    Rd_Text(19,84,R->page==RD_ALLY?"ELEGIR ALIADO  /  L1-R1: TRAJE":"SELECCIONAR JEFE  /  L1-R1: TRAJE",.40f,240,0);
    if(R->page==RD_LOBBY) {
        Rd_Row(0,"Jefe",Port_PcRosterName(Rd_Boss()->chara));
        Rd_Row(1,"Conexion",R->connection==0?"Practica + CPU":R->connection==1?"Crear sala UDP":"Unirse por IP");
        sprintf(buf,"%d aliados vs 1 jefe",R->count);Rd_Row(2,"Jugadores",buf);
        Rd_Row(3,"Dificultad",R->difficulty==0?"Guerrero Z":R->difficulty==1?"Super Saiyan":"Extrema");
        sprintf(buf,"Arena %02d",R->battle.stage+1);Rd_Row(4,"Escenario",buf);
        Rd_Row(5,"Personalizar","Vida, poder y furia");Rd_Row(6,"Red","Direccion y puerto");
        Rd_Row(7,"Mi guerrero","Elegir personaje");Rd_Row(8,"Comprobar","Archivos de la sala");
    } else if(R->page==RD_CUSTOM) {
        Rd_Row(0,"Personaje",Port_PcRosterName(f->chara));sprintf(buf,"%d / 8",f->costume+1);Rd_Row(1,"Traje",buf);
        sprintf(buf,"%d PV",rules->bossHp);Rd_Row(2,"Vida",buf);sprintf(buf,"%d %%",rules->bossDamagePct);Rd_Row(3,"Dano",buf);
        sprintf(buf,"Nivel %d / 30",rules->cpuLevel1);Rd_Row(4,"IA del jefe",buf);
        if(rules->enragePct) sprintf(buf,"Furia al %d %% PV",rules->enragePct);else Ps_Copy(buf,"Sin furia",sizeof(buf));Rd_Row(5,"Fases",buf);
        Rd_Row(6,"Armadura",rules->armorSides?"Activada":"Normal");Rd_Row(7,"Ki infinito",rules->infiniteKiChara1?"Activado":"Desactivado");
        Rd_Row(8,"Guardar","Perfil de este traje");
    } else if(R->page==RD_ALLY) {
        sprintf(buf,"J%d - PERSONAJE",R->ally+1);Rd_Row(0,buf,Port_PcRosterName(f->chara));
        sprintf(buf,"Traje %d / 8",f->costume+1);Rd_Row(1,"Vestuario",buf);Rd_Plate(16,165,239,104,0,0);
        Rd_Text(27,177,"Izquierda / Derecha: personaje",.48f,218,0);Rd_Text(27,199,"Arriba / Abajo: plaza del equipo",.46f,218,0);
        Rd_Text(27,221,"Los personajes repetidos se permiten.",.43f,218,1);Rd_Text(27,242,"X: confirmar y volver a la sala",.45f,218,0);
        Rd_Text(21,286,R->connection==0?"Practica: J1 humano y aliados CPU.":"Online: cada PC elige su propio J1.",.42f,238,1);
    } else {
        for(i=0;i<5;i++) {sprintf(buf,"%d",R->ip[i]);Rd_Row(i,i==4?"Puerto UDP":i==0?"IP - campo 1":i==1?"IP - campo 2":i==2?"IP - campo 3":"IP - campo 4",buf);}
        Rd_Text(22,242,"Izquierda / Derecha: cambiar valor",.44f,234,0);Rd_Text(22,263,"Cuadrado: +10. X: volver a la sala.",.44f,234,0);
        Rd_Text(22,288,"Internet requiere una ruta UDP accesible.",.40f,234,1);
    }
    Rd_Plate(270,270,234,28,0,0);Rd_Text(282,277,Port_PcRosterName(f->chara),.63f,210,0);
    sprintf(buf,"TRAJE %d  /  VISTA 3D",f->costume+1);Rd_Text(310,299,buf,.41f,180,1);
    Rd_Plate(8,331,496,78,0,0);Rd_Text(20,316,"EQUIPO COOPERATIVO",.60f,240,0);
    for(i=0;i<PORT_COOP_MAX;i++) {
        f32 x=16+i*80.0f;PcFighter *hero=&R->battle.fighter[0][i];Rd_Plate(x,337,78,67,R->page==RD_ALLY&&R->ally==i,0);
        if(i<R->count) Port_TeamChip(hero->chara,x+6,343,65,31,128);sprintf(buf,"J%d",i+1);Rd_Text(x+7,340,buf,.46f,25,0);
        Rd_Text(x+6,374,i<R->count?Port_PcRosterName(hero->chara):"Cerrada",.38f,65,0);
        Rd_Text(x+8,389,i>=R->count?"--":R->connection==0?(i?"CPU":"JUGADOR"):i?"ESPERANDO":"MI ELECCION",.32f,64,1);
    }
    Ps_Rect(0,412,512,448,Ps_Rgba(7,18,37,118),Ps_Rgba(3,7,17,128));Rd_Text(12,417,"X: elegir   Triangulo: personalizar   O: volver",.41f,330,0);
    Rd_Plate(383,415,121,29,R->page==RD_LOBBY,1);Rd_Text(407,421,"INICIAR",.64f,92,0);
    Rd_Text(13,434,R->message[0]?R->message:"START: iniciar  |  Misma version y mismos mods en todos los PC",.34f,488,1);
}
static s32 Rd_Check(void) {
    char why[160];R->battle.players=R->connection==0?1:R->count;R->battle.count[0]=R->count;
    if(!Port_PcBattleValidate(&R->battle,why,sizeof(why))) {Ps_Copy(R->message,"Revisa archivos: ",sizeof(R->message));Ps_Cat(R->message,why,sizeof(R->message));return 0;}
    Ps_Copy(R->message,"Archivos locales listos. La red comprobara los mods de cada PC.",sizeof(R->message));return 1;
}
static void Rd_Input(u32 p,u32 repeat) {
    s32 dir=(repeat&(PADG_RIGHT|PADG_RIGHT2))?1:(repeat&(PADG_LEFT|PADG_LEFT2))?-1:0;
    s32 up=repeat&(PADG_UP|PADG_UP2),down=repeat&(PADG_DOWN|PADG_DOWN2);PcBattleRules *r=&R->battle.rules;
    if(p&PADG_CIRCLE) {if(R->page) {R->page=RD_LOBBY;R->cursor=0;}else {R->leaving=1;ColorFade_StartOut(0,0,0,20);}return;}
    if(p&PADG_TRIANGLE) {R->page=R->page==RD_CUSTOM?RD_LOBBY:RD_CUSTOM;R->cursor=0;return;}
    if(p&PADG_L1) Rd_Costume(Rd_Shown(),-1);if(p&PADG_R1) Rd_Costume(Rd_Shown(),1);
    if(R->page==RD_ALLY) {
        if(up||down) R->ally=Rd_Wrap(R->ally+(down?1:-1),R->connection==0?R->count:1);
        if(dir) Rd_Character(Rd_Shown(),dir);if(p&PADG_CROSS) {R->page=RD_LOBBY;R->cursor=7;}return;
    }
    if(up||down) R->cursor=Rd_Wrap(R->cursor+(down?1:-1),R->page==RD_NETWORK?5:9);
    if(R->page==RD_NETWORK) {
        if(p&PADG_SQUARE) dir=10;if(dir) R->ip[R->cursor]=R->cursor==4?Rd_Clamp(R->ip[4]+dir,1024,65535):Rd_Wrap(R->ip[R->cursor]+dir,256);
        if(p&PADG_CROSS) {R->page=RD_LOBBY;R->cursor=6;}return;
    }
    if(R->page==RD_CUSTOM) {
        if(dir) switch(R->cursor) {
        case 0:Rd_Character(Rd_Boss(),dir);break;case 1:Rd_Costume(Rd_Boss(),dir);break;
        case 2:r->bossHp=Rd_Clamp(r->bossHp+dir*10000,10000,250000);break;case 3:r->bossDamagePct=Rd_Clamp(r->bossDamagePct+dir*25,50,400);break;
        case 4:r->cpuLevel1=Rd_Clamp(r->cpuLevel1+dir,1,30);break;case 5:r->enragePct=Rd_Clamp(r->enragePct+dir*10,0,90);break;
        case 6:r->armorSides=r->armorSides?0:2;break;case 7:r->infiniteKiChara1=r->infiniteKiChara1?0:Rd_Boss()->chara+1;break;
        }if((p&PADG_CROSS)&&R->cursor==8) {Rd_ProfileSave();R->page=RD_LOBBY;R->cursor=5;}return;
    }
    if(dir) switch(R->cursor) {
    case 0:Rd_Character(Rd_Boss(),dir);break;case 1:R->connection=Rd_Wrap(R->connection+dir,3);break;
    case 2:R->count=Rd_Clamp(R->count+dir,2,PORT_COOP_MAX);break;case 3:R->difficulty=Rd_Wrap(R->difficulty+dir,3);Rd_Preset();break;
    case 4:R->battle.stage=Rd_Wrap(R->battle.stage+dir,35);gTeamSel->stage->stage=R->battle.stage;TeamSel_RequestStage();break;
    }
    if(p&PADG_CROSS) switch(R->cursor) {
    case 5:R->page=RD_CUSTOM;R->cursor=0;break;case 6:R->page=RD_NETWORK;R->cursor=0;break;case 7:R->page=RD_ALLY;R->ally=0;break;case 8:Rd_Check();break;
    }
    if((p&PADG_START)&&Rd_Check()) {R->leaving=R->launch=1;ColorFade_StartOut(0,0,0,20);}
}
s32 PortRaid_Run(void) {
    TsTeam saved[2];s32 savedPlayers,savedType,savedDp,savedStage,savedBgm,s,i,result=0;
    saved[0]=gTsProgress->team[0];saved[1]=gTsProgress->team[1];savedPlayers=gTsProgress->players;savedType=gTsProgress->battleType;
    savedDp=gTsProgress->dpLevel;savedStage=gTsProgress->stageCell;savedBgm=gTsProgress->bgm;
    memset(gTsProgress->team,0,sizeof(gTsProgress->team));gTsProgress->players=0;gTsProgress->battleType=1;gTsProgress->dpLevel=gTsProgress->stageCell=gTsProgress->bgm=0;
    for(s=0;s<2;s++) for(i=0;i<5;i++) gTsProgress->team[s].member[i].chara=i?-1:s?4:0;
    if(!gMenuArc5) gMenuArc5=File_LoadSync(gProgress->baseFile,NULL,0);
    Rd_Init();TeamSel_Init(2);Port_SelectModelsBegin();gTeamSel->stage->stage=R->battle.stage;TeamSel_RequestStage();Bgm_Play(0x10B18);ColorFade_StartIn(0,0,0,20);
    while(1) {
        u32 p,r;Gfx_BeginFrame();Pad_Update();Snd_Update();ColorFade_Update();TeamSel_UpdateFaceLoad();TeamSel_UpdateStageLoad();
        p=gPad[0].gamePressed;r=gPad[0].gameRepeat|p;R->frames++;
        if(Port_RosterTestMode()==11) {R->connection=0;p|=Port_RosterTestInput(R->frames);r|=p;}
        if(!R->leaving&&ColorFade_IsInDone()) Rd_Input(p,r);
        Rd_Draw();Font_FlushAll();ColorFade_Draw();Gfx_EndFrame(1);Dma_Flush();File_Stub264D90();if(R->leaving&&ColorFade_IsOutDone()) break;
    }
    while(gTeamSel->faceState!=TEAMSEL_LOAD_IDLE||gTeamSel->stageState!=TEAMSEL_LOAD_IDLE) {
        Gfx_BeginFrame();TeamSel_UpdateFaceLoad();TeamSel_UpdateStageLoad();Rd_Draw();Font_FlushAll();Gfx_EndFrame(1);Dma_Flush();File_Stub264D90();
    }
    Port_SelectModelsEnd();TeamSel_Term();Dma_ResetBuffers();
    gTsProgress->team[0]=saved[0];gTsProgress->team[1]=saved[1];gTsProgress->players=savedPlayers;gTsProgress->battleType=savedType;
    gTsProgress->dpLevel=savedDp;gTsProgress->stageCell=savedStage;gTsProgress->bgm=savedBgm;
    if(R->launch) {
        if(!R->connection) {result=Port_PcRosterQueue(&R->battle);if(result) Port_PcNativeRules(1,0,R->battle.rules.bgm1-1);}
        else {char address[64];sprintf(address,"%d.%d.%d.%d",R->ip[0],R->ip[1],R->ip[2],R->ip[3]);result=Port_CoopLaunch(R->connection==1,address,R->ip[4],&R->battle);}
    }return result;
}
#endif

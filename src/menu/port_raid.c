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
enum { RD_LOBBY, RD_CUSTOM, RD_ALLY, RD_NETWORK, RD_BOSS_PICK, RD_ORDER };
extern char *SDL_GetClipboardText(void);
extern int SDL_SetClipboardText(const char *text);
typedef struct RaidRoom {
    PcBattle battle;
    s32 waiting,known,boss,page,cursor,ally,connection,difficulty,count,frames,leaving,launch,ip[5];
    char message[160];
} RaidRoom;
static RaidRoom sRoom;
#define R (&sRoom)
static s32 Rd_Clamp(s32 v,s32 lo,s32 hi) {return v<lo?lo:v>hi?hi:v;}
static s32 Rd_Wrap(s32 v,s32 n) {return (v%n+n)%n;}
static PcFighter *Rd_Boss(void) {return &R->battle.fighter[1][R->boss];}
static PcBattleRules *Rd_Rules(void) {return R->boss?&R->battle.bossRules[R->boss-1]:&R->battle.rules;}
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
    PcBattleRules *r=Rd_Rules();s32 d=R->difficulty;memset(r,0,sizeof(*r));
    r->bossHp=d==0?60000:d==1?120000:200000;r->bossDamagePct=d==0?100:d==1?150:200;
    r->enragePct=50;r->cpuLevel1=d==0?14:d==1?22:30;r->armorSides=d==2?2:0;r->bgm1=R->boss?0:14;
}
static void Rd_ProfileLoad(void) {
    PcBattleRules *r=Rd_Rules();Rd_Preset();
    r->bossHp=Rd_Clamp(Rd_Profile(0,r->bossHp,0),10000,250000);
    r->bossDamagePct=Rd_Clamp(Rd_Profile(1,r->bossDamagePct,0),50,400);
    r->cpuLevel1=Rd_Clamp(Rd_Profile(2,r->cpuLevel1,0),1,30);
    r->enragePct=Rd_Clamp(Rd_Profile(3,r->enragePct,0),0,90);
    r->armorSides=Rd_Profile(4,r->armorSides,0)?2:0;
    r->infiniteKiChara1=Rd_Profile(5,0,0)?Rd_Boss()->chara+1:0;
}
static void Rd_ProfileSave(void) {
    PcBattleRules *r=Rd_Rules();
    Rd_Profile(0,r->bossHp,1);Rd_Profile(1,r->bossDamagePct,1);Rd_Profile(2,r->cpuLevel1,1);
    Rd_Profile(3,r->enragePct,1);Rd_Profile(4,r->armorSides,1);Rd_Profile(5,r->infiniteKiChara1!=0,1);
    Port_SettingSave("raid_boss",R->battle.fighter[1][0].chara);Port_SettingSave("raid_costume",R->battle.fighter[1][0].costume);
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
    memset(R,0,sizeof(*R));Port_PcBattleDefault(&R->battle);R->battle.mode=PC_RAID;R->battle.count[1]=PORT_RAID_BOSSES;
    R->count=PORT_COOP_MAX;R->connection=1;R->difficulty=1;R->ip[0]=127;R->ip[3]=1;R->ip[4]=29740;
    id=Port_Setting("raid_boss",133);Rd_Boss()->chara=Port_PcRosterId(Rd_Index(id));
    Rd_Boss()->costume=Rd_Clamp(Port_Setting("raid_costume",0),0,7);
    if(!Port_ExtraCostumeOk(Rd_Boss()->chara,Rd_Boss()->costume,why,sizeof(why))) Rd_Boss()->costume=0;
    for(i=0;i<PORT_COOP_MAX;i++) R->battle.fighter[0][i].chara=heroes[i];
    R->battle.stage=Rd_Clamp(Port_Setting("raid_stage",0),0,34);
    {static const s32 bosses[PORT_RAID_BOSSES]={133,138,95,204,139};
     for(i=0;i<PORT_RAID_BOSSES;i++) {R->boss=i;if(i) Rd_Boss()->chara=Port_PcRosterId(Rd_Index(bosses[i]));Rd_ProfileLoad();}R->boss=0;}
}

/* Original metallic/blue hierarchy, rebuilt to the approved simultaneous layout. */
#include "menu/tbo_art.h"
extern void Port_RaidRosterName(char *,s32,s32);
extern void Sprite_DrawList(void *,s32,s32,LoadSprite *);
static char sRaidName[128];
static s32 sRaidRoomOn;
s32 Port_RaidRoomActive(void) {return sRaidRoomOn;}
static char *Rd_Name(s32 chara) {Port_RaidRosterName(sRaidName,sizeof(sRaidName),chara);return sRaidName;}
static char *Rd_StageName(s32 stage) {
    static char name[32];
    if(!stage) return "Montanas";
    sprintf(name,"Escenario %02d",stage+1);return name;
}
static void Rd_Text(f32 x,f32 y,char *text,f32 scale,f32 width,s32 gold) {
    Ps_SetTextKind(scale>=.75f?3:0);
    Ps_Align(0);Ps_TextExOuter(x,y,text,scale,width,Ps_Rgba(255,255,gold?105:255,128),
        gold?Ps_Rgba(255,200,15,128):Ps_Rgba(198,216,240,128),Ps_Rgba(0,0,0,128),Ps_Rgba(0,0,0,128),scale*.35f);Ps_SetTextKind(0);
}
static void Rd_Frame(f32 x,f32 y,f32 w,f32 h,s32 gold) {
    u32 edge=gold?Ps_Rgba(255,239,93,128):Ps_Rgba(174,206,239,128),dark=Ps_Rgba(3,8,20,128);
    Ps_Quad(x+5,y,x+w,y,x,y+h,x+w-5,y+h,edge,edge,edge,Ps_Rgba(56,85,118,128));
    Ps_Quad(x+6,y+2,x+w-2,y+2,x+2,y+h-2,x+w-6,y+h-2,dark,dark,dark,dark);
    Ps_Quad(x+8,y+4,x+w-4,y+4,x+4,y+h-4,x+w-8,y+h-4,
        Ps_Rgba(13,48,100,120),Ps_Rgba(8,26,58,124),Ps_Rgba(2,8,20,128),Ps_Rgba(4,13,30,128));
}
static void Rd_Button(f32 x,f32 y,f32 w,f32 h,char *label,s32 selected) {
    Rd_Frame(x,y,w,h,selected);
    if(selected) Ps_Quad(x+7,y+4,x+w-4,y+4,x+4,y+h-4,x+w-7,y+h-4,
        Ps_Rgba(255,244,104,128),Ps_Rgba(255,200,15,128),Ps_Rgba(150,92,0,128),Ps_Rgba(255,212,24,128));
    Rd_Text(x+9,y+3,label,.76f,w-18,0);
}
static void Rd_Row(s32 index,char *label,char *value) {
    f32 y=191+index*21;Rd_Frame(10,y,137,21,index<4 && R->cursor==index+3);
    Rd_Text(16,y+3,label,.47f,66,0);Rd_Text(82,y+3,value,.47f,58,index<4 && R->cursor==index+3);
}
static void Rd_Thumb(f32 x,f32 y,f32 w,f32 h) {
    LoadSprite list[9];s32 row,col,i;void *res;
    Rd_Frame(x-2,y-2,w+4,h+4,R->cursor==7);
    res=(gTeamSel->flags&TEAMSEL_STAGE_READY)?gTeamSel->bg[0]:gTeamSel->bg[1];
    if(!res) return;
    memset(list,0,sizeof(list));list[8].flags=LOAD_SPR_END;
    /* A stage picture is EIGHT tiles, not one texture. Scale every tile;
       never call Sprite_DrawPicture, which always paints a full 512x448. */
    for(row=0;row<2;row++) for(col=0;col<4;col++) {
        i=row*4+col;list[i].flags=LOAD_SPR_DRAW;list[i].tex=i;
        list[i].x0=(s32)(x+w*col/4);list[i].x1=(s32)(x+w*(col+1)/4);
        list[i].y0=(s32)(y+h*row/2);list[i].y1=(s32)(y+h*(row+1)/2);
        list[i].u1=128;list[i].v1=256;list[i].r=list[i].g=list[i].b=list[i].a=128;
    }Sprite_DrawList(res,0,0,list);
    Ps_Rect(x,y+h-13,x+w,y+h,Ps_Rgba(3,7,16,120),Ps_Rgba(3,7,16,128));
    Rd_Text(x+6,y+h-12,Rd_StageName(R->battle.stage),.44f,w-12,0);
}
static void Rd_BossSlots(void) {
    s32 i;char buf[64];sprintf(buf,"JEFES ELEGIDOS - %d / 5",R->battle.count[1]);
    Rd_Frame(9,62,284,101,0);Rd_Text(17,61,buf,.88f,265,0);
    for(i=0;i<PORT_RAID_BOSSES;i++) {
        f32 x=13+i*55.0f;PcFighter *f=&R->battle.fighter[1][i];s32 selected=i==R->boss;
        Rd_Frame(x,81,54,78,selected);
        if(i<R->battle.count[1]) {
            Port_TeamChip(f->chara,x+4,87,45,45,128);
            Rd_Text(x+5,132,Rd_Name(f->chara),.38f,44,0);
            Ps_Rect(x+3,144,x+49,156,Ps_Rgba(138,12,20,128),Ps_Rgba(45,3,10,128));
            Rd_Text(x+8,144,"ACTIVO",.43f,38,1);
        } else Rd_Text(x+16,108,"+",1.0f,30,0);
        Ps_Rect(x+3,82,x+17,94,Ps_Rgba(225,24,37,128),Ps_Rgba(110,0,12,128));
        sprintf(buf,"B%d",i+1);Rd_Text(x+5,82,buf,.49f,30,0);
    }
}
static void Rd_Allies(void) {
    static const u32 colors[6]={0x802828dc,0x80dc7030,0x8037d4f5,0x8060c445,0x80d95dad,0x80909090};
    s32 i;char buf[48];Rd_Frame(9,308,288,91,R->cursor==7);
    if(R->waiting) {sprintf(buf,"EQUIPO ONLINE - %d / %d",R->known,R->count);Rd_Text(16,305,buf,.88f,275,0);}
    else Rd_Text(16,305,R->connection==0?"EQUIPO DE PRACTICA":"EQUIPO ONLINE",.88f,275,0);
    for(i=0;i<PORT_COOP_MAX;i++) {
        f32 x=13+i*47.0f;PcFighter *f=&R->battle.fighter[0][i];
        Rd_Frame(x,324,47,71,R->page==RD_ALLY && R->ally==i);
        if(i<R->count && (!R->waiting || i<R->known)) {Port_TeamChip(f->chara,x+3,329,40,44,128);Rd_Text(x+4,373,Rd_Name(f->chara),.32f,38,0);}
        Ps_Rect(x+3,325,x+17,337,colors[i],colors[i]);
        sprintf(buf,"J%d",i+1);Rd_Text(x+4,325,buf,.50f,30,0);
        Ps_Rect(x+3,383,x+41,393,Ps_Rgba(13,48,28,128),Ps_Rgba(3,19,10,128));
        Rd_Text(x+4,383,i>=R->count?"Cerrada":R->waiting?(i<R->known?"Listo":"Esperando"):R->connection==0?(i?"CPU":"Jugador"):i?"Esperando":"Mi eleccion",.33f,38,1);
    }
}
static void Rd_Draw(void) {
    s32 i,focus=-1;char buf[96];PcBattleRules *r=Rd_Rules();PcFighter *shown=Rd_Shown();
    {extern void Gfx_AddDefaultEnv(void);Gfx_AddDefaultEnv();}
    Ps_PageFull();Ps_SetShift(0);Ps_Sprite(TBO_RAID_ENERGY,0,0,512,448,0,0,1,1,PS_PLAIN,128);
    for(i=0;i<PORT_RAID_BOSSES;i++) {
        if(R->page==RD_ALLY && i==0) continue;
        Port_RaidModelsSet(i,i<R->battle.count[1]?R->battle.fighter[1][i].chara:-1,R->battle.fighter[1][i].costume);
    }
    if(R->page==RD_ALLY) {Port_RaidModelsSet(0,shown->chara,shown->costume);focus=0;}
    else if(R->page==RD_CUSTOM || R->page==RD_BOSS_PICK) focus=R->boss;
    Port_RaidModelsDraw(R->battle.count[1],focus);Ps_PageFull();
    Rd_Text(13,1,R->page==RD_CUSTOM?"PERSONALIZAR JEFE":"JEFE FINAL - SALA ONLINE",2.15f,275,0);
    Rd_Text(16,44,R->page==RD_CUSTOM?"PERFIL INDEPENDIENTE POR PERSONAJE Y TRAJE":"03 - ASALTO SIMULTANEO",.68f,285,0);
    if(R->page==RD_LOBBY || R->page==RD_ORDER) {
        Rd_BossSlots();Rd_Button(9,164,91,23,"Elegir jefes",R->cursor==0);
        Rd_Button(102,164,93,23,"Personalizar",R->cursor==1);Rd_Button(197,164,92,23,"Ordenar",R->cursor==2);
        Rd_Row(0,"Modalidad:",R->connection==0?"Practica (Offline)":R->connection==1?"Crear sala (Host)":"Unirse por IP (Invitado)");sprintf(buf,"%d jugadores",R->count);Rd_Row(1,"Aliados:",buf);
        sprintf(buf,"%d / 5",R->battle.count[1]);Rd_Row(2,"Jefes activos:",buf);
        Rd_Row(3,"Dificultad:",R->waiting?"Anfitrion":R->difficulty==0?"Normal":R->difficulty==1?"Super Saiyan":"Extrema");
        Rd_Row(4,"Victoria:","Vencer a todos");Rd_Thumb(153,194,108,82);
        sprintf(buf,"%d JUGADORES vs %d JEFES",R->count,R->battle.count[1]);Rd_Text(151,282,buf,1.1f,144,0);
        Rd_Allies();
        if(R->page==RD_ORDER) Rd_Text(17,402,"L/R: mover jefe  |  L2/R2: elegir  |  X: confirmar",.44f,279,1);
        else Rd_Text(17,402,"L2/R2: jefe  |  Cuadrado: aliado  |  SELECT: red",.44f,279,0);
    } else if(R->page==RD_CUSTOM) {
        static char *labels[8]={"Personaje","Traje","Vida","Dano","Nivel de IA","Furia","Armadura","Ki infinito"};
        Rd_BossSlots();
        for(i=0;i<8;i++) {
            f32 y=177+i*25;Rd_Frame(11,y,276,24,R->cursor==i);Rd_Text(20,y+4,labels[i],.64f,115,0);
            if(i==0) Ps_Copy(buf,Rd_Name(shown->chara),96);
            else if(i==1) sprintf(buf,"%d / 8",shown->costume+1);
            else if(i==2) sprintf(buf,"%d PV",r->bossHp);
            else if(i==3) sprintf(buf,"%d %%",r->bossDamagePct);
            else if(i==4) sprintf(buf,"%d / 30",r->cpuLevel1);
            else if(i==5) {if(r->enragePct) sprintf(buf,"Al %d %% de vida",r->enragePct);else Ps_Copy(buf,"Desactivada",96);}
            else if(i==6) Ps_Copy(buf,r->armorSides?"Activada":"Normal",96);
            else Ps_Copy(buf,r->infiniteKiChara1?"Activado":"Desactivado",96);
            Rd_Text(139,y+4,buf,.64f,137,R->cursor==i);
        }
        Rd_Button(12,383,133,24,"GUARDAR PERFIL",R->cursor==8);Rd_Button(150,383,137,24,"VOLVER A LA SALA",0);
    } else if(R->page==RD_BOSS_PICK || R->page==RD_ALLY) {
        s32 index=Rd_Index(shown->chara),n=Port_PcRosterCount();
        sprintf(buf,R->page==RD_ALLY?"ELEGIR ALIADO J%d":"ELEGIR JEFE B%d",R->page==RD_ALLY?R->ally+1:R->boss+1);
        Rd_Text(18,68,buf,.92f,270,1);
        for(i=0;i<7;i++) {f32 x=12+(i%4)*69.0f,y=103+(i/4)*80.0f;s32 id=Port_PcRosterId(Rd_Wrap(index+i-3,n));
            Rd_Frame(x,y,66,74,i==3);Port_TeamChip(id,x+4,y+4,56,48,128);Rd_Text(x+5,y+53,Rd_Name(id),.43f,55,0);}
        Rd_Text(18,278,"L/R: personaje  |  Arriba/Abajo: plaza",.51f,264,0);
        Rd_Text(18,301,"L1/R1: traje  |  X: confirmar",.51f,264,0);
        Rd_Text(18,326,"Los personajes repetidos se permiten",.49f,264,1);
        if(R->page==RD_ALLY) Rd_Text(18,355,R->connection==0?"Practica: J1 y aliados CPU":"Cada PC elige su personaje en J1",.48f,264,0);
    } else {
        Rd_Text(16,69,"CONEXION ONLINE DIRECTA",.97f,265,0);
        for(i=0;i<6;i++) {f32 y=109+i*38;Rd_Frame(12,y,272,34,R->cursor==i);
            if(!i) {Ps_Copy(buf,R->connection==0?"Practica (Offline)":R->connection==1?"Crear sala (Host)":"Unirse por IP (Invitado)",96);}
            else sprintf(buf,"%d",R->ip[i-1]);
            Rd_Text(22,y+7,i==0?"Modo":i==5?"Puerto UDP":"Direccion IPv4",.66f,121,0);Rd_Text(150,y+7,buf,.66f,123,1);
        }
        Rd_Text(18,354,"Triangulo: Pegar IP  |  Cuadrado: Copiar IP",.49f,266,1);
        Rd_Text(18,377,"L/R: ajustar numero  |  X: confirmar sala",.46f,266,0);
    }
    if(R->page==RD_LOBBY || R->page==RD_ORDER) {
        Rd_Frame(310,350,190,29,0);Rd_Text(320,355,"ESCUADRON DE JEFES",.88f,166,0);
    } else {Rd_Frame(310,361,190,28,0);Rd_Text(320,365,Rd_Name(shown->chara),.77f,166,0);}
    Ps_Rect(0,416,512,448,Ps_Rgba(3,9,21,124),Ps_Rgba(1,4,10,128));
    Rd_Button(9,419,99,24,"Comprobar mods",R->cursor==8 && !R->page);
    Ps_Button(0,186,431,12);Rd_Text(195,423,"Listo",.55f,39,0);
    Ps_Button(2,240,431,12);Rd_Text(250,423,"Personalizar",.52f,73,0);
    Ps_Button(4,332,431,12);Rd_Text(342,423,"Volver",.52f,44,0);
    Rd_Button(402,417,100,28,R->waiting?"ESPERANDO":"INICIAR",!R->waiting);
    if(R->message[0]) {
        Ps_Rect(9,398,503,414,Ps_Rgba(2,7,18,128),Ps_Rgba(2,7,18,128));
        Rd_Text(16,400,R->message,.46f,480,1);
    }
}

static s32 Rd_Check(void) {
    char why[160];R->battle.players=R->connection==0?1:R->count;R->battle.count[0]=R->count;
    if(!Port_PcBattleValidate(&R->battle,why,sizeof(why))) {Ps_Copy(R->message,"Revisa archivos: ",sizeof(R->message));Ps_Cat(R->message,why,sizeof(R->message));return 0;}
    Ps_Copy(R->message,"Archivos locales listos. La red comprobara los mods de cada PC.",sizeof(R->message));return 1;
}
static void Rd_Input(u32 p,u32 repeat) {
    s32 dir=(repeat&(PADG_RIGHT|PADG_RIGHT2))?1:(repeat&(PADG_LEFT|PADG_LEFT2))?-1:0;
    s32 up=repeat&(PADG_UP|PADG_UP2),down=repeat&(PADG_DOWN|PADG_DOWN2),i,save;
    PcBattleRules *r=Rd_Rules();
    if(p&PADG_CIRCLE) {if(R->page) {R->page=RD_LOBBY;R->cursor=0;}else {R->leaving=1;ColorFade_StartOut(0,0,0,20);}return;}
    if((p&PADG_SQUARE) && R->page==RD_LOBBY) {R->page=RD_ALLY;R->ally=0;return;}
    if(p&PADG_SELECT) {R->page=R->page==RD_NETWORK?RD_LOBBY:RD_NETWORK;R->cursor=0;return;}
    if(p&PADG_TRIANGLE) {R->page=R->page==RD_CUSTOM?RD_LOBBY:RD_CUSTOM;R->cursor=0;return;}
    if(p&PADG_L2) R->boss=Rd_Wrap(R->boss-1,R->battle.count[1]);
    if(p&PADG_R2) R->boss=Rd_Wrap(R->boss+1,R->battle.count[1]);
    if(p&PADG_L1) Rd_Costume(Rd_Shown(),-1);
    if(p&PADG_R1) Rd_Costume(Rd_Shown(),1);
    r=Rd_Rules();
    if(R->page==RD_ALLY || R->page==RD_BOSS_PICK) {
        if(up||down) {if(R->page==RD_ALLY) R->ally=Rd_Wrap(R->ally+(down?1:-1),R->connection==0?R->count:1);else R->boss=Rd_Wrap(R->boss+(down?1:-1),R->battle.count[1]);}
        if(dir) Rd_Character(Rd_Shown(),dir);
        if(p&PADG_CROSS) {R->page=RD_LOBBY;R->cursor=0;}return;
    }
    if(R->page==RD_ORDER) {
        if(dir) {
            PcFighter fighter;PcBattleRules rules,*a,*b;s32 other=Rd_Wrap(R->boss+dir,R->battle.count[1]);
            a=Rd_Rules();b=other?&R->battle.bossRules[other-1]:&R->battle.rules;
            fighter=*Rd_Boss();*Rd_Boss()=R->battle.fighter[1][other];R->battle.fighter[1][other]=fighter;
            rules=*a;*a=*b;*b=rules;R->battle.rules.bgm1=14;
            for(i=0;i<PORT_RAID_BOSSES-1;i++) R->battle.bossRules[i].bgm1=0;R->boss=other;
        }if(p&PADG_CROSS) {R->page=RD_LOBBY;R->cursor=2;}return;
    }
    if(up||down) R->cursor=Rd_Wrap(R->cursor+(down?1:-1),R->page==RD_NETWORK?6:R->page==RD_CUSTOM?9:10);
    if(R->page==RD_NETWORK) {
        if(p&PADG_TRIANGLE) {
            char *clip=SDL_GetClipboardText();
            if(clip) {
                int a,b,c_ip,d,prt=0;
                if(sscanf(clip,"%d.%d.%d.%d:%d",&a,&b,&c_ip,&d,&prt)>=4 || sscanf(clip,"%d.%d.%d.%d",&a,&b,&c_ip,&d)==4) {
                    R->ip[0]=a&0xFF;R->ip[1]=b&0xFF;R->ip[2]=c_ip&0xFF;R->ip[3]=d&0xFF;
                    if(prt>=1024 && prt<=65535) R->ip[4]=prt;
                    Ps_Copy(R->message,"IP pegada con exito desde el portapapeles",sizeof(R->message));
                }
            }
        }
        if((p&PADG_SQUARE) && !dir) {
            char ipbuf[64];
            snprintf(ipbuf,sizeof(ipbuf),"%d.%d.%d.%d:%d",R->ip[0],R->ip[1],R->ip[2],R->ip[3],R->ip[4]);
            SDL_SetClipboardText(ipbuf);
            Ps_Copy(R->message,"IP copiada al portapapeles para tu amigo",sizeof(R->message));
        }
        if(dir) {if(!R->cursor) R->connection=Rd_Wrap(R->connection+(dir>0?1:-1),3);
            else {i=R->cursor-1;R->ip[i]=i==4?Rd_Clamp(R->ip[i]+dir,1024,65535):Rd_Wrap(R->ip[i]+dir,256);}}
        if(p&PADG_CROSS) {R->page=RD_LOBBY;R->cursor=0;}return;
    }
    if(R->page==RD_CUSTOM) {
        if(dir) switch(R->cursor) {
        case 0:Rd_Character(Rd_Boss(),dir);break;case 1:Rd_Costume(Rd_Boss(),dir);break;
        case 2:r->bossHp=Rd_Clamp(r->bossHp+dir*10000,10000,250000);break;case 3:r->bossDamagePct=Rd_Clamp(r->bossDamagePct+dir*25,50,400);break;
        case 4:r->cpuLevel1=Rd_Clamp(r->cpuLevel1+dir,1,30);break;case 5:r->enragePct=Rd_Clamp(r->enragePct+dir*10,0,90);break;
        case 6:r->armorSides=r->armorSides?0:2;break;case 7:r->infiniteKiChara1=r->infiniteKiChara1?0:Rd_Boss()->chara+1;break;
        }
        if((p&PADG_CROSS)&&R->cursor==8) {Rd_ProfileSave();R->page=RD_LOBBY;R->cursor=1;}return;
    }
    if(dir) switch(R->cursor) {
    case 0:R->boss=Rd_Wrap(R->boss+dir,R->battle.count[1]);break;
    case 3:R->connection=Rd_Wrap(R->connection+dir,3);break;
    case 4:R->count=Rd_Clamp(R->count+dir,2,PORT_COOP_MAX);break;
    case 5:R->battle.count[1]=Rd_Clamp(R->battle.count[1]+dir,1,PORT_RAID_BOSSES);R->boss=Rd_Clamp(R->boss,0,R->battle.count[1]-1);break;
    case 6:R->difficulty=Rd_Wrap(R->difficulty+dir,3);save=R->boss;for(i=0;i<PORT_RAID_BOSSES;i++){R->boss=i;Rd_Preset();}R->boss=save;break;
    case 7:R->battle.stage=Rd_Wrap(R->battle.stage+dir,35);gTeamSel->stage->stage=R->battle.stage;TeamSel_RequestStage();break;
    }
    if(p&PADG_CROSS) switch(R->cursor) {
    case 0:R->page=RD_BOSS_PICK;break;case 1:R->page=RD_CUSTOM;R->cursor=0;break;
    case 2:R->page=RD_ORDER;break;case 3:R->page=RD_NETWORK;R->cursor=0;break;
    case 7:R->battle.stage=Rd_Wrap(R->battle.stage+1,35);gTeamSel->stage->stage=R->battle.stage;TeamSel_RequestStage();break;case 8:Rd_Check();break;
    }
    if(((p&PADG_START)||((p&PADG_CROSS)&&R->cursor==9))&&Rd_Check()) {R->leaving=R->launch=1;ColorFade_StartOut(0,0,0,20);}
}
s32 PortRaid_Run(void) {
    TsTeam saved[2];s32 savedPlayers,savedType,savedDp,savedStage,savedBgm,s,i,result=0;
    saved[0]=gTsProgress->team[0];saved[1]=gTsProgress->team[1];savedPlayers=gTsProgress->players;savedType=gTsProgress->battleType;
    savedDp=gTsProgress->dpLevel;savedStage=gTsProgress->stageCell;savedBgm=gTsProgress->bgm;
    memset(gTsProgress->team,0,sizeof(gTsProgress->team));gTsProgress->players=0;gTsProgress->battleType=1;gTsProgress->dpLevel=gTsProgress->stageCell=gTsProgress->bgm=0;
    for(s=0;s<2;s++) for(i=0;i<5;i++) gTsProgress->team[s].member[i].chara=i?-1:s?4:0;
    if(!gMenuArc5) gMenuArc5=File_LoadSync(gProgress->baseFile,NULL,0);
    Rd_Init();TeamSel_Init(2);Port_RaidModelsBegin();sRaidRoomOn=1;gTeamSel->stage->stage=R->battle.stage;TeamSel_RequestStage();Bgm_Play(0x10B18);ColorFade_StartIn(0,0,0,20);
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
    Port_RaidModelsEnd();sRaidRoomOn=0;TeamSel_Term();Dma_ResetBuffers();
    gTsProgress->team[0]=saved[0];gTsProgress->team[1]=saved[1];gTsProgress->players=savedPlayers;gTsProgress->battleType=savedType;
    gTsProgress->dpLevel=savedDp;gTsProgress->stageCell=savedStage;gTsProgress->bgm=savedBgm;
    if(R->launch) {
        if(!R->connection) {result=Port_PcRosterQueue(&R->battle);if(result) Port_PcNativeRules(1,0,R->battle.rules.bgm1-1);}
        else {char address[64];sprintf(address,"%d.%d.%d.%d",R->ip[0],R->ip[1],R->ip[2],R->ip[3]);result=Port_CoopLaunch(R->connection==1,address,R->ip[4],&R->battle);}
    }return result;
}

static s32 sRaidWaitLoaded;
static TsTeam sWaitTeams[2];
static s32 sWaitPlayers,sWaitType,sWaitDp,sWaitStage,sWaitBgm;
void PortRaid_WaitFrame(void) {
    PcBattle battle;s32 phase,known,total,previous;
    phase=Port_CoopLobbyStatus(&known,&total);sRaidRoomOn=1;
    if(phase==2 && Port_CoopLobbyCopy(&battle)) {
        if(!sRaidWaitLoaded) {
            sWaitTeams[0]=gTsProgress->team[0];sWaitTeams[1]=gTsProgress->team[1];
            sWaitPlayers=gTsProgress->players;sWaitType=gTsProgress->battleType;sWaitDp=gTsProgress->dpLevel;
            sWaitStage=gTsProgress->stageCell;sWaitBgm=gTsProgress->bgm;
            gTsProgress->players=0;gTsProgress->battleType=1;gTsProgress->dpLevel=gTsProgress->stageCell=gTsProgress->bgm=0;
            if(!gMenuArc5) gMenuArc5=File_LoadSync(gProgress->baseFile,NULL,0);
            Rd_Init();R->battle=battle;R->waiting=1;R->connection=1;R->cursor=-1;
            TeamSel_Init(2);Port_RaidModelsBegin();gTeamSel->stage->stage=battle.stage;TeamSel_RequestStage();
            sRaidWaitLoaded=1;
        }
        previous=R->battle.stage;R->battle=battle;R->count=total;R->known=known;
        if(previous!=battle.stage) {gTeamSel->stage->stage=battle.stage;TeamSel_RequestStage();}
        Gfx_BeginFrame();TeamSel_UpdateFaceLoad();TeamSel_UpdateStageLoad();
        sprintf(R->message,"Esperando jugadores: %d / %d. Misma version y mismos mods.",known,total);
        Rd_Draw();Font_FlushAll();Gfx_EndFrame(1);Dma_Flush();File_Stub264D90();
    } else {
        Gfx_BeginFrame();Ps_PageFull();Ps_Sprite(TBO_RAID_ENERGY,0,0,512,448,0,0,1,1,PS_PLAIN,128);
        Rd_Text(18,24,"JEFE FINAL - SALA ONLINE",1.6f,475,0);
        Rd_Text(30,220,"Comprobando datos del juego y paquetes de personajes...",.75f,450,1);
        Font_FlushAll();Gfx_EndFrame(1);Dma_Flush();
    }
}
void PortRaid_WaitEnd(void) {
    Port_CoopPauseSimulation(1);
    if(sRaidWaitLoaded) {
        while(gTeamSel->faceState!=TEAMSEL_LOAD_IDLE || gTeamSel->stageState!=TEAMSEL_LOAD_IDLE) {
            Gfx_BeginFrame();TeamSel_UpdateFaceLoad();TeamSel_UpdateStageLoad();Gfx_EndFrame(1);Dma_Flush();File_Stub264D90();
        }
        Port_RaidModelsEnd();TeamSel_Term();Dma_ResetBuffers();
        gTsProgress->team[0]=sWaitTeams[0];gTsProgress->team[1]=sWaitTeams[1];
        gTsProgress->players=sWaitPlayers;gTsProgress->battleType=sWaitType;gTsProgress->dpLevel=sWaitDp;
        gTsProgress->stageCell=sWaitStage;gTsProgress->bgm=sWaitBgm;
    }
    sRaidWaitLoaded=sRaidRoomOn=0;Port_CoopPauseSimulation(0);
}

s32 PortRaid_OnlineError(void) {
    char why[160];s32 age=0,test=Port_CoopTestInput();
    extern void Fade_ResetAll(void);
    Port_CoopErrorText(why,sizeof(why));Port_CoopLeave();Fade_ResetAll();sRaidRoomOn=1;
    if(!why[0]) Ps_Copy(why,"No se pudo iniciar la sala.",sizeof(why));
    for(;;) {
        u32 pad;Gfx_BeginFrame();Pad_Update();Snd_Update();pad=gPad[0].gamePressed;
        {extern void Gfx_AddDefaultEnv(void);Gfx_AddDefaultEnv();}
        Ps_PageFull();Ps_SetShift(0);Ps_Sprite(TBO_RAID_ENERGY,0,0,512,448,0,0,1,1,PS_PLAIN,128);
        Rd_Text(18,25,"JEFE FINAL - SALA CERRADA",1.6f,472,0);
        Rd_Frame(25,155,462,147,0);Rd_Text(40,174,"CONEXION INTERRUMPIDA",1.1f,430,1);
        Rd_Text(40,215,why,.66f,425,0);
        Rd_Text(40,263,"X / Circulo: volver al menu principal",.68f,425,0);
        Font_FlushAll();Gfx_EndFrame(1);Dma_Flush();
        if(++age>15 && ((pad&(PADG_CROSS|PADG_CIRCLE)) || (test&&age>120))) break;
    }
    Dma_ResetBuffers();sRaidRoomOn=0;gProgress->mode=4;return 0;
}
static s32 sRaidOnlineEnding;
void PortRaid_OnlineFinish(s32 done) {
    extern s32 Port_ModesFight(void);
    if(!done) {
        sRaidOnlineEnding=Port_CoopEnabled() && Port_ModesFight()==3;
        if(sRaidOnlineEnding) Port_CoopPauseSimulation(1);
    } else if(sRaidOnlineEnding) {
        if(Port_CoopFailed()) PortRaid_OnlineError();else Port_CoopLeave();
        gProgress->mode=4;sRaidOnlineEnding=0;
    }
}
#endif

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
enum { RD_LOBBY, RD_CUSTOM, RD_ALLY, RD_NETWORK, RD_BOSS_PICK, RD_ORDER, RD_CONNECT, RD_JOIN };
#include "coop_rooms.h"
#ifndef TYPES_H
#define TYPES_H
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
typedef long long s64;
typedef unsigned long long u64;
typedef float f32;
#endif
extern void Port_GetLocalIp(s32 ip[4]);
extern void PortTbo_TextMode(int), PortTbo_Copy(const char *);
extern int PortTbo_TextRead(void);
extern void PortRaid_ClipboardRequest(void);
extern int PortRaid_ClipboardRead(char *,int);
extern void Port_SettingText(const char *,const char *,char *,int), Port_SettingTextSave(const char *,const char *);
extern void Port_VerifyStart(void);
extern void Port_RaidModelsDrawSelector(void), Port_RaidModelsTurn(f32);
extern void Port_RaidRosterDetails(char *,s32,char *,s32,s32);
extern int Port_DataFingerprint(unsigned long long *);
extern unsigned long long Port_CharacterFingerprint(void);

#define LOBBY_PKT_SYNC   0x42543353 /* 'BT3S' */
#define LOBBY_PKT_UPDATE 0x42543355 /* 'BT3U' */
#define LOBBY_PKT_START  0x42543347 /* 'BT3G' */

typedef struct RaidPlayerSlot {
    u8 connected; /* 0 = libre, 1 = conectado */
    u8 state;     /* 0 = desconectado, 1 = conectando, 2 = comprobando, 3 = eligiendo, 4 = listo */
    s16 chara;
    s8 costume;
    char name[20];
    u64 lastSeen;
} RaidPlayerSlot;

typedef struct LobbySyncPacket {
    u32 magic;
    u32 version;
    u32 stage;
    u32 bossCount;
    s32 bossChara[PORT_RAID_BOSSES];
    s32 bossCostume[PORT_RAID_BOSSES];
    s32 bossHp;
    s32 bossDamagePct;
    s32 cpuLevel1;
    s32 enragePct;
    s32 armorSides;
    s32 infiniteKi;
    s32 difficulty;
    u32 playerCount;
    struct {
        u8 connected;
        u8 state;
        s16 chara;
        s8 costume;
        char name[20];
    } players[PORT_COOP_MAX];
    u8 launching;
} LobbySyncPacket;

typedef struct LobbyUpdatePacket {
    u32 magic;
    u32 version;
    u8 slot;
    u8 state;
    s16 chara;
    s8 costume;
    char name[20];
} LobbyUpdatePacket;

typedef struct RaidRoom {
    PcBattle battle;
    s32 waiting,known,boss,page,cursor,ally,connection,difficulty,count,frames,leaving,launch,ip[5];
    char message[160];
    char playerName[32];
    char roomCode[16];
    char editText[32];
    s32 editing,connecting,connectHost;
    s32 grid[PORT_TOTAL_CHARAS],gridCount,gridCursor,gridFilter;
    char search[32];
    u64 connectBegan;
    s32 mySlot;
    s32 myState;
    s32 lobbySock;
    u64 lastStateSend;
    u64 modFingerprint;
    RaidPlayerSlot players[PORT_COOP_MAX];
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
static const char sBase62Chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static void Ip_ToCode(const s32 ip[4], char *out, s32 out_size) {
    u32 val = ((u32)(ip[0] & 0xFF) << 24) |
              ((u32)(ip[1] & 0xFF) << 16) |
              ((u32)(ip[2] & 0xFF) << 8)  |
              ((u32)(ip[3] & 0xFF));
    char code[7];
    s32 i;
    code[6] = '\0';
    for(i = 5; i >= 0; i--) {
        code[i] = sBase62Chars[val % 62];
        val /= 62;
    }
    sprintf(out, "BT3-%c%c%c%c%c%c", code[0], code[1], code[2], code[3], code[4], code[5]); (void)out_size;
}
static s32 Code_ToIp(const char *code, s32 ip[4]) {
    const char *p = code;
    unsigned long long val = 0;
    s32 count = 0;
    if(!code) return 0;
    while(*p == ' ' || *p == '\t') p++;
    if(strncmp(p, "BT3-", 4) == 0 || strncmp(p, "bt3-", 4) == 0) p += 4;
    else if(strncmp(p, "BT3 - ", 6) == 0 || strncmp(p, "bt3 - ", 6) == 0) p += 6;
    while(*p) {
        char c = *p++;
        s32 digit = -1;
        if(c >= '0' && c <= '9') digit = c - '0';
        else if(c >= 'A' && c <= 'Z') digit = c - 'A' + 10;
        else if(c >= 'a' && c <= 'z') digit = c - 'a' + 36;
        else if(c == ' ' || c == '-') continue;
        else break;
        val = val * 62ULL + (unsigned long long)digit;
        count++;
        if(count == 6) break;
    }
    if(count == 6 && val <= 0xFFFFFFFFULL) {
        ip[0] = (s32)((val >> 24) & 0xFF);
        ip[1] = (s32)((val >> 16) & 0xFF);
        ip[2] = (s32)((val >> 8) & 0xFF);
        ip[3] = (s32)(val & 0xFF);
        return 1;
    }
    return 0;
}
static void Rd_Init(void) {
    static const s32 heroes[PORT_COOP_MAX]={3,31,21,23,3,35};s32 i,id;char why[160];
    memset(R,0,sizeof(*R));Port_PcBattleDefault(&R->battle);R->battle.mode=PC_RAID;R->battle.count[1]=PORT_RAID_BOSSES;
    R->count=PORT_COOP_MAX;R->connection=1;R->difficulty=1;Port_GetLocalIp(R->ip);R->ip[4]=29740;
    R->lobbySock = -1;
    Port_SettingText("raid_name","Jugador",R->playerName,sizeof(R->playerName));
    if(!R->playerName[0]) Ps_Copy(R->playerName,"Jugador",sizeof(R->playerName));
    R->playerName[19]=0;
    id=Port_Setting("raid_boss",133);Rd_Boss()->chara=Port_PcRosterId(Rd_Index(id));
    Rd_Boss()->costume=Rd_Clamp(Port_Setting("raid_costume",0),0,7);
    if(!Port_ExtraCostumeOk(Rd_Boss()->chara,Rd_Boss()->costume,why,sizeof(why))) Rd_Boss()->costume=0;
    for(i=0;i<PORT_COOP_MAX;i++) {
        R->battle.fighter[0][i].chara=heroes[i];
        R->players[i].chara=(s16)heroes[i];
        R->players[i].costume=0;
        R->players[i].connected=(i==0);
        R->players[i].state=(i==0)?3:0;
    }
    strncpy(R->players[0].name, R->playerName, 19);
    R->battle.stage=Rd_Clamp(Port_Setting("raid_stage",0),0,34);
    {static const s32 bosses[PORT_RAID_BOSSES]={133,138,95,204,139};
     for(i=0;i<PORT_RAID_BOSSES;i++) {R->boss=i;if(i) Rd_Boss()->chara=Port_PcRosterId(Rd_Index(bosses[i]));Rd_ProfileLoad();}R->boss=0;}
}

static s32 Rd_Contains(const char *text,const char *needle) {
    s32 i,j;for(i=0;text[i];i++) {for(j=0;needle[j];j++) {
        char a=text[i+j],b=needle[j];if(a>='A'&&a<='Z')a+=32;if(b>='A'&&b<='Z')b+=32;
        if(!a||a!=b)break;
    }if(!needle[j])return 1;}return !needle[0];
}
static void Rd_GridBuild(void) {
    s32 i,id,selected=Rd_Shown()->chara;char name[128],form[128];
    R->gridCount=R->gridCursor=0;
    for(i=0;i<Port_PcRosterCount() && R->gridCount<PORT_TOTAL_CHARAS;i++) {
        id=Port_PcRosterId(i);
        if((R->gridFilter==1 && id>=PORT_DISC_CHARAS)||(R->gridFilter==2 && id<PORT_CHARA_FIRST))continue;
        Port_RaidRosterDetails(name,sizeof(name),form,sizeof(form),id);
        if(R->search[0]&&!Rd_Contains(name,R->search)&&!Rd_Contains(form,R->search))continue;
        if(id==selected) R->gridCursor=R->gridCount;
        R->grid[R->gridCount++]=id;
    }
}
static void Rd_GridChoose(void) {
    PcFighter *f=Rd_Shown();s32 id;
    if(!R->gridCount)return;
    id=R->grid[R->gridCursor];
    if(f->chara!=id) {
        f->chara=id;f->costume=0;
        if(f==Rd_Boss())Rd_ProfileLoad();
        else {R->players[R->ally].state=3;R->myState=3;}
    }
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
static void Rd_StepsBanner(s32 active) {
    f32 y=46.0f;
    /* Step 1 */
    {
        s32 a=(active==1);
        u32 bord=a?Ps_Rgba(255,220,50,128):Ps_Rgba(50,80,120,90);
        u32 bg1=a?Ps_Rgba(230,170,10,125):Ps_Rgba(10,24,45,90);
        u32 bg2=a?Ps_Rgba(130,80,0,125):Ps_Rgba(6,14,28,90);
        Ps_Quad(14,y,106,y,10,y+18,102,y+18,bord,bord,bord,bord);
        Ps_Quad(15,y+1,105,y+1,11,y+17,101,y+17,bg1,bg1,bg2,bg2);
        Ps_Disc(21,y+9,6,a?Ps_Rgba(255,240,100,128):Ps_Rgba(80,110,150,100),
                              a?Ps_Rgba(180,110,0,128):Ps_Rgba(30,50,80,100));
        Rd_Text(19,y+4,"1",.40f,10,0);
        Rd_Text(31,y+4,"CONECTAR",.48f,70,a?1:0);
    }
    Rd_Text(112,y+4,">",.50f,10,0);
    /* Step 2 */
    {
        s32 a=(active==2);
        u32 bord=a?Ps_Rgba(255,220,50,128):Ps_Rgba(50,80,120,90);
        u32 bg1=a?Ps_Rgba(230,170,10,125):Ps_Rgba(10,24,45,90);
        u32 bg2=a?Ps_Rgba(130,80,0,125):Ps_Rgba(6,14,28,90);
        Ps_Quad(124,y,246,y,120,y+18,242,y+18,bord,bord,bord,bord);
        Ps_Quad(125,y+1,245,y+1,121,y+17,241,y+17,bg1,bg1,bg2,bg2);
        Ps_Disc(131,y+9,6,a?Ps_Rgba(255,240,100,128):Ps_Rgba(80,110,150,100),
                               a?Ps_Rgba(180,110,0,128):Ps_Rgba(30,50,80,100));
        Rd_Text(129,y+4,"2",.40f,10,0);
        Rd_Text(141,y+4,"PREPARAR EQUIPO",.48f,100,a?1:0);
    }
    Rd_Text(252,y+4,">",.50f,10,0);
    /* Step 3 */
    {
        s32 a=(active==3);
        u32 bord=a?Ps_Rgba(255,220,50,128):Ps_Rgba(50,80,120,90);
        u32 bg1=a?Ps_Rgba(230,170,10,125):Ps_Rgba(10,24,45,90);
        u32 bg2=a?Ps_Rgba(130,80,0,125):Ps_Rgba(6,14,28,90);
        Ps_Quad(264,y,346,y,260,y+18,342,y+18,bord,bord,bord,bord);
        Ps_Quad(265,y+1,345,y+1,261,y+17,341,y+17,bg1,bg1,bg2,bg2);
        Ps_Disc(271,y+9,6,a?Ps_Rgba(255,240,100,128):Ps_Rgba(80,110,150,100),
                               a?Ps_Rgba(180,110,0,128):Ps_Rgba(30,50,80,100));
        Rd_Text(269,y+4,"3",.40f,10,0);
        Rd_Text(281,y+4,"COMBATIR",.48f,60,a?1:0);
    }
}
static void Rd_DrawConnect(void) {
    f32 y;
    Rd_Text(14,8,"JEFE FINAL - ONLINE",1.95f,320,1);
    Rd_StepsBanner(1);
    /* Tu nombre */
    y=72.0f;
    Rd_Frame(12,y,268,34,R->cursor==0);
    Rd_Text(20,y+3,"Tu nombre",.56f,72,0);
    Ps_Quad(98,y+3,268,y+3,96,y+24,266,y+24,
            Ps_Rgba(25,45,80,128),Ps_Rgba(15,30,60,128),
            Ps_Rgba(8,18,38,128),Ps_Rgba(12,24,50,128));
    Rd_Text(106,y+5,R->editing==1?R->editText:R->playerName,.68f,150,1);
    Rd_Text(20,y+24,"Apodo visible para tus amigos.",.36f,180,0);
    /* CREAR SALA */
    y=112.0f;
    {
        s32 sel=(R->cursor==1);
        u32 border=sel?Ps_Rgba(255,230,80,128):Ps_Rgba(160,130,40,100);
        u32 bgTop=sel?Ps_Rgba(240,180,20,125):Ps_Rgba(50,40,15,120);
        u32 bgBot=sel?Ps_Rgba(140,90,5,125):Ps_Rgba(25,20,8,120);
        Ps_Quad(16,y,278,y,10,y+48,272,y+48,border,border,border,border);
        Ps_Quad(18,y+2,276,y+2,12,y+46,270,y+46,bgTop,bgTop,bgBot,bgBot);
        Ps_Disc(36,y+24,16,sel?Ps_Rgba(255,220,50,128):Ps_Rgba(120,100,30,120),
                                sel?Ps_Rgba(180,120,10,128):Ps_Rgba(50,40,10,120));
        Rd_Text(29,y+15,"+",.9f,20,1);
        Rd_Text(58,y+7,"CREAR SALA",1.15f,205,sel?1:0);
        Rd_Text(58,y+29,"Genera un codigo privado para invitar amigos.",.38f,205,0);
    }
    /* UNIRME A UNA SALA */
    y=166.0f;
    {
        s32 sel=(R->cursor==2);
        u32 border=sel?Ps_Rgba(80,200,255,128):Ps_Rgba(40,100,160,100);
        u32 bgTop=sel?Ps_Rgba(20,110,210,125):Ps_Rgba(10,35,75,120);
        u32 bgBot=sel?Ps_Rgba(5,50,130,125):Ps_Rgba(5,18,42,120);
        Ps_Quad(16,y,278,y,10,y+48,272,y+48,border,border,border,border);
        Ps_Quad(18,y+2,276,y+2,12,y+46,270,y+46,bgTop,bgTop,bgBot,bgBot);
        Ps_Disc(36,y+24,16,sel?Ps_Rgba(60,180,255,128):Ps_Rgba(20,80,150,120),
                                sel?Ps_Rgba(10,90,180,128):Ps_Rgba(10,35,80,120));
        Rd_Text(30,y+15,">",.85f,20,sel?1:0);
        Rd_Text(58,y+7,"UNIRME A UNA SALA",1.15f,205,sel?1:0);
        Rd_Text(58,y+29,"Pega el codigo de invitacion que te compartieron.",.38f,205,0);
    }
    /* PRACTICA OFFLINE */
    y=220.0f;
    {
        s32 sel=(R->cursor==3);
        u32 border=sel?Ps_Rgba(100,240,140,128):Ps_Rgba(40,120,60,100);
        u32 bgTop=sel?Ps_Rgba(20,140,70,125):Ps_Rgba(10,50,25,120);
        u32 bgBot=sel?Ps_Rgba(5,70,35,125):Ps_Rgba(5,25,12,120);
        Ps_Quad(16,y,278,y,10,y+48,272,y+48,border,border,border,border);
        Ps_Quad(18,y+2,276,y+2,12,y+46,270,y+46,bgTop,bgTop,bgBot,bgBot);
        Ps_Disc(36,y+24,16,sel?Ps_Rgba(90,250,130,128):Ps_Rgba(20,120,50,120),
                                sel?Ps_Rgba(10,130,50,128):Ps_Rgba(10,45,20,120));
        Rd_Text(28,y+15,"*",.85f,20,sel?1:0);
        Rd_Text(58,y+7,"PRACTICA OFFLINE",1.15f,205,sel?1:0);
        Rd_Text(58,y+29,"Juega en solitario contra los jefes sin red.",.38f,205,0);
    }
    /* Help note */
    y=276.0f;
    Ps_Disc(24,y+7,7,Ps_Rgba(40,140,230,128),Ps_Rgba(15,60,120,128));
    Rd_Text(21,y+2,"?",.45f,15,1);
    Rd_Text(36,y+3,"Sin VPN ni programas externos: invitacion directa.",.40f,240,0);
    /* Pedestal ring under Broly */
    {
        f32 pcx=390.0f,pcy=390.0f;
        Ps_Disc(pcx,pcy,72,Ps_Rgba(255,215,0,70),Ps_Rgba(200,140,0,40));
        Ps_Disc(pcx,pcy,58,Ps_Rgba(255,235,120,90),Ps_Rgba(255,180,0,60));
        Ps_Disc(pcx,pcy,44,Ps_Rgba(255,250,180,110),Ps_Rgba(255,200,20,70));
        Ps_Disc(pcx,pcy,26,Ps_Rgba(255,255,220,130),Ps_Rgba(255,220,50,80));
    }
    /* Footer */
    Ps_Rect(0,416,512,448,Ps_Rgba(3,9,21,124),Ps_Rgba(1,4,10,128));
    Ps_Button(4,20,431,12);Rd_Text(30,423,"Volver",.52f,50,0);
    Ps_Button(0,235,431,12);Rd_Text(245,423,"Seleccionar",.52f,80,1);
    Rd_Text(360,423,"Salas Privadas BT3",.40f,145,0);
}
static void Rd_DrawJoin(void) {
    char buf[64];
    Rd_Text(14,8,"JEFE FINAL - UNIRSE",1.95f,320,1);
    Rd_StepsBanner(1);
    Rd_Frame(10,72,275,258,0);
    Rd_Text(18,76,"CODIGO DE INVITACION",.58f,240,0);
    Ps_Quad(18,100,277,100,14,136,273,136,
            Ps_Rgba(25,45,80,128),Ps_Rgba(15,30,60,128),
            Ps_Rgba(8,18,38,128),Ps_Rgba(12,24,50,128));
    if(R->editing==2) snprintf(buf,sizeof(buf),"%s_",R->editText);
    else if(R->roomCode[0]) sprintf(buf,"BT3 - %s",!strncmp(R->roomCode,"BT3-",4)?R->roomCode+4:R->roomCode);
    else sprintf(buf,"BT3 - ------");
    Rd_Text(26,106,buf,1.20f,230,1);
    Rd_Text(20,144,"Servicio de salas privadas sin VPN.",.38f,255,0);
    Rd_Button(16,166,263,30,"ESCRIBIR CODIGO",R->cursor==0);
    Rd_Button(16,204,263,38,"ENTRAR A LA SALA",R->cursor==1);
    Rd_Button(16,250,263,26,"VOLVER A CONECTAR",R->cursor==2);
    Rd_Text(16,300,"X: escribir  |  Triangulo: pegar  |  Enter: guardar",.40f,265,0);
    if(R->message[0]) {
        Ps_Quad(12,358,283,358,10,380,281,380,
                Ps_Rgba(30,80,160,110),Ps_Rgba(15,45,95,110),
                Ps_Rgba(5,20,45,120),Ps_Rgba(10,30,65,120));
        Rd_Text(16,362,R->message,.40f,260,1);
    }
    {
        f32 pcx=390.0f,pcy=390.0f;
        Ps_Disc(pcx,pcy,72,Ps_Rgba(255,215,0,70),Ps_Rgba(200,140,0,40));
        Ps_Disc(pcx,pcy,58,Ps_Rgba(255,235,120,90),Ps_Rgba(255,180,0,60));
        Ps_Disc(pcx,pcy,44,Ps_Rgba(255,250,180,110),Ps_Rgba(255,200,20,70));
        Ps_Disc(pcx,pcy,26,Ps_Rgba(255,255,220,130),Ps_Rgba(255,220,50,80));
    }
    Ps_Rect(0,416,512,448,Ps_Rgba(3,9,21,124),Ps_Rgba(1,4,10,128));
    Ps_Button(4,20,431,12);Rd_Text(30,423,"Volver",.52f,50,0);
    Ps_Button(3,100,431,12);Rd_Text(110,423,"Pegar codigo",.52f,90,0);
    Ps_Button(0,225,431,12);Rd_Text(235,423,"Seleccionar",.52f,80,1);
    Rd_Text(360,423,"Salas Privadas BT3",.40f,145,0);
}

static void Rd_DrawCustom(void) {
    s32 i;char buf[96];PcBattleRules *r=Rd_Rules();PcFighter *shown=Rd_Shown();
    static char *labels[8]={"Personaje","Traje","Vida","Dano","Nivel de IA","Furia","Armadura","Ki infinito"};
    Rd_Text(14,8,"PERSONALIZAR JEFE",1.95f,320,1);
    for(i=0;i<PORT_RAID_BOSSES;i++) {
        f32 bx=12+i*54.0f;s32 bsel=(i==R->boss);
        Rd_Frame(bx,44,50,24,bsel);
        sprintf(buf,"B%d",i+1);
        Rd_Text(bx+4,48,buf,.45f,18,bsel);
        Rd_Text(bx+20,49,Rd_Name(R->battle.fighter[1][i].chara),.30f,28,0);
    }
    Rd_Frame(10,72,275,22,0);
    sprintf(buf,"Reglas y Atributos - Jefe B%d: %s",R->boss+1,Rd_Name(shown->chara));
    Rd_Text(16,75,buf,.50f,260,1);
    for(i=0;i<8;i++) {
        f32 y=98+i*29.0f;
        Rd_Frame(10,y,275,26,R->cursor==i);
        Rd_Text(18,y+4,labels[i],.58f,110,0);
        if(i==0) Ps_Copy(buf,Rd_Name(shown->chara),96);
        else if(i==1) sprintf(buf,"%d / 8",shown->costume+1);
        else if(i==2) sprintf(buf,"%d PV",r->bossHp);
        else if(i==3) sprintf(buf,"%d %%",r->bossDamagePct);
        else if(i==4) sprintf(buf,"%d / 30",r->cpuLevel1);
        else if(i==5) {if(r->enragePct) sprintf(buf,"Al %d %% de vida",r->enragePct);else Ps_Copy(buf,"Desactivada",96);}
        else if(i==6) Ps_Copy(buf,r->armorSides?"Activada":"Normal",96);
        else Ps_Copy(buf,r->infiniteKiChara1?"Activado":"Desactivado",96);
        Rd_Text(136,y+4,buf,.60f,140,R->cursor==i);
    }
    Rd_Button(10,332,134,26,"GUARDAR PERFIL",R->cursor==8);
    Rd_Button(148,332,137,26,"VOLVER A LA SALA",R->cursor==9);
    Rd_Text(14,366,"L/R: cambiar valor  |  L2/R2: cambiar jefe  |  O: volver",.40f,275,0);
    if(R->message[0]) {
        Ps_Quad(12,388,283,388,10,408,281,408,
                Ps_Rgba(30,80,160,110),Ps_Rgba(15,45,95,110),
                Ps_Rgba(5,20,45,120),Ps_Rgba(10,30,65,120));
        Rd_Text(16,391,R->message,.40f,260,1);
    }
    Ps_Rect(0,416,512,448,Ps_Rgba(3,9,21,124),Ps_Rgba(1,4,10,128));
    Ps_Button(4,20,431,12);Rd_Text(30,423,"Volver a la sala",.52f,110,0);
    Ps_Button(0,160,431,12);Rd_Text(170,423,"Seleccionar / Guardar",.52f,130,1);
    Rd_Text(360,423,"Personalizacion de Jefes BT3",.40f,145,0);
}
static void Rd_CardInvite(void) {
    char buf[48];
    if(R->connection == 0) {
        Rd_Frame(10,68,275,62,R->cursor==0);
        Rd_Text(18,71,"MODO DE JUEGO",.58f,140,0);
        Rd_Text(18,87,"PRACTICA OFFLINE",.95f,250,1);
        Rd_Text(18,115,"Partida individual local con relevos y jefes.",.38f,250,0);
        return;
    }
    if(R->editing==2) snprintf(buf,sizeof(buf),"%s_",R->editText);
    else if(R->roomCode[0]) sprintf(buf,"BT3 - %s",!strncmp(R->roomCode,"BT3-",4)?R->roomCode+4:R->roomCode);
    else sprintf(buf,"BT3 - ------");
    Rd_Frame(10,68,275,62,R->cursor==0);
    Rd_Text(18,71,R->connection==1?"INVITA A TU AMIGO":"SALA CONECTADA",.58f,140,0);
    Rd_Text(18,87,buf,1.25f,140,1);
    Rd_Button(165,88,112,24,"COPIAR CODIGO",R->cursor==0);
    Rd_Text(18,115,R->connection==1?"Tu amigo entra en Unirme a una sala.":"Comparte el codigo con mas amigos.",.38f,250,0);
}
static void Rd_CardBosses(void) {
    s32 i,selected,connectedCount=0;char buf[64];f32 x,y;PcFighter *f;
    for(i=0;i<PORT_COOP_MAX;i++) if(R->players[i].connected) connectedCount++;
    Rd_Frame(10,134,275,114,R->cursor==1);
    Rd_Text(18,137,R->connection==2?"JEFES DEL COMBATE (ANFITRION)":"JEFES DISPONIBLES (ELIGE HASTA 5)",.56f,250,0);
    for(i=0;i<PORT_RAID_BOSSES;i++) {
        x=14+i*53.0f;y=155.0f;f=&R->battle.fighter[1][i];
        selected=(i==R->boss && R->cursor==1 && R->connection!=2);
        Rd_Frame(x,y,49,58,selected);
        if(i<R->battle.count[1]) {
            Port_TeamChip(f->chara,x+3,y+11,43,35,128);
            Rd_Text(x+4,y+45,Rd_Name(f->chara),.32f,41,0);
        } else Rd_Text(x+18,y+20,"+",.9f,20,0);
        Ps_Rect(x+2,y+2,x+16,y+12,Ps_Rgba(225,24,37,128),Ps_Rgba(110,0,12,128));
        sprintf(buf,"B%d",i+1);Rd_Text(x+3,y+2,buf,.44f,20,0);
    }
    if(R->connection!=2) Rd_Button(78,218,138,20,"ELEGIR JEFES",R->cursor==1);
    else Rd_Button(78,218,138,20,"VER JEFES",R->cursor==1);
    sprintf(buf,"%d conectados (Capacidad: 6) | %s",R->connection==0?1:connectedCount,R->difficulty==0?"Facil":R->difficulty==1?"Normal":"Extrema");
    Rd_Text(18,238,buf,.36f,255,0);
}
static void Rd_CardTeam(void) {
    s32 i,connectedCount=0;char buf[64];f32 x,y;
    for(i=0;i<PORT_COOP_MAX;i++) if(R->players[i].connected) connectedCount++;
    sprintf(buf,"EQUIPO (%d CONECTADOS - MAX 6)",R->connection==0?1:connectedCount);
    Rd_Frame(10,252,275,88,R->cursor==2);
    Rd_Text(18,255,buf,.56f,250,0);
    for(i=0;i<PORT_COOP_MAX;i++) {
        x=14+i*44.0f;y=270.0f;
        s32 isMyCard = (R->connection==2 && i==R->mySlot) || (R->connection!=2 && i==0);
        Rd_Frame(x,y,41,64,(R->cursor==2 && isMyCard) || (i==R->mySlot));
        if(R->players[i].connected || (R->connection==0 && i<R->count)) {
            s32 ch = (R->connection==0)?R->battle.fighter[0][i].chara:R->players[i].chara;
            const char *nm = (R->connection==0)?(i==0?R->playerName:"CPU"):R->players[i].name[0]?R->players[i].name:"JUGADOR";
            Port_TeamChip(ch,x+2,y+6,37,34,128);
            Rd_Text(x+2,y+41,(char *)nm,.30f,37,0);
            s32 st = (R->connection==0)?4:R->players[i].state;
            if(st==4) {
                Ps_Rect(x+2,y+51,x+39,y+61,Ps_Rgba(16,128,40,128),Ps_Rgba(8,64,20,128));
                Rd_Text(x+4,y+52,"LISTO",.33f,33,1);
            } else if(st==3) {
                Ps_Rect(x+2,y+51,x+39,y+61,Ps_Rgba(180,140,10,128),Ps_Rgba(90,70,5,128));
                Rd_Text(x+3,y+52,"ELIGE",.33f,33,1);
            } else if(st==2) {
                Ps_Rect(x+2,y+51,x+39,y+61,Ps_Rgba(20,90,180,128),Ps_Rgba(10,45,90,128));
                Rd_Text(x+3,y+52,"MODS",.33f,33,1);
            } else {
                Ps_Rect(x+2,y+51,x+39,y+61,Ps_Rgba(160,120,20,128),Ps_Rgba(80,60,10,128));
                Rd_Text(x+3,y+52,"CONECT",.30f,33,1);
            }
        } else {
            Rd_Text(x+14,y+18,"+",.9f,20,0);
            Rd_Text(x+8,y+44,"Libre",.34f,28,0);
        }
    }
}
static void Rd_CombatButton(void) {
    s32 sel=(R->cursor==3);
    s32 i,connectedCount=0,allReady=1,missingWho=-1;char buf[128];
    for(i=0;i<PORT_COOP_MAX;i++) {
        if(R->players[i].connected) {
            connectedCount++;
            if(R->players[i].state!=4) {
                allReady=0;
                if(missingWho<0) missingWho=i;
            }
        }
    }
    if(R->connection==1) { /* Host */
        if(R->players[0].state!=4) {
            Rd_Text(295,354,"Confirma tu personaje para estar listo.",.42f,210,0);
            Rd_Button(305,368,195,38,"CONFIRMAR LISTO",sel);
        } else if(connectedCount<2) {
            Rd_Text(295,354,"Esperando a un amigo (minimo 2 jugadores).",.42f,210,0);
            Rd_Button(305,368,195,38,"INICIAR COMBATE",0);
        } else if(!allReady) {
            sprintf(buf,"Falta que %s pulse Listo.",(missingWho>=0&&R->players[missingWho].name[0])?R->players[missingWho].name:"AMIGO");
            Rd_Text(295,354,buf,.44f,210,0);
            Rd_Button(305,368,195,38,"INICIAR COMBATE",0);
        } else {
            Rd_Text(295,354,"Todos listos! Pulsa Iniciar combate.",.44f,210,1);
            Rd_Button(305,368,195,38,"INICIAR COMBATE",sel);
        }
    } else if(R->connection==2) { /* Guest */
        if(R->myState==4) {
            Rd_Text(295,354,"Esperando que el anfitrion inicie.",.42f,210,1);
            Rd_Button(305,368,195,38,"ESTAS LISTO (X: Modificar)",sel);
        } else {
            Rd_Text(295,354,"Elige personaje y pulsa Listo.",.42f,210,0);
            Rd_Button(305,368,195,38,"CONFIRMAR LISTO",sel);
        }
    } else { /* Offline */
        Rd_Text(295,354,"Modo practica offline.",.44f,210,0);
        Rd_Button(305,368,195,38,"INICIAR COMBATE",sel);
    }
}

static void Rd_DrawSelector(void) {
    static char *tabs[3]={"TODOS","BASE","EXTRAS"};
    PcFighter *f=Rd_Shown();char name[128],form[128],buf[96];s32 i,start=(R->gridCursor/24)*24,connected=0;
    Port_RaidRosterDetails(name,sizeof(name),form,sizeof(form),f->chara);
    Rd_Text(148,7,R->page==RD_ALLY?"JEFE FINAL - ELEGIR ALIADO":"JEFE FINAL - ELEGIR JEFE",1.1f,355,1);
    Rd_Frame(8,6,132,27,0);Rd_Text(16,11,R->playerName,.62f,114,0);
    Rd_Frame(8,48,158,332,0);Rd_Frame(350,48,154,332,0);
    for(i=0;i<3;i++)Rd_Button(13+i*49,55,48,21,tabs[i],R->gridFilter==i);
    Rd_Frame(13,82,147,24,R->editing==3);
    Rd_Text(19,86,R->editing==3?R->editText:R->search[0]?R->search:"Buscar: Triangulo",.46f,135,R->editing==3);
    for(i=0;i<24 && start+i<R->gridCount;i++) {
        s32 slot=start+i,id=R->grid[slot];f32 x=14+(i%4)*36.0f,y=114+(i/4)*40.0f;
        Rd_Frame(x,y,34,37,slot==R->gridCursor);Port_TeamChip(id,x+2,y+2,29,32,128);
    }
    if(!R->gridCount)Rd_Text(22,194,"Sin coincidencias",.56f,133,1);
    sprintf(buf,"%d personajes | %d / %d",R->gridCount,R->gridCount?R->gridCursor/24+1:0,(R->gridCount+23)/24);
    Rd_Text(15,359,buf,.43f,145,0);Rd_Text(17,372,"L2/R2: pagina",.35f,140,0);
    Rd_Text(182,334,name,.82f,154,0);Rd_Text(181,356,form,.52f,156,1);
    sprintf(buf,"L1 <  TRAJE %d / 8  > R1",f->costume+1);Rd_Button(176,374,165,22,buf,0);
    Rd_Text(362,59,"TU PERSONAJE",.80f,131,1);
    Rd_Text(362,93,"NOMBRE",.43f,132,0);Rd_Text(362,110,name,.59f,130,0);
    Rd_Text(362,142,"FORMA",.43f,130,0);Rd_Text(362,159,form,.58f,130,1);
    sprintf(buf,"Traje instalado: %d / 8",f->costume+1);Rd_Text(362,188,buf,.47f,130,0);
    Rd_Text(362,219,"SALA",.75f,130,1);Rd_Text(362,246,R->playerName,.55f,130,0);
    Rd_Text(362,266,R->page==RD_ALLY?"Eligiendo aliado":"Eligiendo jefe",.48f,130,1);
    Rd_Text(362,291,"Personajes repetidos:",.45f,130,0);Rd_Text(362,309,"Permitidos",.50f,130,1);
    Rd_Button(355,338,144,28,"CONFIRMAR PERSONAJE",1);
    Rd_Text(358,369,"Despues: Listo en la sala",.36f,141,0);
    Ps_Rect(0,400,512,448,Ps_Rgba(2,7,18,128),Ps_Rgba(3,15,36,128));
    for(i=0;i<PORT_COOP_MAX;i++) if(R->players[i].connected)connected++;
    sprintf(buf,"EQUIPO - %d / 6",R->connection==0?1:connected);Rd_Text(10,401,buf,.47f,115,0);
    for(i=0;i<PORT_COOP_MAX;i++) {
        f32 x=10+i*43;Rd_Frame(x,416,41,29,i==R->mySlot);
        if(R->players[i].connected || (R->connection==0 && i<R->count)) {
            Port_TeamChip(R->connection==0?R->battle.fighter[0][i].chara:R->players[i].chara,x+2,418,18,23,128);
            sprintf(buf,"J%d",i+1);Rd_Text(x+23,419,buf,.36f,17,0);
            Rd_Text(x+22,432,R->players[i].state==4?"Listo":"Elige",.28f,18,1);
        }else Rd_Text(x+15,421,"+",.67f,20,0);
    }
    Rd_Text(278,405,"X: confirmar | O: volver",.46f,224,0);
    Rd_Text(278,421,"Cuadrado: filtro | SELECT: plaza",.38f,224,0);
    Rd_Text(278,435,"Stick derecho: girar modelo 3D",.38f,224,0);
}
static void Rd_SelectorPlatform(void) {
    s32 i;for(i=0;i<32;i++) {
        f32 a=i*6.2831853f/32,b=(i+1)*6.2831853f/32;
        u32 gold=Ps_Rgba(255,213,64,125),shade=Ps_Rgba(105,69,15,115);
        Ps_Quad(259+80*cosf(a),324+12*sinf(a),259+80*cosf(b),324+12*sinf(b),
                259+74*cosf(a),324+9*sinf(a),259+74*cosf(b),324+9*sinf(b),gold,gold,shade,shade);
    }
}

static void Rd_Draw(void) {
    s32 i,focus=-1;char buf[96];PcBattleRules *r=Rd_Rules();PcFighter *shown=Rd_Shown();
    {extern void Gfx_AddDefaultEnv(void);Gfx_AddDefaultEnv();}
    Ps_PageFull();Ps_SetShift(0);Ps_Sprite(TBO_RAID_ENERGY,0,0,512,448,0,0,1,1,PS_PLAIN,128);
    if(R->page==RD_ALLY || R->page==RD_BOSS_PICK) {
        for(i=0;i<PORT_RAID_BOSSES;i++)Port_RaidModelsSet(i,i==0?shown->chara:-1,i==0?shown->costume:0);
        Rd_SelectorPlatform();Port_RaidModelsDrawSelector();Ps_PageFull();Rd_DrawSelector();return;
    }
    for(i=0;i<PORT_RAID_BOSSES;i++) {
        if(R->page==RD_ALLY && i==0) continue;
        Port_RaidModelsSet(i,i<R->battle.count[1]?R->battle.fighter[1][i].chara:-1,R->battle.fighter[1][i].costume);
    }
    if(R->page==RD_ALLY) {Port_RaidModelsSet(0,shown->chara,shown->costume);focus=0;}
    else if(R->page==RD_CUSTOM || R->page==RD_BOSS_PICK) focus=R->boss;
    else if(R->page==RD_CONNECT || R->page==RD_JOIN) {Port_RaidModelsSet(0,133,0);focus=0;}
    Port_RaidModelsDraw(R->battle.count[1],focus);Ps_PageFull();
    if(R->page==RD_CONNECT) {
        Rd_DrawConnect();
        if(R->message[0]) {
            Ps_Quad(12,390,283,390,10,410,281,410,
                    Ps_Rgba(30,80,160,110),Ps_Rgba(15,45,95,110),
                    Ps_Rgba(5,20,45,120),Ps_Rgba(10,30,65,120));
            Rd_Text(16,393,R->message,.40f,260,1);
        }
        return;
    }
    if(R->page==RD_JOIN) {
        Rd_DrawJoin();
        return;
    }
    if(R->page==RD_CUSTOM) {
        Rd_DrawCustom();
        return;
    }
    snprintf(buf,sizeof(buf),"JEFE FINAL - SALA DE %.19s",R->connection==2 && R->players[0].name[0]?R->players[0].name:R->playerName);
    Rd_Text(13,1,buf,1.95f,290,0);
    if(R->page==RD_LOBBY || R->page==RD_ORDER) {
        Rd_StepsBanner(2);
        Rd_CardInvite();
        Rd_CardBosses();
        Rd_CardTeam();
        Rd_CombatButton();
        if(R->message[0]) {
            Ps_Quad(12,344,283,344,10,366,281,366,
                    Ps_Rgba(30,80,160,110),Ps_Rgba(15,45,95,110),
                    Ps_Rgba(5,20,45,120),Ps_Rgba(10,30,65,120));
            Rd_Text(16,348,R->message,.40f,260,1);
        }
        Ps_Rect(0,416,512,448,Ps_Rgba(3,9,21,124),Ps_Rgba(1,4,10,128));
        Rd_Text(14,423,"Mods: comprobacion completada",.44f,180,1);
        Ps_Button(2,235,431,12);Rd_Text(245,423,"Cambiar personaje",.48f,95,0);
        Ps_Button(0,355,431,12);Rd_Text(365,423,"Listo",.48f,35,0);
        Ps_Button(4,425,431,12);Rd_Text(435,423,"Salir de la sala",.48f,75,0);
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
        char code[32];
        Ip_ToCode(R->ip,code,sizeof(code));
        Rd_Text(16,69,"CONEXION ONLINE DIRECTA",.97f,265,0);
        sprintf(buf,"Codigo de sala: %s",code);
        Rd_Text(18,92,buf,.70f,265,1);
        for(i=0;i<6;i++) {f32 y=116+i*36;Rd_Frame(12,y,272,32,R->cursor==i);
            if(!i) {Ps_Copy(buf,R->connection==0?"Practica (Offline)":R->connection==1?"Crear sala (Host)":"Unirse por IP (Invitado)",96);}
            else sprintf(buf,"%d",R->ip[i-1]);
            Rd_Text(22,y+6,i==0?"Modo":i==5?"Puerto UDP":"Direccion IPv4",.66f,121,0);Rd_Text(150,y+6,buf,.66f,123,1);
        }
        Rd_Text(18,354,"Triangulo: Pegar IP/Codigo  |  Cuadrado: Copiar",.49f,266,1);
        Rd_Text(18,377,"L/R: ajustar numero  |  X: confirmar sala",.46f,266,0);
    }
}
static s32 Rd_Check(void) {
    char why[160];R->battle.players=R->connection==0?1:R->count;R->battle.count[0]=R->count;
    if(!Port_PcBattleValidate(&R->battle,why,sizeof(why))) {Ps_Copy(R->message,"Revisa archivos: ",sizeof(R->message));Ps_Cat(R->message,why,sizeof(R->message));return 0;}
    Ps_Copy(R->message,"Archivos locales listos. La red comprobara los mods de cada PC.",sizeof(R->message));return 1;
}

static void Rd_EditBegin(s32 kind) {
    R->editing=kind;Ps_Copy(R->editText,kind==1?R->playerName:kind==3?R->search:R->roomCode,sizeof(R->editText));
    PortTbo_TextMode(1);
    Ps_Copy(R->message,kind==1?"Escribe tu apodo. Enter: guardar | Esc: cancelar":kind==3?"Buscar por nombre o forma. Enter: filtrar | Esc: cancelar":"Escribe el codigo. Enter: confirmar | Ctrl+V: pegar",sizeof(R->message));
}
static void Rd_EditEnd(s32 save) {
    s32 n;
    if(save) {
        n=(s32)strlen(R->editText);while(n>0 && R->editText[n-1]==' ') R->editText[--n]=0;
        if(!n && R->editing!=3) {Ps_Copy(R->message,"El campo no puede quedar vacio.",sizeof(R->message));return;}
        if(R->editing==1) {
            Ps_Copy(R->playerName,R->editText,sizeof(R->playerName));
            Ps_Copy(R->players[0].name,R->playerName,sizeof(R->players[0].name));
            Port_SettingTextSave("raid_name",R->playerName);Port_SettingsWrite();
            Ps_Copy(R->message,"Apodo guardado en esta PC.",sizeof(R->message));
        } else if(R->editing==3) {
            Ps_Copy(R->search,R->editText,sizeof(R->search));Rd_GridBuild();Rd_GridChoose();
            Ps_Copy(R->message,"Filtro actualizado.",sizeof(R->message));
        } else {
            Ps_Copy(R->roomCode,R->editText,sizeof(R->roomCode));R->cursor=1;
            Ps_Copy(R->message,"Codigo escrito. Pulsa Entrar a la sala.",sizeof(R->message));
        }
    } else Ps_Copy(R->message,"Edicion cancelada.",sizeof(R->message));
    R->editing=0;PortTbo_TextMode(0);
}
static void Rd_EditInput(u32 p) {
    s32 c,n;while(R->editing && (c=PortTbo_TextRead())!=0) {
        n=(s32)strlen(R->editText);
        if(c==27) Rd_EditEnd(0);
        else if(c==13) Rd_EditEnd(1);
        else if(c==8) {if(n) R->editText[n-1]=0;}
        else if(c>=32 && c<127 && n<(R->editing==1?19:R->editing==3?31:15)) {R->editText[n]=(char)c;R->editText[n+1]=0;}
    }
    if(R->editing && (p&PADG_CIRCLE)) Rd_EditEnd(0);
    else if(R->editing && (p&PADG_CROSS)) Rd_EditEnd(1);
}
static void Rd_ClipboardTick(void) {
    char clip[256],code[16];const char *cp;s32 n=0,a,b,c,d,port=0;
    if(!PortRaid_ClipboardRead(clip,sizeof(clip))) return;
    if(R->page!=RD_JOIN && R->page!=RD_NETWORK) return;
    if(R->page==RD_NETWORK && sscanf(clip,"%d.%d.%d.%d:%d",&a,&b,&c,&d,&port)>=4 &&
       a>=0&&a<256&&b>=0&&b<256&&c>=0&&c<256&&d>=0&&d<256) {
        R->ip[0]=a;R->ip[1]=b;R->ip[2]=c;R->ip[3]=d;
        if(port>0&&port<65536) R->ip[4]=port;
        Ps_Copy(R->message,"Direccion pegada.",sizeof(R->message));return;
    }
    cp=clip;while(*cp==' '||*cp=='\t') cp++;
    if(!strncmp(cp,"BT3-",4)||!strncmp(cp,"bt3-",4)) cp+=4;
    while(*cp && n<6) {char ch=*cp++;if(ch==' '||ch=='-') continue;if(ch>='a'&&ch<='z') ch-=32;code[n++]=ch;}
    code[n]=0;
    if(n!=6) {Ps_Copy(R->message,"El portapapeles no contiene un codigo de seis caracteres.",sizeof(R->message));return;}
    snprintf(R->roomCode,sizeof(R->roomCode),"BT3-%.6s",code);R->cursor=1;
    Ps_Copy(R->message,"Codigo pegado. Pulsa Entrar a la sala.",sizeof(R->message));
}
static void Rd_HostResetReady(void) {
    s32 i;
    if(R->connection != 1) return;
    for(i = 0; i < PORT_COOP_MAX; i++) {
        if(R->players[i].connected && R->players[i].state == 4) {
            R->players[i].state = 3;
        }
    }
}

static s32 Rd_ConnectRequest(s32 host,const char *code) {
    if(R->connecting) return 0;
    if(!host && (!code || !code[0])) {Ps_Copy(R->message,"Escribe o pega el codigo de tu amigo.",sizeof(R->message));return 0;}
    if(R->lobbySock>=0) {RoomRelay_Close(R->lobbySock);RoomRelay_CloseSocket(R->lobbySock);R->lobbySock=-1;}
    R->connecting=1;R->connectHost=host;R->connectBegan=RoomRelay_NowMs();
    if(!host && code!=R->roomCode) Ps_Copy(R->roomCode,code,sizeof(R->roomCode));
    Port_VerifyStart();
    Ps_Copy(R->message,"Comprobando datos... Circulo: cancelar",sizeof(R->message));return 1;
}
static s32 Rd_CreateRoom(void) {return Rd_ConnectRequest(1,NULL);}
static s32 Rd_JoinRoom(const char *code) {return Rd_ConnectRequest(0,code);}
static void Rd_ConnectTick(void) {
    s32 status,sock=-1;unsigned long long base=0;
    if(!R->connecting) return;
    if(R->connecting==1) {
        if(Port_DataFingerprint(&base)!=1) {
            if(RoomRelay_NowMs()-R->connectBegan>30000) {R->connecting=0;Ps_Copy(R->message,"No se pudo comprobar los datos. Reintenta.",sizeof(R->message));}
            return;
        }
        R->modFingerprint=base ^ Port_CharacterFingerprint() ^ ((u64)PORT_TOTAL_CHARAS<<32);
        if(!RoomRelay_ConnectStart(R->connectHost,R->roomCode,R->modFingerprint,2,PORT_COOP_MAX,R->playerName)) {
            R->connecting=0;Ps_Copy(R->message,"No se pudo iniciar la conexion.",sizeof(R->message));return;
        }
        R->connecting=2;Ps_Copy(R->message,"Conectando con el servidor... Circulo: cancelar",sizeof(R->message));
    }
    status=RoomRelay_ConnectPoll(&sock);if(!status) return;
    R->connecting=0;
    if(status!=1) {snprintf(R->message,sizeof(R->message),"%s",status==-2?"Conexion cancelada.":RoomRelay_Error());return;}
    R->lobbySock=sock;Ps_Copy(R->roomCode,RoomRelay_Code(),sizeof(R->roomCode));
    printf("bt3: raid: lobby %s confirmed, code %s, slot %d\n",R->connectHost?"created":"joined",R->roomCode,RoomRelay_Slot());
    R->connection=R->connectHost?1:2;R->mySlot=RoomRelay_Slot();R->myState=3;R->count=PORT_COOP_MAX;
    memset(R->players,0,sizeof(R->players));R->players[R->mySlot].connected=1;R->players[R->mySlot].state=3;
    R->players[R->mySlot].chara=(s16)R->battle.fighter[0][R->mySlot].chara;
    R->players[R->mySlot].costume=(s8)R->battle.fighter[0][R->mySlot].costume;
    Ps_Copy(R->players[R->mySlot].name,R->playerName,sizeof(R->players[R->mySlot].name));
    R->players[R->mySlot].lastSeen=RoomRelay_NowMs();R->page=RD_LOBBY;R->cursor=R->connectHost?0:2;
    if(R->connectHost) {PortTbo_Copy(R->roomCode);sprintf(R->message,"Sala creada. Codigo %s copiado.",R->roomCode);}
    else sprintf(R->message,"Conectado a %s como J%d.",R->roomCode,R->mySlot+1);
}

static void Rd_LobbyClose(void) {
    if(R->lobbySock >= 0) {
        if(RoomRelay_Active()) RoomRelay_Close(R->lobbySock);
        RoomRelay_CloseSocket(R->lobbySock);
        R->lobbySock = -1;
    }
    R->connection = 1;
    R->roomCode[0] = 0;
    R->mySlot = 0;
    R->myState = 3;
}

static void Rd_FillSync(LobbySyncPacket *sync,s32 launching) {
    s32 i;
            memset(sync, 0, sizeof(*sync));
            sync->magic = LOBBY_PKT_SYNC;
            sync->version = 1;
            sync->stage = (u32)R->battle.stage;
            sync->bossCount = (u32)R->battle.count[1];
            sync->bossHp = R->battle.rules.bossHp;
            sync->bossDamagePct = R->battle.rules.bossDamagePct;
            sync->cpuLevel1 = R->battle.rules.cpuLevel1;
            sync->enragePct = R->battle.rules.enragePct;
            sync->armorSides = R->battle.rules.armorSides;
            sync->infiniteKi = R->battle.rules.infiniteKiChara1;
            sync->difficulty = R->difficulty;
            sync->playerCount = PORT_COOP_MAX;
            for(i = 0; i < PORT_RAID_BOSSES; i++) {
                sync->bossChara[i] = R->battle.fighter[1][i].chara;
                sync->bossCostume[i] = R->battle.fighter[1][i].costume;
            }
            for(i = 0; i < PORT_COOP_MAX; i++) {
                sync->players[i].connected = R->players[i].connected;
                sync->players[i].state = R->players[i].state;
                sync->players[i].chara = R->players[i].chara;
                sync->players[i].costume = R->players[i].costume;
                strncpy(sync->players[i].name, R->players[i].name, 19);
            }
            sync->launching = (u8)launching;
}

static void __attribute__((unused)) Rd_LobbyPoll(void) {
    u8 buf[2048];
    s32 senderSlot = -1;
    s32 i;
    u64 now = RoomRelay_NowMs();
    if(R->lobbySock < 0 || !RoomRelay_Active()) return;
    for(;;) {
        s32 len = RoomRelay_RecvSlot(R->lobbySock, buf, &senderSlot);
        if(len <= 0) break;
        if(R->connection == 1) {
            if(len == sizeof(LobbyUpdatePacket)) {
                LobbyUpdatePacket *pkt = (LobbyUpdatePacket *)buf;
                if(pkt->magic == LOBBY_PKT_UPDATE && senderSlot >= 1 && senderSlot < PORT_COOP_MAX) {
                    R->players[senderSlot].connected = 1;
                    R->players[senderSlot].state = pkt->state;
                    R->players[senderSlot].chara = pkt->chara;
                    R->players[senderSlot].costume = pkt->costume;
                    strncpy(R->players[senderSlot].name, pkt->name, 19);
                    R->players[senderSlot].lastSeen = now;
                    R->battle.fighter[0][senderSlot].chara = pkt->chara;
                    R->battle.fighter[0][senderSlot].costume = pkt->costume;
                }
            }
        } else if(R->connection == 2) {
            if(len == sizeof(LobbySyncPacket)) {
                LobbySyncPacket *pkt = (LobbySyncPacket *)buf;
                if(pkt->magic == LOBBY_PKT_SYNC && senderSlot == 0) {
                    s32 rulesChanged = (R->battle.stage != (s32)pkt->stage ||
                                        R->battle.count[1] != (s32)pkt->bossCount ||
                                        R->difficulty != pkt->difficulty ||
                                        R->battle.rules.bossHp != pkt->bossHp ||
                                        R->battle.rules.bossDamagePct != pkt->bossDamagePct);
                    if(rulesChanged && R->myState == 4) {
                        R->myState = 3;
                        R->players[R->mySlot].state = 3;
                        Ps_Copy(R->message, "El anfitrion modifico las reglas. Confirma Listo.", sizeof(R->message));
                    }
                    R->battle.stage = pkt->stage;
                    R->battle.count[1] = pkt->bossCount;
                    R->difficulty = pkt->difficulty;
                    R->battle.rules.bossHp = pkt->bossHp;
                    R->battle.rules.bossDamagePct = pkt->bossDamagePct;
                    R->battle.rules.cpuLevel1 = pkt->cpuLevel1;
                    R->battle.rules.enragePct = pkt->enragePct;
                    R->battle.rules.armorSides = pkt->armorSides;
                    R->battle.rules.infiniteKiChara1 = pkt->infiniteKi;
                    for(i = 0; i < PORT_RAID_BOSSES; i++) {
                        R->battle.fighter[1][i].chara = pkt->bossChara[i];
                        R->battle.fighter[1][i].costume = pkt->bossCostume[i];
                    }
                    if(gTeamSel->stage && gTeamSel->stage->stage != R->battle.stage) {
                        gTeamSel->stage->stage = R->battle.stage;
                        TeamSel_RequestStage();
                    }
                    for(i = 0; i < PORT_COOP_MAX; i++) {
                        if(i != R->mySlot) {
                            R->players[i].connected = pkt->players[i].connected;
                            R->players[i].state = pkt->players[i].state;
                            R->players[i].chara = pkt->players[i].chara;
                            R->players[i].costume = pkt->players[i].costume;
                            strncpy(R->players[i].name, pkt->players[i].name, 19);
                            R->battle.fighter[0][i].chara = pkt->players[i].chara;
                            R->battle.fighter[0][i].costume = pkt->players[i].costume;
                        }
                    }
                    if(pkt->launching && !R->launch) {
                        s32 activeCount = 0;
                        for(i = 0; i < PORT_COOP_MAX; i++) if(R->players[i].connected) activeCount++;
                        R->count = activeCount >= 2 ? activeCount : 2;
                        R->battle.players = R->count;
                        R->battle.count[0] = R->count;
                        R->launch = 1;
                        R->leaving = 1;
                        ColorFade_StartOut(0, 0, 0, 20);
                    }
                }
            }
        }
    }
    if(R->connection == 1) {
        for(i = 1; i < PORT_COOP_MAX; i++) {
            if(R->players[i].connected && (now - R->players[i].lastSeen > 8000)) {
                R->players[i].connected = 0;
                R->players[i].state = 0;
                snprintf(R->message, sizeof(R->message), "%s se ha desconectado.", R->players[i].name[0] ? R->players[i].name : "Un jugador");
            }
        }
    }
    if(now - R->lastStateSend >= 120) {
        R->lastStateSend = now;
        if(R->connection == 1) {
            LobbySyncPacket sync;
            Rd_FillSync(&sync,0);
            for(i = 1; i < PORT_COOP_MAX; i++) {
                if(R->players[i].connected) {
                    RoomRelay_SendSlot(R->lobbySock, i, &sync, sizeof(sync));
                }
            }
        } else if(R->connection == 2) {
            LobbyUpdatePacket upd;
            memset(&upd, 0, sizeof(upd));
            upd.magic = LOBBY_PKT_UPDATE;
            upd.version = 1;
            upd.slot = (u8)R->mySlot;
            upd.state = (u8)R->myState;
            upd.chara = (s16)R->players[R->mySlot].chara;
            upd.costume = (s8)R->players[R->mySlot].costume;
            strncpy(upd.name, R->playerName, 19);
            RoomRelay_SendSlot(R->lobbySock, 0, &upd, sizeof(upd));
        }
    }
}

static void Rd_Input(u32 p,u32 repeat) {
    s32 dir=(repeat&(PADG_RIGHT|PADG_RIGHT2))?1:(repeat&(PADG_LEFT|PADG_LEFT2))?-1:0;
    s32 up=repeat&(PADG_UP|PADG_UP2),down=repeat&(PADG_DOWN|PADG_DOWN2),i,save;
    PcBattleRules *r=Rd_Rules();
    if(R->editing) {Rd_EditInput(p);return;}
    if(R->connecting) {
        if(p&PADG_CIRCLE) {
            if(R->connecting==1) {R->connecting=0;Ps_Copy(R->message,"Conexion cancelada.",sizeof(R->message));}
            else {RoomRelay_ConnectCancel();Ps_Copy(R->message,"Cancelando conexion...",sizeof(R->message));}
        }return;
    }
    if(p&PADG_CIRCLE) {
        if(R->page==RD_LOBBY) {Rd_LobbyClose();R->page=RD_CONNECT;R->cursor=1;Ps_Copy(R->message,"Has salido de la sala.",sizeof(R->message));}
        else if(R->page==RD_JOIN) {R->page=RD_CONNECT;R->cursor=2;}
        else if(R->page!=RD_CONNECT) {R->page=RD_LOBBY;R->cursor=0;}
        else {R->leaving=1;ColorFade_StartOut(0,0,0,20);}
        return;
    }
    if(R->page==RD_CONNECT) {
        if(up||down) R->cursor=Rd_Wrap(R->cursor+(down?1:-1),4);
        if(p&PADG_SQUARE) {
            if(R->roomCode[0]) {
                PortTbo_Copy(R->roomCode);
                sprintf(R->message,"Codigo %s copiado al portapapeles",R->roomCode);
            }
            return;
        }
        if(p&PADG_CROSS) {
            if(R->cursor==0) {
                Rd_EditBegin(1);
                return;
            }
            if(R->cursor==1) {
                Rd_CreateRoom();
                return;
            }
            if(R->cursor==2) {
                R->page=RD_JOIN;R->cursor=0;
                PortRaid_ClipboardRequest();
                Ps_Copy(R->message,"Escribe o pega el codigo de tu amigo.",sizeof(R->message));
                return;
            }
            if(R->cursor==3) {
                R->connection=0;R->page=RD_LOBBY;R->cursor=3;R->mySlot=0;R->myState=4;
                Ps_Copy(R->message,"Modo practica offline. Pulsa Iniciar combate.",sizeof(R->message));
                return;
            }
        }
        return;
    }
    if(R->page==RD_JOIN) {
        if(up||down) R->cursor=Rd_Wrap(R->cursor+(down?1:-1),3);
        if(p&PADG_TRIANGLE) {PortRaid_ClipboardRequest();return;}
        if(R->cursor==0 && (p&PADG_CROSS)) {Rd_EditBegin(2);return;}
        if((p&PADG_START) || (R->cursor==1 && (p&PADG_CROSS))) {
            if(!R->roomCode[0]) {
                Ps_Copy(R->message,"Pega primero el codigo de invitacion.",sizeof(R->message));
                R->cursor=0;
                return;
            }
            Rd_JoinRoom(R->roomCode);
            return;
        }
        if(R->cursor==2 && (p&PADG_CROSS)) {
            R->page=RD_CONNECT;R->cursor=2;return;
        }
        return;
    }
    if(R->page==RD_ALLY || R->page==RD_BOSS_PICK) {
        s32 step=dir+(down?4:up?-4:0);
        if(p&PADG_TRIANGLE) {Rd_EditBegin(3);return;}
        if(p&PADG_SQUARE) {R->gridFilter=Rd_Wrap(R->gridFilter+1,3);Rd_GridBuild();Rd_GridChoose();}
        if(p&PADG_L2)step=-24;
        if(p&PADG_R2)step=24;
        if(step && R->gridCount) {R->gridCursor=Rd_Wrap(R->gridCursor+step,R->gridCount);Rd_GridChoose();}
        if(p&PADG_L1)Rd_Costume(Rd_Shown(),-1);
        if(p&PADG_R1)Rd_Costume(Rd_Shown(),1);
        if(repeat&PADG_RSTICK_LEFT)Port_RaidModelsTurn(-.08f);
        if(repeat&PADG_RSTICK_RIGHT)Port_RaidModelsTurn(.08f);
        if(p&PADG_SELECT) {
            if(R->page==RD_ALLY && R->connection==0)R->ally=Rd_Wrap(R->ally+1,R->count);
            else if(R->page==RD_BOSS_PICK)R->boss=Rd_Wrap(R->boss+1,R->battle.count[1]);
            Rd_GridBuild();
        }
        if(R->page==RD_ALLY) {
            R->players[R->ally].chara=(s16)Rd_Shown()->chara;R->players[R->ally].costume=(s8)Rd_Shown()->costume;
            if(step || (p&(PADG_L1|PADG_R1|PADG_SQUARE))) {R->players[R->ally].state=3;R->myState=3;}
        } else if(step || (p&(PADG_L1|PADG_R1|PADG_SQUARE)))Rd_HostResetReady();
        if(p&PADG_CROSS) {R->page=RD_LOBBY;R->cursor=2;}
        return;
    }
    if((p&PADG_SQUARE) && R->page==RD_LOBBY) {
        R->page=RD_ALLY;
        R->ally=(R->connection==2)?R->mySlot:0;Rd_GridBuild();
        return;
    }
    if(p&PADG_SELECT) {R->page=R->page==RD_NETWORK?RD_LOBBY:RD_NETWORK;R->cursor=0;return;}
    if(p&PADG_TRIANGLE) {
        if(R->connection!=2) {
            R->page=R->page==RD_CUSTOM?RD_LOBBY:RD_CUSTOM;R->cursor=0;return;
        } else {
            Ps_Copy(R->message,"Solo el anfitrion puede personalizar las reglas.",sizeof(R->message));
            return;
        }
    }
    if(p&PADG_L2) { if(R->connection!=2) { R->boss=Rd_Wrap(R->boss-1,R->battle.count[1]); Rd_HostResetReady(); } }
    if(p&PADG_R2) { if(R->connection!=2) { R->boss=Rd_Wrap(R->boss+1,R->battle.count[1]); Rd_HostResetReady(); } }
    if(p&PADG_L1) { if(R->connection!=2 || R->page==RD_ALLY) Rd_Costume(Rd_Shown(),-1); if(R->page!=RD_ALLY) Rd_HostResetReady(); }
    if(p&PADG_R1) { if(R->connection!=2 || R->page==RD_ALLY) Rd_Costume(Rd_Shown(),1); if(R->page!=RD_ALLY) Rd_HostResetReady(); }
    r=Rd_Rules();
    if(R->page==RD_ALLY || R->page==RD_BOSS_PICK) {
        if(up||down) {
            if(R->page==RD_ALLY) R->ally=Rd_Wrap(R->ally+(down?1:-1),R->connection==0?R->count:1);
            else R->boss=Rd_Wrap(R->boss+(down?1:-1),R->battle.count[1]);
        }
        if(dir) {
            Rd_Character(Rd_Shown(),dir);
            if(R->page==RD_ALLY) {
                R->players[R->ally].chara=(s16)Rd_Shown()->chara;
                if(R->connection==2) {
                    R->myState=3;R->players[R->mySlot].state=3;
                }
            } else {
                Rd_HostResetReady();
            }
        }
        if(p&PADG_CROSS) {
            if(R->page==RD_ALLY && R->connection==2) {
                R->players[R->mySlot].chara=(s16)Rd_Shown()->chara;
                R->players[R->mySlot].costume=(s8)Rd_Shown()->costume;
                R->myState=3;R->players[R->mySlot].state=3;
            }
            R->page=RD_LOBBY;R->cursor=2;
        }
        return;
    }
    if(R->page==RD_ORDER) {
        if(dir) {
            PcFighter fighter;PcBattleRules rules,*a,*b;s32 other=Rd_Wrap(R->boss+dir,R->battle.count[1]);
            a=Rd_Rules();b=other?&R->battle.bossRules[other-1]:&R->battle.rules;
            fighter=*Rd_Boss();*Rd_Boss()=R->battle.fighter[1][other];R->battle.fighter[1][other]=fighter;
            rules=*a;*a=*b;*b=rules;R->battle.rules.bgm1=14;
            for(i=0;i<PORT_RAID_BOSSES-1;i++) { R->battle.bossRules[i].bgm1=0; }
            R->boss=other;
            Rd_HostResetReady();
        }if(p&PADG_CROSS) {R->page=RD_LOBBY;R->cursor=2;}return;
    }
    if((p&PADG_START) && R->page==RD_LOBBY) {p|=PADG_CROSS;R->cursor=3;}
    if(up||down) R->cursor=Rd_Wrap(R->cursor+(down?1:-1),R->page==RD_NETWORK?6:R->page==RD_CUSTOM?9:4);
    if(R->page==RD_NETWORK) {
        if(p&PADG_TRIANGLE) {PortRaid_ClipboardRequest();return;}
        if((p&PADG_SQUARE) && !dir) {
            char code[32];Ip_ToCode(R->ip,code,sizeof(code));PortTbo_Copy(code);
            Ps_Copy(R->message,"Codigo de sala copiado al portapapeles",sizeof(R->message));
        }
        if(dir) {if(!R->cursor) R->connection=Rd_Wrap(R->connection+(dir>0?1:-1),3);
            else {i=R->cursor-1;R->ip[i]=i==4?Rd_Clamp(R->ip[i]+dir,1024,65535):Rd_Wrap(R->ip[i]+dir,256);}}
        if(p&PADG_CROSS) {R->page=RD_LOBBY;R->cursor=0;}return;
    }
    if(R->page==RD_CUSTOM) {
        if(up||down) R->cursor=Rd_Wrap(R->cursor+(down?1:-1),10);
        if(dir && R->cursor<8) {
            switch(R->cursor) {
            case 0:Rd_Character(Rd_Boss(),dir);break;case 1:Rd_Costume(Rd_Boss(),dir);break;
            case 2:r->bossHp=Rd_Clamp(r->bossHp+dir*10000,10000,250000);break;case 3:r->bossDamagePct=Rd_Clamp(r->bossDamagePct+dir*25,50,400);break;
            case 4:r->cpuLevel1=Rd_Clamp(r->cpuLevel1+dir,1,30);break;case 5:r->enragePct=Rd_Clamp(r->enragePct+dir*10,0,90);break;
            case 6:r->armorSides=r->armorSides?0:2;break;case 7:r->infiniteKiChara1=r->infiniteKiChara1?0:Rd_Boss()->chara+1;break;
            }
            Rd_HostResetReady();
        }
        if(p&PADG_CROSS) {
            if(R->cursor==8) {Rd_ProfileSave();R->page=RD_LOBBY;R->cursor=1;}
            else if(R->cursor==9) {R->page=RD_LOBBY;R->cursor=1;}
        }
        return;
    }
    if(dir) switch(R->cursor) {
    case 0:break;
    case 1:
        if(R->connection!=2) {
            R->battle.count[1]=Rd_Clamp(R->battle.count[1]+dir,1,PORT_RAID_BOSSES);
            R->boss=Rd_Clamp(R->boss,0,R->battle.count[1]-1);
            Rd_HostResetReady();
        }
        break;
    case 2:R->ally=Rd_Wrap(R->ally+dir,R->count);break;
    case 3:R->connection=Rd_Wrap(R->connection+dir,3);break;
    case 4:R->count=Rd_Clamp(R->count+dir,2,PORT_COOP_MAX);break;
    case 5:R->battle.count[1]=Rd_Clamp(R->battle.count[1]+dir,1,PORT_RAID_BOSSES);R->boss=Rd_Clamp(R->boss,0,R->battle.count[1]-1);break;
    case 6:
        if(R->connection!=2) {
            R->difficulty=Rd_Wrap(R->difficulty+dir,3);save=R->boss;
            for(i=0;i<PORT_RAID_BOSSES;i++){R->boss=i;Rd_Preset();}
            R->boss=save;Rd_HostResetReady();
        }
        break;
    case 7:
        if(R->connection!=2) {
            R->battle.stage=Rd_Wrap(R->battle.stage+dir,35);gTeamSel->stage->stage=R->battle.stage;TeamSel_RequestStage();Rd_HostResetReady();
        }
        break;
    }
    if(p&PADG_CROSS) switch(R->cursor) {
    case 0:
        if(R->roomCode[0]) {
            PortTbo_Copy(R->roomCode);
            Ps_Copy(R->message,"Codigo copiado al portapapeles!",sizeof(R->message));
        }
        break;
    case 1:
        if(R->connection!=2) {R->page=RD_BOSS_PICK;Rd_GridBuild();}
        else Ps_Copy(R->message,"Solo el anfitrion puede elegir los jefes.",sizeof(R->message));
        break;
    case 2:
        R->page=RD_ALLY;
        R->ally=(R->connection==2)?R->mySlot:0;Rd_GridBuild();
        break;
    case 3:
        if(R->connection==1) { /* Host */
            s32 conn=0,ready=1,missingWho=-1;
            if(R->players[0].state!=4) {R->players[0].state=R->myState=4;Ps_Copy(R->message,"Estas listo. Cuando todos confirmen, pulsa Iniciar.",sizeof(R->message));break;}
            for(i=0;i<PORT_COOP_MAX;i++) {
                if(R->players[i].connected) {
                    conn++;
                    if(R->players[i].state!=4) { ready=0; if(missingWho<0) missingWho=i; }
                }
            }
            if(conn<2) {
                Ps_Copy(R->message,"Esperando a un amigo (minimo 2 jugadores para iniciar).",sizeof(R->message));
            } else if(!ready) {
                sprintf(R->message,"Falta que %s pulse Listo.",(missingWho>=0&&R->players[missingWho].name[0])?R->players[missingWho].name:"un amigo");
            } else if(Rd_Check()) {
                LobbySyncPacket launchSync;
                Rd_FillSync(&launchSync,1);
                for(i=1;i<PORT_COOP_MAX;i++) {
                    if(R->players[i].connected) {
                        for(s32 rep=0;rep<5;rep++) RoomRelay_SendSlot(R->lobbySock,i,&launchSync,sizeof(launchSync));
                    }
                }
                R->count=conn;R->battle.players=conn;R->battle.count[0]=conn;
                R->leaving=R->launch=1;ColorFade_StartOut(0,0,0,20);
            }
        } else if(R->connection==2) { /* Guest */
            if(R->myState!=4) {
                R->myState=4;R->players[R->mySlot].state=4;
                Ps_Copy(R->message,"Listo! Esperando que el anfitrion inicie.",sizeof(R->message));
            } else {
                R->myState=3;R->players[R->mySlot].state=3;
                Ps_Copy(R->message,"Estado cambiado a Eligiendo.",sizeof(R->message));
            }
            if(R->lobbySock>=0 && RoomRelay_Active()) {
                LobbyUpdatePacket upd;
                memset(&upd,0,sizeof(upd));upd.magic=LOBBY_PKT_UPDATE;upd.version=1;
                upd.slot=(u8)R->mySlot;upd.state=(u8)R->myState;
                upd.chara=(s16)R->players[R->mySlot].chara;upd.costume=(s8)R->players[R->mySlot].costume;
                strncpy(upd.name,R->playerName,19);
                RoomRelay_SendSlot(R->lobbySock,0,&upd,sizeof(upd));
            }
        } else { /* Offline Practice */
            if(Rd_Check()){R->leaving=R->launch=1;ColorFade_StartOut(0,0,0,20);}
        }
        break;
    case 7:
        if(R->connection!=2) {
            R->battle.stage=Rd_Wrap(R->battle.stage+1,35);gTeamSel->stage->stage=R->battle.stage;TeamSel_RequestStage();Rd_HostResetReady();
        }
        break;
    case 8:Rd_Check();break;
    }

}

s32 PortRaid_Run(void) {
    TsTeam saved[2];s32 savedPlayers,savedType,savedDp,savedStage,savedBgm,s,i,result=0;
    saved[0]=gTsProgress->team[0];saved[1]=gTsProgress->team[1];savedPlayers=gTsProgress->players;savedType=gTsProgress->battleType;
    savedDp=gTsProgress->dpLevel;savedStage=gTsProgress->stageCell;savedBgm=gTsProgress->bgm;
    memset(gTsProgress->team,0,sizeof(gTsProgress->team));gTsProgress->players=0;gTsProgress->battleType=1;gTsProgress->dpLevel=gTsProgress->stageCell=gTsProgress->bgm=0;
    for(s=0;s<2;s++) for(i=0;i<5;i++) gTsProgress->team[s].member[i].chara=i?-1:s?4:0;
    if(!gMenuArc5) gMenuArc5=File_LoadSync(gProgress->baseFile,NULL,0);
    Rd_Init();R->page=RD_CONNECT;R->cursor=1;TeamSel_Init(2);Port_RaidModelsBegin();sRaidRoomOn=1;gTeamSel->stage->stage=R->battle.stage;TeamSel_RequestStage();Bgm_Play(0x10B18);ColorFade_StartIn(0,0,0,20);
    while(1) {
        u32 p,r;Gfx_BeginFrame();Pad_Update();Snd_Update();ColorFade_Update();TeamSel_UpdateFaceLoad();TeamSel_UpdateStageLoad();
        p=gPad[0].gamePressed;r=gPad[0].gameRepeat|p;R->frames++;
        if(Port_RosterTestMode()==11) {R->connection=0;p|=Port_RosterTestInput(R->frames);r|=p;}
        if(R->lobbySock>=0 && (R->page==RD_LOBBY||R->page==RD_ALLY||R->page==RD_CUSTOM||R->page==RD_BOSS_PICK||R->page==RD_ORDER)) {
            Rd_LobbyPoll();
        }
        Rd_ClipboardTick();Rd_ConnectTick();
        if(!R->leaving&&ColorFade_IsInDone()) Rd_Input(p,r);
        Rd_Draw();Font_FlushAll();ColorFade_Draw();Gfx_EndFrame(1);Dma_Flush();File_Stub264D90();if(R->leaving&&ColorFade_IsOutDone()) break;
    }
    while(gTeamSel->faceState!=TEAMSEL_LOAD_IDLE||gTeamSel->stageState!=TEAMSEL_LOAD_IDLE) {
        Gfx_BeginFrame();TeamSel_UpdateFaceLoad();TeamSel_UpdateStageLoad();Rd_Draw();Font_FlushAll();Gfx_EndFrame(1);Dma_Flush();File_Stub264D90();
    }
    PortTbo_TextMode(0);Port_RaidModelsEnd();sRaidRoomOn=0;TeamSel_Term();Dma_ResetBuffers();
    gTsProgress->team[0]=saved[0];gTsProgress->team[1]=saved[1];gTsProgress->players=savedPlayers;gTsProgress->battleType=savedType;
    gTsProgress->dpLevel=savedDp;gTsProgress->stageCell=savedStage;gTsProgress->bgm=savedBgm;
    if(R->launch) {
        if(!R->connection) {result=Port_PcRosterQueue(&R->battle);if(result) Port_PcNativeRules(1,0,R->battle.rules.bgm1-1);}
        else {
            const char *addr = R->roomCode[0] ? R->roomCode : "127.0.0.1";
            PcBattle compact=R->battle;s32 slots[PORT_COOP_MAX],n=0;
            for(i=0;i<PORT_COOP_MAX;i++)if(R->players[i].connected) {
                slots[n]=i;compact.fighter[0][n].chara=R->players[i].chara;
                compact.fighter[0][n].costume=R->players[i].costume;n++;
            }
            if(n>=2) {
                compact.players=compact.count[0]=n;
                RoomRelay_Party(slots,n,R->players[R->mySlot].chara,R->players[R->mySlot].costume);
                result=Port_CoopLaunch(R->connection==1,addr,R->ip[4],&compact);
            }
        }
    } else {
        Rd_LobbyClose();
    }
    return result;
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
    char why[160];s32 age=0,test=Port_CoopTestInput(),reconnect=0;
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
        Rd_Text(40,251,"X: volver a conectar",.76f,425,1);
        Rd_Text(40,278,"Circulo: menu principal",.68f,425,0);
        Font_FlushAll();Gfx_EndFrame(1);Dma_Flush();
        if(++age>15 && ((pad&(PADG_CROSS|PADG_CIRCLE)) || (test&&age>120))) {reconnect=(pad&PADG_CROSS)!=0;break;}
    }
    Dma_ResetBuffers();sRaidRoomOn=0;
    if(reconnect) {gPortScreen=PORT_SCREEN_RAID;gProgress->mode=MPROG_MODE_PORT_SCREEN;}
    else gProgress->mode=4;
    printf("bt3: raid: recovery menu completed, reconnect=%d\n",reconnect);
    return reconnect;
}
static s32 sRaidOnlineEnding;
void PortRaid_OnlineFinish(s32 done) {
    extern s32 Port_ModesFight(void);
    if(!done) {
        sRaidOnlineEnding=Port_CoopEnabled() && Port_ModesFight()==3;
        if(sRaidOnlineEnding) Port_CoopPauseSimulation(1);
    } else if(sRaidOnlineEnding) {
        if(Port_CoopFailed()) PortRaid_OnlineError();else {Port_CoopLeave();gProgress->mode=4;}
        sRaidOnlineEnding=0;
    }
}
#endif

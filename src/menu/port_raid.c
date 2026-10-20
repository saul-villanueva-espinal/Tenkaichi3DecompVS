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
extern void Port_RosterName(char *,s32,s32);
extern int Port_CharaForms(int chara, int *out, int max);
extern void Dma_AddScissor(s32 x0, s32 x1, s32 y0, s32 y1);
static char sRaidName[128];
static char *Rd_Name(s32 chara) {
    Port_RosterName(sRaidName,sizeof(sRaidName),chara);return sRaidName;
}

static const char *Rd_StageName(s32 stage) {
    switch (stage % 35) {
    case 0: return "Montanas";
    case 1: return "Paramo";
    case 2: return "Desierto";
    case 3: return "Llanura";
    case 4: return "Ciudad";
    case 5: return "Carretera";
    case 6: return "Islas";
    case 7: return "Glaciar";
    case 8: return "Volcan";
    case 9: return "Ruinas";
    case 10: return "Namek";
    case 11: return "Namek destruido";
    case 12: return "Planeta Vegeta";
    case 13: return "Espacio";
    case 14: return "Torneo Mundial";
    case 15: return "Torneo Mas Alla";
    case 16: return "Juegos de Cell";
    case 17: return "Juegos de Bojack";
    case 18: return "Palacio de Kami";
    case 19: return "Sala del Tiempo";
    case 20: return "Mundo Kaioshin";
    case 21: return "Infierno";
    case 22: return "Aldea Pinguino";
    case 23: return "Capital del Este";
    case 24: return "Archipielago";
    case 25: return "Cueva";
    case 26: return "Templo Sagrado";
    case 27: return "Rocoso noche";
    case 28: return "Desierto noche";
    case 29: return "Ciudad noche";
    case 30: return "Torneo noche";
    case 31: return "Juegos noche";
    case 32: return "Paramo tarde";
    case 33: return "Namek noche";
    default: return "Espacio profundo";
    }
}

static const char *Rd_DiffName(s32 diff) {
    return diff == 0 ? "Guerrero Z" : diff == 1 ? "Super Saiyan" : "Extrema";
}

static s32 Rd_Forms(s32 chara, s32 *out, s32 max) {
    s32 count = 0, i, k;
    if (chara < 0 || max < 1) return 0;
    count = Port_CharaForms(chara, out, max);
    if (count > 0) {
        for (i = count; i < max; i++) out[i] = out[count - 1];
        return count;
    }
    if (gTeamSel && gTeamSel->cells[0]) {
        TsCell *cells = gTeamSel->cells[0];
        s32 n = gTeamSel->masterCount[0];
        for (i = 0; i < n; i++) {
            if (cells[i].id == chara) {
                count = cells[i].formCount; if (count > max) count = max;
                for (k = 0; k < count; k++) out[k] = cells[i].form[k];
                for (k = count; k < max; k++) out[k] = out[count ? count - 1 : 0];
                return count;
            }
            for (k = 0; k < cells[i].formCount; k++) {
                if (cells[i].form[k] == chara) {
                    count = cells[i].formCount; if (count > max) count = max;
                    for (i = 0; i < count; i++) out[i] = cells[i].form[i];
                    for (i = count; i < max; i++) out[i] = out[count ? count - 1 : 0];
                    return count;
                }
            }
        }
    }
    for (i = 0; i < max; i++) out[i] = chara;
    return 1;
}

static void Rd_Text(f32 x, f32 y, char *text, f32 scale, f32 width, s32 gold) {
    Ps_TextExOuter(x, y, text, scale, width,
                   Ps_Rgba(255, 255, gold ? 95 : 255, 128),
                   gold ? Ps_Rgba(255, 204, 12, 128) : Ps_Rgba(195, 210, 235, 128),
                   Ps_Rgba(0, 0, 0, 128), Ps_Rgba(0, 0, 0, 128), 1.0f);
}

static void Rd_Header(const char *title, const char *badge) {
    u32 silver = Ps_Rgba(180, 205, 225, 128), dark = Ps_Rgba(10, 22, 45, 128), black = Ps_Rgba(4, 8, 16, 128);
    u32 topNavy = Ps_Rgba(24, 60, 120, 128), botNavy = Ps_Rgba(8, 20, 48, 128);
    u32 redBorder = Ps_Rgba(220, 60, 80, 128);
    f32 w = 230, h = 22;
    Ps_Quad(9, 3, w, 3, w - 12, 3 + h, -5, 3 + h, silver, silver, dark, silver);
    Ps_Quad(11, 5, w - 2, 5, w - 14, 1 + h, -3, 1 + h, black, black, black, black);
    Ps_Quad(13, 6, w - 4, 6, w - 16, h, -1, h, topNavy, topNavy, botNavy, botNavy);
    Ps_TextExOuter(18, 7, (char*)title, 0.70f, 205, Ps_Rgba(255, 255, 255, 128), Ps_Rgba(200, 230, 255, 128), Ps_Rgba(0, 0, 0, 128), Ps_Rgba(10, 30, 60, 128), 1.2f);
    if (badge) {
        Ps_Quad(405, 4, 506, 4, 503, 20, 402, 20, redBorder, redBorder, redBorder, redBorder);
        Ps_Rect(404, 5, 504, 19, Ps_Rgba(160, 20, 42, 128), Ps_Rgba(95, 10, 25, 128));
        Ps_TextExOuter(409, 8, (char*)badge, 0.32f, 96, Ps_Rgba(255, 225, 230, 128), Ps_Rgba(255, 180, 190, 128), Ps_Rgba(0, 0, 0, 128), Ps_Rgba(40, 0, 10, 128), 1.0f);
    }
}

static void Rd_Arrow(f32 cx, f32 cy, s32 dir, s32 gold) {
    u32 color = gold ? Ps_Rgba(255, 180, 20, 128) : Ps_Rgba(200, 210, 230, 128);
    f32 s = dir > 0 ? 5.0f : -5.0f;
    Ps_ScreenQuad(cx - s, cy - 6, cx + s, cy, cx - s, cy + 6, cx - s, cy, color, color, color, color);
}

static void Rd_ChipSlot(f32 x, f32 y, f32 w, f32 h, s32 isBoss) {
    u32 silver = Ps_Rgba(180, 205, 225, 128), black = Ps_Rgba(6, 12, 22, 128), dark = Ps_Rgba(20, 40, 70, 128);
    if (isBoss) {
        Ps_Quad(x - 2, y - 2, x + w + 2, y - 2, x + w + 2, y + h + 2, x - 2, y + h + 2,
                Ps_Rgba(80, 255, 60, 128), Ps_Rgba(80, 255, 60, 128), Ps_Rgba(255, 235, 50, 128), Ps_Rgba(255, 235, 50, 128));
        Ps_Quad(x, y, x + w, y, x + w, y + h, x, y + h, black, black, black, black);
    } else {
        Ps_Quad(x, y, x + w, y, x + w, y + h, x, y + h, silver, silver, dark, dark);
        Ps_Quad(x + 1, y + 1, x + w - 1, y + 1, x + w - 1, y + h - 1, x + 1, y + h - 1, black, black, black, black);
    }
}

static void Rd_Card(f32 x, f32 y, f32 w, f32 h, s32 redBorder) {
    u32 silver = redBorder ? Ps_Rgba(240, 45, 100, 128) : Ps_Rgba(150, 185, 215, 128);
    u32 dark   = redBorder ? Ps_Rgba(140, 15, 50, 128) : Ps_Rgba(35, 60, 95, 128);
    u32 bgTop  = Ps_Rgba(14, 38, 76, 115);
    u32 bgBot  = Ps_Rgba(6, 16, 36, 128);
    Ps_Quad(x + 5, y, x + w - 5, y, x + w, y + 5, x, y + 5, silver, silver, dark, dark);
    Ps_Quad(x, y + 5, x + w, y + 5, x + w, y + h - 5, x, y + h - 5, dark, dark, dark, dark);
    Ps_Quad(x, y + h - 5, x + w, y + h - 5, x + w - 5, y + h, x + 5, y + h, dark, dark, silver, silver);
    Ps_Rect(x + 2, y + 2, x + w - 2, y + h - 2, bgTop, bgBot);
}

static void Rd_Button(f32 x, f32 y, f32 w, f32 h, const char *label, s32 isGold, f32 scale) {
    u32 silver = isGold ? Ps_Rgba(255, 245, 120, 128) : Ps_Rgba(175, 205, 230, 128);
    u32 black = Ps_Rgba(6, 12, 22, 128);
    Ps_Quad(x + 5, y, x + w, y, x + w - 5, y + h, x, y + h, silver, silver, black, silver);
    Ps_Quad(x + 6, y + 2, x + w - 2, y + 2, x + w - 6, y + h - 2, x + 2, y + h - 2, black, black, black, black);
    Ps_Quad(x + 7, y + 3, x + w - 3, y + 3, x + w - 7, y + h - 3, x + 3, y + h - 3,
            isGold ? Ps_Rgba(255, 242, 80, 128) : Ps_Rgba(30, 85, 160, 128),
            isGold ? Ps_Rgba(255, 185, 12, 128) : Ps_Rgba(16, 45, 95, 128),
            isGold ? Ps_Rgba(185, 99, 0, 128) : Ps_Rgba(6, 18, 42, 128),
            isGold ? Ps_Rgba(249, 189, 13, 128) : Ps_Rgba(10, 32, 70, 128));
    if (isGold) {
        Ps_TextExOuter(x + 8, y + 4, (char*)label, scale, w - 16,
                       Ps_Rgba(12, 22, 38, 128), Ps_Rgba(5, 10, 20, 128),
                       Ps_Rgba(255, 240, 160, 128), Ps_Rgba(255, 240, 160, 128), 0.8f);
    } else {
        Ps_TextExOuter(x + 8, y + 4, (char*)label, scale, w - 16,
                       Ps_Rgba(255, 255, 255, 128), Ps_Rgba(200, 225, 245, 128),
                       Ps_Rgba(0, 0, 0, 128), Ps_Rgba(0, 0, 0, 128), 1.0f);
    }
}

static void Rd_Tab(f32 x, f32 y, f32 w, f32 h, const char *label, s32 active) {
    Rd_Button(x, y, w, h, label, active, 0.44f);
}

static void Rd_Row(f32 x, f32 y, f32 w, f32 h, const char *label, const char *value, s32 selected) {
    if (selected) {
        Ps_Rect(x, y, x + w, y + h, Ps_Rgba(25, 90, 175, 100), Ps_Rgba(10, 40, 90, 120));
        Ps_Rect(x + 1, y + h - 2, x + w - 1, y + h, Ps_Rgba(255, 230, 40, 128), Ps_Rgba(255, 230, 40, 128));
    }
    Rd_Text(x + 4, y + 3, (char*)label, 0.42f, (w * 0.5f) - 6, 0);
    Rd_Text(x + (w * 0.48f), y + 3, (char*)value, 0.42f, (w * 0.48f) - 14, selected ? 1 : 0);
    Rd_Arrow(x + w - 6, y + (h * 0.5f), 1, selected ? 1 : 0);
}

static void Rd_StageThumb(f32 x, f32 y, f32 w, f32 h, s32 stage) {
    u32 silver = Ps_Rgba(170, 200, 225, 128), black = Ps_Rgba(5, 10, 20, 128);
    Ps_Rect(x, y, x + w, y + h, silver, silver);
    Ps_Rect(x + 1, y + 1, x + w - 1, y + h - 1, black, black);
    if ((gTeamSel->flags & TEAMSEL_STAGE_READY) && gTeamSel->bg[0]) {
        Dma_AddScissor((s32)(x + 2), (s32)(x + w - 2), (s32)(y + 2), (s32)(y + h - 2));
        Sprite_DrawPicture(gTeamSel->bg[0], x - 10, y - 5, 128);
        Dma_AddScissor(0, 511, 0, 447);
    } else {
        Ps_Rect(x + 2, y + 2, x + w - 2, y + h - 2, Ps_Rgba(50, 90, 120, 128), Ps_Rgba(20, 45, 60, 128));
    }
    Ps_Rect(x + 2, y + h - 16, x + w - 2, y + h - 2, Ps_Rgba(5, 12, 25, 120), Ps_Rgba(5, 12, 25, 128));
    Rd_Text(x + 4, y + h - 14, (char*)Rd_StageName(stage), 0.32f, w - 8, 1);
}

static void Rd_Vista3D(f32 cx, f32 cy) {
    u32 orange = Ps_Rgba(255, 175, 20, 128);
    Ps_Quad(cx - 15, cy - 4, cx - 10, cy - 6, cx - 10, cy - 4, cx - 15, cy - 2, orange, orange, orange, orange);
    Ps_Quad(cx - 10, cy - 6, cx + 10, cy - 6, cx + 10, cy - 4, cx - 10, cy - 4, orange, orange, orange, orange);
    Ps_Quad(cx + 10, cy - 6, cx + 15, cy - 2, cx + 13, cy - 1, cx + 10, cy - 4, orange, orange, orange, orange);
    Ps_Quad(cx + 15, cy - 5, cx + 18, cy - 1, cx + 12, cy - 1, cx + 15, cy - 3, orange, orange, orange, orange);
    Rd_Text(cx - 10, cy + 3, "Vista 3D", 0.44f, 60, 1);
}

static void Rd_Badge(f32 x, f32 y, f32 w, f32 h, const char *name, const char *sub) {
    u32 silver = Ps_Rgba(180, 205, 225, 128), dark = Ps_Rgba(10, 18, 30, 128);
    u32 redTop = Ps_Rgba(55, 12, 22, 128), redBot = Ps_Rgba(18, 6, 12, 128);
    Ps_Quad(x + 8, y, x + w - 8, y, x + w, y + (h * 0.5f), x, y + (h * 0.5f), silver, silver, dark, dark);
    Ps_Quad(x, y + (h * 0.5f), x + w, y + (h * 0.5f), x + w - 8, y + h, x + 8, y + h, dark, dark, silver, silver);
    Ps_Quad(x + 9, y + 2, x + w - 9, y + 2, x + w - 2, y + (h * 0.5f), x + 2, y + (h * 0.5f), redTop, redTop, redBot, redBot);
    Ps_Quad(x + 2, y + (h * 0.5f), x + w - 2, y + (h * 0.5f), x + w - 9, y + h - 2, x + 9, y + h - 2, redBot, redBot, redBot, redBot);
    Rd_Text(x + 14, y + 2, (char*)name, 0.58f, w - 28, 0);
    Rd_Text(x + 22, y + 15, (char*)sub, 0.32f, w - 44, 1);
}

static void Rd_SectionBanner(f32 x, f32 y, f32 w, f32 h, const char *text) {
    u32 silver = Ps_Rgba(170, 200, 225, 128), dark = Ps_Rgba(10, 24, 52, 128);
    Ps_Quad(x + 10, y, x + w, y, x + w - 8, y + h, x, y + h, silver, silver, dark, dark);
    Ps_Quad(x + 11, y + 1, x + w - 1, y + 1, x + w - 9, y + h - 1, x + 1, y + h - 1, dark, dark, dark, dark);
    Rd_Text(x + 12, y + 2, (char*)text, 0.50f, w - 20, 0);
}

static void Rd_PlayerCard(f32 x, f32 y, f32 w, f32 h, s32 idx, PcFighter *hero, s32 isChoosing) {
    char tag[16];
    Rd_Card(x, y, w, h, isChoosing);
    sprintf(tag, "J%d", idx + 1);
    if (isChoosing) {
        Ps_Rect(x + 4, y + 3, x + 24, y + 15, Ps_Rgba(240, 40, 90, 128), Ps_Rgba(180, 20, 60, 128));
    } else {
        Ps_Rect(x + 4, y + 3, x + 24, y + 15, Ps_Rgba(25, 75, 140, 128), Ps_Rgba(10, 30, 70, 128));
    }
    Rd_Text(x + 6, y + 4, tag, 0.34f, 18, 0);
    Port_TeamChip(hero->chara, x + 8, y + 16, 60, 40, 128);
    Rd_Text(x + 6, y + 58, Rd_Name(hero->chara), 0.36f, w - 12, 0);
    if (isChoosing) {
        Ps_Quad(x + 6, y + 73, x + w - 6, y + 73, x + w - 6, y + 87, x + 6, y + 87,
                Ps_Rgba(240, 40, 90, 128), Ps_Rgba(240, 40, 90, 128), Ps_Rgba(240, 40, 90, 128), Ps_Rgba(240, 40, 90, 128));
        Ps_Rect(x + 7, y + 74, x + w - 7, y + 86, Ps_Rgba(50, 10, 25, 128), Ps_Rgba(30, 5, 15, 128));
        Rd_Text(x + 18, y + 75, "Elegir", 0.38f, w - 36, 1);
    } else {
        Ps_Quad(x + 6, y + 73, x + w - 6, y + 73, x + w - 6, y + 87, x + 6, y + 87,
                Ps_Rgba(60, 130, 200, 128), Ps_Rgba(60, 130, 200, 128), Ps_Rgba(30, 70, 130, 128), Ps_Rgba(30, 70, 130, 128));
        Ps_Rect(x + 7, y + 74, x + w - 7, y + 86, Ps_Rgba(15, 45, 90, 128), Ps_Rgba(8, 22, 50, 128));
        Rd_Text(x + 18, y + 75, "Listo", 0.36f, w - 36, 0);
    }
}

static void Rd_Draw(void) {
    s32 i, index = Rd_Index(Rd_Shown()->chara), n = Port_PcRosterCount();
    char buf[96];
    PcFighter *f = Rd_Shown();
    PcBattleRules *rules = &R->battle.rules;

    Ps_PageFull();
    if (gTeamSel->flags & TEAMSEL_STAGE_READY) Sprite_DrawPicture(gTeamSel->bg[0], 0, 0, 128);
    else if (gTeamSel->bg[1]) Sprite_DrawPicture(gTeamSel->bg[1], 0, 0, 128);
    else Ps_Rect(0, 0, 512, 448, Ps_Rgba(90, 177, 222, 128), Ps_Rgba(60, 118, 46, 128));

    Port_SelectModelsSet(0, f->chara, f->costume);
    Port_SelectModelsSet(1, -1, 0);
    Port_SelectModelsDrawRaidEx(R->page == RD_CUSTOM ? 2 : 1);
    Ps_PageFull();

    if (R->page == RD_LOBBY) {
        Rd_Header("JEFE FINAL - SALA ONLINE", "PROPUESTA - SIN IMPLEMENTAR");
        /* Top Carousel */
        Rd_Arrow(14, 47, -1, 1);
        Rd_Arrow(498, 47, 1, 1);
        for (i = 0; i < 8; i++) {
            f32 x = 24 + i * 58.0f;
            s32 id = Port_PcRosterId(Rd_Wrap(index + i - 1, n));
            s32 isBoss = (i == 1);
            Rd_ChipSlot(x, 28, 54, 38, isBoss);
            Port_TeamChip(id, x + 2, 30, 50, 34, 128);
        }
        /* Settings Card on Left */
        Rd_Card(14, 74, 224, 94, 0);
        Rd_Row(18, 78, 136, 21, "Modalidad:", R->connection == 0 ? "Asalto cooperativo" : R->connection == 1 ? "Crear sala UDP" : "Unirse por IP", R->cursor == 1);
        sprintf(buf, "%d / 6", R->count);
        Rd_Row(18, 100, 136, 21, "Jugadores:", buf, R->cursor == 2);
        Rd_Row(18, 122, 136, 21, "Dificultad:", Rd_DiffName(R->difficulty), R->cursor == 3);
        Rd_Row(18, 144, 136, 21, "Escenario:", Rd_StageName(R->battle.stage), R->cursor == 4);
        Rd_StageThumb(158, 78, 74, 86, R->battle.stage);
        /* Action buttons below settings */
        Rd_Button(18, 172, 106, 22, "Seleccionar jefe", R->cursor == 0, 0.44f);
        Rd_Button(128, 172, 104, 22, "Personalizar", R->cursor == 5, 0.44f);
        /* 3D Model indicators */
        Rd_Vista3D(426, 114);
        Rd_Badge(320, 172, 174, 28, Rd_Name(Rd_Boss()->chara), "JEFE SELECCIONADO");
        /* Bottom: Equipo Cooperativo */
        Rd_SectionBanner(14, 202, 180, 18, "EQUIPO COOPERATIVO");
        Rd_Card(14, 218, 484, 100, 0);
        for (i = 0; i < PORT_COOP_MAX; i++) {
            f32 x = 20 + i * 79.0f;
            s32 isChoosing = (i == 5 || (R->page == RD_ALLY && R->ally == i));
            Rd_PlayerCard(x, 222, 76, 92, i, &R->battle.fighter[0][i], isChoosing);
        }
        /* Footer */
        Ps_Rect(0, 412, 512, 448, Ps_Rgba(7, 18, 37, 118), Ps_Rgba(3, 7, 17, 128));
        Rd_Button(14, 416, 116, 24, "Comprobar mods", R->cursor == 8, 0.40f);
        Rd_Text(148, 421, "(X) Listo    (Triangulo) Personalizar    (O) Volver", 0.40f, 230, 0);
        Rd_Button(398, 414, 102, 28, "INICIAR", 1, 0.65f);
        if (R->message[0]) Rd_Text(14, 439, R->message, 0.33f, 484, 1);
    } else if (R->page == RD_CUSTOM) {
        s32 forms[3], nf;
        Rd_Header("PERSONALIZAR JEFE", "PROPUESTA - SIN IMPLEMENTAR");
        Rd_Tab(24, 28, 76, 20, "Asalto", 1);
        Rd_Tab(106, 28, 86, 20, "Parejas Tag", 0);
        Rd_Tab(198, 28, 74, 20, "Desafio", 0);
        /* Left Options Card */
        Rd_Card(16, 52, 196, 264, 0);
        Rd_Row(20, 56, 188, 30, "Personaje:", Rd_Name(f->chara), R->cursor == 0);
        sprintf(buf, "%d / 8", f->costume + 1);
        Rd_Row(20, 88, 188, 30, "Traje:", buf, R->cursor == 1);
        Ps_Copy(buf, rules->bossHp >= 150000 ? "Muy alta" : rules->bossHp >= 100000 ? "Alta" : "Normal", sizeof(buf));
        Rd_Row(20, 120, 188, 30, "Vida:", buf, R->cursor == 2);
        Ps_Copy(buf, rules->bossDamagePct >= 200 ? "Extremo" : rules->bossDamagePct >= 150 ? "Alto" : "Normal", sizeof(buf));
        Rd_Row(20, 152, 188, 30, "Dano:", buf, R->cursor == 3);
        Ps_Copy(buf, rules->cpuLevel1 >= 25 ? "Agresivo" : rules->cpuLevel1 >= 18 ? "Equilibrado" : "Normal", sizeof(buf));
        Rd_Row(20, 184, 188, 30, "Comportamiento:", buf, R->cursor == 4);
        Ps_Copy(buf, rules->enragePct ? "3" : "1", sizeof(buf));
        Rd_Row(20, 216, 188, 30, "Fases:", buf, R->cursor == 5);
        Rd_Row(20, 248, 188, 30, "Reanimaciones:", "Compartidas", R->cursor == 6);
        Rd_Row(20, 280, 188, 30, "Victoria:", "Derrotar al jefe", R->cursor == 7);
        /* Right Fases Card */
        Rd_Card(338, 52, 164, 90, 0);
        Rd_Text(344, 56, "Fases del jefe", 0.44f, 150, 0);
        nf = Rd_Forms(f->chara, forms, 3);
        (void)nf;
        Rd_ChipSlot(344, 72, 38, 30, 0);
        Port_TeamChip(forms[0], 346, 73, 34, 28, 128);
        Rd_Text(344, 104, "NORMAL", 0.30f, 38, 0);
        Rd_Arrow(392, 86, 1, 1);
        Rd_ChipSlot(400, 72, 38, 30, 0);
        Port_TeamChip(forms[1], 402, 73, 34, 28, 128);
        Rd_Text(402, 104, "FURIA", 0.30f, 38, 0);
        Rd_Arrow(448, 86, 1, 1);
        Rd_ChipSlot(456, 72, 38, 30, 0);
        Port_TeamChip(forms[2], 458, 73, 34, 28, 128);
        Rd_Text(452, 104, "PODER MAXIMO", 0.28f, 48, 0);
        /* Repeated characters option card */
        Rd_Card(338, 148, 164, 26, 0);
        Rd_Text(344, 153, "Personajes repetidos:", 0.36f, 96, 0);
        Rd_Text(440, 153, "< Permitidos >", 0.34f, 60, 1);
        /* Footer buttons */
        Ps_Rect(0, 412, 512, 448, Ps_Rgba(7, 18, 37, 118), Ps_Rgba(3, 7, 17, 128));
        Rd_Button(145, 414, 114, 26, "GUARDAR PERFIL", R->cursor == 8, 0.46f);
        Rd_Button(268, 414, 114, 26, "VOLVER A LA SALA", 0, 0.44f);
        Rd_Text(390, 421, "(X) Confirmar  (O) Volver  (L1/R1) Traje", 0.35f, 120, 0);
    } else if (R->page == RD_ALLY) {
        sprintf(buf, "ELEGIR ALIADO J%d", R->ally + 1);
        Rd_Header(buf, "PROPUESTA - SIN IMPLEMENTAR");
        Rd_Arrow(14, 47, -1, 1);
        Rd_Arrow(498, 47, 1, 1);
        for (i = 0; i < 8; i++) {
            f32 x = 24 + i * 58.0f;
            s32 id = Port_PcRosterId(Rd_Wrap(index + i - 1, n));
            s32 isCur = (i == 1);
            Rd_ChipSlot(x, 28, 54, 38, isCur);
            Port_TeamChip(id, x + 2, 30, 50, 34, 128);
        }
        Rd_Card(14, 74, 224, 114, 0);
        sprintf(buf, "J%d - %s", R->ally + 1, Rd_Name(f->chara));
        Rd_Text(22, 80, buf, 0.52f, 206, 1);
        sprintf(buf, "Traje %d / 8", f->costume + 1);
        Rd_Text(22, 100, buf, 0.42f, 206, 0);
        Rd_Text(22, 122, "Izquierda / Derecha: personaje", 0.38f, 206, 0);
        Rd_Text(22, 138, "Arriba / Abajo: cambiar plaza J1..J6", 0.38f, 206, 0);
        Rd_Text(22, 154, "L1 / R1: cambiar traje", 0.38f, 206, 0);
        Rd_Text(22, 170, "X: confirmar y volver a la sala", 0.38f, 206, 1);
        Rd_Vista3D(426, 114);
        Rd_Badge(320, 172, 174, 28, Rd_Name(f->chara), "ALIADO SELECCIONADO");
        Rd_SectionBanner(14, 202, 180, 18, "EQUIPO COOPERATIVO");
        Rd_Card(14, 218, 484, 100, 0);
        for (i = 0; i < PORT_COOP_MAX; i++) {
            f32 x = 20 + i * 79.0f;
            s32 isChoosing = (R->ally == i);
            Rd_PlayerCard(x, 222, 76, 92, i, &R->battle.fighter[0][i], isChoosing);
        }
        Ps_Rect(0, 412, 512, 448, Ps_Rgba(7, 18, 37, 118), Ps_Rgba(3, 7, 17, 128));
        Rd_Text(148, 421, "(X) Confirmar    (O) Volver", 0.44f, 230, 0);
    } else {
        Rd_Header("CONFIGURACION DE RED UDP", "PROPUESTA - SIN IMPLEMENTAR");
        Rd_Card(14, 74, 240, 160, 0);
        for (i = 0; i < 5; i++) {
            sprintf(buf, "%d", R->ip[i]);
            Rd_Row(18, 78 + i * 28, 230, 26, i == 4 ? "Puerto UDP:" : i == 0 ? "IP - octeto 1:" : i == 1 ? "IP - octeto 2:" : i == 2 ? "IP - octeto 3:" : "IP - octeto 4:", buf, R->cursor == i);
        }
        Rd_Text(22, 248, "Izquierda / Derecha: cambiar valor  |  Cuadrado: +10", 0.38f, 230, 0);
        Rd_Text(22, 266, "X: confirmar y volver a la sala", 0.38f, 230, 1);
        Ps_Rect(0, 412, 512, 448, Ps_Rgba(7, 18, 37, 118), Ps_Rgba(3, 7, 17, 128));
        Rd_Text(148, 421, "(X) Volver a la sala", 0.44f, 230, 0);
    }
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

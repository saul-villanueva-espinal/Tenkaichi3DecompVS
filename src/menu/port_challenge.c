#include "common.h"
#ifdef PORT
#include "menu/menu_e.h"
#include "sys/pad.h"
#include "sys/save.h"
#include "menu/tbo_art.h"
#include "menu/tbo_sizes.h"
#include "menu/port_screen.h"
#include "port_pc_battle.h"
#include "port_fighters.h"
#include "plat_songs.h"

extern void Gfx_BeginFrame(void);
extern void Gfx_EndFrame(s32 arg);
extern void Dma_Flush(void);
extern void Dma_ResetBuffers(void);
extern void File_Stub264D90(void);
extern void ColorFade_StartIn(s32 r, s32 g, s32 b, s32 frames);
extern void ColorFade_StartOut(s32 r, s32 g, s32 b, s32 frames);
extern void ColorFade_Update(void);
extern void ColorFade_Draw(void);
extern void Bgm_Play(s32 bgm);
extern void Adx_StopAll(void);
extern u32 Rand_Range(u32 n);
extern void Font_FlushAll(void);
extern s32 Port_Setting(const char *name, int def);
extern void Port_SettingSave(const char *name, int val);
extern void Port_SettingsWrite(void);
extern int Port_RosterPointerRead(int *x, int *y);
extern s32 Snd_PlaySe(u32 mask, s32 id);
extern f32 cosf(f32 x);
extern f32 sinf(f32 x);
extern s32 ColorFade_IsOutDone(void);
extern s32 ColorFade_IsInDone(void);
extern s32 Port_NativeRosterPick(PcBattle *out, s32 kind, s32 dp);
extern int Port_PcRosterQueue(const PcBattle *b);
extern const char *Port_PcRosterName(int chara);

#define CHALLENGE_TABS 3
#define CHALLENGE_FLOORS 5

typedef struct ChallengeFloorDef {
    const char *floorTag;
    const char *title;
    const char *saga;
    const char *desc1;
    const char *desc2;
    const char *modifier;
    s32 difficulty;
    s32 stage;
    s32 bgm;
    s32 enemyCount;
    s32 enemies[10];
    s32 costumes[10];
} ChallengeFloorDef;

static const ChallengeFloorDef sFloors[CHALLENGE_FLOORS] = {
    {
        "PISO 1",
        "Asalto Saiyan en la Tierra",
        "Saga Saiyan",
        "La Tierra es invadida por los guerreros del espacio exterior.",
        "Derrota a los Saibaman, Raditz, Nappa y al principe Vegeta.",
        "Ganancia de Ki +50% | Sin limite de tiempo",
        2, 0, 0, 5,
        { 79, 77, 80, 29, 30 },
        { 0, 0, 0, 0, 0 }
    },
    {
        "PISO 2",
        "La Tirania de Freezer & Fuerzas Ginyu",
        "Saga de Namek",
        "El tirano espacial Freezer y su temible Fuerza Ginyu en Namek.",
        "Supera a Recoome, Burter, Jeice, Ginyu y las formas de Freezer.",
        "Danio de combate aumentado (x1.25) | Atmosfera agonizante",
        3, 3, 1, 7,
        { 87, 88, 89, 86, 91, 94, 95 },
        { 0, 0, 0, 0, 0, 0, 0 }
    },
    {
        "PISO 3",
        "El Gran Torneo de Cell",
        "Saga de los Androides & Cell",
        "El destino de la humanidad se decide en el Ring de Cell.",
        "Enfrenta al ejercito de la Red Ribbon y al bio-androide supremo.",
        "Enemigos: +150 HP/segundo | IA nivel 20",
        4, 6, 10, 9,
        { 100, 101, 102, 103, 104, 105, 106, 107, 108 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0 }
    },
    {
        "PISO 4",
        "El Despertar del Caos Majin",
        "Saga de Majin Buu",
        "La magia milenaria de Babidi ha despertado al monstruo Buu.",
        "Erradica a Babidi, Dabura y todas las absorciones de Majin Buu.",
        "Frenesi de Ki Infinito en Kid Buu | Caos Dimensional",
        5, 7, 5, 8,
        { 110, 111, 112, 113, 114, 115, 116, 117 },
        { 0, 0, 0, 0, 0, 0, 0, 0 }
    },
    {
        "PISO 5",
        "Dominio Divino & Jefes Colosales",
        "Sagas Peliculas & Multiverso",
        "La prueba suprema contra los titanes mas destructivos.",
        "Vence a Broly LSS, Janemba, Baby Simio, Omega Shenron y Vegito.",
        "Super Armadura Titanica | Nivel de IA Maximo Z-Master (29)",
        5, 16, 18, 5,
        { 133, 138, 143, 147, 52 },
        { 0, 0, 0, 0, 0 }
    }
};

/* Stage and character name lookup tables */
static const char *const sStageNames[35] = {
    "Paramo (Dia)", "Zona Rocosa (Dia)", "Planeta Namek", "Namek Agonizante",
    "Torneo de las Artes Marciales", "Palacio de Kamisama", "Ring del Torneo de Cell",
    "Mundo Sagrado de Kaioshin", "Habitacion del Tiempo", "Ruinas de la Ciudad",
    "Camino de la Montana", "Islas", "Kame House", "Planeta (Noche)", "Glaciar",
    "Tierra Destruida", "Espacio Exterior", "Villa Pinguino", "Infierno", "Desierto",
    "Castillo del Rey", "Torre de la Fuerza", "Monte Paoz", "Paramo (Tarde)",
    "Paramo (Noche)", "Zona Rocosa (Tarde)", "Zona Rocosa (Noche)", "Torneo (Tarde)",
    "Juegos de Cell (Dia)", "Ruinas (Tarde)", "Ruinas (Noche)", "Desierto (Tarde)",
    "Desierto (Noche)", "Camino (Tarde)", "Planeta (Tarde)"
};

static const char *const sBgmNames[20] = {
    "The Meteor", "Vital Burner", "Innocent World", "After The Fire",
    "Sweet Vibration", "Survive", "Heat Capacity", "Overture",
    "Shine", "Power Scale", "Edge of Spirit", "Caution !",
    "Menace", "Hot Soul", "High and Scream", "Shootout In Meteor",
    "Dynamite Battle", "Burnin' Up", "Wild Rush", "Evolution"
};

static const char *Challenge_CharaName(s32 id) {
    static char name[96];
    extern void Port_RosterName(char *,s32,s32);
    Port_RosterName(name,sizeof(name),id);
    return name;
}

static s32 sChallengeFloor=-1,sChallengeSurvival;
void PortChallenge_RecordResult(s32 won) {
    if(won && sChallengeFloor>=0) {
        s32 clears=Port_Setting("challenge_cleared",0);
        Port_SettingSave("challenge_cleared",clears|(1<<sChallengeFloor));
        Port_SettingsWrite();
    }
    if(won && sChallengeSurvival) {
        Port_SettingSave("challenge_best_waves",Port_Setting("challenge_best_waves",0)+1);
        Port_SettingsWrite();
    }
    sChallengeFloor=-1;sChallengeSurvival=0;
}

/* Custom Battle Options State */
static s32 sCustomFormat = 0;   /* 0: Relevos 10v10, 1: Tag Team 5v5, 2: Duelo 1v1 */
static s32 sCustomEnemyNum = 5;  /* 1 to 10 enemies */
static s32 sCustomArchetype = 4; /* 0: Z-Warriors, 1: Villanos, 2: Androides, 3: Dioses, 4: Aleatorio */
static s32 sCustomAi = 2;        /* 0: Novato, 1: Normal, 2: Fuerte, 3: Z-Master */
static s32 sCustomStage = 6;     /* 0..34 */
static s32 sCustomBgm = 0;       /* 0..19 */
static s32 sCustomMod = 0;       /* 0: Normal 1.0x, 1: Danio 1.25x, 2: Caos 1.5x, 3: Berserk 2.0x */
static s32 sCustomCur = 0;       /* Currently focused custom setting */

#define CUSTOM_SETTINGS_COUNT 7

static const char *const sFormatNames[3] = { "Duelo por Relevos (10v10)", "Batalla Tag Team (5v5)", "Duelo Individual (1v1)" };
static const char *const sArchetypeNames[5] = { "Guerreros Z", "Villanos Principales", "Legion Androide", "Jefes Multiverso", "Seleccion Aleatoria" };
static const char *const sAiNames[4] = { "Novato (Nivel 5)", "Normal (Nivel 13)", "Fuerte (Nivel 20)", "Z-Master (Nivel 29)" };
static const char *const sModNames[4] = { "Normal (1.0x)", "Danio Aumentado (1.25x)", "Caos Letal (1.5x)", "Furia Berserk (2.0x)" };

static void Challenge_Star(f32 x, f32 y, f32 r, u32 col) {
    s32 k;
    for (k = 0; k < 5; k++) {
        f32 a = -1.5707963f + k * 1.2566371f;
        f32 b = a + 0.6283185f;
        f32 c = a + 1.2566371f;
        Ps_Quad(x, y, x + cosf(a) * r, y + sinf(a) * r,
                x + cosf(b) * (r * 0.45f), y + sinf(b) * (r * 0.45f),
                x + cosf(c) * r, y + sinf(c) * r,
                col, col, col, col);
    }
}

/* Reference UI revision 6 - manga tower: also reload resources in the picker cancel frame. */
#include "port_challenge_ui.inc"

static void Challenge_GenerateSurvivalEnemies(PcFighter *outFighters, s32 count) {
    s32 i;
    for (i = 0; i < count && i < 10; i++) {
        outFighters[i].chara = Rand_Range(PORT_DISC_CHARAS);
        outFighters[i].costume = 0;
    }
}

static void Challenge_GenerateCustomEnemies(PcFighter *outFighters, s32 count, s32 archetype) {
    s32 i;
    static const s32 sZWarriors[] = { 0, 3, 14, 17, 21, 23, 25, 26, 27, 31, 39, 41, 48, 51, 53 };
    static const s32 sVillains[] = { 82, 91, 94, 95, 107, 108, 111, 114, 117, 126, 128, 133, 138 };
    static const s32 sAndroids[] = { 100, 101, 102, 103, 104, 105, 106, 107, 108, 129, 130 };
    static const s32 sGods[] = { 133, 138, 143, 147, 108, 107, 52, 6, 10, 21, 59, 111, 112 };

    for (i = 0; i < count && i < 10; i++) {
        s32 chara = 0;
        if (archetype == 0) chara = sZWarriors[Rand_Range(sizeof(sZWarriors) / sizeof(sZWarriors[0]))];
        else if (archetype == 1) chara = sVillains[Rand_Range(sizeof(sVillains) / sizeof(sVillains[0]))];
        else if (archetype == 2) chara = sAndroids[Rand_Range(sizeof(sAndroids) / sizeof(sAndroids[0]))];
        else if (archetype == 3) chara = sGods[Rand_Range(sizeof(sGods) / sizeof(sGods[0]))];
        else chara = Rand_Range(PORT_DISC_CHARAS);

        outFighters[i].chara = chara;
        outFighters[i].costume = 0;
    }
}

/* The Main Frame Loop of the Challenge Screen */
s32 PortChallenge_Run(void) {
    s32 activeTab = 0;
    s32 floorCur = 0;
    s32 clears = Port_Setting("challenge_cleared", 0);
    s32 bestSurvival = Port_Setting("challenge_best_waves", 0);
    s32 result = 0, done = 0, frameAge = 0;
    s32 savedPointerActive=gPortNativeRosterActive;


    Bgm_Play(PORT_BGM_FILE(0)); /* Epic BGM */
    ColorFade_StartIn(0, 0, 0, 15);

    while (!done) {
        u32 p0, r0;
        s32 mx, my, click, i, k;

        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();

        { extern int Port_RosterTestMode(void);extern unsigned Port_RosterTestInput(unsigned);
          p0 = gPad[0].gamePressed | (Port_RosterTestMode()==10?Port_RosterTestInput(frameAge+1):0); }
        r0 = gPad[0].gameRepeat | p0;
        if (gProgress->flags & MPROG_FREEZE) p0 = r0 = 0;

        Challenge_ResourcesBegin();
        gPortNativeRosterActive=1;
        TeamSel_UpdateFaceLoad();TeamSel_UpdateStageLoad();
        Ps_PageFull();Ps_SetShift(0);
        click = Port_RosterPointerRead(&mx, &my);

        /* Hit regions are the same virtual 512 x 448 rectangles as the artwork. */
        if (click) Challenge_Pointer(activeTab, mx, my, &activeTab, &floorCur, &p0);
        r0 |= p0;

        /* Pad navigation */
        if (p0 & (PADG_L1 | PADG_R1)) {
            activeTab = Ps_Wrap(activeTab + ((p0 & PADG_L1) ? -1 : 1), 0, CHALLENGE_TABS - 1);
            Snd_PlaySe(1, 0);
        }

        if (activeTab == 0) {
            /* TAB 0: TORRE DE SAGAS */
            s32 old = floorCur;
            if (r0 & (PADG_UP | PADG_UP2)) floorCur++;
            else if (r0 & (PADG_DOWN | PADG_DOWN2)) floorCur--;
            floorCur = Ps_Wrap(floorCur, 0, CHALLENGE_FLOORS - 1);
            if (floorCur != old) Snd_PlaySe(1, 0);

            if (p0 & PADG_CROSS) {
                const ChallengeFloorDef *fl = &sFloors[floorCur];
                PcBattle battle;
                memset(&battle, 0, sizeof(battle));
                battle.mode = PC_RELAY;
                battle.players = 1;
                battle.stage = fl->stage;
                battle.rules.cpuLevel1=fl->difficulty==2?6:fl->difficulty==3?14:fl->difficulty==4?21:30;
                battle.rules.bgm1=fl->bgm+1;
                if(floorCur==0) battle.rules.kiPct=150;
                if(floorCur==1) battle.rules.damagePct=125;
                if(floorCur==2) {battle.rules.regenPerSec=150;battle.rules.regenSides=2;}
                if(floorCur==3) battle.rules.infiniteKiChara1=118; /* Kid Buu 117 */
                if(floorCur==4) battle.rules.armorSides=2;
                battle.count[0] = 5; /* Player confirms five warriors */
                battle.count[1] = fl->enemyCount;
                for (i = 0; i < fl->enemyCount; i++) {
                    battle.fighter[1][i].chara = fl->enemies[i];
                    battle.fighter[1][i].costume = fl->costumes[i];
                }

                Snd_PlaySe(1, 1);
                /* Launch team selection with preset enemies (kind == 3) */
                if (Challenge_Pick(&battle, floorCur)) {
                    if(!Port_PcRosterQueue(&battle)) {Snd_PlaySe(1,7);continue;}
                    sChallengeFloor=floorCur;sChallengeSurvival=0;
                    result = 1;
                    done = 1;
                    break;
                }
            }
        } else if (activeTab == 1) {
            /* TAB 1: MODO SUPERVIVENCIA 10v10 */
            if (p0 & PADG_CROSS) {
                PcBattle battle;
                memset(&battle, 0, sizeof(battle));
                battle.mode = PC_RELAY;
                battle.players = 1;
                battle.stage = Rand_Range(35);
                battle.rules.healthPct=25;
                battle.rules.cpuLevel1=21;
                battle.count[0] = 10; /* Full 10 warriors for survival */
                battle.count[1] = 10;
                Challenge_GenerateSurvivalEnemies(battle.fighter[1], 10);

                Snd_PlaySe(1, 1);
                if (Challenge_Pick(&battle, -1)) {
                    if(!Port_PcRosterQueue(&battle)) {Snd_PlaySe(1,7);continue;}
                    sChallengeFloor=-1;sChallengeSurvival=1;
                    result = 1;
                    done = 1;
                    break;
                }
            }
            if (p0 & PADG_TRIANGLE) {
                bestSurvival = 0;
                Port_SettingSave("challenge_best_waves", 0);
                Port_SettingsWrite();
                Snd_PlaySe(1, 2);
            }
        } else {
            /* TAB 2: BATALLAS PERSONALIZADAS */
            if (r0 & (PADG_UP | PADG_UP2)) { sCustomCur = Ps_Wrap(sCustomCur - 1, 0, CUSTOM_SETTINGS_COUNT - 1); Snd_PlaySe(1, 0); }
            else if (r0 & (PADG_DOWN | PADG_DOWN2)) { sCustomCur = Ps_Wrap(sCustomCur + 1, 0, CUSTOM_SETTINGS_COUNT - 1); Snd_PlaySe(1, 0); }

            if (r0 & (PADG_LEFT | PADG_LEFT2)) {
                switch (sCustomCur) {
                case 0: sCustomFormat = Ps_Wrap(sCustomFormat - 1, 0, 2); break;
                case 1: sCustomEnemyNum = Ps_Wrap(sCustomEnemyNum - 1, 1, 10); break;
                case 2: sCustomArchetype = Ps_Wrap(sCustomArchetype - 1, 0, 4); break;
                case 3: sCustomAi = Ps_Wrap(sCustomAi - 1, 0, 3); break;
                case 4: sCustomStage = Ps_Wrap(sCustomStage - 1, 0, 34); break;
                case 5: sCustomBgm = Ps_Wrap(sCustomBgm - 1, 0, 19); Bgm_Play(PORT_BGM_FILE(sCustomBgm)); break;
                case 6: sCustomMod = Ps_Wrap(sCustomMod - 1, 0, 3); break;
                }
                Snd_PlaySe(1, 0);
            } else if (r0 & (PADG_RIGHT | PADG_RIGHT2)) {
                switch (sCustomCur) {
                case 0: sCustomFormat = Ps_Wrap(sCustomFormat + 1, 0, 2); break;
                case 1: sCustomEnemyNum = Ps_Wrap(sCustomEnemyNum + 1, 1, 10); break;
                case 2: sCustomArchetype = Ps_Wrap(sCustomArchetype + 1, 0, 4); break;
                case 3: sCustomAi = Ps_Wrap(sCustomAi + 1, 0, 3); break;
                case 4: sCustomStage = Ps_Wrap(sCustomStage + 1, 0, 34); break;
                case 5: sCustomBgm = Ps_Wrap(sCustomBgm + 1, 0, 19); Bgm_Play(PORT_BGM_FILE(sCustomBgm)); break;
                case 6: sCustomMod = Ps_Wrap(sCustomMod + 1, 0, 3); break;
                }
                Snd_PlaySe(1, 0);
            }

            if (p0 & PADG_CROSS) {
                PcBattle battle;
                memset(&battle, 0, sizeof(battle));
                battle.mode = (sCustomFormat == 1) ? PC_TAG : (sCustomFormat == 2) ? PC_DUEL : PC_RELAY;
                battle.players = 1;
                battle.stage = sCustomStage;
                battle.rules.cpuLevel1=sCustomAi==0?6:sCustomAi==1?14:sCustomAi==2?21:30;
                battle.rules.bgm1=sCustomBgm+1;
                battle.rules.damagePct=sCustomMod==0?100:sCustomMod==1?125:sCustomMod==2?150:200;
                battle.count[0] = (sCustomFormat == 2) ? 1 : (sCustomFormat == 1) ? 5 : 10;
                battle.count[1] = (sCustomFormat == 2) ? 1 : (sCustomFormat == 1 && sCustomEnemyNum>5) ? 5 : sCustomEnemyNum;
                Challenge_GenerateCustomEnemies(battle.fighter[1], battle.count[1], sCustomArchetype);

                Snd_PlaySe(1, 1);
                if (Challenge_Pick(&battle, -1)) {
                    if(!Port_PcRosterQueue(&battle)) {Snd_PlaySe(1,7);continue;}
                    result = 1;
                    done = 1;
                    break;
                }
            }
        }

        if (p0 & PADG_CIRCLE) {
            Snd_PlaySe(1, 2);
            result = 0;
            done = 1;
            break;
        }

        Challenge_DrawReference(activeTab, floorCur, clears, bestSurvival);

        Font_FlushAll();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        frameAge++;
    }

    ColorFade_StartOut(0, 0, 0, 12);
    while (!ColorFade_IsOutDone()) {
        Gfx_BeginFrame();
        ColorFade_Update();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
    }

    Challenge_ResourcesEnd();
    gPortNativeRosterActive=savedPointerActive;
    Ps_SetTextKind(0);Ps_SetShift(0);
    return result;
}
#endif

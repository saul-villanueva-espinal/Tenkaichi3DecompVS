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
extern s32 Rand_Range(s32 max);
extern void Font_FlushAll(void);
extern s32 Port_Setting(const char *name, int def);
extern void Port_SettingSave(const char *name, int val);
extern void Port_SettingsWrite(void);
extern int Port_RosterPointerRead(int *x, int *y);
extern s32 Snd_PlaySe(s32 bank, s32 se);
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
        "Carga de Ki Veloz (+50%) | Sin limite de tiempo",
        2, 0, 0, 5,
        { 132, 130, 133, 82, 83 },
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
        { 140, 141, 142, 139, 144, 147, 148 },
        { 0, 0, 0, 0, 0, 0, 0 }
    },
    {
        "PISO 3",
        "El Gran Torneo de Cell",
        "Saga de los Androides & Cell",
        "El destino de la humanidad se decide en el Ring de Cell.",
        "Enfrenta al ejercito de la Red Ribbon y al bio-androide supremo.",
        "Regeneracion Bio-Androide | IA Celular Ultra Agresiva",
        4, 6, 10, 9,
        { 156, 157, 154, 155, 153, 158, 159, 160, 161 },
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
        { 163, 164, 165, 166, 167, 168, 169, 170 },
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
        { 186, 191, 196, 200, 105 },
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
    static const char *const sFighterNames[] = {
        "Goku (Inicio)", "Goku (Medio)", "Goku SSJ", "Goku (Fin)", "Goku SSJ",
        "Goku SSJ2", "Goku SSJ3", "Goku GT", "Goku GT SSJ", "Goku GT SSJ3",
        "Goku GT SSJ4", "Goku Nino", "Mono Gigante", "Gohan Nino", "Gohan Joven",
        "Gohan Joven SSJ", "Gohan Joven SSJ2", "Gohan", "Gohan SSJ", "Gohan SSJ2",
        "Gran Saiyaman", "Gohan Definitivo", "Piccolo (Inicio)", "Piccolo (Fin)",
        "Nail", "Krillin", "Yamcha", "Ten Shin Han", "Chaos", "Vegeta (Scouter)",
        "Vegeta Simio Gigante", "Vegeta", "Vegeta SSJ", "Super Vegeta", "Vegeta 2da",
        "Vegeta 2da SSJ", "Vegeta 2da SSJ2", "Majin Vegeta", "Vegeta SSJ4",
        "Trunks Espada", "Trunks Espada SSJ", "Trunks Combate", "Trunks SSJ",
        "Super Trunks", "Trunks Nino", "Trunks Nino SSJ", "Goten", "Goten SSJ",
        "Gotenks", "Gotenks SSJ", "Gotenks SSJ3", "Vegetto", "Super Vegetto",
        "Super Gogeta", "Gogeta SSJ4", "Mr. Satan", "Videl", "Gran Saiyaman 2",
        "Kaio Shin", "Kibito Shin", "Yajirobe", "Paikuhan", "Tapion", "Pan",
        "Uub", "Majuub", "Mutenroshi", "Mutenroshi Max", "Abuelo Gohan", "Nam",
        "Androide 8", "Rey Vegeta", "Rey Vegeta Simio", "Bardock", "Bardock Simio",
        "Fasha", "Fasha Simio", "Raditz", "Raditz Simio", "Saibaman", "Nappa",
        "Nappa Simio", "Zarbon", "Zarbon Monstruo", "Dodoria", "Cui", "Capitan Ginyu",
        "Recoome", "Burter", "Jeice", "Guldo", "Freezer 1a Forma", "Freezer 2a Forma",
        "Freezer 3a Forma", "Freezer Forma Final", "Freezer 100% Maximo", "Mecha Freezer",
        "Rey Cold", "Appule", "Soldado Freezer", "Androide 16", "Androide 17",
        "Androide 18", "Androide 19", "Dr. Gero (A20)", "Cell 1a Forma", "Cell 2a Forma",
        "Cell Perfecto", "Super Cell Perfecto", "Cell Jr.", "Babidi", "Dabura",
        "Majin Buu Gordo", "Evil Buu", "Super Buu", "Gotenks Buu", "Gohan Buu",
        "Kid Buu", "Garlic Jr.", "Super Garlic Jr.", "Dr. Wheelo", "Turles",
        "Turles Simio", "Slug", "Slug Gigante", "Salza", "Cooler", "Cooler Final",
        "Metal Cooler", "Androide 13", "Fusion Androide 13", "Broly", "Broly SSJ",
        "Broly SSJ Legendario", "Zangya", "Bojack", "Bojack Max", "Janemba",
        "Super Janemba", "Hildegarn", "Baby Vegeta", "Super Baby 1", "Super Baby 2",
        "Super Baby Simio", "Super 17", "Nuova Shenron", "Syn Shenron", "Omega Shenron"
    };
    if (id >= 0 && id < (s32)(sizeof(sFighterNames) / sizeof(sFighterNames[0]))) {
        return sFighterNames[id];
    }
    return Port_PcRosterName(id);
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

#define CUSTOM_SETTINGS_COUNT 8

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

static void Challenge_DrawBg(void) {
    s32 k;
    /* Deep space dark navy gradient */
    Ps_Rect(0, 0, 512, 448, Ps_Rgba(8, 14, 26, 255), Ps_Rgba(4, 7, 14, 255));
    /* Ambient diagonal beams */
    Ps_Quad(0, 0, 260, 0, 0, 260, 140, 140, Ps_Rgba(0, 180, 255, 18), Ps_Rgba(0, 180, 255, 0), Ps_Rgba(0, 180, 255, 0), Ps_Rgba(0, 180, 255, 0));
    Ps_Quad(250, 448, 512, 240, 512, 448, 380, 448, Ps_Rgba(255, 120, 20, 16), Ps_Rgba(255, 120, 20, 0), Ps_Rgba(255, 120, 20, 0), Ps_Rgba(255, 120, 20, 0));

    /* Top gold border and bottom action bar border */
    Ps_Rect(0, 44, 512, 46, Ps_Rgba(255, 196, 56, 220), Ps_Rgba(255, 120, 20, 220));
    Ps_Rect(0, 404, 512, 406, Ps_Rgba(0, 180, 255, 180), Ps_Rgba(0, 90, 160, 180));
}

static void Challenge_DrawHeader(s32 activeTab) {
    s32 i;
    u32 gold = Ps_Rgba(255, 204, 50, 255);
    u32 cyan = Ps_Rgba(0, 210, 255, 255);

    /* Header title & subtitle */
    Ps_Text(18, 8, "TORRE DE DESAFIOS Y BATALLAS PERSONALIZADAS", 0.60f, 476, 1);
    Ps_Text(20, 27, "MODO OFFLINE REVOLUCIONARIO - MOTOR DE RELEVOS Y TAG TEAM 10v10", 0.33f, 474, 2);

    /* Tabs Bar */
    {
        static const char *const sTabLabels[CHALLENGE_TABS] = {
            "[1] TORRE DE SAGAS",
            "[2] SUPERVIVENCIA 10v10",
            "[3] BATALLA PERSONALIZADA"
        };
        f32 tabW = 484.0f / CHALLENGE_TABS;
        for (i = 0; i < CHALLENGE_TABS; i++) {
            f32 tx = 14.0f + i * tabW;
            s32 sel = (i == activeTab);
            if (sel) {
                Ps_Rect(tx, 50, tx + tabW - 4, 72, gold, Ps_Rgba(220, 110, 10, 240));
                Ps_Rect(tx + 1, 51, tx + tabW - 5, 71, Ps_Rgba(16, 24, 44, 230), Ps_Rgba(8, 12, 24, 240));
                Ps_Text(tx + 10, 55, (char *)sTabLabels[i], 0.38f, tabW - 20, 1);
            } else {
                Ps_Rect(tx, 52, tx + tabW - 4, 70, Ps_Rgba(25, 38, 60, 150), Ps_Rgba(12, 18, 30, 170));
                Ps_Text(tx + 10, 56, (char *)sTabLabels[i], 0.34f, tabW - 20, 3);
            }
        }
    }
}

static void Challenge_GenerateSurvivalEnemies(PcFighter *outFighters, s32 count) {
    s32 i;
    for (i = 0; i < count && i < 10; i++) {
        outFighters[i].chara = Rand_Range(PORT_DISC_CHARAS);
        outFighters[i].costume = Rand_Range(2);
    }
}

static void Challenge_GenerateCustomEnemies(PcFighter *outFighters, s32 count, s32 archetype) {
    s32 i;
    static const s32 sZWarriors[] = { 0, 3, 14, 17, 21, 23, 25, 26, 27, 31, 39, 41, 48, 51, 53 };
    static const s32 sVillains[] = { 82, 130, 133, 139, 147, 160, 161, 167, 170, 174, 180, 186, 191 };
    static const s32 sAndroids[] = { 153, 154, 155, 156, 157, 158, 159, 160, 161, 182, 183, 197 };
    static const s32 sGods[] = { 186, 191, 196, 200, 105, 107, 52, 6, 10, 21, 59, 111, 112 };

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
    u32 gold = Ps_Rgba(255, 204, 50, 255);
    u32 cyan = Ps_Rgba(0, 210, 255, 255);
    u32 green = Ps_Rgba(50, 255, 100, 255);

    Bgm_Play(PORT_BGM_FILE(0)); /* Epic BGM */
    ColorFade_StartIn(0, 0, 0, 15);

    while (!done) {
        u32 p0, r0;
        s32 mx, my, click, i, k;

        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();

        p0 = gPad[0].gamePressed;
        r0 = gPad[0].gameRepeat | p0;
        if (gProgress->flags & MPROG_FREEZE) p0 = r0 = 0;

        Ps_PageSet();
        click = Port_RosterPointerRead(&mx, &my);

        /* Mouse interactions */
        if (click) {
            /* Click on top tabs (y: 50..72) */
            if (my >= Ps_Y(50) && my <= Ps_Y(72) && mx >= Ps_X(14) && mx <= Ps_X(498)) {
                s32 t = (s32)((mx - Ps_X(14)) * CHALLENGE_TABS / (Ps_X(498) - Ps_X(14)));
                if (t >= 0 && t < CHALLENGE_TABS) {
                    activeTab = t;
                    Snd_PlaySe(1, 0);
                }
            }
            /* Click on floor list items in Tab 0 */
            if (activeTab == 0 && mx >= Ps_X(14) && mx <= Ps_X(198)) {
                for (i = 0; i < CHALLENGE_FLOORS; i++) {
                    f32 fy = 82.0f + i * 62.0f;
                    if (my >= Ps_Y(fy) && my <= Ps_Y(fy + 58)) {
                        if (floorCur == i) p0 |= PADG_CROSS;
                        else { floorCur = i; Snd_PlaySe(1, 0); }
                    }
                }
            }
            /* Click bottom action buttons (y: 408..440) */
            if (my >= Ps_Y(408) && my <= Ps_Y(440)) {
                if (mx < Ps_X(220)) p0 |= PADG_CROSS;
                else if (mx < Ps_X(360)) p0 |= PADG_CIRCLE;
                else p0 |= PADG_SQUARE;
            }
        }

        /* Pad navigation */
        if (p0 & (PADG_L1 | PADG_R1)) {
            activeTab = Ps_Wrap(activeTab + ((p0 & PADG_L1) ? -1 : 1), 0, CHALLENGE_TABS - 1);
            Snd_PlaySe(1, 0);
        }

        if (activeTab == 0) {
            /* TAB 0: TORRE DE SAGAS */
            s32 old = floorCur;
            if (r0 & (PADG_UP | PADG_UP2)) floorCur--;
            else if (r0 & (PADG_DOWN | PADG_DOWN2)) floorCur++;
            floorCur = Ps_Wrap(floorCur, 0, CHALLENGE_FLOORS - 1);
            if (floorCur != old) Snd_PlaySe(1, 0);

            if (p0 & PADG_CROSS) {
                const ChallengeFloorDef *fl = &sFloors[floorCur];
                PcBattle battle;
                memset(&battle, 0, sizeof(battle));
                battle.mode = PC_RELAY;
                battle.players = 1;
                battle.stage = fl->stage;
                battle.count[0] = 5; /* Player chooses up to 5 warriors */
                battle.count[1] = fl->enemyCount;
                for (i = 0; i < fl->enemyCount; i++) {
                    battle.fighter[1][i].chara = fl->enemies[i];
                    battle.fighter[1][i].costume = fl->costumes[i];
                }

                Snd_PlaySe(1, 1);
                /* Launch team selection with preset enemies (kind == 3) */
                if (Port_NativeRosterPick(&battle, 3, 0)) {
                    clears |= (1 << floorCur);
                    Port_SettingSave("challenge_cleared", clears);
                    Port_SettingsWrite();
                    Port_PcRosterQueue(&battle);
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
                battle.count[0] = 10; /* Full 10 warriors for survival */
                battle.count[1] = 10;
                Challenge_GenerateSurvivalEnemies(battle.fighter[1], 10);

                Snd_PlaySe(1, 1);
                if (Port_NativeRosterPick(&battle, 3, 0)) {
                    bestSurvival++;
                    Port_SettingSave("challenge_best_waves", bestSurvival);
                    Port_SettingsWrite();
                    Port_PcRosterQueue(&battle);
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
                battle.count[0] = (sCustomFormat == 2) ? 1 : (sCustomFormat == 1) ? 5 : 10;
                battle.count[1] = (sCustomFormat == 2) ? 1 : sCustomEnemyNum;
                Challenge_GenerateCustomEnemies(battle.fighter[1], battle.count[1], sCustomArchetype);

                Snd_PlaySe(1, 1);
                if (Port_NativeRosterPick(&battle, 3, 0)) {
                    Port_PcRosterQueue(&battle);
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

        /* ------------------ RENDERING ------------------ */
        Challenge_DrawBg();
        Challenge_DrawHeader(activeTab);

        if (activeTab == 0) {
            /* TAB 0: Floor cards on left, Mission Briefing on right */
            const ChallengeFloorDef *fl = &sFloors[floorCur];
            for (i = 0; i < CHALLENGE_FLOORS; i++) {
                f32 fy = 82.0f + i * 62.0f;
                s32 isCur = (i == floorCur);
                s32 isClear = (clears & (1 << i));

                if (isCur) {
                    Ps_Rect(14, fy, 202, fy + 58, gold, Ps_Rgba(255, 120, 10, 255));
                    Ps_Rect(16, fy + 2, 200, fy + 56, Ps_Rgba(20, 32, 60, 240), Ps_Rgba(10, 16, 32, 250));
                } else {
                    Ps_Rect(14, fy, 202, fy + 58, Ps_Rgba(30, 45, 75, 160), Ps_Rgba(15, 22, 40, 180));
                    Ps_Rect(16, fy + 2, 200, fy + 56, Ps_Rgba(12, 18, 30, 220), Ps_Rgba(6, 10, 18, 240));
                }

                Ps_Text(22, fy + 6, (char *)sFloors[i].floorTag, 0.46f, 70, isCur ? 1 : 2);
                Ps_Text(22, fy + 23, (char *)sFloors[i].title, 0.36f, 172, isCur ? 1 : 0);

                /* Status & Difficulty */
                if (isClear) Ps_Text(100, fy + 6, "COMPLETADO", 0.32f, 90, 5);
                else Ps_Text(120, fy + 6, "PENDIENTE", 0.30f, 70, 3);

                for (k = 0; k < 5; k++) {
                    u32 sCol = (k < sFloors[i].difficulty) ? gold : Ps_Rgba(60, 80, 110, 180);
                    Challenge_Star(26.0f + k * 12.0f, fy + 45.0f, 4.0f, sCol);
                }
            }

            /* Right Mission Briefing Box (X: 210 to 498, Y: 82 to 398) */
            Ps_Rect(210, 82, 498, 398, Ps_Rgba(255, 196, 56, 180), Ps_Rgba(0, 160, 240, 180));
            Ps_Rect(212, 84, 496, 396, Ps_Rgba(12, 20, 38, 250), Ps_Rgba(6, 10, 20, 255));

            /* Header bar inside briefing */
            Ps_Rect(212, 84, 496, 116, Ps_Rgba(25, 42, 80, 240), Ps_Rgba(12, 20, 42, 250));
            Ps_Text(224, 88, (char *)fl->title, 0.52f, 260, 1);
            Ps_Text(224, 104, (char *)fl->saga, 0.34f, 260, 2);

            /* Lore text */
            Ps_Text(224, 126, (char *)fl->desc1, 0.35f, 264, 0);
            Ps_Text(224, 142, (char *)fl->desc2, 0.35f, 264, 0);

            /* Enemy Squad Roster Chips (Y: 168..270) */
            {
                char cntBuf[80];
                sprintf(cntBuf, "ESCUADRON RIVAL DE LA CPU (%d LUCHADORES):", fl->enemyCount);
                Ps_Text(224, 168, cntBuf, 0.38f, 264, 1);

                for (i = 0; i < fl->enemyCount; i++) {
                    f32 cx = 224.0f + (i % 2) * 132.0f;
                    f32 cy = 188.0f + (i / 2) * 20.0f;
                    Ps_Rect(cx, cy, cx + 128, cy + 18, Ps_Rgba(30, 50, 90, 200), Ps_Rgba(15, 25, 50, 220));
                    Ps_Disc(cx + 8, cy + 9, 4, gold, Ps_Rgba(255, 120, 10, 255));
                    Ps_Text(cx + 16, cy + 2, (char *)Challenge_CharaName(fl->enemies[i]), 0.32f, 108, 0);
                }
            }

            /* Combat Arena & Music (Y: 295..340) */
            {
                char arenaBuf[120], bgmBuf[120];
                sprintf(arenaBuf, "ESCENARIO:  %s", sStageNames[fl->stage % 35]);
                sprintf(bgmBuf,   "BGM MUSICA: %s", sBgmNames[fl->bgm % 20]);

                Ps_Rect(224, 296, 484, 338, Ps_Rgba(16, 28, 54, 220), Ps_Rgba(8, 14, 28, 240));
                Ps_Text(232, 302, arenaBuf, 0.34f, 244, 2);
                Ps_Text(232, 320, bgmBuf, 0.34f, 244, 2);
            }

            /* Modifier Alert Box (Y: 346..388) */
            Ps_Rect(224, 346, 484, 388, Ps_Rgba(255, 140, 20, 220), Ps_Rgba(180, 80, 10, 240));
            Ps_Rect(225, 347, 483, 387, Ps_Rgba(35, 15, 5, 245), Ps_Rgba(20, 8, 4, 255));
            Ps_Text(232, 352, "CONDICION ESPECIAL DEL PISO:", 0.34f, 244, 1);
            Ps_Text(232, 368, (char *)fl->modifier, 0.32f, 244, 4);
        } else if (activeTab == 1) {
            /* TAB 1: MODO SUPERVIVENCIA 10v10 */
            /* Left Trophy & Rules Box */
            Ps_Rect(14, 82, 220, 398, Ps_Rgba(255, 196, 56, 180), Ps_Rgba(255, 120, 20, 180));
            Ps_Rect(16, 84, 218, 396, Ps_Rgba(12, 18, 32, 250), Ps_Rgba(6, 9, 18, 255));

            /* Golden Star Trophy Emblem */
            Ps_Disc(117, 138, 36, gold, Ps_Rgba(255, 120, 10, 255));
            Ps_Disc(117, 138, 30, Ps_Rgba(16, 26, 48, 255), Ps_Rgba(8, 14, 26, 255));
            Challenge_Star(117, 138, 18, gold);

            /* Record Waves */
            {
                char recBuf[80];
                sprintf(recBuf, "%d OLEADAS", bestSurvival);
                Ps_Text(24, 188, "RECORD ACTUAL DE VICTORIAS:", 0.32f, 186, 1);
                Ps_Text(46, 206, recBuf, 0.65f, 160, 1);
            }

            /* Rules list */
            Ps_Text(24, 246, "REGLAS DE SUPERVIVENCIA:", 0.36f, 186, 2);
            Ps_Text(24, 268, "- Relevos continuos de 10 luchadores.", 0.31f, 186, 0);
            Ps_Text(24, 286, "- La salud de tu equipo se conserva.", 0.31f, 186, 0);
            Ps_Text(24, 304, "- La CPU aumenta su nivel progresivo.", 0.31f, 186, 0);
            Ps_Text(24, 322, "- Roster completo de 290 guerreros.", 0.31f, 186, 0);
            Ps_Text(24, 350, "Triangulo: Reiniciar record personal.", 0.30f, 186, 3);

            /* Right Action Box */
            Ps_Rect(228, 82, 498, 398, Ps_Rgba(0, 210, 255, 180), Ps_Rgba(0, 120, 220, 180));
            Ps_Rect(230, 84, 496, 396, Ps_Rgba(10, 18, 36, 250), Ps_Rgba(5, 9, 18, 255));

            Ps_Text(244, 100, "DESAFIO DE RESISTENCIA INFINITA 10v10", 0.52f, 240, 1);
            Ps_Text(244, 124, "Lleva al limite tus habilidades en el modo definitivo.", 0.36f, 240, 0);
            Ps_Text(244, 142, "Elige a tus diez mejores combatientes y resiste", 0.36f, 240, 0);
            Ps_Text(244, 160, "el embate de escuadrones rivales implacables.", 0.36f, 240, 0);

            Ps_Rect(244, 200, 482, 310, Ps_Rgba(20, 36, 68, 220), Ps_Rgba(10, 18, 34, 240));
            Ps_Text(256, 212, "CONFIGURACION DEL DESAFIO:", 0.40f, 210, 2);
            Ps_Text(256, 234, "Equipo del Jugador:  10 Combatientes", 0.34f, 210, 0);
            Ps_Text(256, 254, "Equipo Rival CPU:    10 Invasores por Oleada", 0.34f, 210, 0);
            Ps_Text(256, 274, "Escenario de Batalla: Arena Sorpresa Aleatoria", 0.34f, 210, 0);

            Ps_Rect(244, 330, 482, 380, gold, Ps_Rgba(255, 120, 10, 255));
            Ps_Rect(246, 332, 480, 378, Ps_Rgba(16, 24, 44, 240), Ps_Rgba(8, 12, 24, 255));
            Ps_Text(260, 346, "PULSA [X] PARA ELEGIR TUS 10 GUERREROS", 0.42f, 210, 1);
        } else {
            /* TAB 2: BATALLA PERSONALIZADA (Editor) */
            /* Left Settings Column (X: 14 to 290) */
            Ps_Rect(14, 82, 290, 398, Ps_Rgba(0, 210, 255, 180), Ps_Rgba(0, 120, 220, 180));
            Ps_Rect(16, 84, 288, 396, Ps_Rgba(10, 16, 30, 250), Ps_Rgba(5, 8, 16, 255));

            {
                static const char *const sOptLabels[CUSTOM_SETTINGS_COUNT] = {
                    "1. Formato de Batalla",
                    "2. Cantidad de Rivales",
                    "3. Arquetipo Enemigo",
                    "4. Dificultad de la IA",
                    "5. Escenario de Combate",
                    "6. Banda Sonora (BGM)",
                    "7. Modificador de Danio",
                    "8. Iniciar Combate Ahora"
                };

                for (i = 0; i < CUSTOM_SETTINGS_COUNT; i++) {
                    f32 oy = 90.0f + i * 38.0f;
                    s32 isCur = (i == sCustomCur);

                    if (isCur) {
                        Ps_Rect(22, oy, 282, oy + 34, gold, Ps_Rgba(255, 120, 10, 255));
                        Ps_Rect(24, oy + 2, 280, oy + 32, Ps_Rgba(20, 36, 68, 240), Ps_Rgba(10, 18, 34, 250));
                    } else {
                        Ps_Rect(22, oy, 282, oy + 34, Ps_Rgba(20, 30, 52, 160), Ps_Rgba(10, 16, 28, 180));
                    }

                    Ps_Text(30, oy + 4, (char *)sOptLabels[i], 0.32f, 240, isCur ? 1 : 2);

                    /* Value text */
                    {
                        char valBuf[80];
                        valBuf[0] = 0;
                        switch (i) {
                        case 0: Ps_Copy(valBuf, sFormatNames[sCustomFormat], 80); break;
                        case 1: sprintf(valBuf, "%d Guerreros", sCustomEnemyNum); break;
                        case 2: Ps_Copy(valBuf, sArchetypeNames[sCustomArchetype], 80); break;
                        case 3: Ps_Copy(valBuf, sAiNames[sCustomAi], 80); break;
                        case 4: Ps_Copy(valBuf, sStageNames[sCustomStage % 35], 80); break;
                        case 5: Ps_Copy(valBuf, sBgmNames[sCustomBgm % 20], 80); break;
                        case 6: Ps_Copy(valBuf, sModNames[sCustomMod], 80); break;
                        case 7: Ps_Copy(valBuf, "-> Pulsa [X] para confirmar", 80); break;
                        }
                        Ps_Text(30, oy + 18, valBuf, 0.30f, 240, isCur ? 0 : 3);
                    }
                }
            }

            /* Right Live Summary Box (X: 298 to 498) */
            Ps_Rect(298, 82, 498, 398, gold, Ps_Rgba(255, 120, 10, 255));
            Ps_Rect(300, 84, 496, 396, Ps_Rgba(12, 20, 38, 250), Ps_Rgba(6, 10, 20, 255));

            Ps_Text(312, 96, "RESUMEN DE LA BATALLA:", 0.44f, 180, 1);
            Ps_Text(312, 122, "Modo Seleccionado:", 0.33f, 180, 2);
            Ps_Text(312, 138, (char *)sFormatNames[sCustomFormat], 0.31f, 180, 0);

            Ps_Text(312, 164, "Formacion Rival:", 0.33f, 180, 2);
            {
                char rivalBuf[80];
                sprintf(rivalBuf, "%d combatientes (%s)", sCustomEnemyNum, sArchetypeNames[sCustomArchetype]);
                Ps_Text(312, 180, rivalBuf, 0.31f, 180, 0);
            }

            Ps_Text(312, 206, "Escenario de Duelo:", 0.33f, 180, 2);
            Ps_Text(312, 222, (char *)sStageNames[sCustomStage % 35], 0.31f, 180, 0);

            Ps_Text(312, 248, "Banda Sonora (BGM):", 0.33f, 180, 2);
            Ps_Text(312, 264, (char *)sBgmNames[sCustomBgm % 20], 0.31f, 180, 0);

            Ps_Text(312, 290, "Condicion de Danio:", 0.33f, 180, 2);
            Ps_Text(312, 306, (char *)sModNames[sCustomMod], 0.31f, 180, 4);

            Ps_Rect(310, 336, 486, 384, Ps_Rgba(0, 210, 255, 200), Ps_Rgba(0, 120, 220, 220));
            Ps_Rect(311, 337, 485, 383, Ps_Rgba(10, 18, 36, 240), Ps_Rgba(5, 9, 18, 255));
            Ps_Text(320, 350, "PULSA [X] PARA SELECCIONAR", 0.38f, 160, 1);
            Ps_Text(320, 364, "A TUS LUCHADORES DEL EQUIPO", 0.32f, 160, 2);
        }

        /* ------------------ BOTTOM ACTION BAR ------------------ */
        Ps_Rect(0, 406, 512, 448, Ps_Rgba(8, 12, 22, 240), Ps_Rgba(4, 6, 12, 255));
        Ps_Text(18, 416, "Cruceta: navegar  |  X: confirmar desafio  |  O: volver  |  L1/R1: cambiar pestanias", 0.34f, 476, 3);

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

    return result;
}
#endif

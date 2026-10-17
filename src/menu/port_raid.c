#include "common.h"
#include "menu/port_screen.h"
#include "menu/tbo_art.h"
#include "port/include/port_coop.h"
#include "port/include/port_fighters.h"
#include "port/include/port_pc_battle.h"
#include "sys/pad.h"

#ifdef PORT
/*
 * PC only: Modo Raid Boss Online Cooperativo (4 Jugadores vs 1 Boss Colosal)
 * Permite a 4 jugadores unirse en tiempo real por red local/directa UDP
 * y cooperar simultaneamente para derrotar a jefes titanicos de Dragon Ball Z.
 */

extern void Job_Run(void);
extern void Gfx_BeginFrame(void);
extern void Gfx_EndFrame(s32 arg);
extern void Dma_Flush(void);
extern void Dma_ResetBuffers(void);
extern void File_Stub264D90(void);
extern void ColorFade_StartIn(s32 r, s32 g, s32 b, s32 frames);
extern void ColorFade_StartOut(s32 r, s32 g, s32 b, s32 frames);
extern void ColorFade_Update(void);
extern void ColorFade_Draw(void);
extern s32 ColorFade_IsInDone(void);
extern s32 ColorFade_IsOutDone(void);
extern u32 Rand_Range(u32 n);
extern s32 Snd_PlaySe(u32 mask, s32 id);
extern void Bgm_Play(s32 id);
extern void Battle_ClearWork(void);
extern int Port_PcRosterQueue(const PcBattle *b);
extern void Port_PcNativeRules(int players, int training, int bgm);
extern int Port_NativeRosterPick(PcBattle *out, s32 kind, s32 dpMax);
extern int Port_PcBattleBuild(void);

typedef struct {
    char name[40];
    char subtitle[60];
    s32 chara;
    s32 stage;
    s32 bgm;
    s32 hp;
    char specialPower[80];
} RaidBossDef;

static const RaidBossDef sBosses[] = {
    {
        "Gran Mono Vegeta (Oozaru)",
        "El orgullo Saiyajin convertido en bestia colosal",
        30, /* Vegeta Scouter Great Ape */
        0,  /* Rocky Area */
        12, /* BGM */
        50000,
        "Super Armadura implacable, canon bucal de energia y terremoto."
    },
    {
        "Hildegarn (Coloso Fantasmal)",
        "El monstruo milenario de la destruccion absoluta",
        139, /* Hildegarn */
        18,  /* Ruined Earth */
        15,  /* BGM */
        60000,
        "Teletransporte de humo espectral, rafagas de fuego y fuerza bruta."
    },
    {
        "Super Janemba (Demonio Cósmico)",
        "El soberano de la corrupcion y el desorden dimensional",
        138, /* Super Janemba */
        26,  /* Hell */
        14,  /* BGM */
        55000,
        "Cortes dimensionales, apertura de portales y disparo de espadas."
    },
    {
        "Dr. Wheelo (Fortaleza Mecánica)",
        "El cerebro cientifico con cuerpo de acorazado gigante",
        120, /* Wheelo */
        24,  /* Glacier */
        11,  /* BGM */
        50000,
        "Armadura de adamantium, laser gravitatorio de largo alcance."
    },
    {
        "Broly SSJ Legendario",
        "El poder maximo que se incrementa sin limite alguno",
        133, /* Broly LSSJ */
        3,   /* Wasteland */
        13,  /* BGM */
        65000,
        "Ki infinito inagotable, super armadura e inmunidad a aturdimiento."
    },
    {
        "Super Baby Gran Mono",
        "La mutacion biomecanica del parasito Tsufur",
        143, /* Super Baby Great Ape */
        18,  /* City */
        10,  /* BGM */
        58000,
        "Canon Garlick destructor, rabia desatada y garras aplastantes."
    },
    {
        "Slug Gigante (Monarca Namekiano)",
        "El namekiano corrompido con el poder del tamano titánico",
        124, /* Slug Giant */
        12,  /* Namek */
        9,   /* BGM */
        48000,
        "Brazos elasticos de agarre infinito, onda demoniaca expansiva."
    }
};

#define BOSS_COUNT 7

typedef struct {
    s32 bossIndex;
    s32 connectionMode; /* 0: Local / Practica, 1: Crear Sala (Host UDP), 2: Unirse a Sala (Client UDP) */
    s32 numPlayers;     /* 2, 3 o 4 jugadores */
    s32 difficulty;     /* 0: Guerrero Z (x1.0), 1: Super Saiyan (x1.5), 2: Dios de la Destruccion (x2.5) */
    s32 tab;            /* 0: JEFE, 1: SALA ONLINE, 2: REGLAS */
    s32 cursor;
    s32 ip[5];          /* IP address octets 0..3, and port 4 */
    s32 ipField;
    s32 frames;
    s32 leaving;
    s32 launchBattle;
} RaidSystem;

static RaidSystem *sRd;

static void Rd_Init(void) {
    sRd->bossIndex = 0;
    sRd->connectionMode = 1; /* Host por defecto */
    sRd->numPlayers = 4;     /* 4 Jugadores Coop */
    sRd->difficulty = 1;     /* Super Saiyan */
    sRd->tab = 0;
    sRd->cursor = 0;
    sRd->ip[0] = 127;
    sRd->ip[1] = 0;
    sRd->ip[2] = 0;
    sRd->ip[3] = 1;
    sRd->ip[4] = 29740; /* Default UDP port */
    sRd->ipField = 0;
    sRd->frames = 0;
    sRd->leaving = 0;
    sRd->launchBattle = 0;
}

static void Rd_DrawBossCard(f32 x, f32 y, f32 w, f32 h) {
    const RaidBossDef *b = &sBosses[sRd->bossIndex];
    char hpBuf[32];
    f32 mult = (sRd->difficulty == 0) ? 1.0f : (sRd->difficulty == 1) ? 1.5f : 2.5f;
    s32 actualHp = (s32)(b->hp * mult);

    Ps_Panel(TBO_KIT_WINDOW, x, y, x + w, y + h, 0x85);
    Ps_Heading(x + 16, y + 16, (char *)b->name, 0.85f, w - 32, 0x80);
    Ps_Text(x + 16, y + 42, (char *)b->subtitle, 0.44f, w - 32, 3);

    /* HP & Stat Bar */
    Ps_Pill(TBO_KIT_BAR, x + 16, y + 66, x + w - 16, y + 110, 0x80);
    Ps_Text(x + 28, y + 74, "SALUD TOTAL DEL JEFE:", 0.44f, 150.0f, 0);
    sprintf(hpBuf, "%d HP", actualHp);
    Ps_Text(x + 180, y + 72, hpBuf, 0.72f, 140.0f, 2);

    /* Special abilities */
    Ps_Text(x + 16, y + 124, "PODER ESPECIAL:", 0.46f, 140.0f, 1);
    Ps_Text(x + 16, y + 144, (char *)b->specialPower, 0.40f, w - 32, 0);

    /* Boss catalog selector */
    {
        char selBuf[48];
        sprintf(selBuf, "Jefe %d de %d  (Usa Izquierda / Derecha para cambiar)", sRd->bossIndex + 1, BOSS_COUNT);
        Ps_Text(x + 16, y + h - 28, selBuf, 0.40f, w - 32, 5);
    }
}

static void Rd_DrawRoomCard(f32 x, f32 y, f32 w, f32 h) {
    static const char *sConnModes[] = {
        "PRACTICA LOCAL (Solo / Con aliados CPU)",
        "CREAR SALA ONLINE (Anfitrion UDP)",
        "UNIRSE A SALA ONLINE (Cliente UDP)"
    };
    static const char *sDiffNames[] = {
        "Guerrero Z  (Salud x1.0 / Dano x1.0)",
        "Super Saiyan  (Salud x1.5 / Dano x1.5 / Ki Veloz)",
        "Dios de la Destruccion  (Salud x2.5 / Super Armadura / Dano x2.0)"
    };

    Ps_Panel(TBO_KIT_WINDOW, x, y, x + w, y + h, 0x85);
    Ps_Heading(x + 16, y + 16, "CONFIGURACION DE SALA COOPERATIVA", 0.75f, w - 32, 0x80);

    /* Connection mode */
    Ps_Text(x + 16, y + 44, "Modo de Conexion:", 0.45f, 140.0f, 1);
    Ps_Pill(sRd->cursor == 0 ? TBO_KIT_SEL : TBO_KIT_BAR, x + 16, y + 62, x + w - 16, y + 90, 0x80);
    Ps_Text(x + 28, y + 68, (char *)sConnModes[sRd->connectionMode], 0.50f, w - 50, sRd->cursor == 0 ? 1 : 0);

    /* Num players */
    Ps_Text(x + 16, y + 102, "Jugadores Aliados (Escuadron):", 0.45f, 180.0f, 1);
    Ps_Pill(sRd->cursor == 1 ? TBO_KIT_SEL : TBO_KIT_BAR, x + 16, y + 120, x + w - 16, y + 148, 0x80);
    {
        char pBuf[48];
        sprintf(pBuf, "%d Jugadores Cooperando vs 1 Jefe", sRd->numPlayers);
        Ps_Text(x + 28, y + 126, pBuf, 0.52f, w - 50, sRd->cursor == 1 ? 1 : 0);
    }

    /* Difficulty */
    Ps_Text(x + 16, y + 160, "Intensidad del Raid:", 0.45f, 140.0f, 1);
    Ps_Pill(sRd->cursor == 2 ? TBO_KIT_SEL : TBO_KIT_BAR, x + 16, y + 178, x + w - 16, y + 206, 0x80);
    Ps_Text(x + 28, y + 184, (char *)sDiffNames[sRd->difficulty], 0.46f, w - 50, sRd->cursor == 2 ? 1 : 0);

    /* Network IP & Port Box */
    if (sRd->connectionMode != 0) {
        s32 i;
        char ipStr[64];
        Ps_Text(x + 16, y + 220, sRd->connectionMode == 1 ? "Tu Direccion y Puerto UDP (para tus companeros):" : "Direccion IP del Anfitrion y Puerto:", 0.42f, w - 32, 5);
        for (i = 0; i < 5; i++) {
            f32 bx = x + 16 + i * ((w - 32) / 5.0f);
            f32 bw = ((w - 32) / 5.0f) - 6.0f;
            s32 isField = (sRd->cursor == 3 && sRd->ipField == i);
            Ps_Pill(isField ? TBO_KIT_SEL : TBO_KIT_BAR, bx, y + 240, bx + bw, y + 270, 0x80);
            sprintf(ipStr, "%d", sRd->ip[i]);
            Ps_Align(1);
            Ps_Text(bx + bw * 0.5f, y + 247, ipStr, 0.55f, bw, isField ? 1 : 0);
            Ps_Align(0);
        }
        Ps_Text(x + 16, y + 280, "Usa L1/R1 para cambiar campo IP  |  Arriba/Abajo para cambiar valor", 0.36f, w - 32, 3);
    }
}

s32 PortRaid_Run(void) {
    s32 result = 0;

    sRd = Heap_Alloc(sizeof(RaidSystem), 0x20, 0, 2);
    memset(sRd, 0, sizeof(RaidSystem));
    Rd_Init();

    Bgm_Play(0x10B16 + 2);
    ColorFade_StartIn(0, 0, 0, 0x14);

    while (1) {
        Pad *pad = &gPad[0];
        s32 live;

        Job_Run();
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        live = !sRd->leaving && ColorFade_IsInDone();

        Ps_PromptsClear();
        Ps_Prompt(0, "Elegir Equipo & Iniciar");
        Ps_Prompt(1, "Cambiar Opcion");
        Ps_Prompt(2, "Volver");
        Ps_Prompt(3, "Cambiar Tab");

        if (live) {
            /* Switch tabs with TRIANGLE */
            if (pad->gamePressed & PADG_TRIANGLE) {
                sRd->tab = (sRd->tab == 0) ? 1 : 0;
                Snd_PlaySe(1, 0);
            }

            if (sRd->tab == 0) {
                /* Boss selection */
                if (pad->gamePressed & (PADG_LEFT | PADG_LEFT2)) {
                    sRd->bossIndex = (sRd->bossIndex + BOSS_COUNT - 1) % BOSS_COUNT;
                    Snd_PlaySe(1, 0);
                } else if (pad->gamePressed & (PADG_RIGHT | PADG_RIGHT2)) {
                    sRd->bossIndex = (sRd->bossIndex + 1) % BOSS_COUNT;
                    Snd_PlaySe(1, 0);
                }
            } else {
                /* Room setup navigation */
                if (pad->gamePressed & (PADG_UP | PADG_UP2)) {
                    sRd->cursor = (sRd->cursor + 3) % 4;
                    Snd_PlaySe(1, 0);
                } else if (pad->gamePressed & (PADG_DOWN | PADG_DOWN2)) {
                    sRd->cursor = (sRd->cursor + 1) % 4;
                    Snd_PlaySe(1, 0);
                }

                if (sRd->cursor == 0) {
                    if (pad->gamePressed & (PADG_LEFT | PADG_RIGHT | PADG_SQUARE)) {
                        sRd->connectionMode = (sRd->connectionMode + 1) % 3;
                        Snd_PlaySe(1, 0);
                    }
                } else if (sRd->cursor == 1) {
                    if (pad->gamePressed & (PADG_LEFT | PADG_RIGHT | PADG_SQUARE)) {
                        sRd->numPlayers = (sRd->numPlayers == 4) ? 2 : (sRd->numPlayers + 1);
                        Snd_PlaySe(1, 0);
                    }
                } else if (sRd->cursor == 2) {
                    if (pad->gamePressed & (PADG_LEFT | PADG_RIGHT | PADG_SQUARE)) {
                        sRd->difficulty = (sRd->difficulty + 1) % 3;
                        Snd_PlaySe(1, 0);
                    }
                } else if (sRd->cursor == 3) {
                    if (pad->gamePressed & PADG_L1) sRd->ipField = (sRd->ipField + 4) % 5;
                    if (pad->gamePressed & PADG_R1) sRd->ipField = (sRd->ipField + 1) % 5;
                    if (pad->gameRepeat & PADG_UP) {
                        sRd->ip[sRd->ipField] = (sRd->ipField == 4) ? (sRd->ip[4] + 1) : (sRd->ip[sRd->ipField] + 1);
                    } else if (pad->gameRepeat & PADG_DOWN) {
                        sRd->ip[sRd->ipField] = (sRd->ipField == 4) ? (sRd->ip[4] - 1) : (sRd->ip[sRd->ipField] - 1);
                    } else if (pad->gamePressed & PADG_SQUARE) {
                        sRd->ip[sRd->ipField] += 10;
                    }
                    if (sRd->ipField == 4) {
                        if (sRd->ip[4] < 1024) sRd->ip[4] = 29740;
                        if (sRd->ip[4] > 65535) sRd->ip[4] = 65535;
                    } else {
                        if (sRd->ip[sRd->ipField] < 0) sRd->ip[sRd->ipField] = 255;
                        if (sRd->ip[sRd->ipField] > 255) sRd->ip[sRd->ipField] = 0;
                    }
                }
            }

            /* START / CROSS: Pick Team and Launch Raid! */
            if (pad->gamePressed & (PADG_CROSS | PADG_START)) {
                const RaidBossDef *boss = &sBosses[sRd->bossIndex];
                PcBattle battle;
                memset(&battle, 0, sizeof(battle));
                battle.mode = PC_RAID;
                battle.players = sRd->numPlayers;
                battle.count[0] = sRd->numPlayers;
                battle.count[1] = 1;
                battle.fighter[1][0].chara = boss->chara;
                battle.fighter[1][0].costume = 0;
                battle.stage = boss->stage;

                /* Default allies */
                battle.fighter[0][0].chara = 3;  /* Goku Fin */
                battle.fighter[0][1].chara = 31; /* Vegeta */
                battle.fighter[0][2].chara = 21; /* Gohan Definitivo */
                battle.fighter[0][3].chara = 23; /* Piccolo */

                /* Pass to HD Roster so player can customize their squad */
                if (Port_NativeRosterPick(&battle, 3, 0)) {
                    if (sRd->connectionMode == 0) {
                        /* Local Raid battle */
                        Port_PcRosterQueue(&battle);
                        Port_PcNativeRules(sRd->numPlayers, 0, boss->bgm);
                        sRd->launchBattle = 1;
                        result = 1;
                    } else {
                        /* Online 4-Player UDP Coop Launch! */
                        char addr[80];
                        sprintf(addr, "%d.%d.%d.%d", sRd->ip[0], sRd->ip[1], sRd->ip[2], sRd->ip[3]);
                        Port_CoopLaunch(sRd->connectionMode == 1, addr, sRd->ip[4], &battle);
                        result = 1;
                    }
                    sRd->leaving = 1;
                    Snd_PlaySe(1, 1);
                    ColorFade_StartOut(0, 0, 0, 0x14);
                }
            } else if (pad->gamePressed & PADG_CIRCLE) {
                sRd->leaving = 1;
                ColorFade_StartOut(0, 0, 0, 0x14);
                Snd_PlaySe(1, 2);
            }
        }

        /* Rendering */
        Ps_PageSet();
        Ps_ScreenQuad(0, 0, 512, 0, 512, 448, 0, 448,
                      Ps_Rgba(18, 10, 14, 255), Ps_Rgba(30, 14, 22, 255),
                      Ps_Rgba(12, 6, 10, 255), Ps_Rgba(22, 10, 16, 255));

        /* Header */
        Ps_Heading(24.0f, 30.0f, "RAID BOSS ONLINE COOPERATIVO (4v1)", 0.95f, 400.0f, 0x80);
        Ps_Text(24.0f, 50.0f, "4 Guerreros Z en tiempo real vs 1 Jefe Colosal Titánico  |  UDP Direct LAN / Online", 0.38f, 460.0f, 3);

        /* Tabs indicator */
        {
            f32 t0 = (sRd->tab == 0) ? 1.0f : 0.6f;
            f32 t1 = (sRd->tab == 1) ? 1.0f : 0.6f;
            Ps_Pill(sRd->tab == 0 ? TBO_KIT_SEL : TBO_KIT_BAR, 24.0f, 68.0f, 180.0f, 92.0f, 0x80);
            Ps_Text(44.0f, 74.0f, "1. JEFE COLOSAL", 0.50f * t0, 140.0f, sRd->tab == 0 ? 1 : 0);

            Ps_Pill(sRd->tab == 1 ? TBO_KIT_SEL : TBO_KIT_BAR, 190.0f, 68.0f, 360.0f, 92.0f, 0x80);
            Ps_Text(206.0f, 74.0f, "2. SALA COOP & RED", 0.50f * t1, 150.0f, sRd->tab == 1 ? 1 : 0);
        }

        if (sRd->tab == 0) {
            Rd_DrawBossCard(24.0f, 102.0f, 464.0f, 276.0f);
        } else {
            Rd_DrawRoomCard(24.0f, 102.0f, 464.0f, 276.0f);
        }

        Ps_PromptsDraw();
        ColorFade_Draw();
        File_Stub264D90();
        Gfx_EndFrame(1);
        Dma_Flush();

        sRd->frames++;
        if (sRd->leaving && ColorFade_IsOutDone()) {
            break;
        }
    }

    Heap_Free(sRd);
    sRd = NULL;
    Dma_ResetBuffers();
    return result;
}

#endif

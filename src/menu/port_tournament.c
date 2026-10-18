#include "common.h"
#include "menu/port_screen.h"
#include "menu/tbo_art.h"
#include "port_pc_battle.h"
#include "sys/pad.h"

#ifdef PORT
/*
 * Local tournament with an entry to the synchronized direct UDP tournament
 * Soporta llaves interactivas de 8 participantes:
 * Cuartos de Final -> Semifinales -> Gran Final y Coronacion del Campeon del Mundo.
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

typedef struct {
    s32 chara;
    s32 costume;
    char name[32];
    s32 isPlayer;
    s32 seed;
    s32 wins;
    s32 eliminated;
} TourneyFighter;

typedef struct {
    s32 f0; /* fighter index 0 */
    s32 f1; /* fighter index 1 */
    s32 score0;
    s32 score1;
    s32 winner; /* -1: pending, 0: f0 won, 1: f1 won */
    s32 round;  /* 0: cuartos, 1: semis, 2: final */
    s32 status; /* 0: espera, 1: combate activo, 2: finalizado */
} TourneyMatch;

typedef struct {
    TourneyFighter fighters[8];
    TourneyMatch matches[7]; /* 0..3 cuartos, 4..5 semis, 6 final */
    s32 currentMatch;
    s32 selectedMatch;
    s32 stage;
    s32 ringOut;
    s32 timeLimit;
    s32 difficulty;
    s32 tab; /* 0: CUADRO EN VIVO, 1: REGLAS, 2: ESPECTADOR */
    s32 champion;
    s32 frames;
    s32 leaving;
    s32 launchBattle;
} TourneySystem;

static TourneySystem sTrData;
static TourneySystem *sTr = &sTrData;
static s32 sTournamentReady,sTournamentBattle=-1;

static const char *sRivalPoolNames[] = {
    "Goku (Fin)", "Vegeta", "Gohan Definitivo", "Piccolo", 
    "Trunks del Futuro", "Freezer", "Cell Perfecto", "Kid Buu",
    "Broly SSJ", "Gotenks SSJ3", "Vegetto", "Gogeta",
    "Bardock", "Androide 18", "Paikuhan", "Kibitoshin"
};

static const s32 sRivalPoolIds[] = {
    3, 31, 21, 23, 41, 94, 107, 117, 132, 50, 52, 53, 73, 102, 61, 59
};

static void Tr_InitTournament(void) {
    s32 i;
    u8 picked[16];
    memset(picked, 0, sizeof(picked));

    /* Player is Seed 1 */
    sTr->fighters[0].chara = 3; /* Default Goku Fin */
    sTr->fighters[0].costume = 0;
    Ps_Copy(sTr->fighters[0].name, "Jugador 1 (Tu)", 32);
    sTr->fighters[0].isPlayer = 1;
    sTr->fighters[0].seed = 1;
    sTr->fighters[0].wins = 0;
    sTr->fighters[0].eliminated = 0;

    /* Fill other 7 seeds with high profile combatants */
    for (i = 1; i < 8; i++) {
        s32 r;
        do {
            r = Rand_Range(16);
        } while (picked[r]);
        picked[r] = 1;

        sTr->fighters[i].chara = sRivalPoolIds[r];
        sTr->fighters[i].costume = 0;
        Ps_Copy(sTr->fighters[i].name, sRivalPoolNames[r], 32);
        sTr->fighters[i].isPlayer = 0;
        sTr->fighters[i].seed = i + 1;
        sTr->fighters[i].wins = 0;
        sTr->fighters[i].eliminated = 0;
    }

    /* Initialize Matches */
    /* Cuartos de final: 4 matches */
    for (i = 0; i < 4; i++) {
        sTr->matches[i].f0 = i * 2;
        sTr->matches[i].f1 = i * 2 + 1;
        sTr->matches[i].score0 = 0;
        sTr->matches[i].score1 = 0;
        sTr->matches[i].winner = -1;
        sTr->matches[i].round = 0;
        sTr->matches[i].status = (i == 0) ? 1 : 0;
    }
    /* Semifinales: 2 matches */
    for (i = 4; i < 6; i++) {
        sTr->matches[i].f0 = -1;
        sTr->matches[i].f1 = -1;
        sTr->matches[i].score0 = 0;
        sTr->matches[i].score1 = 0;
        sTr->matches[i].winner = -1;
        sTr->matches[i].round = 1;
        sTr->matches[i].status = 0;
    }
    /* Gran Final */
    sTr->matches[6].f0 = -1;
    sTr->matches[6].f1 = -1;
    sTr->matches[6].score0 = 0;
    sTr->matches[6].score1 = 0;
    sTr->matches[6].winner = -1;
    sTr->matches[6].round = 2;
    sTr->matches[6].status = 0;

    sTr->currentMatch = 0;
    sTr->selectedMatch = 0;
    sTr->stage = 27; /* Torneo Mundial Arena */
    sTr->ringOut = 1;
    sTr->timeLimit = 1; /* 90 Segundos */
    sTr->difficulty = 2; /* Fuerte */
    sTr->champion = -1;
}

static void Tr_AdvanceBracket(s32 matchIdx, s32 winnerChoice) {
    TourneyMatch *m = &sTr->matches[matchIdx];
    s32 winnerFighter;

    if (m->winner >= 0 || m->f0 < 0 || m->f1 < 0) return;

    m->winner = winnerChoice;
    m->status = 2; /* Finalizado */
    winnerFighter = (winnerChoice == 0) ? m->f0 : m->f1;
    sTr->fighters[winnerFighter].wins++;

    /* Eliminate loser */
    {
        s32 loserFighter = (winnerChoice == 0) ? m->f1 : m->f0;
        sTr->fighters[loserFighter].eliminated = 1;
    }

    /* Route winner to next round */
    if (matchIdx == 0) sTr->matches[4].f0 = winnerFighter;
    else if (matchIdx == 1) sTr->matches[4].f1 = winnerFighter;
    else if (matchIdx == 2) sTr->matches[5].f0 = winnerFighter;
    else if (matchIdx == 3) sTr->matches[5].f1 = winnerFighter;
    else if (matchIdx == 4) sTr->matches[6].f0 = winnerFighter;
    else if (matchIdx == 5) sTr->matches[6].f1 = winnerFighter;
    else if (matchIdx == 6) {
        sTr->champion = winnerFighter;
        Snd_PlaySe(1, 10);
        return;
    }

    /* Find next pending match */
    {
        s32 k;
        sTr->currentMatch = -1;
        for (k = 0; k < 7; k++) {
            if (sTr->matches[k].winner < 0 && sTr->matches[k].f0 >= 0 && sTr->matches[k].f1 >= 0) {
                sTr->currentMatch = k;
                sTr->matches[k].status = 1;
                break;
            }
        }
        if (sTr->currentMatch >= 0) {
            sTr->selectedMatch = sTr->currentMatch;
        }
    }
}

void PortTournament_RecordResult(s32 won) {
    if(sTournamentBattle>=0)Tr_AdvanceBracket(sTournamentBattle,won?0:1);
    sTournamentBattle=-1;
}

static void Tr_SimulateCpuMatches(void) {
    s32 i;
    s32 simulated = 0;
    for (i = 0; i < 7; i++) {
        TourneyMatch *m = &sTr->matches[i];
        if (m->winner < 0 && m->f0 >= 0 && m->f1 >= 0) {
            /* If this is the human player's match, don't simulate it without player choice */
            if (sTr->fighters[m->f0].isPlayer || sTr->fighters[m->f1].isPlayer) {
                continue;
            }
            {
                s32 w = Rand_Range(2);
                Tr_AdvanceBracket(i, w);
                simulated++;
            }
        }
    }
    if (simulated > 0) {
        Snd_PlaySe(1, 1);
    } else {
        Snd_PlaySe(1, 7);
    }
}

static void Tr_DrawBracketLine(f32 x0, f32 y0, f32 x1, f32 y1, u32 col) {
    Ps_ScreenQuad(x0, y0, x1, y0, x1, y1, x0, y1, col, col, col, col);
}

static void Tr_DrawMatchBox(s32 idx, f32 x, f32 y, f32 w, f32 h) {
    TourneyMatch *m = &sTr->matches[idx];
    s32 isSel = (sTr->selectedMatch == idx);
    s32 isCur = (sTr->currentMatch == idx);
    u32 borderCol = isCur ? Ps_Rgba(255, 215, 0, 240) : (isSel ? Ps_Rgba(80, 200, 255, 230) : Ps_Rgba(40, 60, 90, 180));
    u32 bgCol = Ps_Rgba(16, 22, 34, 230);
    char buf[48];

    /* Match Frame */
    Ps_ScreenQuad(x, y, x + w, y, x + w, y + h, x, y + h, bgCol, bgCol, bgCol, bgCol);
    Ps_ScreenQuad(x, y, x + w, y, x + w, y + 2, x, y + 2, borderCol, borderCol, borderCol, borderCol);
    Ps_ScreenQuad(x, y + h - 2, x + w, y + h - 2, x + w, y + h, x, y + h, borderCol, borderCol, borderCol, borderCol);
    Ps_ScreenQuad(x, y, x + 2, y, x + 2, y + h, x, y + h, borderCol, borderCol, borderCol, borderCol);
    Ps_ScreenQuad(x + w - 2, y, x + w, y, x + w, y + h, x + w - 2, y + h, borderCol, borderCol, borderCol, borderCol);

    /* Participant 0 */
    if (m->f0 >= 0) {
        TourneyFighter *f = &sTr->fighters[m->f0];
        s32 isWinner = (m->winner == 0);
        u32 nameCol = isWinner ? 1 : (f->eliminated ? 4 : (f->isPlayer ? 5 : 0));
        sprintf(buf, "#%d %s", f->seed, f->name);
        Ps_Text(x + 6, y + 4, buf, 0.40f, w - 12, nameCol);
    } else {
        Ps_Text(x + 6, y + 4, "TBD (Ganador)", 0.38f, w - 12, 4);
    }

    /* Middle divider */
    Tr_DrawBracketLine(x + 4, y + (h * 0.5f) - 1, x + w - 4, y + (h * 0.5f) + 1, Ps_Rgba(50, 70, 100, 150));

    /* Participant 1 */
    if (m->f1 >= 0) {
        TourneyFighter *f = &sTr->fighters[m->f1];
        s32 isWinner = (m->winner == 1);
        u32 nameCol = isWinner ? 1 : (f->eliminated ? 4 : (f->isPlayer ? 5 : 0));
        sprintf(buf, "#%d %s", f->seed, f->name);
        Ps_Text(x + 6, y + (h * 0.5f) + 3, buf, 0.40f, w - 12, nameCol);
    } else {
        Ps_Text(x + 6, y + (h * 0.5f) + 3, "TBD (Ganador)", 0.38f, w - 12, 4);
    }
}

static void Tr_DrawBracketView(void) {
    s32 i;
    u32 lineCol = Ps_Rgba(80, 120, 180, 180);
    u32 goldLine = Ps_Rgba(255, 215, 0, 220);

    /* Cuartos de final (Columna Izquierda: 4 cajas) */
    for (i = 0; i < 4; i++) {
        f32 y = 78.0f + i * 74.0f;
        Tr_DrawMatchBox(i, 20.0f, y, 130.0f, 44.0f);

        /* Conectores hacia Semifinales */
        {
            u32 c = (sTr->matches[i].winner >= 0) ? goldLine : lineCol;
            Tr_DrawBracketLine(150.0f, y + 22.0f, 175.0f, y + 24.0f, c);
            if (i % 2 == 0) {
                Tr_DrawBracketLine(173.0f, y + 22.0f, 175.0f, y + 96.0f, c);
            } else {
                Tr_DrawBracketLine(173.0f, y - 52.0f, 175.0f, y + 24.0f, c);
            }
        }
    }

    /* Semifinales (Columna Central: 2 cajas) */
    for (i = 0; i < 2; i++) {
        f32 y = 115.0f + i * 148.0f;
        Tr_DrawBracketLine(175.0f, y + 22.0f, 195.0f, y + 24.0f, (sTr->matches[4 + i].f0 >= 0 || sTr->matches[4 + i].f1 >= 0) ? goldLine : lineCol);
        Tr_DrawMatchBox(4 + i, 195.0f, y, 135.0f, 46.0f);

        /* Conectores hacia Gran Final */
        {
            u32 c = (sTr->matches[4 + i].winner >= 0) ? goldLine : lineCol;
            Tr_DrawBracketLine(330.0f, y + 23.0f, 355.0f, y + 25.0f, c);
            if (i == 0) {
                Tr_DrawBracketLine(353.0f, y + 23.0f, 355.0f, y + 97.0f, c);
            } else {
                Tr_DrawBracketLine(353.0f, y - 51.0f, 355.0f, y + 25.0f, c);
            }
        }
    }

    /* Gran Final (Columna Derecha: 1 caja) */
    {
        f32 y = 188.0f;
        Tr_DrawBracketLine(355.0f, y + 24.0f, 375.0f, y + 26.0f, (sTr->matches[6].f0 >= 0 || sTr->matches[6].f1 >= 0) ? goldLine : lineCol);
        Tr_DrawMatchBox(6, 375.0f, y, 140.0f, 50.0f);
    }

    /* Trofeo / Campeon Card */
    if (sTr->champion >= 0) {
        f32 pulse = (sTr->frames % 30 < 15) ? 1.0f : 0.8f;
        Ps_Panel(TBO_KIT_WINDOW, 360.0f, 260.0f, 520.0f, 370.0f, 0x90);
        Ps_Align(1);
        Ps_Text(440.0f, 270.0f, "!CAMPEON DEL MUNDO!", 0.55f * pulse, 150.0f, 1);
        Ps_Text(440.0f, 296.0f, sTr->fighters[sTr->champion].name, 0.65f, 150.0f, 2);
        Ps_Text(440.0f, 330.0f, "TROFEO BUDOKAI TENKAICHI", 0.42f, 150.0f, 5);
        Ps_Align(0);
    } else {
        /* Mission & Info Box */
        Ps_Panel(TBO_KIT_WINDOW, 360.0f, 260.0f, 520.0f, 370.0f, 0x80);
        Ps_Text(370.0f, 270.0f, "INFORMACION DE LA RONDA", 0.46f, 140.0f, 1);
        if (sTr->currentMatch >= 0) {
            TourneyMatch *cm = &sTr->matches[sTr->currentMatch];
            char info[64];
            sprintf(info, "Combate #%d (%s)", sTr->currentMatch + 1, cm->round == 0 ? "Cuartos" : cm->round == 1 ? "Semifinal" : "Gran Final");
            Ps_Text(370.0f, 292.0f, info, 0.42f, 140.0f, 0);
            if (sTr->fighters[cm->f0].isPlayer || sTr->fighters[cm->f1].isPlayer) {
                Ps_Text(370.0f, 314.0f, "!ES TU TURNO DE LUCHAR!", 0.46f, 140.0f, 5);
                Ps_Text(370.0f, 334.0f, "Presiona X para pelear.", 0.38f, 140.0f, 3);
            } else {
                Ps_Text(370.0f, 314.0f, "Combate entre rivales CPU.", 0.40f, 140.0f, 0);
                Ps_Text(370.0f, 334.0f, "Presiona Cuadrado para simular.", 0.38f, 140.0f, 3);
            }
        }
    }
}

static void Tr_DrawSpectator(void) {
    TourneyMatch *m = &sTr->matches[sTr->selectedMatch];
    Ps_Panel(TBO_KIT_WINDOW, 40.0f, 80.0f, 480.0f, 360.0f, 0x85);
    Ps_Heading(60.0f, 96.0f, "VISTA DEL COMBATE & ANALISIS", 0.85f, 300.0f, 0x80);

    if (m->f0 >= 0 && m->f1 >= 0) {
        TourneyFighter *f0 = &sTr->fighters[m->f0];
        TourneyFighter *f1 = &sTr->fighters[m->f1];

        /* Fighter 0 Card */
        Ps_Pill(TBO_KIT_BAR, 60.0f, 136.0f, 230.0f, 240.0f, 0x80);
        Ps_Text(70.0f, 144.0f, f0->name, 0.65f, 150.0f, f0->isPlayer ? 5 : 1);
        Ps_Text(70.0f, 172.0f, "Semilla:", 0.45f, 80.0f, 0);
        { char s[16]; sprintf(s, "#%d", f0->seed); Ps_Text(140.0f, 172.0f, s, 0.45f, 50.0f, 2); }
        Ps_Text(70.0f, 194.0f, "Victorias:", 0.45f, 80.0f, 0);
        { char s[16]; sprintf(s, "%d", f0->wins); Ps_Text(140.0f, 194.0f, s, 0.45f, 50.0f, 2); }

        /* VS Emblem */
        Ps_Align(1);
        Ps_Text(260.0f, 180.0f, "VS", 1.0f, 0.0f, 1);
        Ps_Align(0);

        /* Fighter 1 Card */
        Ps_Pill(TBO_KIT_BAR, 290.0f, 136.0f, 460.0f, 240.0f, 0x80);
        Ps_Text(300.0f, 144.0f, f1->name, 0.65f, 150.0f, f1->isPlayer ? 5 : 1);
        Ps_Text(300.0f, 172.0f, "Semilla:", 0.45f, 80.0f, 0);
        { char s[16]; sprintf(s, "#%d", f1->seed); Ps_Text(370.0f, 172.0f, s, 0.45f, 50.0f, 2); }
        Ps_Text(300.0f, 194.0f, "Victorias:", 0.45f, 80.0f, 0);
        { char s[16]; sprintf(s, "%d", f1->wins); Ps_Text(370.0f, 194.0f, s, 0.45f, 50.0f, 2); }

        /* Tactical Preview */
        Ps_Text(60.0f, 260.0f, "FICHA DE LOS PARTICIPANTES", 0.50f, 200.0f, 1);
        Ps_Text(60.0f, 285.0f, "El espectador en vivo esta disponible en la sala online (L3).", 0.42f, 400.0f, 0);
        Ps_Text(60.0f, 305.0f, "Esta ficha local no representa una retransmision de combate.", 0.40f, 400.0f, 3);
    } else {
        Ps_Text(60.0f, 160.0f, "Esperando definicion de los participantes anteriores.", 0.55f, 350.0f, 4);
    }
}

s32 PortTournament_Run(void) {
    s32 result = 0;

    if(!sTournamentReady) {Tr_InitTournament();sTournamentReady=1;}
    sTr->leaving=sTr->launchBattle=sTr->frames=0;

    Bgm_Play(0x10B16 + 2); /* High energy online/tournament music */
    ColorFade_StartIn(0, 0, 0, 0x14);

    while (1) {
        Pad *pad = &gPad[0];
        s32 live;

        Job_Run();
        Gfx_BeginFrame();
        Pad_Update();
        { extern int Port_RosterTestMode(void);extern unsigned Port_RosterTestInput(unsigned);
          if(Port_RosterTestMode()==12)pad->gamePressed|=Port_RosterTestInput(sTr->frames+1); }
        Snd_Update();
        ColorFade_Update();
        live = !sTr->leaving && ColorFade_IsInDone();

        Ps_PromptsClear();
        if (sTr->tab == 0) {
            Ps_Prompt(0, "Luchar / Avanzar");
            Ps_Prompt(1, "Simular CPU");
            Ps_Prompt(2, "Volver");
            Ps_Prompt(3, "Ficha / SELECT: Online");
        } else {
            Ps_Prompt(2, "Volver al Cuadro");
        }

        if (live) {
            if(pad->gamePressed & (PADG_L3|PADG_SELECT)) {
                extern s32 PortTournamentOnline_Run(void);
                if(PortTournamentOnline_Run()) return 1;
            }
            /* Tab navigation */
            if (pad->gamePressed & PADG_TRIANGLE) {
                sTr->tab = (sTr->tab == 0) ? 1 : 0;
                Snd_PlaySe(1, 0);
            }

            if (sTr->tab == 0) {
                /* Bracket navigation */
                if (pad->gamePressed & (PADG_UP | PADG_UP2)) {
                    sTr->selectedMatch = (sTr->selectedMatch + 6) % 7;
                    Snd_PlaySe(1, 0);
                } else if (pad->gamePressed & (PADG_DOWN | PADG_DOWN2)) {
                    sTr->selectedMatch = (sTr->selectedMatch + 1) % 7;
                    Snd_PlaySe(1, 0);
                }

                /* Luchar / Avanzar */
                if (pad->gamePressed & (PADG_CROSS | PADG_START)) {
                    if (sTr->currentMatch >= 0) {
                        TourneyMatch *cm = &sTr->matches[sTr->currentMatch];
                        if (sTr->fighters[cm->f0].isPlayer || sTr->fighters[cm->f1].isPlayer) {
                            /* Player's real fight! */
                            PcBattle battle;
                            memset(&battle, 0, sizeof(battle));
                            battle.mode = PC_DUEL;
                            battle.players = 1;
                            battle.count[0] = 1;
                            battle.count[1] = 1;
                            battle.fighter[0][0].chara = sTr->fighters[cm->f0].chara;
                            battle.fighter[0][0].costume = 0;
                            battle.fighter[1][0].chara = sTr->fighters[cm->f1].chara;
                            battle.fighter[1][0].costume = 0;
                            battle.stage = sTr->stage;

                            if(!Port_PcRosterQueue(&battle)) {Snd_PlaySe(1,7);continue;}
                            sTournamentBattle=sTr->currentMatch;
                            Port_PcNativeRules(1, 0, 16); /* Tournament rules */
                            sTr->launchBattle = 1;
                            sTr->leaving = 1;
                            result = 1;
                            Snd_PlaySe(1, 1);
                            ColorFade_StartOut(0, 0, 0, 0x14);
                        } else {
                            /* Simulate current CPU match */
                            s32 w = Rand_Range(2);
                            Tr_AdvanceBracket(sTr->currentMatch, w);
                            Snd_PlaySe(1, 1);
                        }
                    }
                } else if (pad->gamePressed & PADG_SQUARE) {
                    /* Simulate all pending CPU matches in this round */
                    Tr_SimulateCpuMatches();
                } else if (pad->gamePressed & (PADG_L1 | PADG_R1)) {
                    /* New Tournament Draw */
                    Tr_InitTournament();
                    Snd_PlaySe(1, 1);
                } else if (pad->gamePressed & PADG_CIRCLE) {
                    sTr->leaving = 1;
                    ColorFade_StartOut(0, 0, 0, 0x14);
                    Snd_PlaySe(1, 2);
                }
            } else {
                if (pad->gamePressed & (PADG_CIRCLE | PADG_CROSS)) {
                    sTr->tab = 0;
                    Snd_PlaySe(1, 2);
                }
            }
        }

        /* Rendering */
        Ps_PageSet();
        Ps_ScreenQuad(0, 0, 512, 0, 512, 448, 0, 448,
                      Ps_Rgba(8, 12, 22, 255), Ps_Rgba(14, 20, 36, 255),
                      Ps_Rgba(6, 10, 18, 255), Ps_Rgba(12, 18, 30, 255));

        /* Header */
        Ps_Heading(24.0f, 32.0f, "TORNEO LOCAL | SELECT: ONLINE", 1.0f, 400.0f, 0x80);
        Ps_Text(24.0f, 52.0f, "8 Luchadores Legendarios  |  Arena Torneo Mundial  |  Eliminatoria Directa", 0.40f, 450.0f, 3);

        if (sTr->tab == 0) {
            Tr_DrawBracketView();
        } else {
            Tr_DrawSpectator();
        }

        Ps_PromptsDraw();
        ColorFade_Draw();
        File_Stub264D90();
        Gfx_EndFrame(1);
        Dma_Flush();

        sTr->frames++;
        if (sTr->leaving && ColorFade_IsOutDone()) {
            break;
        }
    }

    Dma_ResetBuffers();
    return result;
}

#endif

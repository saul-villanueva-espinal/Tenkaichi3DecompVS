#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include "../include/port_pc_battle.h"
#include "../include/plat_charas.h"
extern const char *Port_FighterName(int chara);
extern int Port_ExtraCostumeOk(int chara, int costume, char *why, int size);
extern int Port_NetActive(void), Port_NetSession(void), Port_ModesFight(void);
static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static PcBattle sPending;
static int sRequested;
volatile int gPortPcHubReady;
volatile int gPortPcTeamReady;
volatile int gPortPcTeamDp;
extern int ChrTbl_GetCost(int chara);
static PcBattle sTeams;
static int sTeamsPending;
void Port_PcBattleDefault(PcBattle *b) {
    int s, i;
    memset(b, 0, sizeof(*b));
    b->players = 1;
    b->count[0] = b->count[1] = 1;
    for (s = 0; s < 2; s++) for (i = 0; i < PORT_RELAY_MAX; i++) b->fighter[s][i].chara = s ? 4 : 0;
}
int Port_PcRosterCount(void) { return PORT_DISC_CHARAS + Port_CharaCount(); }
int Port_PcRosterId(int i) {
    return i < 0 || i >= Port_PcRosterCount() ? -1 : i < PORT_DISC_CHARAS ? i : PORT_CHARA_FIRST + i - PORT_DISC_CHARAS;
}
const char *Port_PcRosterName(int c) { return Port_FighterName(c); }
int Port_PcBattleValidate(const PcBattle *b, char *why, int size) {
    int s, i;
    const char *error = NULL;
    if (!b || b->mode < 0 || b->mode >= PC_MODE_COUNT) error = "Modo no valido";
    else if (b->players < 1 || b->players > PORT_COOP_MAX) error = "Elige entre 1 y 4 jugadores";
    else if (b->stage < 0 || b->stage >= 36) error = "Escenario no valido";
    else if ((b->mode == PC_DUEL || b->mode == PC_RELAY) && b->players > 2) error = "Duelo por relevos admite uno o dos mandos";
    else if (b->mode == PC_TRAINING && b->players != 1) error = "Entrenamiento utiliza un mando";
    for (s = 0; b && !error && s < 2; s++) {
        int max = b->mode == PC_RELAY || b->mode == PC_DUEL ? PORT_RELAY_MAX : b->mode == PC_TAG ? 5 : b->mode == PC_RAID && s == 0 ? 4 : 1;
        if (b->count[s] < 1 || b->count[s] > max) { error = "Cantidad de integrantes no valida para el modo"; break; }
        for (i = 0; i < b->count[s]; i++) {
            const PcFighter *f = &b->fighter[s][i];
            if (f->chara < 0 || !(f->chara < PORT_DISC_CHARAS || (f->chara >= PORT_CHARA_FIRST && f->chara < PORT_CHARA_FIRST + Port_CharaCount()))) {
                error = "Falta instalar el personaje seleccionado"; break;
            }
            if (f->costume < 0 || f->costume > 7) { error = "Traje fuera de rango"; break; }
            if (!Port_ExtraCostumeOk(f->chara, f->costume, why, size)) return 0;
        }
    }
    if (!error && b->mode == PC_TAG && (b->count[0] < (b->players >= 3 ? 2 : 1) || b->count[1] < (b->players >= 4 ? 2 : 1))) error = "Selecciona un integrante para cada mando";
    if (!error && b->mode == PC_RAID && b->count[0] < b->players) error = "Selecciona un aliado por jugador";
    if (why && size > 0) snprintf(why, (size_t)size, "%s", error ? error : "");
    return error == NULL;
}
int Port_PcBattleRequest(const PcBattle *b) {
    char why[160];
    if (!gPortPcHubReady || Port_NetActive() || Port_NetSession() || !Port_PcBattleValidate(b, why, sizeof(why))) return 0;
    pthread_mutex_lock(&sLock);
    sPending = *b;
    sRequested = 1;
    pthread_mutex_unlock(&sLock);
    return 1;
}
int Port_PcBattlePending(void) {
    int result;
    pthread_mutex_lock(&sLock); result = sRequested; pthread_mutex_unlock(&sLock);
    return result;
}
int Port_PcTeamsRequest(const PcBattle *b) {
    char why[160];
    if (!gPortPcTeamReady || Port_ModesFight() != 0 || (b->mode != PC_DUEL && b->mode != PC_RELAY) ||
        Port_NetActive() || Port_NetSession() || !Port_PcBattleValidate(b, why, sizeof(why))) return 0;
    if (gPortPcTeamDp > 0) {
        int s, i;
        for (s = 0; s < 2; s++) {
            int cost = 0;
            for (i = 0; i < b->count[s]; i++) cost += ChrTbl_GetCost(b->fighter[s][i].chara);
            if (cost > gPortPcTeamDp) return 0;
        }
    }
    pthread_mutex_lock(&sLock); sTeams = *b; sTeamsPending = 1; pthread_mutex_unlock(&sLock);
    return 1;
}
int Port_PcTeamsPreview(PcBattle *b) {
    int result;
    pthread_mutex_lock(&sLock); result = sTeamsPending; if (result) *b = sTeams; pthread_mutex_unlock(&sLock);
    return result;
}
int Port_PcTeamsTake(PcBattle *b) {
    int result;
    pthread_mutex_lock(&sLock); result = sTeamsPending; if (result) *b = sTeams; sTeamsPending = 0; pthread_mutex_unlock(&sLock);
    return result;
}
int Port_PcBattleTake(PcBattle *b) {
    int result;
    pthread_mutex_lock(&sLock);
    result = sRequested;
    if (result) { *b = sPending; sRequested = 0; }
    pthread_mutex_unlock(&sLock);
    if (!result && getenv("BT3_PC_CONFIG")) {
        const char *p = getenv("BT3_PC_CONFIG");
        unsigned i; int *v = (int *)b; char why[160];
        for (i = 0; i < sizeof(*b)/sizeof(int); i++) {
            char *end; long n = strtol(p, &end, 10);
            if (end == p || (i + 1 < sizeof(*b)/sizeof(int) && *end != ',')) return 0;
            v[i] = (int)n; p = *end ? end + 1 : end;
        }
        result = Port_PcBattleValidate(b, why, sizeof(why));
    }
    return result;
}

void Port_PcRelayLog(int side, int entered, int chara) {
    fprintf(stderr, "bt3: relevo: equipo %d, reserva %d/%d, personaje %d\n", side + 1, entered, PORT_RELAY_MAX, chara);
}

int Port_PcTestHealth(void) {
    const char *e = getenv("BT3_PC_HEALTH_PERCENT");
    int n = e ? atoi(e) : 100;
    return n >= 1 && n <= 100 ? n : 100;
}

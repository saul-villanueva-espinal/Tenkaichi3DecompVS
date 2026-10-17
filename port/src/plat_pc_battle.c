#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "../include/port_pc_battle.h"
#include "../include/plat_charas.h"
extern const char *Port_FighterName(int chara);
extern int Port_ExtraCostumeOk(int chara, int costume, char *why, int size);
extern int Port_NetActive(void), Port_NetSession(void);
static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static PcBattle sPending;
static int sRequested;
volatile int gPortPcHubReady;
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
        int max = b->mode == PC_RELAY ? PORT_RELAY_MAX : b->mode == PC_TAG ? 5 : b->mode == PC_RAID && s == 0 ? 4 : 1;
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
int Port_PcBattleTake(PcBattle *b) {
    int result;
    pthread_mutex_lock(&sLock);
    result = sRequested;
    if (result) { *b = sPending; sRequested = 0; }
    pthread_mutex_unlock(&sLock);
    return result;
}

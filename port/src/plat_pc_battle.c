#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
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
static int sTeamsMask = 3, sTakenMask = 3;
volatile int gPortNativeRosterActive;
void Port_PcBattleDefault(PcBattle *b) {
    int s, i;
    memset(b, 0, sizeof(*b));
    b->players = 1;
    b->count[0] = b->count[1] = 1;
    for (s = 0; s < 2; s++) for (i = 0; i < PORT_RELAY_MAX; i++) b->fighter[s][i].chara = s ? 4 : 0;
}
/* Accept complete old teams (45 ints) and complete current configurations.
   Never accept a partial extension, overflow, or trailing garbage. */
int Port_PcBattleParse(PcBattle *b, const char *text) {
    PcBattle parsed; unsigned i, n=sizeof(parsed)/sizeof(int); int *v=(int *)&parsed;
    char why[160];
    if(!b || !text) return 0;
    Port_PcBattleDefault(&parsed);
    for(i=0;i<n;i++) {
        char *end; long value;
        errno=0; value=strtol(text,&end,10);
        if(end==text || errno==ERANGE || value<INT_MIN || value>INT_MAX) return 0;
        v[i]=(int)value;
        if(!*end) {
            if(i+1!=45 && i+1!=57 && i+1!=n) return 0;
            if(!Port_PcBattleValidate(&parsed,why,sizeof(why))) return 0;
            *b=parsed;return 1;
        }
        if(*end!=',') return 0;
        text=end+1;
    }
    return 0;
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
    else if (b->players < 1 || b->players > PORT_COOP_MAX) error = "Elige entre 1 y 6 jugadores";
    else if (b->stage < 0 || b->stage >= 36) error = "Escenario no valido";
    else if ((b->mode == PC_DUEL || b->mode == PC_RELAY) && b->players > 2) error = "Duelo por relevos admite uno o dos mandos";
    else if ((b->mode == PC_TAG || b->mode == PC_FFA) && b->players > 4) error = "Tag Team local admite hasta cuatro mandos";
    else if (b->mode == PC_TRAINING && b->players != 1) error = "Entrenamiento utiliza un mando";
    if(b && !error) {
        const PcBattleRules *r=&b->rules;
        if(r->damagePct<0 || r->damagePct>400 || r->kiPct<0 || r->kiPct>400 ||
           r->regenPerSec<0 || r->regenPerSec>1000 || r->regenSides<0 || r->regenSides>3 ||
           r->healthPct<0 || r->healthPct>100 || r->bossHp<0 || r->bossHp>250000 ||
           r->bossDamagePct<0 || r->bossDamagePct>400 || r->enragePct<0 || r->enragePct>90 ||
           r->cpuLevel1<0 || r->cpuLevel1>30 || r->bgm1<0 || r->bgm1>20 ||
           r->infiniteKiChara1<0 || r->infiniteKiChara1>PORT_CHARA_ID_LIMIT ||
           r->armorSides<0 || r->armorSides>3) error="Reglas de combate fuera de rango";
        else if(b->mode!=PC_RAID && (r->bossHp || r->bossDamagePct || r->enragePct)) error="Reglas de jefe fuera de Raid";
    }
    if(b && !error) for(i=0;i<PORT_RAID_BOSSES-1;i++) {
        const PcBattleRules *r=&b->bossRules[i];
        if(r->bossHp<0 || r->bossHp>250000 || r->bossDamagePct<0 || r->bossDamagePct>400 ||
           r->enragePct<0 || r->enragePct>90 || r->cpuLevel1<0 || r->cpuLevel1>30 ||
           r->infiniteKiChara1<0 || r->infiniteKiChara1>PORT_CHARA_ID_LIMIT ||
           r->armorSides<0 || r->armorSides>3 || r->damagePct || r->kiPct ||
           r->regenPerSec || r->regenSides || r->healthPct || r->bgm1)
            error="Perfil adicional del jefe fuera de rango";
    }
    for (s = 0; b && !error && s < 2; s++) {
        int max = b->mode == PC_RELAY || b->mode == PC_DUEL ? PORT_RELAY_MAX : (b->mode == PC_TAG || b->mode == PC_FFA) ? 5 : b->mode == PC_RAID ? (s == 0 ? PORT_COOP_MAX : PORT_RAID_BOSSES) : 1;
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
    if (!error && (b->mode == PC_TAG || b->mode == PC_FFA) && (b->count[0] < (b->players >= 3 ? 2 : 1) || b->count[1] < (b->players >= 4 ? 2 : 1))) error = "Selecciona un integrante para cada mando";
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
    pthread_mutex_lock(&sLock); sTeams = *b; sTeamsMask = 3; sTeamsPending = 1; pthread_mutex_unlock(&sLock);
    return 1;
}
int Port_PcTeamsPreview(PcBattle *b) {
    int result;
    pthread_mutex_lock(&sLock); result = sTeamsPending; if (result) *b = sTeams; pthread_mutex_unlock(&sLock);
    return result;
}
int Port_PcTeamsTake(PcBattle *b) {
    int result;
    pthread_mutex_lock(&sLock); result = sTeamsPending; if (result) { *b = sTeams; sTakenMask = sTeamsMask; } sTeamsPending = 0; pthread_mutex_unlock(&sLock);
    return result;
}
int Port_PcBattleTake(PcBattle *b) {
    int result;
    pthread_mutex_lock(&sLock);
    result = sRequested;
    if (result) { *b = sPending; sRequested = 0; }
    pthread_mutex_unlock(&sLock);
    if (!result && getenv("BT3_PC_CONFIG")) {
        result = Port_PcBattleParse(b, getenv("BT3_PC_CONFIG"));
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

/* Native menus hand the same validated configuration to the game thread. */
int Port_PcRosterQueue(const PcBattle *b) {
    char why[160];
    if (Port_NetActive() || Port_NetSession() || !Port_PcBattleValidate(b, why, sizeof(why))) return 0;
    pthread_mutex_lock(&sLock); sPending = *b; sRequested = 1; pthread_mutex_unlock(&sLock);
    return 1;
}
/* The original team selector supplies both ordered ten-member benches. */
int Port_PcNativeTeamsQueue(const PcBattle *b) {
    char why[160];
    if (Port_NetActive() || Port_NetSession() || Port_ModesFight() != 0 || !Port_PcBattleValidate(b,why,sizeof(why))) return 0;
    pthread_mutex_lock(&sLock); sTeams=*b;sTeamsMask=3;sTeamsPending=1;pthread_mutex_unlock(&sLock);
    return 1;
}
int Port_PcRosterPlayerQueue(const PcBattle *b, int dp) {
    char why[160]; int i,cost=0;
    if (Port_NetActive() || Port_NetSession() || !Port_PcBattleValidate(b,why,sizeof(why))) return 0;
    for(i=0;i<b->count[0];i++) cost+=ChrTbl_GetCost(b->fighter[0][i].chara);
    if(dp>0 && cost>dp) return 0;
    pthread_mutex_lock(&sLock);sTeams=*b;sTeamsMask=1;sTeamsPending=1;pthread_mutex_unlock(&sLock);
    return 1;
}
int Port_PcTeamsMask(void) { return sTakenMask; }

#ifdef _WIN32
#include <windows.h>
#endif

void Port_GetLocalIp(int ip[4]) {
    ip[0] = 192; ip[1] = 168; ip[2] = 1; ip[3] = 111;
#ifdef _WIN32
    {
        HMODULE ws = LoadLibraryA("ws2_32.dll");
        if (ws) {
            typedef int (PASCAL *fn_WSAStartup)(WORD, LPWSADATA);
            typedef int (PASCAL *fn_WSACleanup)(void);
            typedef UINT_PTR (PASCAL *fn_socket)(int, int, int);
            typedef int (PASCAL *fn_connect)(UINT_PTR, const struct sockaddr *, int);
            typedef int (PASCAL *fn_getsockname)(UINT_PTR, struct sockaddr *, int *);
            typedef int (PASCAL *fn_closesocket)(UINT_PTR);
            typedef unsigned long (PASCAL *fn_inet_addr)(const char *);
            typedef unsigned long (PASCAL *fn_ntohl)(unsigned long);
            typedef unsigned short (PASCAL *fn_htons)(unsigned short);

            fn_WSAStartup pWSAStartup = (fn_WSAStartup)(void *)GetProcAddress(ws, "WSAStartup");
            fn_WSACleanup pWSACleanup = (fn_WSACleanup)(void *)GetProcAddress(ws, "WSACleanup");
            fn_socket psocket = (fn_socket)(void *)GetProcAddress(ws, "socket");
            fn_connect pconnect = (fn_connect)(void *)GetProcAddress(ws, "connect");
            fn_getsockname pgetsockname = (fn_getsockname)(void *)GetProcAddress(ws, "getsockname");
            fn_closesocket pclosesocket = (fn_closesocket)(void *)GetProcAddress(ws, "closesocket");
            fn_inet_addr pinet_addr = (fn_inet_addr)(void *)GetProcAddress(ws, "inet_addr");
            fn_ntohl pntohl = (fn_ntohl)(void *)GetProcAddress(ws, "ntohl");
            fn_htons phtons = (fn_htons)(void *)GetProcAddress(ws, "htons");

            if (pWSAStartup && pWSACleanup && psocket && pconnect && pgetsockname && pclosesocket && pinet_addr && pntohl && phtons) {
                WSADATA w;
                if (!pWSAStartup(0x0202, &w)) {
                    UINT_PTR s = psocket(2 /* AF_INET */, 2 /* SOCK_DGRAM */, 0);
                    if (s != (UINT_PTR)~0) {
                        struct sockaddr_in target, local;
                        memset(&target, 0, sizeof(target));
                        target.sin_family = 2;
                        target.sin_port = phtons(80);
                        target.sin_addr.s_addr = pinet_addr("8.8.8.8");
                        if (pconnect(s, (const struct sockaddr *)&target, (int)sizeof(target)) == 0) {
                            int len = (int)sizeof(local);
                            if (pgetsockname(s, (struct sockaddr *)&local, &len) == 0) {
                                unsigned long addr = pntohl(local.sin_addr.s_addr);
                                if (addr != 0 && (addr >> 24) != 127) {
                                    ip[0] = (int)((addr >> 24) & 0xFF);
                                    ip[1] = (int)((addr >> 16) & 0xFF);
                                    ip[2] = (int)((addr >> 8) & 0xFF);
                                    ip[3] = (int)(addr & 0xFF);
                                }
                            }
                        }
                        pclosesocket(s);
                    }
                    pWSACleanup();
                }
            }
            FreeLibrary(ws);
        }
    }
#endif
}


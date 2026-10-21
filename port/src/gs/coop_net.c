/* Four-peer Raid, independent of Dragon Net Battle's two-peer rollback protocol.
   The host accepts each client's assigned input slot and endpoint; every copy simulates the
   boss. Fixed input delay + retransmission, no prediction. IPv4 LAN/direct UDP. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#define closesocket close
#endif
#include "../../include/port_coop.h"
#include "../../include/port_tournament.h"
#include <pthread.h>
#include <setjmp.h>
enum { CMAGIC = 0x42543343, CVERSION = 7, C_HELLO = 1, C_CONFIG, C_ACK, C_START, C_INPUT, C_ERROR, C_LOADED, C_GO,
       CRING = 4096, CPAD = 18, CREDUNDANT = 16, CDELAY = 3, CHEAD = 48 };
_Static_assert(sizeof(PcBattle) == 420, "Update the co-op protocol when PcBattle changes");
static int sSock = -1, sHost, sMe, sPlayers, sReady, sSimulationPaused;
static int sBattleArmed;
static unsigned sLoadRound;
static uint32_t sNonce, sTick, sStartTick, sSampled;
static PcBattle sBattle;
static PcFighter sMyHero;
static uint32_t sEpoch = 1;
static int sTournament,sActive[2]={0,1},sRecorded,sFailed,sJumpActive,sCancel;
static jmp_buf sFailureJump;
static char sFailureText[160];
static PortTourney sTourney;
static pthread_mutex_t sTourneyLock=PTHREAD_MUTEX_INITIALIZER;
static struct sockaddr_in sPeer[PORT_TOURNEY_MAX];
static int sKnown[PORT_TOURNEY_MAX], sAck[PORT_TOURNEY_MAX];
static unsigned char sInput[PORT_TOURNEY_MAX][CRING][CPAD];
static uint32_t sTag[PORT_TOURNEY_MAX][CRING], sHashTick[CRING];
static uint64_t sHashes[CRING], sFingerprint;
static int sLobbyPhase, sLobbyKnown, sLobbyTotal = 4;
static const unsigned char kIdle[CPAD] = {255,255,128,128,128,128};
extern int Port_PadRead(int, unsigned char *);
extern void Port_VerifyStart(void), PortCharas_Init(void);
extern int Port_DataFingerprint(unsigned long long *);
extern unsigned long long Port_CharacterFingerprint(void), Port_NetDigest(void);
extern int BtlCharApi_GetHp(int);
extern void BtlCharApi_GetPos(int, float *);
extern void Port_CoopRelaunch(void);
extern void Port_CoopWaitFrame(void);

static uint64_t millis(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
#endif
}
static void pause_ms(void) {
#ifdef _WIN32
    Sleep(1);
#else
    struct timespec t = {0,1000000}; nanosleep(&t, NULL);
#endif
}
static uint32_t get32(const unsigned char *p) { uint32_t n; memcpy(&n,p,4); return ntohl(n); }
static void put32(unsigned char *p, uint32_t n) { n = htonl(n); memcpy(p,&n,4); }
static uint64_t get64(const unsigned char *p) { return (uint64_t)get32(p)<<32 | get32(p+4); }
static void put64(unsigned char *p,uint64_t n) { put32(p,(uint32_t)(n>>32)); put32(p+4,(uint32_t)n); }
static int same(const struct sockaddr_in *a,const struct sockaddr_in *b) {
    return a->sin_addr.s_addr == b->sin_addr.s_addr && a->sin_port == b->sin_port;
}
static void send_to(const unsigned char *p,int n,int who) {
    sendto(sSock,(const char *)p,n,0,(struct sockaddr *)&sPeer[who],sizeof(sPeer[who]));
}
static void header(unsigned char *p,int type,int who) {
    memset(p,0,CHEAD); put32(p,CMAGIC);put32(p+4,CVERSION);put32(p+8,type);put32(p+12,sNonce);
    put32(p+16,who);put32(p+20,sPlayers);put64(p+24,sFingerprint);put32(p+32,sEpoch);
}
static void control(int type,int who) {
    unsigned char p[CHEAD+sizeof(PcBattle)+sizeof(PortTourney)];
    header(p,type,who);
    if(type==C_LOADED || type==C_GO) put32(p+44,sLoadRound);
    if (type == C_CONFIG) {
        int known=1,j;for(j=1;j<sPlayers;j++) known+=sKnown[j];put32(p+40,known);
        const int *v = (const int *)&sBattle;
        unsigned i; for(i=0;i<sizeof(PcBattle)/4;i++) put32(p+CHEAD+i*4,(uint32_t)v[i]);
        put32(p+36,sTournament?1:0);
        if(sTournament) {v=(const int *)&sTourney;for(i=0;i<sizeof(PortTourney)/4;i++) put32(p+CHEAD+sizeof(PcBattle)+i*4,(uint32_t)v[i]);}
    }
    if(type==C_HELLO) {put32(p+CHEAD,sMyHero.chara);put32(p+CHEAD+4,sMyHero.costume);}
    send_to(p,CHEAD+(type==C_CONFIG ? sizeof(PcBattle)+(sTournament?sizeof(PortTourney):0):type==C_HELLO?8:0),who);
}
static void fail(const char *why) {
    int i;
    fprintf(stderr,"bt3: coop: ERROR: %s\n",why); fflush(stderr);
    if(sHost) for(i=1;i<sPlayers && i<PORT_TOURNEY_MAX;i++) if(sKnown[i]) control(C_ERROR,i);
    if(sSock>=0) {closesocket(sSock);sSock=-1;}
    if(sJumpActive) {
        snprintf(sFailureText,sizeof(sFailureText),"%s",why);sFailed=1;
        __atomic_store_n(&sLobbyPhase,0,__ATOMIC_RELEASE);
        longjmp(sFailureJump,1);
    }
    exit(2);
}
int Port_CoopEnabled(void) {
    return getenv("BT3_COOP_HOST") != NULL || getenv("BT3_COOP_JOIN") != NULL;
}
int Port_CoopMe(void) { return sMe; }
unsigned Port_CoopSeed(void) {
    const char *test=getenv("BT3_COOP_TEST_SEED");
    return (!sTournament && test)?(unsigned)strtoul(test,NULL,0):sNonce ^ (sTournament?(unsigned)(sTourney.current+1)*0x9e3779b9u:0);
}
int Port_CoopLobbyStatus(int *known, int *total) {
    *known = __atomic_load_n(&sLobbyKnown, __ATOMIC_ACQUIRE);
    *total = __atomic_load_n(&sLobbyTotal, __ATOMIC_ACQUIRE);
    return __atomic_load_n(&sLobbyPhase, __ATOMIC_ACQUIRE);
}
int Port_CoopLobbyCopy(PcBattle *out) {
    if(sTournament || !out || __atomic_load_n(&sLobbyPhase,__ATOMIC_ACQUIRE)!=2) return 0;
    *out=sBattle;return 1;
}
static void lobby_frame(void) {
    static uint64_t last;
    if(millis()-last>=16) { Port_CoopWaitFrame(); last=millis(); }
}
static void parse_config(const char *text) {
    Port_PcBattleDefault(&sBattle); sBattle.mode=PC_RAID;sBattle.players=4;sBattle.count[0]=4;
    if(!text) return;
    if(!Port_PcBattleParse(&sBattle,text)) fail("Configuracion incompleta o no valida");
}
static void start_socket(int port) {
    struct sockaddr_in local;
#ifdef _WIN32
    WSADATA w; u_long on=1;
    if(WSAStartup(MAKEWORD(2,2),&w)) fail("Winsock no disponible");
#endif
    sSock=(int)socket(AF_INET,SOCK_DGRAM,0);
    if(sSock<0) fail("No se pudo abrir UDP");
#ifdef _WIN32
    ioctlsocket(sSock,FIONBIO,&on);
#else
    fcntl(sSock,F_SETFL,fcntl(sSock,F_GETFL,0)|O_NONBLOCK);
#endif
    memset(&local,0,sizeof(local));local.sin_family=AF_INET;local.sin_port=htons((unsigned short)port);
    if(bind(sSock,(struct sockaddr *)&local,sizeof(local))) fail("Puerto UDP ocupado");
}
static int packet(unsigned char *p,struct sockaddr_in *from) {
    socklen_t n=sizeof(*from);
    int len=(int)recvfrom(sSock,(char *)p,1400,0,(struct sockaddr *)from,&n);
    if(len<CHEAD || get32(p)!=CMAGIC || get32(p+4)!=CVERSION) return 0;
    return len;
}
int Port_CoopConfig(PcBattle *out) {
    uint64_t begun,last=0; unsigned long long base=0;
    char why[160]; int i;
    if(!Port_CoopEnabled()) return 0;
    sTournament=Port_TourneyEnabled();
    if(sFailed) return 0;
    if(setjmp(sFailureJump)) {sJumpActive=0;return 0;}
    sJumpActive=1;
    if(sReady) {
        if(sTournament) {
            int a,b;
            if(!Port_TourneyPair(&sTourney,sTourney.current,&a,&b)) {sJumpActive=0;return 0;}
            pthread_mutex_lock(&sTourneyLock);
            sActive[0]=a;sActive[1]=b;sRecorded=0;
            sBattle.fighter[0][0]=sTourney.fighters[a];sBattle.fighter[1][0]=sTourney.fighters[b];
            pthread_mutex_unlock(&sTourneyLock);
        }
        *out=sBattle;sJumpActive=0;return 1;
    }
    __atomic_store_n(&sLobbyPhase,1,__ATOMIC_RELEASE);
    Port_VerifyStart();PortCharas_Init();
    begun=millis();
    while(Port_DataFingerprint(&base)!=1) { if(millis()-begun>30000) fail("No se pudo verificar los datos del juego");lobby_frame();pause_ms(); }
    sFingerprint=base ^ Port_CharacterFingerprint() ^ (uint64_t)PORT_TOTAL_CHARAS<<32;
    sHost=getenv("BT3_COOP_HOST")!=NULL;
    sMe=sHost?0:-1;
    __atomic_store_n(&sLobbyKnown,sHost?1:0,__ATOMIC_RELEASE);
    parse_config(getenv("BT3_COOP_CONFIG"));
    sTournament=getenv("BT3_TOURNAMENT")!=NULL;
    sPlayers=sTournament?atoi(getenv("BT3_TOURNAMENT")):sBattle.players;
    sMyHero=sBattle.fighter[0][0];
    if(sTournament) {
        if(!Port_TourneyInit(&sTourney,sPlayers)) fail("Torneo requiere 4, 8 o 16 participantes");
        sTourney.fighters[0]=sMyHero;
    } else if(sPlayers<2 || sPlayers>PORT_COOP_MAX) fail("Raid online requiere de dos a seis jugadores");
    if(sHost) {
        int port=atoi(getenv("BT3_COOP_HOST")); if(port<1||port>65535) fail("Puerto no valido");
        if(!Port_PcBattleValidate(&sBattle,why,sizeof(why)) || sBattle.mode!=(sTournament?PC_DUEL:PC_RAID) || (sTournament && sBattle.players!=2)) fail("Plantilla de sala no valida");
        start_socket(port);sNonce=(uint32_t)millis()|1;sKnown[0]=sAck[0]=1;
    } else {
        char address[256],*colon;struct addrinfo hints,*result=NULL;
        snprintf(address,sizeof(address),"%s",getenv("BT3_COOP_JOIN"));colon=strrchr(address,':');
        if(!colon) fail("Usa direccion:puerto");*colon++=0;
        start_socket(0); /* Winsock must be initialized before resolving a host. */
        memset(&hints,0,sizeof(hints));hints.ai_family=AF_INET;hints.ai_socktype=SOCK_DGRAM;
        if(getaddrinfo(address,colon,&hints,&result)) fail("Direccion no valida");
        memcpy(&sPeer[0],result->ai_addr,sizeof(sPeer[0]));freeaddrinfo(result);
    }
    fprintf(stderr,"bt3: coop: %s, esperando %d jugadores, datos %016llx\n",sHost?"anfitrion":"cliente",sPlayers,(unsigned long long)sFingerprint);
    __atomic_store_n(&sLobbyTotal,sPlayers,__ATOMIC_RELEASE);
    __atomic_store_n(&sLobbyPhase,2,__ATOMIC_RELEASE);
    begun=millis();
    for(;;) {
        unsigned char p[1400];struct sockaddr_in from; int len=packet(p,&from),type,who=-1,all;
        if(__atomic_load_n(&sCancel,__ATOMIC_ACQUIRE)) fail("Sala cancelada");
        if(millis()-begun>120000) fail("Tiempo de espera de la sala agotado");
        if(millis()-last>100) {
            if(!sHost) control(sMe<0?C_HELLO:C_ACK,0);
            else for(i=1;i<sPlayers;i++) if(sKnown[i]) control(C_CONFIG,i);
            last=millis();
        }
        if(!len) { lobby_frame();pause_ms();continue; }
        type=(int)get32(p+8);
        if(sHost) {
            for(i=1;i<sPlayers;i++) if(sKnown[i]&&same(&from,&sPeer[i])) who=i;
            if(type==C_HELLO && who<0) {
                if(get64(p+24)!=sFingerprint) {
                    unsigned char error[CHEAD];header(error,C_ERROR,0);
                    sendto(sSock,(char *)error,CHEAD,0,(struct sockaddr *)&from,sizeof(from));continue;
                }
                if(len!=CHEAD+8) continue;
                for(i=1;i<sPlayers;i++) if(!sKnown[i]) {
                    PcBattle proposed=sBattle;
                    int slot=sTournament?0:i;
                    proposed.fighter[0][slot].chara=(int)get32(p+CHEAD);
                    proposed.fighter[0][slot].costume=(int)get32(p+CHEAD+4);
                    if(!Port_PcBattleValidate(&proposed,why,sizeof(why))) break;
                    if(sTournament) sTourney.fighters[i]=proposed.fighter[0][0];
                    else sBattle=proposed;
                    who=i;sPeer[i]=from;sKnown[i]=1;sEpoch++;
                    { int j;for(j=1;j<sPlayers;j++) sAck[j]=0; }
                    break;
                }
                { int count=1;for(i=1;i<sPlayers;i++) count+=sKnown[i];__atomic_store_n(&sLobbyKnown,count,__ATOMIC_RELEASE); }
            }
            if(who<0) continue;
            if(type==C_HELLO) control(C_CONFIG,who);
            if(type==C_ACK && get32(p+12)==sNonce && get64(p+24)==sFingerprint && get32(p+32)==sEpoch) sAck[who]=1;
            all=1;for(i=1;i<sPlayers;i++) all &= sAck[i];
            if(all) { for(i=1;i<sPlayers;i++) control(C_START,i);break; }
        } else {
            if(!same(&from,&sPeer[0])) continue;
            if(type==C_ERROR) fail("El anfitrion rechazo la conexion: version, datos o paquetes diferentes");
            if(type==C_CONFIG && get32(p+36)==(unsigned)sTournament &&
               len==CHEAD+sizeof(PcBattle)+(sTournament?sizeof(PortTourney):0) && get64(p+24)==sFingerprint) {
                int *v=(int *)&sBattle;unsigned k;
                if(sMe>0 && (get32(p+12)!=sNonce || get32(p+32)<sEpoch)) continue;
                sMe=(int)get32(p+16);sPlayers=(int)get32(p+20);sNonce=get32(p+12);sEpoch=get32(p+32);
                __atomic_store_n(&sLobbyTotal,sPlayers,__ATOMIC_RELEASE);
                __atomic_store_n(&sLobbyKnown,(int)get32(p+40),__ATOMIC_RELEASE);
                if(sMe<1||sMe>=sPlayers||sPlayers>(sTournament?PORT_TOURNEY_MAX:PORT_COOP_MAX)) fail("Asignacion de jugador no valida");
                for(k=0;k<sizeof(PcBattle)/4;k++) v[k]=(int)get32(p+CHEAD+k*4);
                if(sBattle.mode!=(sTournament?PC_DUEL:PC_RAID)||sBattle.players!=(sTournament?2:sPlayers)||!Port_PcBattleValidate(&sBattle,why,sizeof(why))) fail("Plantilla del anfitrion no valida");
                if(sTournament) {
                    PortTourney received,initial;v=(int *)&received;
                    for(k=0;k<sizeof(PortTourney)/4;k++) v[k]=(int)get32(p+CHEAD+sizeof(PcBattle)+k*4);
                    if(!Port_TourneyInit(&initial,sPlayers) || received.count!=sPlayers || received.current!=0 || received.champion!=-1) fail("Cuadro no valido");
                    for(k=0;k<PORT_TOURNEY_MAX-1;k++) if(received.winners[k]!=-1) fail("Cuadro ya iniciado");
                    for(k=0;k<(unsigned)sPlayers;k++) {PcBattle check=sBattle;check.fighter[0][0]=received.fighters[k];if(!Port_PcBattleValidate(&check,why,sizeof(why))) fail("Participante no valido");}
                    sTourney=received;
                }
                control(C_ACK,0);
            }
            if(type==C_START && sMe>0 && get32(p+12)==sNonce && get32(p+32)==sEpoch) break;
        }
    }
    memset(sTag,255,sizeof(sTag));memset(sHashTick,255,sizeof(sHashTick));
    for(i=0;i<sPlayers;i++) { int t;for(t=0;t<CDELAY;t++) {memcpy(sInput[i][t],kIdle,CPAD);sTag[i][t]=(uint32_t)t;} }
    sStartTick=UINT32_MAX;sSampled=UINT32_MAX;sReady=1;sBattleArmed=sTournament;
    if(sTournament) {sActive[0]=0;sActive[1]=1;sBattle.fighter[0][0]=sTourney.fighters[0];sBattle.fighter[1][0]=sTourney.fighters[1];}
    *out=sBattle;
    __atomic_store_n(&sLobbyPhase,0,__ATOMIC_RELEASE);
    fprintf(stderr,"bt3: coop: conectado jugador %d/%d, entradas sincronizadas sin prediccion\n",sMe+1,sPlayers);
    sJumpActive=0;return 1;
}
void Port_CoopBeginLoad(void) {
    if(!sReady || sTournament) return;
    sBattleArmed=0;sLoadRound++;
}
void Port_CoopBattleReady(void) {
    uint64_t begun,last=0;int loaded[PORT_COOP_MAX]={1},i,all;
    if(!sReady || sTournament) return;
    if(setjmp(sFailureJump)) {sJumpActive=0;return;}
    sJumpActive=1;
    begun=millis();
    for(;;) {
        unsigned char p[1400];struct sockaddr_in from;int len=packet(p,&from),type,who=-1;
        if(millis()-begun>120000) fail("Un jugador no termino de cargar el combate");
        if(!sHost && millis()-last>=50) {control(C_LOADED,0);last=millis();}
        if(!len) {pause_ms();continue;}
        if(get32(p+12)!=sNonce || get64(p+24)!=sFingerprint) continue;
        type=(int)get32(p+8);
        if(type==C_ERROR && (!sHost && same(&from,&sPeer[0]))) fail("La sala se cerro durante la carga");
        if(get32(p+44)!=sLoadRound) continue;
        if(sHost) {
            for(i=1;i<sPlayers;i++) if(sKnown[i]&&same(&from,&sPeer[i])) who=i;
            if(who<0 || type!=C_LOADED) continue;
            loaded[who]=1;all=1;for(i=1;i<sPlayers;i++) all&=loaded[i];
            if(all) {for(i=1;i<sPlayers;i++) control(C_GO,i);break;}
        } else if(same(&from,&sPeer[0]) && type==C_GO) break;
    }
    memset(sTag,255,sizeof(sTag));memset(sHashTick,255,sizeof(sHashTick));
    for(i=0;i<sPlayers;i++) {int t;for(t=0;t<CDELAY;t++) {memcpy(sInput[i][t],kIdle,CPAD);sTag[i][t]=(uint32_t)t;}}
    sTick=0;sStartTick=UINT32_MAX;sSampled=UINT32_MAX;sBattleArmed=1;sJumpActive=0;
    fprintf(stderr,"bt3: coop: todos cargados, comienza el combate sincronizado\n");
}
static uint64_t digest(void) {
    uint64_t h=Port_NetDigest();int i,j;
    for(i=0;i<(sTournament?2:sBattle.count[0]+sBattle.count[1]);i++) {
        float pos[4] __attribute__((aligned(16)));unsigned char data[16];int hp=BtlCharApi_GetHp(i);
        BtlCharApi_GetPos(i,pos);memcpy(data,&hp,4);memcpy(data+4,pos,12);
        for(j=0;j<16;j++) {h^=data[j];h*=1099511628211ull;}
    }
    return h;
}
static void inputs(int source,int dest) {
    unsigned char p[1400];unsigned n=0,k,t,last=sTick+CDELAY;
    header(p,C_INPUT,source);
    {
        unsigned checked = sTick / 60 * 60;
        if(sHashTick[checked%CRING]==checked) {put32(p+32,checked);put64(p+36,sHashes[checked%CRING]);}
    }
    for(k=0;k<CREDUNDANT && k<=last;k++) {
        t=last-k;if(sTag[source][t%CRING]!=t) continue;
        put32(p+CHEAD+n*22,t);memcpy(p+CHEAD+n*22+4,sInput[source][t%CRING],CPAD);n++;
    }
    put32(p+44,n);send_to(p,CHEAD+(int)n*22,dest);
}
static void receive_inputs(void) {
    unsigned char p[1400];struct sockaddr_in from;int len,who,i;
    while((len=packet(p,&from))>0) {
        int type=(int)get32(p+8),source=(int)get32(p+16);unsigned n=get32(p+44),k;
        if(get32(p+12)!=sNonce) continue;
        who=-1;if(sHost) {for(i=1;i<sPlayers;i++) if(sKnown[i]&&same(&from,&sPeer[i])) who=i;}
        else if(same(&from,&sPeer[0])) who=0;
        if(who<0) continue;
        if(sHost&&type==C_ACK) {control(C_START,who);continue;}
        if(sHost&&type==C_LOADED && get32(p+44)==sLoadRound) {control(C_GO,who);continue;}
        if(type==C_ERROR) fail("La sala termino o perdio sincronizacion");
        if(type!=C_INPUT||source<0||source>=sPlayers||(sHost&&source!=who)||(!sHost&&source==sMe)||n>CREDUNDANT||len!=CHEAD+(int)n*22) continue;
        for(k=0;k<n;k++) {
            unsigned t=get32(p+CHEAD+k*22);
            if(t+CREDUNDANT<sTick || t>sTick+120) continue;
            if(sTag[source][t%CRING]==t && memcmp(sInput[source][t%CRING],p+CHEAD+k*22+4,CPAD)) fail("Entrada contradictoria");
            memcpy(sInput[source][t%CRING],p+CHEAD+k*22+4,CPAD);sTag[source][t%CRING]=t;
        }
        /* Compare only the sender's own simulation, never a forwarded hash. */
        if(source==who) {
            unsigned t=get32(p+32);uint64_t h=get64(p+36);
            if(h&&sHashTick[t%CRING]==t&&sHashes[t%CRING]!=h) {
                fprintf(stderr,"bt3: coop: diferencia en turno %u, par %d, local %016llx remoto %016llx\n",
                    t,who,(unsigned long long)sHashes[t%CRING],(unsigned long long)h);
                fail("Las simulaciones difieren");
            }
        }
    }
}
void Port_CoopPauseSimulation(int paused) {sSimulationPaused=paused!=0;}
void Port_CoopBeginTick(unsigned tick) {
    uint64_t begun,last=0;int i,j,all;
    if(!sReady || sFailed || sSimulationPaused || !sBattleArmed) return;
    if(setjmp(sFailureJump)) {sJumpActive=0;return;}
    sJumpActive=1;
    if(sStartTick==UINT32_MAX) sStartTick=tick;
    sTick=tick-sStartTick;
    if(sSampled!=sTick) {
        unsigned at=sTick+CDELAY;unsigned char p[CPAD];memcpy(p,kIdle,CPAD);
        if(!sTournament || sMe==sActive[0] || sMe==sActive[1]) Port_PadRead(0,p);
        /* Opt-in full-engine regression: distinct real network inputs, with
           movement, attacks and target cycling. Never enabled in normal play. */
        if(!sTournament && getenv("BT3_COOP_TEST_INPUT") && sTick>240) {
            unsigned held=0,phase=sTick+(unsigned)sMe*17;
            if(phase%16<4) held|=0x8000; /* square */
            if(phase%120>=40 && phase%120<44) held|=0x1000; /* triangle */
            if(phase%180<2) held|=2; /* L3 target cycle */
            memcpy(p,kIdle,CPAD);p[0]=(unsigned char)~held;p[1]=(unsigned char)(~held>>8);
            p[4]=(unsigned char)(phase%240<120?64:192);p[5]=64;
        }
        memcpy(sInput[sMe][at%CRING],p,CPAD);sTag[sMe][at%CRING]=at;sSampled=sTick;
        if(sTick%60==0) {sHashTick[sTick%CRING]=sTick;sHashes[sTick%CRING]=digest();}
    }
    begun=millis();
    do {
        if(millis()-last>=4) {
            if(sHost) {for(i=1;i<sPlayers;i++) for(j=0;j<sPlayers;j++) if(i!=j) inputs(j,i);}
            else inputs(sMe,0);
            last=millis();
        }
        receive_inputs();all=1;for(i=0;i<sPlayers;i++) all &= sTag[i][sTick%CRING]==sTick;
        if(millis()-begun>15000) fail("Un jugador dejo de responder");
        if(!all) pause_ms();
    } while(!all);
    if(sHost) for(i=1;i<sPlayers;i++) for(j=0;j<sPlayers;j++) if(i!=j) inputs(j,i);
    sJumpActive=0;
    if(sTick%120==0) fprintf(stderr,"bt3: coop: frame %u, %d/%d entradas confirmadas, HP %d/%d\n",sTick,sPlayers,sPlayers,BtlCharApi_GetHp(0),BtlCharApi_GetHp(1));
}
void Port_CoopInput(int player,unsigned char *data) {
    int source=player;
    if(sFailed || (sTournament && sRecorded)) {memcpy(data,kIdle,CPAD);return;}
    if(sTournament) source=player>=0&&player<2?sActive[player]:-1;
    memcpy(data,sReady&&source>=0&&source<sPlayers&&sTag[source][sTick%CRING]==sTick?sInput[source][sTick%CRING]:kIdle,CPAD);
}
int Port_CoopLaunch(int host,const char *address,int port,const PcBattle *b) {
    char endpoint[300],config[900],why[160];unsigned i;size_t used=0;
    const int *v=(const int *)b;
    if(Port_CoopEnabled()||!b||b->mode!=PC_RAID||b->players<2||port<1||port>65535||!Port_PcBattleValidate(b,why,sizeof(why))) return 0;
    if(!host && (!address||!address[0]||strlen(address)>200)) return 0;
    if(host) snprintf(endpoint,sizeof(endpoint),"%d",port);
    if(!host) snprintf(endpoint,sizeof(endpoint),"%s:%d",address,port);
    for(i=0;i<sizeof(PcBattle)/4;i++) used+=(size_t)snprintf(config+used,sizeof(config)-used,"%s%d",i?",":"",v[i]);
#ifdef _WIN32
    SetEnvironmentVariableA("BT3_COOP_HOST",host?endpoint:NULL);SetEnvironmentVariableA("BT3_COOP_JOIN",host?NULL:endpoint);
    SetEnvironmentVariableA("BT3_COOP_CONFIG",config);
    /* CreateProcess inherits Win32's environment, while this module's getenv
       reads the CRT copy. Keep both consistent before relaunch checks it. */
    _putenv_s("BT3_COOP_HOST",host?endpoint:"");
    _putenv_s("BT3_COOP_JOIN",host?"":endpoint);
    _putenv_s("BT3_COOP_CONFIG",config);
#else
    setenv(host?"BT3_COOP_HOST":"BT3_COOP_JOIN",endpoint,1);setenv("BT3_COOP_CONFIG",config,1);
#endif
    Port_CoopRelaunch();return 1;
}

int Port_TourneyEnabled(void) {return getenv("BT3_TOURNAMENT")!=NULL;}
int Port_TourneyCopy(PortTourney *out) {
    if(!sReady || !sTournament || !out) return 0;
    pthread_mutex_lock(&sTourneyLock);*out=sTourney;pthread_mutex_unlock(&sTourneyLock);return 1;
}
int Port_TourneySpectating(void) {
    int spectator;pthread_mutex_lock(&sTourneyLock);
    spectator=sTournament && sReady && sMe!=sActive[0] && sMe!=sActive[1];
    pthread_mutex_unlock(&sTourneyLock);return spectator;
}
void Port_TourneyActive(int *a,int *b) {pthread_mutex_lock(&sTourneyLock);*a=sActive[0];*b=sActive[1];pthread_mutex_unlock(&sTourneyLock);}
void Port_TourneyResult(int resultBits) {
    if(!sTournament || !sReady || sRecorded) return;
    pthread_mutex_lock(&sTourneyLock);
    sRecorded=1;
    if(resultBits==1 || resultBits==2) Port_TourneyAdvance(&sTourney,resultBits==2);
    fprintf(stderr,"bt3: torneo: resultado %d, siguiente %d, campeon %d\n",resultBits,sTourney.current,sTourney.champion);
    pthread_mutex_unlock(&sTourneyLock);
}
void Port_CoopLeave(void) {
    if(sSock>=0) {closesocket(sSock);sSock=-1;}
    sReady=sTournament=sFailed=sCancel=sBattleArmed=sSimulationPaused=0;sLoadRound=0;
#ifdef _WIN32
    SetEnvironmentVariableA("BT3_COOP_HOST",NULL);_putenv_s("BT3_COOP_HOST","");
    SetEnvironmentVariableA("BT3_COOP_JOIN",NULL);_putenv_s("BT3_COOP_JOIN","");
    SetEnvironmentVariableA("BT3_COOP_CONFIG",NULL);_putenv_s("BT3_COOP_CONFIG","");
    SetEnvironmentVariableA("BT3_TOURNAMENT",NULL);_putenv_s("BT3_TOURNAMENT","");
#else
    unsetenv("BT3_COOP_HOST");unsetenv("BT3_COOP_JOIN");unsetenv("BT3_COOP_CONFIG");unsetenv("BT3_TOURNAMENT");
#endif
}
void Port_TourneyLeave(void) {if(sTournament) Port_CoopLeave();}
int Port_CoopFailed(void) {return sFailed;}
int Port_CoopTestInput(void) {return getenv("BT3_COOP_TEST_INPUT")!=NULL;}
void Port_CoopErrorText(char *out,int size) {if(size>0)snprintf(out,(size_t)size,"%s",sFailureText);}
int Port_TourneyLaunch(int host,const char *address,int port,int count,const PcFighter *hero) {
    PcBattle b;char endpoint[300],text[900],size[16],why[160];unsigned i;size_t used=0;const int *v=(const int *)&b;
    if(Port_CoopEnabled() || !hero || (count!=4 && count!=8 && count!=16) || port<1 || port>65535 ||
       (!host && (!address || !address[0] || strlen(address)>200))) return 0;
    Port_PcBattleDefault(&b);b.mode=PC_DUEL;b.players=2;b.stage=4;b.fighter[0][0]=*hero;
    if(!Port_PcBattleValidate(&b,why,sizeof(why))) return 0;
    snprintf(size,sizeof(size),"%d",count);
    if(host) snprintf(endpoint,sizeof(endpoint),"%d",port);
    if(!host) snprintf(endpoint,sizeof(endpoint),"%s:%d",address,port);
    for(i=0;i<sizeof(b)/4;i++) used+=(size_t)snprintf(text+used,sizeof(text)-used,"%s%d",i?",":"",v[i]);
#ifdef _WIN32
    SetEnvironmentVariableA("BT3_TOURNAMENT",size);_putenv_s("BT3_TOURNAMENT",size);
    SetEnvironmentVariableA("BT3_COOP_HOST",host?endpoint:NULL);_putenv_s("BT3_COOP_HOST",host?endpoint:"");
    SetEnvironmentVariableA("BT3_COOP_JOIN",host?NULL:endpoint);_putenv_s("BT3_COOP_JOIN",host?"":endpoint);
    SetEnvironmentVariableA("BT3_COOP_CONFIG",text);_putenv_s("BT3_COOP_CONFIG",text);
#else
    setenv("BT3_TOURNAMENT",size,1);setenv(host?"BT3_COOP_HOST":"BT3_COOP_JOIN",endpoint,1);setenv("BT3_COOP_CONFIG",text,1);
#endif
    Port_CoopRelaunch();return 1;
}

int Port_TourneyTestBots(void) {return Port_TourneyEnabled() && getenv("BT3_TOURNAMENT_TEST_BOTS")!=NULL;}

int Port_TourneyFailed(void) {return sFailed;}
void Port_TourneyErrorText(char *out,int size) {if(size>0)snprintf(out,(size_t)size,"%s",sFailureText);}
void Port_TourneyCancel(void) {__atomic_store_n(&sCancel,1,__ATOMIC_RELEASE);}

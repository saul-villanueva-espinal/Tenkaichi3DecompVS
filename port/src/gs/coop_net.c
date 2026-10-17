/* Four-peer Raid, independent of Dragon Net Battle's two-peer rollback protocol.
   A host relays authenticated-by-endpoint input slots; every copy simulates the
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
enum { CMAGIC = 0x42543343, CVERSION = 1, C_HELLO = 1, C_CONFIG, C_ACK, C_START, C_INPUT, C_ERROR,
       CRING = 4096, CPAD = 18, CREDUNDANT = 16, CDELAY = 3, CHEAD = 48 };
static int sSock = -1, sHost, sMe, sPlayers, sReady;
static uint32_t sNonce, sTick, sStartTick, sSampled;
static PcBattle sBattle;
static struct sockaddr_in sPeer[4];
static int sKnown[4], sAck[4];
static unsigned char sInput[4][CRING][CPAD];
static uint32_t sTag[4][CRING], sHashTick[CRING];
static uint64_t sHashes[CRING], sFingerprint;
static const unsigned char kIdle[CPAD] = {255,255,128,128,128,128};
extern int Port_PadRead(int, unsigned char *);
extern void Port_VerifyStart(void), PortCharas_Init(void);
extern int Port_DataFingerprint(unsigned long long *);
extern unsigned long long Port_CharacterFingerprint(void), Port_NetDigest(void);
extern int BtlCharApi_GetHp(int);
extern void BtlCharApi_GetPos(int, float *);
extern void Port_CoopRelaunch(void);

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
    put32(p+16,who);put32(p+20,sPlayers);put64(p+24,sFingerprint);
}
static void control(int type,int who) {
    unsigned char p[CHEAD+sizeof(PcBattle)];
    header(p,type,who);
    if (type == C_CONFIG) {
        const int *v = (const int *)&sBattle;
        unsigned i; for(i=0;i<sizeof(PcBattle)/4;i++) put32(p+CHEAD+i*4,(uint32_t)v[i]);
    }
    send_to(p,CHEAD+(type==C_CONFIG ? sizeof(PcBattle):0),who);
}
static void fail(const char *why) {
    int i;
    fprintf(stderr,"bt3: coop: ERROR: %s\n",why); fflush(stderr);
    if(sHost) for(i=1;i<sPlayers;i++) if(sKnown[i]) control(C_ERROR,i);
    if(sSock>=0) closesocket(sSock);
    exit(2);
}
int Port_CoopEnabled(void) {
    return getenv("BT3_COOP_HOST") != NULL || getenv("BT3_COOP_JOIN") != NULL;
}
int Port_CoopMe(void) { return sMe; }
static void parse_config(const char *text) {
    int *v=(int *)&sBattle; unsigned i;
    Port_PcBattleDefault(&sBattle); sBattle.mode=PC_RAID;sBattle.players=4;sBattle.count[0]=4;
    if(!text) return;
    for(i=0;i<sizeof(PcBattle)/4;i++) {
        char *end; long n=strtol(text,&end,10);
        if(end==text || (i+1<sizeof(PcBattle)/4 && *end!=',')) fail("Configuracion incompleta");
        v[i]=(int)n; text=*end ? end+1:end;
    }
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
    if(sReady) { *out=sBattle; return 1; }
    Port_VerifyStart();PortCharas_Init();
    begun=millis();
    while(Port_DataFingerprint(&base)!=1) { if(millis()-begun>30000) fail("No se pudo verificar los datos del juego");pause_ms(); }
    sFingerprint=base ^ Port_CharacterFingerprint() ^ (uint64_t)PORT_TOTAL_CHARAS<<32;
    sHost=getenv("BT3_COOP_HOST")!=NULL;
    sMe=sHost?0:-1;
    parse_config(getenv("BT3_COOP_CONFIG"));
    sPlayers=sBattle.players;
    if(sPlayers<2 || sPlayers>4) fail("Raid online requiere de dos a cuatro jugadores");
    if(sHost) {
        int port=atoi(getenv("BT3_COOP_HOST")); if(port<1||port>65535) fail("Puerto no valido");
        if(!Port_PcBattleValidate(&sBattle,why,sizeof(why)) || sBattle.mode!=PC_RAID) fail("Plantilla Raid no valida");
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
    begun=millis();
    for(;;) {
        unsigned char p[1400];struct sockaddr_in from; int len=packet(p,&from),type,who=-1,all;
        if(millis()-begun>120000) fail("Tiempo de espera de la sala agotado");
        if(millis()-last>100) {
            if(!sHost) control(sMe<0?C_HELLO:C_ACK,0);
            else for(i=1;i<sPlayers;i++) if(sKnown[i]) control(C_CONFIG,i);
            last=millis();
        }
        if(!len) { pause_ms();continue; }
        type=(int)get32(p+8);
        if(sHost) {
            for(i=1;i<sPlayers;i++) if(sKnown[i]&&same(&from,&sPeer[i])) who=i;
            if(type==C_HELLO && who<0) {
                if(get64(p+24)!=sFingerprint) {
                    unsigned char error[CHEAD];header(error,C_ERROR,0);
                    sendto(sSock,(char *)error,CHEAD,0,(struct sockaddr *)&from,sizeof(from));continue;
                }
                for(i=1;i<sPlayers;i++) if(!sKnown[i]) { who=i;sPeer[i]=from;sKnown[i]=1;break; }
            }
            if(who<0) continue;
            if(type==C_HELLO) control(C_CONFIG,who);
            if(type==C_ACK && get32(p+12)==sNonce && get64(p+24)==sFingerprint) sAck[who]=1;
            all=1;for(i=1;i<sPlayers;i++) all &= sAck[i];
            if(all) { for(i=1;i<sPlayers;i++) control(C_START,i);break; }
        } else {
            if(!same(&from,&sPeer[0])) continue;
            if(type==C_ERROR) fail("El anfitrion rechazo la conexion: version, datos o paquetes diferentes");
            if(type==C_CONFIG && len==CHEAD+sizeof(PcBattle) && get64(p+24)==sFingerprint) {
                int *v=(int *)&sBattle;unsigned k;
                sMe=(int)get32(p+16);sPlayers=(int)get32(p+20);sNonce=get32(p+12);
                if(sMe<1||sMe>=sPlayers||sPlayers>4) fail("Asignacion de jugador no valida");
                for(k=0;k<sizeof(PcBattle)/4;k++) v[k]=(int)get32(p+CHEAD+k*4);
                if(sBattle.mode!=PC_RAID||sBattle.players!=sPlayers||!Port_PcBattleValidate(&sBattle,why,sizeof(why))) fail("Plantilla del anfitrion no valida");
                control(C_ACK,0);
            }
            if(type==C_START && sMe>0 && get32(p+12)==sNonce) break;
        }
    }
    memset(sTag,255,sizeof(sTag));memset(sHashTick,255,sizeof(sHashTick));
    for(i=0;i<sPlayers;i++) { int t;for(t=0;t<CDELAY;t++) {memcpy(sInput[i][t],kIdle,CPAD);sTag[i][t]=(uint32_t)t;} }
    sStartTick=UINT32_MAX;sSampled=UINT32_MAX;sReady=1;*out=sBattle;
    fprintf(stderr,"bt3: coop: conectado jugador %d/%d, entradas sincronizadas sin prediccion\n",sMe+1,sPlayers);
    return 1;
}
static uint64_t digest(void) {
    uint64_t h=Port_NetDigest();int i,j;
    for(i=0;i<=sPlayers;i++) {
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
            if(h&&sHashTick[t%CRING]==t&&sHashes[t%CRING]!=h) fail("Las simulaciones difieren");
        }
    }
}
void Port_CoopBeginTick(unsigned tick) {
    uint64_t begun,last=0;int i,j,all;
    if(!sReady) return;
    if(sStartTick==UINT32_MAX) sStartTick=tick;
    sTick=tick-sStartTick;
    if(sSampled!=sTick) {
        unsigned at=sTick+CDELAY;unsigned char p[CPAD];memcpy(p,kIdle,CPAD);
        Port_PadRead(0,p);memcpy(sInput[sMe][at%CRING],p,CPAD);sTag[sMe][at%CRING]=at;sSampled=sTick;
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
    if(sTick%120==0) fprintf(stderr,"bt3: coop: frame %u, %d/%d entradas confirmadas, HP %d/%d\n",sTick,sPlayers,sPlayers,BtlCharApi_GetHp(0),BtlCharApi_GetHp(1));
}
void Port_CoopInput(int player,unsigned char *data) {
    memcpy(data,sReady&&player>=0&&player<sPlayers&&sTag[player][sTick%CRING]==sTick?sInput[player][sTick%CRING]:kIdle,CPAD);
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
#else
    setenv(host?"BT3_COOP_HOST":"BT3_COOP_JOIN",endpoint,1);setenv("BT3_COOP_CONFIG",config,1);
#endif
    Port_CoopRelaunch();return 1;
}

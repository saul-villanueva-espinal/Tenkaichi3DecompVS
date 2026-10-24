/* Six-player relay client for our private room directory, not the official
   matchmaking API. Both PCs send outbound UDP to the same reachable server. */
#include "coop_rooms.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef int room_socklen;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
typedef socklen_t room_socklen;
#endif
enum { RH=40, MAX_INNER=1360, RR_CREATE=1, RR_JOIN, RR_ACCEPT, RR_DATA, RR_PING, RR_PONG, RR_LEAVE, RR_ERROR, RR_RESUME };
static struct sockaddr_in sServer;
static unsigned char sToken[16];
static char sCode[9],sError[160];
static int sActive,sSlot,sActiveSock = -1;
static uint32_t sRequest;
static uint64_t sPing;
static int sConnectBusy,sConnectDone,sConnectCancel;
static pthread_t sConnectThread;
static struct {
    int host,protocol,capacity,sock,result;
    uint64_t fingerprint;
    char code[32],name[32];
} sConnect;
static uint64_t now_ms(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;
#endif
}
static void idle(void) {
#ifdef _WIN32
    Sleep(1);
#else
    struct timespec t={0,1000000};nanosleep(&t,NULL);
#endif
}
static int random_token(void) {
    int i;
    for(i=0;i<(int)sizeof(sToken);i++) if(sToken[i]) return 1;
#ifdef _WIN32
    typedef LONG (WINAPI *RandomFn)(void *,unsigned char *,unsigned long,unsigned long);
    HMODULE lib=LoadLibraryA("bcrypt.dll");RandomFn fn;int ok;
    if(!lib) return 0;
    fn=(RandomFn)(void *)GetProcAddress(lib,"BCryptGenRandom");
    ok=fn && fn(NULL,sToken,sizeof(sToken),2)>=0;FreeLibrary(lib);return ok;
#else
    FILE *f=fopen("/dev/urandom","rb");int ok;
    if(!f) return 0;ok=fread(sToken,1,sizeof(sToken),f)==sizeof(sToken);fclose(f);return ok;
#endif
}
static void put32(unsigned char *p,uint32_t n) {n=htonl(n);memcpy(p,&n,4);}
static uint32_t get32(const unsigned char *p) {uint32_t n;memcpy(&n,p,4);return ntohl(n);}
static int same(const struct sockaddr_in *a,const struct sockaddr_in *b) {
    return a->sin_addr.s_addr==b->sin_addr.s_addr && a->sin_port==b->sin_port;
}
const char *RoomRelay_Server(void) {
    static char saved[256];const char *env=getenv("BT3_ROOM_SERVER");FILE *f;
    if(env && env[0]) return env;
    saved[0]=0;f=fopen("tbo/online_server.txt","r");
    if(f) {if(fgets(saved,sizeof(saved),f)) saved[strcspn(saved,"\r\n \t")]=0;fclose(f);}
    if(!saved[0]) snprintf(saved,sizeof(saved),"127.0.0.1:29741");
    return saved;
}
static const char *host_server(void) {
    static char saved[256];const char *env=getenv("BT3_ROOM_HOST_SERVER");FILE *f;
    if(env && env[0])return env;
    saved[0]=0;f=fopen("tbo/online_host_server.txt","r");
    if(f) {if(fgets(saved,sizeof(saved),f))saved[strcspn(saved,"\r\n \t")]=0;fclose(f);}
    return saved[0]?saved:RoomRelay_Server();
}
static void room_env(const char *key,const char *value) {
#ifdef _WIN32
    SetEnvironmentVariableA(key,value);_putenv_s(key,value?value:"");
#else
    if(value)setenv(key,value,1);else unsetenv(key);
#endif
}
int RoomRelay_ExportSession(void) {
    char endpoint[128],ip[64],hex[33],slot[8];int i;
    if(!sActive)return 0;
    if(!inet_ntop(AF_INET,&sServer.sin_addr,ip,sizeof(ip)))return 0;
    snprintf(endpoint,sizeof(endpoint),"%s:%u",ip,(unsigned)ntohs(sServer.sin_port));
    for(i=0;i<16;i++)snprintf(hex+i*2,3,"%02x",sToken[i]);
    snprintf(slot,sizeof(slot),"%d",sSlot);
    room_env("BT3_ROOM_RESUME_SERVER",endpoint);room_env("BT3_ROOM_RESUME_CODE",sCode);
    room_env("BT3_ROOM_RESUME_TOKEN",hex);room_env("BT3_ROOM_RESUME_SLOT",slot);return 1;
}
void RoomRelay_Party(const int *slots,int count,int hero,int costume) {
    char map[32],fighter[32];int i,n=0;
    if(count<2||count>6||!slots)return;
    for(i=0;i<count;i++) {if(slots[i]<0||slots[i]>=6)return;n+=snprintf(map+n,sizeof(map)-(size_t)n,"%s%d",i?",":"",slots[i]);}
    snprintf(fighter,sizeof(fighter),"%d,%d",hero,costume);
    room_env("BT3_ROOM_PARTY",map);room_env("BT3_ROOM_HERO",fighter);
}
int RoomRelay_PeerSlot(int player) {
    const char *p=getenv("BT3_ROOM_PARTY");int i;
    if(!p)return player;
    for(i=0;i<player;i++) {p=strchr(p,',');if(!p)return -1;p++;}
    return *p>='0'&&*p<='5' && (!p[1]||p[1]==',')?*p-'0':-1;
}
int RoomRelay_LocalHero(int *hero,int *costume) {
    const char *p=getenv("BT3_ROOM_HERO");return p&&sscanf(p,"%d,%d",hero,costume)==2;
}
void RoomRelay_ForgetSession(void) {
    room_env("BT3_ROOM_RESUME_SERVER",NULL);room_env("BT3_ROOM_RESUME_CODE",NULL);
    room_env("BT3_ROOM_RESUME_TOKEN",NULL);room_env("BT3_ROOM_RESUME_SLOT",NULL);
    room_env("BT3_ROOM_PARTY",NULL);room_env("BT3_ROOM_HERO",NULL);
}
const char *RoomRelay_Code(void) {return sCode;}
const char *RoomRelay_Error(void) {return sError;}
int RoomRelay_Active(void) {return sActive;}
int RoomRelay_Slot(void) {return sSlot;}
int RoomRelay_Socket(void) {return sActiveSock;}
void RoomRelay_SetSocket(int sock) {sActiveSock=sock;}
void RoomRelay_GetToken(unsigned char *out) {if(out) memcpy(out,sToken,sizeof(sToken));}
void RoomRelay_SetToken(const unsigned char *in) {if(in) memcpy(sToken,in,sizeof(sToken));}
static void send_packet(int sock,int op,int target,const void *data,int size) {
    unsigned char p[RH+MAX_INNER];
    if(size<0 || size>MAX_INNER) return;
    memset(p,0,RH);memcpy(p,"BT3R",4);p[4]=1;p[5]=(unsigned char)op;
    put32(p+8,sRequest);memcpy(p+12,sToken,16);memcpy(p+28,sCode,8);
    p[36]=(unsigned char)sSlot;p[37]=(unsigned char)target;p[38]=(unsigned char)(size>>8);p[39]=(unsigned char)size;
    if(size) memcpy(p+RH,data,(size_t)size);
    sendto(sock,(const char *)p,RH+size,0,(const struct sockaddr *)&sServer,sizeof(sServer));
}
static int receive(int sock,unsigned char *p) {
    struct sockaddr_in from;room_socklen n=sizeof(from);
    int len=(int)recvfrom(sock,(char *)p,RH+MAX_INNER+1,0,(struct sockaddr *)&from,&n);
    if(len<RH || len>RH+MAX_INNER || !same(&from,&sServer) || memcmp(p,"BT3R",4) || p[4]!=1 || p[6] || p[7] ||
       memcmp(p+12,sToken,16) || ((int)p[38]*256+p[39])!=len-RH) return 0;
    return len;
}
static int error_packet(const unsigned char *p,int len) {
    int n=len-RH;if(n>=(int)sizeof(sError)) n=sizeof(sError)-1;
    memcpy(sError,p+RH,(size_t)n);sError[n]=0;
    if(!n) snprintf(sError,sizeof(sError),"El servicio rechazo la solicitud");
    return 0;
}
static int open_room(int sock,int host,const char *code,uint64_t fingerprint,
                   int protocol,int capacity,const char *name,void (*pump)(void),int resume) {
    char address[256],*colon;struct addrinfo hints,*resolved=NULL;
    unsigned char profile[42],packet[RH+MAX_INNER+1];uint64_t begun,last=0;int len;
    const char *server=resume?getenv("BT3_ROOM_RESUME_SERVER"):host?host_server():RoomRelay_Server();
    int expectedSlot=0;
    sActive=0;sCode[0]=sError[0]=0;sSlot=0;
    if(!server || strlen(server)>=sizeof(address)) {snprintf(sError,sizeof(sError),"Falta la direccion de la sala");return 0;}
    snprintf(address,sizeof(address),"%s",server);colon=strrchr(address,':');
    if(!colon || colon==address || !colon[1]) {snprintf(sError,sizeof(sError),"Falta configurar el servidor de salas");return 0;}
    *colon++=0;memset(&hints,0,sizeof(hints));hints.ai_family=AF_INET;hints.ai_socktype=SOCK_DGRAM;
    if(getaddrinfo(address,colon,&hints,&resolved)) {snprintf(sError,sizeof(sError),"No se encuentra el servidor de salas");return 0;}
    memcpy(&sServer,resolved->ai_addr,sizeof(sServer));freeaddrinfo(resolved);
    if(!host || resume) {
        int i;if(code && !strncmp(code,"BT3-",4)) code+=4;
        if(!code || strlen(code)!=6) {snprintf(sError,sizeof(sError),"Pega el codigo BT3 de seis caracteres");return 0;}
        for(i=0;i<6;i++) if(!strchr("23456789ABCDEFGHJKLMNPQRSTUVWXYZ",code[i])) {
            snprintf(sError,sizeof(sError),"Codigo no valido");return 0;
        }
        memcpy(sCode,code,6);sCode[6]=0;
    }
    if(resume) {
        const char *hex=getenv("BT3_ROOM_RESUME_TOKEN"),*slot=getenv("BT3_ROOM_RESUME_SLOT");int i;
        if(!hex||strlen(hex)!=32||!slot||strlen(slot)!=1||*slot<'0'||*slot>'5') {
            snprintf(sError,sizeof(sError),"No se conservo la sesion de la sala");return 0;
        }
        expectedSlot=*slot-'0';sSlot=expectedSlot;
        for(i=0;i<16;i++) {char digits[3]={hex[i*2],hex[i*2+1],0},*end;unsigned long n=strtoul(digits,&end,16);
            if(*end) {snprintf(sError,sizeof(sError),"Sesion de sala no valida");return 0;}sToken[i]=(unsigned char)n;}
    } else if(!random_token()) {snprintf(sError,sizeof(sError),"No se pudo proteger la invitacion");return 0;}
    ++sRequest;memset(profile,0,sizeof(profile));put32(profile,(uint32_t)(fingerprint>>32));put32(profile+4,(uint32_t)fingerprint);
    profile[8]=(unsigned char)protocol;profile[9]=(unsigned char)capacity;
    if(name) snprintf((char *)profile+10,32,"%.31s",name);
    begun=now_ms();
    do {
        if(__atomic_load_n(&sConnectCancel,__ATOMIC_ACQUIRE)) {
            snprintf(sError,sizeof(sError),"Conexion cancelada");return 0;
        }
        if(now_ms()-last>=250) {send_packet(sock,resume?RR_RESUME:host?RR_CREATE:RR_JOIN,0,profile,sizeof(profile));last=now_ms();}
        len=receive(sock,packet);
        if(len && get32(packet+8)==sRequest) {
            if(packet[5]==RR_ERROR) return error_packet(packet,len);
            if(packet[5]==RR_ACCEPT && len==RH+2 && packet[RH]<6 && packet[RH+1]>=2 && packet[RH+1]<=6 &&
               packet[RH]<packet[RH+1] && (resume?packet[RH]==expectedSlot:host?packet[RH]==0:packet[RH]>0)) {
                memcpy(sCode,packet+28,8);sCode[8]=0;sSlot=packet[RH];sActive=1;sActiveSock=sock;sPing=now_ms();return 1;
            }
        }
        if(pump) pump();
        idle();
    } while(now_ms()-begun<8000);
    snprintf(sError,sizeof(sError),"El servidor de salas no responde. Revisa la conexion o reintenta.");return 0;
}
int RoomRelay_Open(int sock,int host,const char *code,uint64_t fingerprint,
                   int protocol,int capacity,const char *name,void (*pump)(void)) {
    return open_room(sock,host,code,fingerprint,protocol,capacity,name,pump,0);
}
int RoomRelay_Resume(int sock,uint64_t fingerprint,int protocol,int capacity,void (*pump)(void)) {
    const char *code=getenv("BT3_ROOM_RESUME_CODE"),*slot=getenv("BT3_ROOM_RESUME_SLOT");
    if(!code||!slot) {snprintf(sError,sizeof(sError),"No se encontro la sesion creada en el lobby");return 0;}
    return open_room(sock,*slot=='0',code,fingerprint,protocol,capacity,"",pump,1);
}
static void *connect_worker(void *unused) {
    int ok=0;(void)unused;
    sConnect.sock=RoomRelay_CreateSocket();
    if(sConnect.sock>=0) ok=RoomRelay_Open(sConnect.sock,sConnect.host,sConnect.code,
        sConnect.fingerprint,sConnect.protocol,sConnect.capacity,sConnect.name,NULL);
    else snprintf(sError,sizeof(sError),"No se pudo abrir la conexion UDP");
    sConnect.result=__atomic_load_n(&sConnectCancel,__ATOMIC_ACQUIRE)?-2:ok?1:-1;
    if(sConnect.result==1) {fprintf(stderr,"bt3: raid: server confirmed room %s, slot %d\n",sCode,sSlot);fflush(stderr);}
    if(sConnect.result!=1 && sConnect.sock>=0) {
        RoomRelay_Close(sConnect.sock);RoomRelay_CloseSocket(sConnect.sock);sConnect.sock=-1;
    }
    __atomic_store_n(&sConnectDone,1,__ATOMIC_RELEASE);return NULL;
}
int RoomRelay_ConnectStart(int host,const char *code,uint64_t fingerprint,
                          int protocol,int capacity,const char *name) {
    if(sConnectBusy || sActive) return 0;
    memset(sToken,0,sizeof(sToken)); /* A new invitation owns a fresh capability. */
    memset(&sConnect,0,sizeof(sConnect));sConnect.host=host;sConnect.protocol=protocol;
    sConnect.capacity=capacity;sConnect.fingerprint=fingerprint;sConnect.sock=-1;
    snprintf(sConnect.code,sizeof(sConnect.code),"%s",code?code:"");
    snprintf(sConnect.name,sizeof(sConnect.name),"%.31s",name?name:"");
    __atomic_store_n(&sConnectDone,0,__ATOMIC_RELEASE);
    __atomic_store_n(&sConnectCancel,0,__ATOMIC_RELEASE);sConnectBusy=1;
    if(pthread_create(&sConnectThread,NULL,connect_worker,NULL)) {
        sConnectBusy=0;snprintf(sError,sizeof(sError),"No se pudo iniciar la conexion");return 0;
    }
    return 1;
}
int RoomRelay_ConnectPoll(int *sock) {
    if(!sConnectBusy) return -1;
    if(!__atomic_load_n(&sConnectDone,__ATOMIC_ACQUIRE)) return 0;
    pthread_join(sConnectThread,NULL);sConnectBusy=0;
    if(sock) *sock=sConnect.sock;
    __atomic_store_n(&sConnectCancel,0,__ATOMIC_RELEASE);
    return sConnect.result;
}
void RoomRelay_ConnectCancel(void) {__atomic_store_n(&sConnectCancel,1,__ATOMIC_RELEASE);}
void RoomRelay_Address(struct sockaddr_in *address,int slot) {
    memset(address,0,sizeof(*address));address->sin_family=AF_INET;
    address->sin_addr.s_addr=htonl(0x7f000001u+(unsigned)slot);address->sin_port=htons(29740);
}
void RoomRelay_Send(int sock,const void *data,int size,const struct sockaddr_in *to) {
    uint32_t ip=ntohl(to->sin_addr.s_addr);
    if(sActive && ip>=0x7f000001u && ip<=0x7f000006u) send_packet(sock,RR_DATA,(int)(ip-0x7f000001u),data,size);
}
int RoomRelay_Receive(int sock,unsigned char *data,struct sockaddr_in *from) {
    unsigned char p[RH+MAX_INNER+1];int len;
    if(now_ms()-sPing>=10000) {send_packet(sock,RR_PING,0,NULL,0);sPing=now_ms();}
    len=receive(sock,p);if(!len || memcmp(p+28,sCode,8)) return 0;
    if(p[5]==RR_ERROR) {error_packet(p,len);return -1;}
    if(p[5]!=RR_DATA || p[36]>=6 || p[37]!=sSlot || (sSlot && p[36]!=0)) return 0;
    RoomRelay_Address(from,p[36]);memcpy(data,p+RH,(size_t)(len-RH));return len-RH;
}
void RoomRelay_Close(int sock) {
    if(sActive && sock>=0) send_packet(sock,RR_LEAVE,0,NULL,0);
    sActive=0;sActiveSock=-1;sCode[0]=0;
}
int RoomRelay_CreateSocket(void) {
#ifdef _WIN32
    static int sWsaInit = 0;
    if(!sWsaInit) {
        WSADATA w;
        WSAStartup(MAKEWORD(2,2), &w);
        sWsaInit = 1;
    }
    u_long on = 1;
#endif
    int sock = (int)socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0) return -1;
#ifdef _WIN32
    ioctlsocket(sock, FIONBIO, &on);
#else
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
#endif
    struct sockaddr_in local;
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = 0;
    if(bind(sock, (struct sockaddr *)&local, sizeof(local)) != 0) {
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        return -1;
    }
    return sock;
}
void RoomRelay_CloseSocket(int sock) {
    if(sock >= 0) {
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
    }
}
uint64_t RoomRelay_NowMs(void) {
    return now_ms();
}
void RoomRelay_SendSlot(int sock, int targetSlot, const void *data, int size) {
    struct sockaddr_in to;
    RoomRelay_Address(&to, targetSlot);
    RoomRelay_Send(sock, data, size, &to);
}
int RoomRelay_RecvSlot(int sock, void *data, int *senderSlot) {
    struct sockaddr_in from;
    int len = RoomRelay_Receive(sock, (unsigned char *)data, &from);
    if(len > 0 && senderSlot) {
        uint32_t ip = ntohl(from.sin_addr.s_addr);
        *senderSlot = (int)(ip - 0x7f000001u);
    }
    return len;
}


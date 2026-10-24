#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include "../src/gs/coop_rooms.h"
static unsigned long long now(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (unsigned long long)t.tv_sec*1000+t.tv_nsec/1000000;
#endif
}
int main(int argc,char **argv) {
    int sock,host=argc==1,received[6]={0},total=0;unsigned char data[1400];
    unsigned long long begun,last=0,completed=0;struct sockaddr_in local,from,to;
    if(argc==2 && (!strcmp(argv[1],"--async-cancel") || !strcmp(argv[1],"--async-timeout"))) {
        int result=0,polls=0,cancel=!strcmp(argv[1],"--async-cancel");
        begun=now();assert(RoomRelay_ConnectStart(1,NULL,123,7,6,"fixture"));
        assert(now()-begun<250); /* Creating a room cannot block the menu. */
        while(!(result=RoomRelay_ConnectPoll(&sock)) && now()-begun<11000) {
            polls++;if(cancel && now()-begun>=50) RoomRelay_ConnectCancel();
#ifdef _WIN32
            Sleep(1);
#else
            {struct timespec t={0,1000000};nanosleep(&t,NULL);}
#endif
        }
        if(result!=(cancel?-2:-1)) fprintf(stderr,"async result=%d polls=%d error=%s\n",result,polls,RoomRelay_Error());
        /* Windows Sleep(1) may advance by 15.6 ms: do not assume 1-ms ticks. */
        assert(polls>0 && result==(cancel?-2:-1) && !RoomRelay_Active());
        if(cancel) assert(now()-begun<1500);
        else assert(strstr(RoomRelay_Error(),"no responde"));
        printf("ROOM_ASYNC_%s_PASS: %d menu polls during connection\n",cancel?"CANCEL":"TIMEOUT",polls);return 0;
    }
#ifdef _WIN32
    WSADATA w;u_long nonblock=1;assert(!WSAStartup(MAKEWORD(2,2),&w));
#endif
    sock=(int)socket(AF_INET,SOCK_DGRAM,0);assert(sock>=0);
#ifdef _WIN32
    ioctlsocket(sock,FIONBIO,&nonblock);
#else
    fcntl(sock,F_SETFL,fcntl(sock,F_GETFL,0)|O_NONBLOCK);
#endif
    memset(&local,0,sizeof(local));local.sin_family=AF_INET;local.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    assert(!bind(sock,(struct sockaddr *)&local,sizeof(local)));
    if(!RoomRelay_Open(sock,host,host?NULL:argv[1],argc>2?456:123,7,6,"fixture",NULL)) {
        fprintf(stderr,"%s\n",RoomRelay_Error());return argc>2?0:2;
    }
    assert(argc<=2);
    if(host) {printf("CODE=%s\n",RoomRelay_Code());fflush(stdout);}
    begun=now();
    while(now()-begun<10000) {
        int n=RoomRelay_Receive(sock,data,&from);
        assert(n>=0);
        if(!host && now()-last>25) {
            RoomRelay_Address(&to,0);RoomRelay_Send(sock,"hello",5,&to);last=now();
        }
        if(n==5 && !memcmp(data,"hello",5)) {
            if(host) {
                unsigned slot=ntohl(from.sin_addr.s_addr)-0x7f000001u;
                assert(slot>0 && slot<6);if(!received[slot]) {received[slot]=1;total++;}
                RoomRelay_Send(sock,"reply",5,&from);
            }
        }
        if(!host && n==5 && !memcmp(data,"reply",5)) {puts("ROOM_RELAY_CLIENT_PASS");return 0;}
        if(host && total==5) {
            if(!completed) completed=now();
            /* Keep answering retries while the last UDP reply is delivered. */
            if(now()-completed>=500) {puts("ROOM_RELAY_HOST_PASS: five authenticated guests exchanged data");return 0;}
        }
#ifdef _WIN32
        Sleep(1);
#else
        {struct timespec t={0,1000000};nanosleep(&t,NULL);}
#endif
    }
    fputs("relay timed out\n",stderr);return 3;
}

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "port_coop.h"
#include "../src/gs/coop_rooms.h"
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif
static char *self;
int Port_MapValid(int id) {return id>=0&&id<36;}
unsigned long long Port_MapFingerprint(int id) {(void)id;return 0;}
static unsigned frame;
void Port_VerifyStart(void) {}
void Port_CoopWaitFrame(void) {}
int Port_CoopWaitEvents(void) { return getenv("FIXTURE_CANCEL_WAIT") && frame>=60 && Port_CoopMe()==0; }
void PortCharas_Init(void) {}
int Port_DataFingerprint(unsigned long long *out) { *out=0x123456789abcdef0ull;return 1; }
unsigned long long Port_CharacterFingerprint(void) { return getenv("FIXTURE_BAD_ASSET")?123:456; }
unsigned long long Port_NetDigest(void) { return frame+1; }
int BtlCharApi_GetHp(int id) { return 30000-id; }
void BtlCharApi_GetPos(int id,float *out) { out[0]=id;out[1]=out[2]=out[3]=0; }
int Port_PadRead(int p,unsigned char *data) {
    (void)p;memset(data,0,18);data[0]=(unsigned char)(Port_CoopMe()+10);data[1]=(unsigned char)frame;return 18;
}
int Port_CharaCount(void) { return 0; }
const char *Port_FighterName(int c) { (void)c;return "fixture"; }
int Port_ExtraCostumeOk(int c,int costume,char *why,int size) { (void)c;(void)costume;(void)why;(void)size;return 1; }
int Port_NetActive(void) { return 0; }
int Port_NetSession(void) { return 0; }
int Port_ModesFight(void) { return 0; }
int ChrTbl_GetCost(int chara) { (void)chara;return 2; }
static int launched;
void Port_CoopRelaunch(void) {
    assert(Port_CoopEnabled());launched=1;
    if(getenv("FIXTURE_RESTART")) {
        char *args[]={self,"--relay-play",NULL};
#ifdef _WIN32
        /* Keep the test supervisor alive so failed children can be reaped.
           The child is still a fresh process with no relay globals. */
        intptr_t result=_spawnv(_P_WAIT,self,(const char *const *)args);
        exit(result<0?2:(int)result);
#else
        execv(self,args);
#endif
        abort();
    }
}
int main(int argc,char **argv) {
    PcBattle b;unsigned char pad[18];int p;
    self=argv[0];
    if(argc>=2 && !strcmp(argv[1],"--relay-setup")) {
        int host=getenv("FIXTURE_HOST")!=NULL,sock,slots[6],i;
        unsigned long long fp;
        /* Parent creates/joins, then execs a fresh child like the real game. */
        fp=0x123456789abcdef0ull ^ 456 ^ ((unsigned long long)PORT_TOTAL_CHARAS<<32);
        assert(Port_PcBattleParse(&b,getenv("BT3_COOP_CONFIG")));
        sock=RoomRelay_CreateSocket();assert(sock>=0);
        assert(RoomRelay_Open(sock,host,host?NULL:argv[2],fp,2,b.players,"fixture",NULL));
        if(host){printf("CODE=%s\n",RoomRelay_Code());fflush(stdout);}
        else {printf("JOINED=%d\n",RoomRelay_Slot());fflush(stdout);}
        for(i=0;i<b.players;i++)slots[i]=i;
        RoomRelay_Party(slots,b.players,getenv("FIXTURE_HERO")?atoi(getenv("FIXTURE_HERO")):0,0);
        assert(Port_CoopLaunch(host,RoomRelay_Code(),29740,&b));abort();
    }
    if(argc==2 && !strcmp(argv[1],"--launch-test")) {
        Port_PcBattleDefault(&b);b.mode=PC_RAID;b.players=2;b.count[0]=2;
        assert(Port_CoopLaunch(1,"127.0.0.1",29740,&b));
        assert(launched && getenv("BT3_COOP_HOST") && getenv("BT3_COOP_CONFIG"));
        puts("coop_launch_test: inherited and CRT environments agree");return 0;
    }
    if(getenv("FIXTURE_BAD_ASSET")) {
        assert(!Port_CoopConfig(&b) && Port_CoopFailed());Port_CoopLeave();return 2;
    }
    assert(Port_CoopConfig(&b));
    if(getenv("FIXTURE_BOSSES")) {
        int k;assert(b.count[1]==5);
        for(k=0;k<4;k++) assert(b.bossRules[k].bossHp==100000+k*10000 && b.bossRules[k].bossDamagePct==125+k*25);
    }
    assert(b.rules.bossHp==162500 && b.rules.bossDamagePct==200 && b.rules.enragePct==50);
    assert(b.rules.regenPerSec==150 && b.rules.damagePct==125 && b.rules.armorSides==2);
    assert(b.rules.cpuLevel1==30 && b.rules.bgm1==14 && b.rules.infiniteKiChara1==134);
    assert(b.fighter[0][Port_CoopMe()].chara==(getenv("FIXTURE_HERO")?atoi(getenv("FIXTURE_HERO")):0));
    for(p=0;p<b.players;p++) {int k,found=0;for(k=0;k<b.players;k++) found+=b.fighter[0][k].chara==p*2;assert(found==1);}
    Port_CoopBeginLoad();
    Port_CoopBeginTick(Port_CoopMe()*117); /* loading frames are intentionally different */
    Port_CoopBattleReady();
    /* Unequal local menu cleanup must not consume synchronized input ticks. */
    Port_CoopPauseSimulation(1);Port_CoopBeginTick(76543);Port_CoopPauseSimulation(0);
    for(frame=0;frame<300;frame++) {
        if(getenv("FIXTURE_PACE_MS")) {
#ifdef _WIN32
            _sleep((unsigned)atoi(getenv("FIXTURE_PACE_MS")));
#else
            {struct timespec pause={0,atoi(getenv("FIXTURE_PACE_MS"))*1000000};nanosleep(&pause,NULL);}
#endif
        }
        if(getenv("FIXTURE_DROP_GUEST") && Port_CoopMe()==b.players-1 && frame==80) {
            puts("coop_peer_test: intentional guest departure");return 0;
        }
        if(getenv("FIXTURE_CANCEL_WAIT") && Port_CoopMe()==b.players-1 && frame==60) {
#ifdef _WIN32
            _sleep(1500);
#else
            {struct timespec pause={1,500000000};nanosleep(&pause,NULL);}
#endif
        }
        Port_CoopBeginTick(frame);
        if(Port_CoopFailed()) {
            assert(getenv("FIXTURE_CANCEL_WAIT"));
            Port_CoopLeave();assert(!Port_CoopEnabled());
            puts("coop_peer_test: cancellation recovered without process exit");return 0;
        }
        for(p=0;p<b.players;p++) {
            Port_CoopInput(p,pad);
            if(Port_CoopPlayerDropped(p)) {assert(getenv("FIXTURE_DROP_GUEST") && p==b.players-1 && pad[0]==255);continue;}
            int delay=Port_CoopInputDelay();
            assert(pad[0]==(frame<(unsigned)delay?255:p+10));
            if(frame>=(unsigned)delay) assert(pad[1]==(unsigned char)(frame-delay));
        }
    }
    if((getenv("FIXTURE_RESTART") || getenv("FIXTURE_DROP_GUEST")) && Port_CoopMe()==0) {
        /* The fixture exits at 300 whereas slower guests may still need the
           host's last redundant datagrams. Keep serving tick 299 briefly. */
        unsigned i;frame=299;
        for(i=0;i<100;i++) {
            Port_CoopBeginTick(299);
#ifdef _WIN32
            _sleep(5);
#else
            {struct timespec pause={0,5000000};nanosleep(&pause,NULL);}
#endif
        }
    }
    if(getenv("FIXTURE_DROP_GUEST")) {assert(Port_CoopPlayerDropped(b.players-1));assert(Port_CoopConnected()==b.players-1);puts("coop_peer_test: guest lost, remaining peers continued to tick 300");}
    printf("coop_peer_test: all %d distinct pads match for 300 ticks\n",b.players);
}

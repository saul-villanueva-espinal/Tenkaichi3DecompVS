#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_coop.h"
static unsigned frame;
void Port_VerifyStart(void) {}
void Port_CoopWaitFrame(void) {}
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
void Port_CoopRelaunch(void) { assert(Port_CoopEnabled()); launched=1; }
int main(int argc,char **argv) {
    PcBattle b;unsigned char pad[18];int p;
    if(argc==2 && !strcmp(argv[1],"--launch-test")) {
        Port_PcBattleDefault(&b);b.mode=PC_RAID;b.players=2;b.count[0]=2;
        assert(Port_CoopLaunch(1,"127.0.0.1",29740,&b));
        assert(launched && getenv("BT3_COOP_HOST") && getenv("BT3_COOP_CONFIG"));
        puts("coop_launch_test: inherited and CRT environments agree");return 0;
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
    for(frame=0;frame<300;frame++) {
        Port_CoopBeginTick(frame);
        for(p=0;p<b.players;p++) {
            Port_CoopInput(p,pad);
            assert(pad[0]==(frame<3?255:p+10));
            if(frame>=3) assert(pad[1]==(unsigned char)(frame-3));
        }
    }
    printf("coop_peer_test: all %d distinct pads match for 300 ticks\n",b.players);
}

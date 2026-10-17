#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_coop.h"
static unsigned frame;
void Port_VerifyStart(void) {}
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
void Port_CoopRelaunch(void) { abort(); }
int main(void) {
    PcBattle b;unsigned char pad[18];int p;
    assert(Port_CoopConfig(&b));
    for(frame=0;frame<300;frame++) {
        Port_CoopBeginTick(frame);
        for(p=0;p<b.players;p++) {
            Port_CoopInput(p,pad);
            assert(pad[0]==(frame<3?255:p+10));
            if(frame>=3) assert(pad[1]==(unsigned char)(frame-3));
        }
    }
    puts("coop_peer_test: all four distinct pads match for 300 ticks");
}

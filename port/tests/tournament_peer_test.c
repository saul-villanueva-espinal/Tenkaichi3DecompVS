#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_tournament.h"
#include "port_coop.h"
static unsigned frame;
void Port_VerifyStart(void){}
void Port_CoopWaitFrame(void){}
void PortCharas_Init(void){}
int Port_DataFingerprint(unsigned long long *p){*p=123456;return 1;}
unsigned long long Port_CharacterFingerprint(void){return 456789;}
unsigned long long Port_NetDigest(void){return frame+1;}
int BtlCharApi_GetHp(int id){return 30000-id;}
void BtlCharApi_GetPos(int id,float *p){p[0]=id;p[1]=p[2]=p[3]=0;}
int Port_PadRead(int p,unsigned char *data){(void)p;memset(data,0,18);data[0]=Port_CoopMe()+10;data[1]=frame;return 18;}
int Port_CharaCount(void){return 0;}
const char *Port_FighterName(int c){(void)c;return "fixture";}
int Port_ExtraCostumeOk(int c,int t,char *why,int n){(void)c;(void)t;(void)why;(void)n;return 1;}
int Port_NetActive(void){return 0;}
int Port_NetSession(void){return 0;}
int Port_ModesFight(void){return 0;}
int ChrTbl_GetCost(int c){(void)c;return 2;}
void Port_CoopRelaunch(void){}
int main(void){
    PcBattle b;PortTourney t,before;int match,n=atoi(getenv("BT3_TOURNAMENT"));unsigned total=0;
    assert(Port_CoopConfig(&b));
    assert(Port_TourneyCopy(&t) && t.count==n);
    {int i,j;for(i=0;i<n;i++){int found=0;for(j=0;j<n;j++)found+=t.fighters[j].chara==i*2;assert(found==1);}}
    for(match=0;match<n-1;match++){
        int a,bseed,k;
        assert(Port_CoopConfig(&b) && b.mode==PC_DUEL && b.players==2);
        assert(Port_TourneyCopy(&t) && Port_TourneyPair(&t,t.current,&a,&bseed));
        assert(b.fighter[0][0].chara==t.fighters[a].chara && b.fighter[1][0].chara==t.fighters[bseed].chara);
        assert(Port_TourneySpectating()==(Port_CoopMe()!=a && Port_CoopMe()!=bseed));
        for(k=0;k<64;k++,total++){
            unsigned char p[18];int side;
            frame=total;Port_CoopBeginTick(total);
            for(side=0;side<2;side++){
                int source=side?bseed:a;Port_CoopInput(side,p);
                /* Inputs sampled three frames before a role change can belong
                   to a previous spectator, and must remain neutral. */
                if(total<3 || k<3) continue;
                assert(p[0]==source+10 && p[1]==(unsigned char)(total-3));
            }
            Port_CoopInput(2,p);assert(p[0]==255);
        }
        before=t;
        Port_TourneyResult(match%2?2:1);
        Port_TourneyResult(2); /* duplicate finish notification must be ignored */
        assert(Port_TourneyCopy(&t));assert(t.winners[before.current]==(match%2?bseed:a));
    }
    assert(t.champion>=0 && t.current==-1);
    assert(!Port_CoopConfig(&b));
    Port_TourneyLeave();assert(!Port_CoopEnabled() && !Port_TourneyEnabled());
    printf("tournament_peer_test: %d peers, %d real input turns, champion %d, spectators isolated\n",n,n-1,t.champion);
}

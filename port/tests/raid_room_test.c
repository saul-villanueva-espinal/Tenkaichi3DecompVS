#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "port_pc_battle.h"
typedef int s32;
typedef unsigned u32;
/* BUTTONS */
static struct { int stage; } stage;
static struct { void *unused; } placeholder;
static struct { struct { int stage; } *stage; } team;
#define gTeamSel (&team)
static char keys[64][48];
static int values[64], used, writes, fades, missing;
int Port_Setting(const char *key,int def) {
    int i;for(i=0;i<used;i++) if(!strcmp(key,keys[i])) return values[i];return def;
}
void Port_SettingSave(const char *key,int v) {
    int i;for(i=0;i<used;i++) if(!strcmp(key,keys[i])) {values[i]=v;return;}
    assert(used<64);snprintf(keys[used],sizeof(keys[used]),"%s",key);values[used++]=v;
}
void Port_SettingsWrite(void) {writes++;}
int Port_CharaCount(void) {return 8;}
const char *Port_FighterName(int c) {(void)c;return "fixture";}
int Port_ExtraCostumeOk(int c,int costume,char *why,int size) {
    (void)c;if(missing || costume>1) {snprintf(why,size,"missing fixture");return 0;}return 1;
}
int Port_NetActive(void) {return 0;}
int Port_NetSession(void) {return 0;}
int Port_ModesFight(void) {return 0;}
int ChrTbl_GetCost(int c) {(void)c;return 2;}
void Ps_Copy(char *out,const char *src,int max) {snprintf(out,max,"%s",src);}
void Ps_Cat(char *out,const char *src,int max) {strncat(out,src,max-strlen(out)-1);}
void TeamSel_RequestStage(void) {}
void ColorFade_StartOut(int a,int b,int c,int d) {(void)a;(void)b;(void)c;(void)d;fades++;}
/* ACTUAL LOBBY */
int main(void) {
    (void)placeholder;team.stage=(void *)&stage;
    Rd_Init();assert(R->count==6 && Rd_Boss()->chara==133);
    assert(Rd_Check() && R->battle.players==6 && R->battle.count[0]==6);
    R->cursor=4;Rd_Input(0,PADG_LEFT);assert(R->count==5 && Rd_Check());
    R->count=6;R->connection=0;assert(Rd_Check() && R->battle.players==1);
    R->cursor=7;Rd_Input(PADG_SQUARE,PADG_SQUARE);assert(R->page==RD_ALLY);
    Rd_Input(0,PADG_UP);assert(R->ally==5);
    R->connection=1;R->ally=0;Rd_Input(0,PADG_DOWN);assert(R->ally==0);
    Rd_Input(PADG_CROSS,0);assert(R->page==RD_LOBBY);
    /* Added IDs, skipped reserved IDs and full wrap remain selectable. */
    Rd_Boss()->chara=160;Rd_Character(Rd_Boss(),1);assert(Rd_Boss()->chara==198);
    Rd_Character(Rd_Boss(),-1);assert(Rd_Boss()->chara==160);
    Rd_Boss()->chara=205;Rd_Character(Rd_Boss(),1);assert(Rd_Boss()->chara==0);
    /* Profiles of two independent costume mods must not overwrite each other. */
    Rd_Boss()->chara=92;Rd_Boss()->costume=0;Rd_ProfileLoad();R->battle.rules.bossHp=180000;Rd_ProfileSave();
    Rd_Costume(Rd_Boss(),1);assert(Rd_Boss()->costume==1 && R->battle.rules.bossHp==120000);
    R->battle.rules.bossHp=90000;R->battle.rules.enragePct=0;Rd_ProfileSave();
    Rd_Costume(Rd_Boss(),1);assert(Rd_Boss()->costume==0 && R->battle.rules.bossHp==180000);
    Rd_Costume(Rd_Boss(),-1);assert(Rd_Boss()->costume==1 && R->battle.rules.bossHp==90000 && !R->battle.rules.enragePct);
    assert(writes==2);
    R->page=RD_CUSTOM;R->cursor=2;R->battle.rules.bossHp=250000;Rd_Input(0,PADG_RIGHT);assert(R->battle.rules.bossHp==250000);
    Rd_Input(PADG_CIRCLE,0);assert(R->page==RD_LOBBY && !R->leaving);
    missing=1;Rd_Input(PADG_START,0);assert(!R->launch && !fades && strstr(R->message,"missing"));
    missing=0;Rd_Input(PADG_START,0);assert(R->launch && R->leaving && fades==1);
    puts("raid_room_test: native controls, six allies, costume profiles, validation and launch pass");
    return 0;
}

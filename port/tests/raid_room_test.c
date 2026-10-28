#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "port_pc_battle.h"
typedef int s32;
typedef unsigned u32;
/* No external maps in this lobby fixture. */
int Port_MapId(int i) {i=(i%35+35)%35;return i;}
int Port_MapIndex(int id) {return id>=0&&id<35?id:0;}
int Port_MapStep(int id,int step) {return Port_MapId(Port_MapIndex(id)+step);}
int Port_MapValid(int id) {return id>=0&&id<36;}
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
static char savedName[32]="Jugador",clipboard[256];
static int typing,textKeys[128],textAt,textCount,clipboardDue,fpState=1;
void PortTbo_TextMode(int on) {typing=on;}
int PortTbo_TextRead(void) {return textAt<textCount?textKeys[textAt++]:0;}
void PortTbo_Copy(const char *text) {snprintf(clipboard,sizeof(clipboard),"%s",text);}
void PortRaid_ClipboardRequest(void) {clipboardDue=1;}
int PortRaid_ClipboardRead(char *out,int size) {if(!clipboardDue)return 0;clipboardDue=0;snprintf(out,size,"%s",clipboard);return 1;}
void Port_SettingText(const char *key,const char *def,char *out,int size) {(void)key;(void)def;snprintf(out,size,"%s",savedName);}
void Port_SettingTextSave(const char *key,const char *text) {(void)key;snprintf(savedName,sizeof(savedName),"%s",text);}
void Port_VerifyStart(void) {}
void Port_RaidModelsTurn(float delta) {(void)delta;}
void Port_RaidRosterDetails(char *name,int size,char *form,int formSize,int chara) {
    snprintf(name,size,"Personaje %d",chara);snprintf(form,formSize,"Forma %d",chara);
}
int Port_DataFingerprint(unsigned long long *out) {if(out) *out=12345;return fpState;}
unsigned long long Port_CharacterFingerprint(void) {return 67890;}
/* ACTUAL LOBBY */
int main(void) {
    int i;const char *nick="AMIGO_2";
    (void)placeholder;team.stage=(void *)&stage;
    Rd_Init();assert(R->count==6 && R->lobbySock==-1);
    assert(!strcmp(R->playerName,"Jugador"));
    R->page=RD_CONNECT;R->cursor=0;Rd_Input(PADG_CROSS,0);
    assert(R->editing==1 && typing);
    for(i=0;i<19;i++)textKeys[textCount++]=8;
    for(i=0;nick[i];i++)textKeys[textCount++]=nick[i];
    textKeys[textCount++]=13;Rd_Input(0,0);
    assert(!R->editing && !typing && !strcmp(savedName,nick));
    Rd_Init();assert(!strcmp(R->playerName,nick));
    /* Cancel restores the saved nickname; reject blank names. */
    Rd_EditBegin(1);R->editText[0]=0;Rd_EditEnd(1);assert(R->editing==1);
    Rd_EditEnd(0);assert(!strcmp(R->playerName,nick));
    /* Editing consumes START instead of accidentally starting a fight. */
    Rd_EditBegin(1);Rd_Input(PADG_START,PADG_START);assert(!R->launch);Rd_EditEnd(0);
    /* Paste crosses the renderer mailbox and is consumed next frame. */
    R->page=RD_JOIN;snprintf(clipboard,sizeof(clipboard),"BT3-k7m9q2");
    Rd_Input(PADG_TRIANGLE,0);Rd_ClipboardTick();assert(!strcmp(R->roomCode,"BT3-K7M9Q2"));
    /* Requesting a connection never invokes a blocking Open in this frame. */
    R->page=RD_CONNECT;R->cursor=1;fpState=0;
    Rd_Input(PADG_CROSS,0);assert(R->connecting==1 && R->lobbySock==-1 && !R->launch);
    Rd_ConnectTick();assert(R->connecting==1);
    Rd_Input(PADG_CIRCLE,0);assert(!R->connecting && !R->leaving);fpState=1;
    /* START cannot bypass the minimum party size or either ready state. */
    R->page=RD_LOBBY;R->connection=1;R->players[0].state=4;
    Rd_Input(PADG_START,0);assert(!R->launch);
    R->players[1].connected=1;R->players[1].state=3;
    Rd_Input(PADG_START,0);assert(!R->launch);
    {
        LobbySyncPacket sync;R->battle.fighter[1][2].chara=95;
        R->battle.fighter[1][2].costume=3;R->battle.rules.bossHp=180000;
        Rd_FillSync(&sync,1);assert(sync.launching==1 && sync.bossChara[2]==95 &&
            sync.bossCostume[2]==3 && sync.bossHp==180000);
        R->battle.fighter[1][2].costume=0;
    }
    /* Preserve practice/profile/roster behavior. */
    R->connection=0;R->page=RD_LOBBY;assert(Rd_Check() && R->battle.players==1);
    R->cursor=2;Rd_Input(PADG_CROSS,0);assert(R->page==RD_ALLY);
    assert(R->gridCount==Port_PcRosterCount());
    R->gridCursor=0;Rd_GridChoose();
    Rd_Input(0,PADG_RIGHT);assert(R->gridCursor==1 && Rd_Shown()->chara==1);
    Rd_Input(0,PADG_DOWN);assert(R->gridCursor==5 && Rd_Shown()->chara==5);
    Rd_Input(PADG_R2,0);assert(R->gridCursor==29);
    Rd_Input(PADG_SQUARE,0);assert(R->gridFilter==1 && R->gridCount==PORT_DISC_CHARAS);
    Rd_Input(PADG_SQUARE,0);assert(R->gridFilter==2 && R->gridCount==8);
    snprintf(R->search,sizeof(R->search),"personaje 205");Rd_GridBuild();assert(R->gridCount==1);
    R->search[0]=0;R->gridFilter=0;Rd_GridBuild();
    Rd_Boss()->chara=160;Rd_Character(Rd_Boss(),1);assert(Rd_Boss()->chara==198);
    Rd_Character(Rd_Boss(),-1);assert(Rd_Boss()->chara==160);
    Rd_Boss()->chara=92;Rd_Boss()->costume=0;Rd_ProfileLoad();R->battle.rules.bossHp=180000;Rd_ProfileSave();
    Rd_Costume(Rd_Boss(),1);assert(Rd_Boss()->costume==1 && R->battle.rules.bossHp==120000);
    R->page=RD_LOBBY;R->cursor=3;Rd_Input(PADG_CROSS,0);assert(R->launch && fades==1);
    puts("raid_room_test: persistent nickname, keyboard editing/cancel, clipboard mailbox, nonblocking request/cancel, practice and costume profiles PASS");
    return 0;
}

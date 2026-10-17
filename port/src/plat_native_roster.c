#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/port_pc_battle.h"
static unsigned sPointer;
extern const char *Port_FighterName(int);
/* Host strings can live above 4 GB. Copy into the game's supplied buffer. */
void Port_RosterName(char *out,int size,int chara) {if(size>0)snprintf(out,(size_t)size,"%s",Port_FighterName(chara));}
extern void Port_SettingText(const char *,const char *,char *,int);
extern void Port_SettingTextSave(const char *,const char *);
extern void Port_SettingsWrite(void);
void Port_RosterRemember(const PcBattle *b) {
    char key[40],text[900];size_t used=0;unsigned i;const int *v=(const int *)b;
    snprintf(key,sizeof(key),"native_roster_%d",b->mode);
    for(i=0;i<sizeof(*b)/sizeof(int);i++) used+=(size_t)snprintf(text+used,sizeof(text)-used,"%s%d",i?",":"",v[i]);
    Port_SettingTextSave(key,text);Port_SettingsWrite();
}
int Port_RosterRecall(int mode,PcBattle *b) {
    char key[40],text[900];
    snprintf(key,sizeof(key),"native_roster_%d",mode);Port_SettingText(key,"",text,sizeof(text));
    return Port_PcBattleParse(b,text) && b->mode==mode;
}
int Port_RosterFavorite(int chara,int toggle) {
    char text[PORT_TOTAL_CHARAS*12],out[PORT_TOTAL_CHARAS*12];int ids[PORT_TOTAL_CHARAS],n=0,i,found=0;char *p;size_t used=0;
    Port_SettingText("native_favorites","",text,sizeof(text));p=text;
    while(*p && n<PORT_TOTAL_CHARAS) {char *end;long v=strtol(p,&end,10);if(end==p) break;ids[n++]=(int)v;found|=v==chara;p=*end?end+1:end;}
    if(!toggle) return found;
    for(i=0;i<n;i++) if(ids[i]!=chara) used+=(size_t)snprintf(out+used,sizeof(out)-used,"%s%d",used?",":"",ids[i]);
    if(!found) used+=(size_t)snprintf(out+used,sizeof(out)-used,"%s%d",used?",":"",chara);
    out[used]=0;Port_SettingTextSave("native_favorites",out);Port_SettingsWrite();return !found;
}
int Port_RosterTestMode(void) { const char *s=getenv("BT3_NATIVE_ROSTER");return s?atoi(s):0; }
unsigned Port_RosterTestInput(unsigned age) {
    const char *p=getenv("BT3_ROSTER_SCRIPT");
    while(p&&*p) {char *end;unsigned long at=strtoul(p,&end,10),key;if(end==p||*end!=':')break;p=end+1;key=strtoul(p,&end,16);if(end==p)break;if(at==age)return (unsigned)key;p=*end?end+1:end;}
    return 0;
}
void Port_RosterPointerSet(int x,int y) { if(gPortNativeRosterActive && x>=0&&x<512&&y>=0&&y<448) __atomic_store_n(&sPointer,0x80000000u|((unsigned)x<<16)|(unsigned)y,__ATOMIC_RELEASE); }
int Port_RosterPointerRead(int *x,int *y) {unsigned p=__atomic_exchange_n(&sPointer,0,__ATOMIC_ACQ_REL);*x=(p>>16)&0x7fff;*y=p&0xffff;return (p>>31)!=0;}

void Port_RosterLog(int event,int a,int b,int c,int d) {
    if(event==0) fprintf(stderr,"bt3: roster nativo: tipo %d modo %d instalados %d casillas %d\n",a,b,c,d);
    else if(event==1) fprintf(stderr,"bt3: roster nativo: equipo %d posicion %d personaje %d traje %d\n",a+1,b+1,c,d);
    else fprintf(stderr,"bt3: roster nativo: confirmado tipo %d equipos %d vs %d escenario %d\n",a,b,c,d);
}

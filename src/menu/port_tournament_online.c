#include "common.h"
#ifdef PORT
#include "menu/port_screen.h"
#include "menu/tbo_art.h"
#include "sys/pad.h"
#include "port_tournament.h"
extern void Gfx_BeginFrame(void),Gfx_EndFrame(s32),Dma_Flush(void),Dma_ResetBuffers(void),Snd_Update(void);
extern void Port_RosterName(char *,s32,s32);
extern s32 Port_CoopMe(void),Port_TourneyTestBots(void);
extern s32 Snd_PlaySe(u32,s32);

static void Tn_Background(char *title) {
    Ps_PageSet();
    Ps_Rect(0,0,512,448,Ps_Rgba(9,23,48,255),Ps_Rgba(4,8,20,255));
    Ps_Rect(0,0,512,8,Ps_Rgba(255,200,20,255),Ps_Rgba(255,110,0,255));
    Ps_Heading(16,20,title,.83f,480,0x80);
    Ps_Rect(12,51,500,53,Ps_Rgba(255,200,20,255),Ps_Rgba(255,110,0,255));
}
static void Tn_Fighter(char *out,s32 seed,const PortTourney *t) {
    char name[96];
    if(seed<0) {Ps_Copy(out,"Por decidir",96);return;}
    Port_RosterName(name,sizeof(name),t->fighters[seed].chara);
    sprintf(out,"%d. ",seed+1);Ps_Cat(out,name,96);
}
static void Tn_Bracket(const PortTourney *t) {
    s32 rounds=t->count==16?4:t->count==8?3:2;
    s32 width=t->count/2,base=0,round;
    f32 cw=484.0f/rounds;
    for(round=0;width;round++,base+=width,width/=2) {
        s32 k;
        f32 step=286.0f/width;
        char heading[40];
        Ps_Copy(heading,width==1?"FINAL":width==2?"SEMIFINALES":width==4?"CUARTOS":"OCTAVOS",40);
        Ps_Text(15+round*cw,83,heading,.35f,cw-8,1);
        for(k=0;k<width;k++) {
            s32 index=base+k,a=-1,b=-1;
            f32 x=14+round*cw,y=102+(k+.5f)*step-14;
            char line[96];
            Port_TourneyPair(t,index,&a,&b);
            Ps_Pill(index==t->current?TBO_KIT_SEL:TBO_KIT_BAR,x,y,x+cw-10,y+29,0x80);
            Tn_Fighter(line,a,t);Ps_Text(x+4,y+2,line,.31f,cw-18,t->winners[index]==a&&a>=0?5:0);
            Tn_Fighter(line,b,t);Ps_Text(x+4,y+15,line,.31f,cw-18,t->winners[index]==b&&b>=0?5:0);
            if(round+1<rounds) {
                f32 nx=x+cw-5,ny=102+((k/2)+.5f)*(step*2);
                Ps_Rect(x+cw-10,y+14,nx,y+15,Ps_Rgba(240,175,30,255),Ps_Rgba(240,175,30,255));
                Ps_Rect(nx,y+14<ny?y+14:ny,nx+1,y+14<ny?ny:y+14,Ps_Rgba(240,175,30,255),Ps_Rgba(240,175,30,255));
            }
        }
    }
}
/* Every participant sees the same bracket between actual fights. The final
   page becomes local after the completed network session has been closed. */
s32 PortTournamentOnline_Card(void) {
    PortTourney t;s32 age=0,done,test=Port_TourneyTestBots();char info[128];
    if(!Port_TourneyCopy(&t)) return 0;
    done=t.champion>=0;
    if(done) Port_TourneyLeave();
    for(;;) {
        Gfx_BeginFrame();Pad_Update();Snd_Update();
        Tn_Background(done?"TORNEO COMPLETADO":"TORNEO ONLINE | CUADRO EN VIVO");
        if(done) {char hero[96];Tn_Fighter(hero,t.champion,&t);Ps_Copy(info,"CAMPEON: ",128);Ps_Cat(info,hero,128);}
        else {s32 a,b;Port_TourneyPair(&t,t.current,&a,&b);sprintf(info,"Combate %d/%d | Puestos %d vs %d | Tu puesto: %d",t.current+1,t.count-1,a+1,b+1,Port_CoopMe()+1);}
        Ps_Text(16,61,info,.41f,480,done?5:1);
        Tn_Bracket(&t);
        Ps_Text(16,407,done?"X / Circulo: volver al menu principal":"Dos luchan; los demas miran. Empates: se repite el combate.",.37f,480,0);
        if(!done) {sprintf(info,"El combate comienza en %d...",(180-age+59)/60);Ps_Text(16,426,info,.4f,480,1);}
        Gfx_EndFrame(1);Dma_Flush();age++;
        if((!done && age>=180) || (done && test && age>=180) || (done && age>15 && (gPad[0].gamePressed&(PADG_CROSS|PADG_CIRCLE)))) break;
    }
    Dma_ResetBuffers();return done;
}

s32 PortTournamentOnline_Run(void) {
    static s32 host=1,count=8,ip[5]={127,0,0,1,29740};
    static PcFighter hero={3,0};
    s32 cur=0,field=0,age=0,result=0;
    char message[120]="";
    for(;;) {
        u32 p;char text[128],name[96];s32 i;
        Gfx_BeginFrame();Pad_Update();Snd_Update();p=gPad[0].gamePressed;
        {extern s32 Port_RosterTestMode(void);extern u32 Port_RosterTestInput(u32);if(Port_RosterTestMode()==13)p|=Port_RosterTestInput(age+1);}
        if(age>15) {
            if(p&PADG_UP) cur=Ps_Wrap(cur-1,0,4);
            if(p&PADG_DOWN) cur=Ps_Wrap(cur+1,0,4);
            if(p&PADG_CIRCLE) {Gfx_EndFrame(1);Dma_Flush();break;}
            if(cur==0 && (p&(PADG_LEFT|PADG_RIGHT))) host=!host;
            if(cur==1 && (p&(PADG_LEFT|PADG_RIGHT))) count=count==4?8:count==8?16:4;
            if(cur==3) {
                if(p&PADG_L1) field=Ps_Wrap(field-1,0,4);
                if(p&PADG_R1) field=Ps_Wrap(field+1,0,4);
                if(p&PADG_LEFT) ip[field]--;
                if(p&PADG_RIGHT) ip[field]++;
                ip[field]=Ps_Wrap(ip[field],field==4?1024:0,field==4?65535:255);
            }
            if(p&(PADG_CROSS|PADG_START)) {
                if(cur==2) {
                    PcBattle b;Port_PcBattleDefault(&b);b.mode=PC_DUEL;b.players=1;b.fighter[0][0]=hero;
                    if(Port_NativeRosterPick(&b,3,0)) hero=b.fighter[0][0];
                } else if(cur==4) {
                    char address[80];sprintf(address,"%d.%d.%d.%d",ip[0],ip[1],ip[2],ip[3]);
                    if(Port_TourneyLaunch(host,address,ip[4],count,&hero)) {result=1;Gfx_EndFrame(1);Dma_Flush();break;}
                    Ps_Copy(message,"No se pudo preparar la sala. Revisa personaje y direccion.",120);
                }
            }
        }
        Tn_Background("SALA DE TORNEO | UDP DIRECTO");
        Ps_Text(16,66,"Todos permanecen conectados y ven cada combate hasta la final.",.39f,480,0);
        for(i=0;i<5;i++) {
            f32 y=104+i*49;
            Ps_Pill(cur==i?TBO_KIT_SEL:TBO_KIT_BAR,16,y,496,y+39,0x80);
            if(i==0) Ps_Copy(text,host?"CREAR SALA (anfitrion)":"UNIRSE A SALA",128);
            if(i==1) sprintf(text,"Participantes: %d (el anfitrion define el cuadro)",count);
            if(i==2) {Port_RosterName(name,sizeof(name),hero.chara);Ps_Copy(text,"Personaje: ",128);Ps_Cat(text,name,128);Ps_Cat(text," | X: elegir",128);}
            if(i==3) sprintf(text,"IP: %d.%d.%d.%d  Puerto: %d | Campo %d",ip[0],ip[1],ip[2],ip[3],ip[4],field+1);
            if(i==4) Ps_Copy(text,host?"X: CREAR TORNEO":"X: UNIRSE AL TORNEO",128);
            Ps_Text(27,y+10,text,.48f,454,cur==i?1:0);
        }
        Ps_Text(16,366,"Izq/Der: cambiar | L1/R1: campo IP | Circulo: volver",.38f,480,3);
        Ps_Text(16,390,"Internet: IP del anfitrion y puerto UDP accesible. LAN: IP local.",.35f,480,0);
        Ps_Text(16,418,message,.37f,480,5);
        Gfx_EndFrame(1);Dma_Flush();age++;
    }
    Dma_ResetBuffers();return result;
}
#endif

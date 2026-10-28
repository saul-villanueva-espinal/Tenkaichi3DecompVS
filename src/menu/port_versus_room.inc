/* Oracle versus lobby. Relay service stays opaque to game packets. Each peer
   chooses its own fighter; only the host commits membership and starts play. */
#define VS_MAGIC 0x42545653
typedef struct VsMember {s32 connected,ready,chara,costume,team;char name[20];} VsMember;
typedef struct VsPacket {
    u32 magic,version,mode,revision;
    s32 kind,stage,launching;
    VsMember member[4];
} VsPacket;
static void Vs_ResetReady(void) {
    s32 i;sVsRevision++;sVsStarting=0;
    for(i=0;i<4;i++)if(R->players[i].connected)R->players[i].state=3;
    R->myState=3;
}
static void Vs_Packet(VsPacket *p,s32 kind) {
    s32 i;memset(p,0,sizeof(*p));p->magic=VS_MAGIC;p->version=1;
    p->mode=sVsMode;p->revision=sVsRevision;p->kind=kind;p->stage=R->battle.stage;p->launching=sVsStarting;
    for(i=0;i<4;i++) {
        p->member[i].connected=R->players[i].connected;p->member[i].ready=R->players[i].state;
        p->member[i].chara=R->players[i].chara;p->member[i].costume=R->players[i].costume;
        p->member[i].team=sVsTeam[i];memcpy(p->member[i].name,R->players[i].name,20);
    }
}
static s32 Vs_ValidMember(const VsMember *m) {
    char why[160];s32 i,found=0;
    if(m->team<0||m->team>1||m->ready<3||m->ready>5||m->costume<0||m->costume>7)return 0;
    for(i=0;i<Port_PcRosterCount();i++)if(Port_PcRosterId(i)==m->chara){found=1;break;}
    return found&&Port_ExtraCostumeOk(m->chara,m->costume,why,sizeof(why));
}
/* No synthetic fighters: pack precisely the connected participants. The map
   is peer -> side*8+member; host remains peer zero even on team two. */
static s32 Vs_Build(PcBattle *b,s32 *slots,s32 *members) {
    s32 i,n=0,side,k;Port_PcBattleDefault(b);b->mode=sVsMode;b->stage=R->battle.stage;
    memset(&b->rules,0,sizeof(b->rules));b->count[0]=b->count[1]=0;
    for(i=0;i<4;i++)if(R->players[i].connected) {
        side=sVsMode==PC_TAG?sVsTeam[i]:n%2;k=b->count[side]++;
        slots[n]=i;members[n]=side*8+k;b->fighter[side][k].chara=R->players[i].chara;
        b->fighter[side][k].costume=R->players[i].costume;n++;
    }
    b->players=n;return n;
}
static s32 Vs_CanStart(void) {
    s32 i,n=0,teams[2]={0,0};
    for(i=0;i<4;i++)if(R->players[i].connected) {
        n++;teams[sVsTeam[i]]++;
        if(R->players[i].state!=4)return 0;
    }
    return n>=2&&(sVsMode!=PC_TAG||(teams[0]&&teams[1]));
}
static void Vs_Send(void) {
    VsPacket p;s32 i;Vs_Packet(&p,R->connection==1?1:2);
    if(R->connection==1){for(i=1;i<4;i++)if(R->players[i].connected)RoomRelay_SendSlot(R->lobbySock,i,&p,sizeof(p));}
    else RoomRelay_SendSlot(R->lobbySock,0,&p,sizeof(p));
}
static void Vs_Poll(void) {
    u8 buf[2048];s32 len,sender,i;u64 now=RoomRelay_NowMs();
    if(R->lobbySock<0)return;
    for(;;) {
        VsPacket p;len=RoomRelay_RecvSlot(R->lobbySock,buf,&sender);
        if(len<0){Ps_Copy(R->message,"La sala se ha cerrado. Puedes crear otra o volver.",sizeof(R->message));Rd_LobbyClose();R->page=RD_CONNECT;sVsStarting=0;return;}
        if(!len)break;
        if(len!=sizeof(p))continue;memcpy(&p,buf,sizeof(p));
        if(p.magic!=VS_MAGIC||p.version!=1||p.mode!=(u32)sVsMode)continue;
        if(R->connection==1&&sender>0&&sender<4&&p.kind==2) {
            VsMember *m=&p.member[sender];RaidPlayerSlot *d=&R->players[sender];
            if(!Vs_ValidMember(m))continue;
            if(sVsStarting) {
                if(p.revision==sVsRevision&&m->ready==5)d->state=5;
            }else {
                s32 changed=!d->connected||d->chara!=m->chara||d->costume!=m->costume||sVsTeam[sender]!=m->team;
                if(changed)Vs_ResetReady();
                d->connected=1;d->chara=m->chara;d->costume=m->costume;sVsTeam[sender]=m->team;
                d->state=!changed&&p.revision==sVsRevision&&m->ready==4?4:3;
                memcpy(d->name,m->name,19);d->name[19]=0;
            }
            d->lastSeen=now;
        }else if(R->connection==2&&sender==0&&p.kind==1) {
            s32 valid=Port_MapValid(p.stage);
            for(i=0;i<4;i++)if(p.member[i].connected&&!Vs_ValidMember(&p.member[i]))valid=0;
            if(!valid||!p.member[0].connected||!p.member[R->mySlot].connected)continue;
            sVsLastHost=now;
            if(p.revision<sVsRevision)continue;
            if(p.revision!=sVsRevision){R->myState=3;R->players[R->mySlot].state=3;sVsRevision=p.revision;}
            R->battle.stage=p.stage;
            if(gTeamSel->stage->stage!=p.stage){gTeamSel->stage->stage=p.stage;TeamSel_RequestStage();}
            for(i=0;i<4;i++)if(i!=R->mySlot) {
                R->players[i].connected=p.member[i].connected;R->players[i].state=p.member[i].ready;
                R->players[i].chara=p.member[i].chara;R->players[i].costume=p.member[i].costume;
                sVsTeam[i]=p.member[i].team;memcpy(R->players[i].name,p.member[i].name,19);R->players[i].name[19]=0;
            }
            if(p.launching&&!sVsStarting&&R->myState==4&&p.member[R->mySlot].ready>=4&&
               p.member[R->mySlot].chara==R->players[R->mySlot].chara&&
               p.member[R->mySlot].costume==R->players[R->mySlot].costume&&p.member[R->mySlot].team==sVsTeam[R->mySlot]) {
                sVsStarting=1;sVsStartAt=now;R->players[R->mySlot].state=R->myState=5;
                printf("bt3: versus: start acknowledged mode %d slot %d\n",sVsMode,R->mySlot);
            }
            if(p.launching==2&&sVsStarting==1){sVsStarting=2;sVsStartAt=now;}
            if(!p.launching&&sVsStarting){sVsStarting=0;R->players[R->mySlot].state=R->myState=3;}
        }
    }
    if(R->connection==1) {
        for(i=1;i<4;i++)if(R->players[i].connected&&now-R->players[i].lastSeen>8000) {
            R->players[i].connected=0;R->players[i].state=0;Vs_ResetReady();
            Ps_Copy(R->message,"Un jugador salio. Confirmen Listo de nuevo.",sizeof(R->message));
        }
    }else if(now-sVsLastHost>10000){Rd_LobbyClose();R->page=RD_CONNECT;sVsStarting=0;Ps_Copy(R->message,"Se perdio la conexion con el anfitrion.",sizeof(R->message));return;}
    if(now-R->lastStateSend>=100){R->lastStateSend=now;Vs_Send();}
    if(sVsStarting&&!R->leaving) {
        s32 ack=1;for(i=1;i<4;i++)if(R->players[i].connected&&R->players[i].state!=5)ack=0;
        if(R->connection==1&&sVsStarting==1&&ack){sVsStarting=2;sVsStartAt=now;Vs_Send();}
        if(sVsStarting==2&&now-sVsStartAt>=1200) {
            R->leaving=R->launch=1;ColorFade_StartOut(0,0,0,20);
        }else if(R->connection==1&&now-sVsStartAt>6000){Vs_ResetReady();Ps_Copy(R->message,"Un jugador no confirmo el inicio. Reintenta.",sizeof(R->message));}
    }
}
#include "port_versus_manga.inc"
static void Vs_Draw(void) {
    if(R->page==RD_ALLY||R->page==RD_LOBBY){Vm_Draw();return;}
    s32 i,selector=R->page==RD_ALLY;char text[160];
    extern void Gfx_AddDefaultEnv(void);Gfx_AddDefaultEnv();Ps_PageFull();Ps_SetShift(0);
    Ps_Sprite(TBO_RAID_ENERGY,0,0,512,448,0,0,1,1,PS_PLAIN,128);
    if(selector) {
        PcFighter *f=Rd_Shown();for(i=0;i<PORT_RAID_BOSSES;i++)Port_RaidModelsSet(i,i? -1:f->chara,i?0:f->costume);
        Rd_SelectorPlatform();Port_RaidModelsDrawSelector();Ps_PageFull();Rd_DrawSelector();
        Ps_Rect(143,0,512,37,Ps_Rgba(4,12,28,128),Ps_Rgba(4,12,28,128));
        Rd_Text(149,9,sVsMode==PC_TAG?"TAG TEAM - TU GUERRERO":"TODOS VS TODOS - TU GUERRERO",1.1f,353,1);
        Ps_Rect(350,208,503,333,Ps_Rgba(4,12,28,128),Ps_Rgba(4,12,28,128));
        Rd_Text(361,222,"TU PLAZA ONLINE",.65f,132,1);Rd_Text(361,252,R->playerName,.6f,132,0);
        sprintf(text,sVsMode==PC_TAG?"Equipo %d":"Individual",sVsTeam[R->mySlot]+1);Rd_Text(361,280,text,.6f,132,1);
        Ps_Rect(0,400,512,448,Ps_Rgba(3,9,21,128),Ps_Rgba(3,9,21,128));
        Rd_Text(14,409,"X: confirmar | O: volver | L1/R1: traje",.61f,475,0);
        Rd_Text(14,431,"L2/R2: pagina | Cuadrado: filtro | Triangulo: buscar",.5f,475,0);
    }else {
        Rd_Text(15,8,sVsMode==PC_TAG?"TAG TEAM - SALAS ORACLE":"TODOS VS TODOS - SALAS ORACLE",1.35f,484,1);
        if(R->page==RD_CONNECT||R->page==RD_JOIN) {
            Rd_Frame(55,65,402,294,0);
            Rd_Text(75,82,"CREA UNA SALA Y COMPARTE EL CODIGO",.85f,365,0);
            if(R->page==RD_CONNECT) {
                sprintf(text,"APODO: %.19s",R->editing==1?R->editText:R->playerName);
                Rd_Button(75,128,362,42,text,R->cursor==0);
                Rd_Button(75,183,362,42,"CREAR SALA",R->cursor==1);
                Rd_Button(75,238,362,42,"UNIRSE CON CODIGO",R->cursor==2);
                Rd_Text(78,308,"De 2 a 4 amigos. Sin completar plazas con CPU.",.57f,360,1);
            }else {
                sprintf(text,"CODIGO: %.20s",R->editing==2?R->editText:R->roomCode);
                Rd_Button(75,128,362,42,text,R->cursor==0);
                Rd_Button(75,183,362,42,"ENTRAR A LA SALA",R->cursor==1);
                Rd_Button(75,238,362,42,"VOLVER",R->cursor==2);
                Rd_Text(78,308,"Triangulo: pegar codigo | X: escribir / confirmar",.57f,360,1);
            }
        }else {
            sprintf(text,"CODIGO: BT3-%s",!strncmp(R->roomCode,"BT3-",4)?R->roomCode+4:R->roomCode);
            Rd_Button(14,49,484,32,text,R->cursor==0);
            for(i=0;i<4;i++) {
                f32 y=94+i*48;Rd_Frame(14,y,484,44,i==R->mySlot);
                if(!R->players[i].connected){Rd_Text(29,y+12,"PLAZA LIBRE - invita a un amigo",.64f,450,0);continue;}
                Port_TeamChip(R->players[i].chara,22,y+3,33,36,128);
                sprintf(text,"J%d %s%.19s",i+1,i==R->mySlot?"(TU) ":"",R->players[i].name);Rd_Text(63,y+3,text,.61f,225,0);
                Rd_Text(63,y+24,Rd_Name(R->players[i].chara),.5f,225,1);
                sprintf(text,sVsMode==PC_TAG?"EQUIPO %d":"INDIVIDUAL",sVsTeam[i]+1);Rd_Text(294,y+6,text,.56f,100,0);
                Rd_Text(402,y+13,R->players[i].state>=4?"LISTO":"ELIGIENDO",.57f,83,R->players[i].state>=4);
            }
            sprintf(text,sVsMode==PC_TAG?"TU EQUIPO: < %d >":"TODOS CONTRA TODOS - SIN EQUIPOS",sVsTeam[R->mySlot]+1);
            Rd_Button(14,291,238,29,text,R->cursor==1);
            sprintf(text,"ESCENARIO: %02d %s",R->battle.stage+1,R->connection==1?"< >":"(Host)");Rd_Button(259,291,239,29,text,R->cursor==2);
            Rd_Button(14,331,238,32,"ELEGIR PERSONAJE",R->cursor==3);
            Rd_Button(259,331,239,32,R->myState>=4?"LISTO - X: MODIFICAR":"CONFIRMAR LISTO",R->cursor==4);
            Rd_Button(14,374,484,32,sVsStarting?"INICIANDO COMBATE...":R->connection==1?"INICIAR COMBATE (ANFITRION)":"ESPERANDO AL ANFITRION",R->cursor==5);
        }
    }
    if(R->message[0]) {Ps_Rect(5,412,507,431,Ps_Rgba(3,9,21,128),Ps_Rgba(3,9,21,128));Rd_Text(12,415,R->message,.49f,488,1);}
    Rd_Text(12,435,R->editing?"Enter: guardar | Esc: cancelar":"X: confirmar | O: volver | Flechas: navegar / ajustar",.48f,488,0);
}
static void Vs_Pointer(s32 x,s32 y,u32 *p) {
    if(R->editing||R->connecting||sVsStarting)return;
    if(R->page==RD_ALLY||R->page==RD_LOBBY){Vm_Pointer(x,y,p);return;}
    if(R->page==RD_CONNECT||R->page==RD_JOIN) {
        if(x>=75&&x<=437) {
            if(y>=128&&y<170)R->cursor=0;
            else if(y>=183&&y<225)R->cursor=1;
            else if(y>=238&&y<280)R->cursor=2;else return;
            *p|=PADG_CROSS;
        }
    }else if(R->page==RD_LOBBY) {
        if(y>=49&&y<=81)R->cursor=0;
        else if(y>=291&&y<=320)R->cursor=x<255?1:2;
        else if(y>=331&&y<=363)R->cursor=x<255?3:4;
        else if(y>=374&&y<=406)R->cursor=5;else return;
        *p|=PADG_CROSS;
    }
}
static void Vs_Input(u32 p,u32 repeat) {
    s32 dir=(repeat&(PADG_RIGHT|PADG_RIGHT2))?1:(repeat&(PADG_LEFT|PADG_LEFT2))?-1:0;
    s32 up=repeat&(PADG_UP|PADG_UP2),down=repeat&(PADG_DOWN|PADG_DOWN2),me=R->mySlot;
    if(R->page==RD_ALLY&&!R->connecting){Vm_SelectorInput(p,repeat);return;}
    if(R->editing){Rd_EditInput(p);return;}
    if(R->connecting){if(p&PADG_CIRCLE){if(R->connecting==1)R->connecting=0;else RoomRelay_ConnectCancel();}return;}
    if(sVsStarting)return;
    if(p&PADG_CIRCLE) {
        if(R->page==RD_CONNECT){R->leaving=1;ColorFade_StartOut(0,0,0,20);}
        else if(R->page==RD_ALLY){R->page=RD_LOBBY;R->cursor=3;}
        else if(R->page==RD_JOIN){R->page=RD_CONNECT;R->cursor=2;}
        else{Rd_LobbyClose();R->page=RD_CONNECT;R->cursor=1;}return;
    }
    if(R->page==RD_CONNECT) {
        if(up||down)R->cursor=Rd_Wrap(R->cursor+(down?1:-1),3);
        if(p&PADG_CROSS){if(!R->cursor)Rd_EditBegin(1);else if(R->cursor==1)Rd_CreateRoom();else{R->page=RD_JOIN;R->cursor=0;PortRaid_ClipboardRequest();}}return;
    }
    if(R->page==RD_JOIN){Rd_Input(p,repeat);return;}
    if(R->page==RD_ALLY){Vm_SelectorInput(p,repeat);return;}
    if(up||down)R->cursor=Rd_Wrap(R->cursor+(down?1:-1),6);
    if(dir&&R->cursor==1&&sVsMode==PC_TAG){sVsTeam[me]^=1;R->myState=R->players[me].state=3;if(R->connection==1)Vs_ResetReady();}
    if(dir&&R->cursor==2&&R->connection==1){R->battle.stage=Port_MapStep(R->battle.stage,dir);gTeamSel->stage->stage=R->battle.stage;TeamSel_RequestStage();Vs_ResetReady();}
    if(p&PADG_SQUARE){R->cursor=3;p|=PADG_CROSS;}
    if(p&PADG_CROSS) {
        if(R->cursor==0){PortTbo_Copy(R->roomCode);Ps_Copy(R->message,"Codigo copiado. Enviaselo a tus amigos.",sizeof(R->message));}
        else if(R->cursor==1&&sVsMode==PC_TAG){sVsTeam[me]^=1;R->myState=R->players[me].state=3;if(R->connection==1)Vs_ResetReady();}
        else if(R->cursor==3){R->ally=me;R->battle.fighter[0][me].chara=R->players[me].chara;R->battle.fighter[0][me].costume=R->players[me].costume;R->page=RD_ALLY;R->myState=R->players[me].state=3;Vm_List(0);}
        else if(R->cursor==4){R->myState=R->players[me].state=R->myState==4?3:4;Vs_Send();}
        else if(R->cursor==5&&R->connection==1) {
            if(!Vs_CanStart()){Ps_Copy(R->message,sVsMode==PC_TAG?"Minimo 2, ambos equipos y todos Listo.":"Minimo 2 jugadores y todos Listo.",sizeof(R->message));return;}
            sVsStarting=1;sVsStartAt=RoomRelay_NowMs();R->players[0].state=R->myState=5;Vs_Send();
            printf("bt3: versus: host starting mode %d\n",sVsMode);
        }
    }
}

#include "port_manga_widgets.inc"
extern s32 Port_CharaFormTarget(s32,s32);
extern void Port_MangaModelsDraw(void);
static s32 sVmFilter;
static s32 Vm_Saga(s32 id) {
    if(id>=PORT_CHARA_FIRST)return 4;
    if((id>=7&&id<=10)||id==38||id==54||(id>=63&&id<=65)||(id>=140&&id<=157))return 3;
    return 1;
}
static void Vm_List(s32 filter) {
    s32 i,id,selected=Rd_Shown()->chara;sVmFilter=Rd_Wrap(filter,5);R->gridCount=R->gridCursor=0;
    for(i=0;i<Port_PcRosterCount();i++) {
        char name[96],form[96];id=Port_PcRosterId(i);
        if(sVmFilter&&Vm_Saga(id)!=sVmFilter&&!(sVmFilter==2&&id>=PORT_CHARA_FIRST))continue;
        Port_RaidRosterDetails(name,96,form,96,id);
        if(R->search[0]&&!Rd_Contains(name,R->search)&&!Rd_Contains(form,R->search))continue;
        if(id==selected)R->gridCursor=R->gridCount;R->grid[R->gridCount++]=id;
    }
}
static s32 Vm_Forms(s32 *forms) {
    s32 k,j,n=1,id=Rd_Shown()->chara,target;forms[0]=id;
    for(k=0;k<4;k++){target=Port_CharaFormTarget(id,k);if(target<0||target==255)continue;for(j=0;j<n;j++)if(forms[j]==target)break;if(j==n&&n<8)forms[n++]=target;}
    return n;
}
static void Vm_Form(s32 dir) {
    s32 forms[8],n=Vm_Forms(forms);if(n>1){Rd_Shown()->chara=forms[dir<0?n-1:1];Rd_Shown()->costume=0;Vm_List(sVmFilter);}
}
static void Vm_ResetChoice(void){s32 me=R->mySlot;R->players[me].chara=Rd_Shown()->chara;R->players[me].costume=Rd_Shown()->costume;R->myState=R->players[me].state=3;}
static void Vm_Selector(void) {
    static const char tabs[5][8]={"TODOS","Z","SUPER","GT","MODS"};
    s32 i,page=R->gridCursor/30,n,forms[8],me=R->mySlot;char name[96],form[96],text[96];
    PcFighter *f=Rd_Shown();n=Vm_Forms(forms);Port_RaidRosterDetails(name,96,form,96,f->chara);
    Ps_Sprite(TBO_MANGA_ONLINE_SELECT,0,0,512,448,0,0,1,1,PS_PLAIN,128);
    if(sVsMode==PC_FFA){Mg_Ink(420,6,650,69);Mg_Text(434,17,"TODOS VS TODOS - ELIGE TU GUERRERO",31,620,1);}
    for(i=0;i<5;i++){Mg_Paper(39+i*111,124,102,39);if(i==sVmFilter)Mg_Rect(39+i*111,124,102,39,Ps_Rgba(193,8,20,128));Mg_Text(51+i*111,132,tabs[i],24,84,i==sVmFilter);}
    for(i=0;i<30;i++) {
        s32 idx=page*30+i;f32 x=39+(i%5)*112,y=177+(i/5)*105;
        if(idx<R->gridCount)Mg_Portrait(R->grid[idx],x,y,103,97);
        Mg_Outline(x,y,103,97,idx==R->gridCursor);
    }
    Mg_Paper(267,817,94,31);sprintf(text,"%d / %d",page+1,(R->gridCount+29)/30);Mg_Text(274,821,text,24,88,0);
    Port_RaidModelsSet(0,f->chara,f->costume);Port_MangaModelsDraw();Ps_PageFull();
    Mg_Text(731,599,name,42,340,0);Mg_Text(725,650,form,25,350,0);
    for(i=0;i<n&&i<4;i++){Mg_Portrait(forms[i],647+i*114,719,105,65);Mg_Outline(647+i*114,719,105,65,forms[i]==f->chara);}
    Mg_Ink(628,803,120,28);sprintf(text,"TRAJE %d / 8",f->costume+1);Mg_Text(640,807,text,20,111,1);
    for(i=0;i<7;i++){Mg_Portrait(f->chara,798+i*44,805,36,43);Mg_Outline(798+i*44,805,36,43,i==f->costume);}
    Mg_Portrait(f->chara,1186,348,105,107);Mg_Ink(1307,364,310,40);Mg_Text(1320,375,R->playerName,27,280,1);
    Mg_Rect(1307,417,235,35,Ps_Rgba(sVsTeam[me]?174:12,36,sVsTeam[me]?36:176,128));
    sprintf(text,sVsMode==PC_TAG?"EQUIPO %d":"INDIVIDUAL",sVsTeam[me]+1);Mg_Text(1325,422,text,25,205,1);
    Mg_Ink(1183,478,435,31);Mg_Text(1201,483,"ELIGIENDO",24,400,1);
    for(i=0;i<4;i++) {
        f32 x=1188+i*109;Mg_Ink(x,521,100,100);
        if(R->players[i].connected){Mg_Portrait(R->players[i].chara,x,521,99,88);Mg_Rect(x+72,588,26,23,Ps_Rgba(R->players[i].state>=4?40:230,R->players[i].state>=4?208:160,60,128));Mg_Text(x+75,590,R->players[i].state>=4?"OK":"...",16,23,0);}
        else Mg_Text(x+38,546,"+",42,40,1);
    }
    Mg_Outline(1182,666,448,76,1);
    if(R->editing){Mg_Paper(35,113,553,54);Mg_Text(44,126,R->editText,26,530,0);}
    /* The artwork owns the single footer. Transient feedback stays above it. */
    if(R->message[0]&&R->editing){Mg_Ink(30,858,1610,25);Mg_Text(40,862,R->message,17,1580,1);}
}
static void Vm_Card(s32 slot,f32 x,f32 y,f32 w,f32 h) {
    char name[96],form[96],text[96];RaidPlayerSlot *p=slot>=0?&R->players[slot]:NULL;
    Mg_Ink(x,y,w,h);
    if(!p){Mg_Text(x+w*.40f,y+h*.35f,"+",68,w*.4f,1);Mg_Text(x+14,y+h*.58f,"PLAZA LIBRE",29,w-28,1);Mg_Text(x+14,y+h*.72f,"INVITA A UN AMIGO",19,w-28,1);return;}
    Mg_Portrait(p->chara,x+3,y+3,w-6,h*.64f);
    Port_RaidRosterDetails(name,96,form,96,p->chara);Mg_Text(x+12,y+h*.65f,name,31,w-24,1);
    sprintf(text,"TRAJE %d",p->costume+1);Mg_Text(x+12,y+h*.74f,text,19,w-24,1);
    Mg_Text(x+12,y+h*.81f,p->name,20,w-24,1);
    Mg_Rect(x+8,y+h*.90f,w-16,h*.09f,Ps_Rgba(p->state>=4?22:192,p->state>=4?156:112,32,128));Mg_Text(x+16,y+h*.90f,p->state>=4?"LISTO":"ELIGIENDO",23,w-32,1);
    Mg_Outline(x,y,w,h,slot==R->mySlot);
}
static void Vm_Lobby(void) {
    s32 i,s,indices[4],n,total=0;char text[120];
    Ps_Sprite(TBO_MANGA_LOBBY,0,0,512,448,0,0,1,1,PS_PLAIN,128);
    for(i=0;i<4;i++)total+=R->players[i].connected;
    Mg_Ink(1047,57,126,29);Mg_Text(1052,61,R->roomCode,20,116,1);
    Mg_Ink(1458,51,169,37);sprintf(text,"%d / 4 JUGADORES",total);Mg_Text(1464,61,text,21,153,1);
    if(sVsMode==PC_FFA) {
        Mg_Ink(327,20,590,77);Mg_Text(342,39,"TODOS VS TODOS - SALA",36,560,1);
        Mg_Rect(68,139,611,59,Ps_Rgba(23,94,171,128));Mg_Text(200,149,"CADA GUERRERO POR SU CUENTA",27,465,1);
        Mg_Rect(1019,139,570,59,Ps_Rgba(175,14,28,128));Mg_Text(1110,149,"TODOS SON RIVALES",27,460,1);
        for(i=0;i<4;i++)Vm_Card(R->players[i].connected?i:-1,(f32[]){76,411,1041,1339}[i],225,i<2?264:258,382);
    }else for(s=0;s<2;s++) {
        n=0;for(i=0;i<4;i++)if(R->players[i].connected&&sVsTeam[i]==s)indices[n++]=i;
        /* The approved two-card arrangement expands within its team panel for 3vs1. */
        for(i=0;i<(n>2?n:2);i++) {
            f32 left=s?1041:76,available=s?556:592,w=n>2?(available-18)/3:(available-18)/2;
            Vm_Card(i<n?indices[i]:-1,left+i*(w+18),225,w,382);
        }
    }
    Mg_Stage(1168,697,453,151);Mg_Ink(1168,667,453,30);sprintf(text,"ESCENARIO - %s",Mg_StageName(R->battle.stage));Mg_Text(1181,672,text,21,420,1);
    Mg_Outline(978,48,402,47,R->cursor==0);Mg_Outline(55,658,415,54,R->cursor==3);
    Mg_Outline(55,726,415,54,R->cursor==1);Mg_Outline(55,794,415,54,R->cursor==4);Mg_Outline(1164,665,460,190,R->cursor==2);
    if(sVsMode==PC_FFA){Mg_Paper(132,731,335,45);Mg_Text(145,740,"COMBATE INDIVIDUAL",24,310,0);}
    if(!Vs_CanStart()||R->connection!=1)Mg_Rect(575,716,525,87,Ps_Rgba(35,35,35,85));
    Mg_Outline(575,716,525,87,R->cursor==5);
    if(sVsStarting){Mg_Ink(610,735,442,56);Mg_Text(625,747,"INICIANDO COMBATE...",33,412,1);}
    Mg_Paper(610,817,502,45);Mg_Text(625,826,R->connection==2?"El anfitrion iniciara cuando todos esten listos":Vs_CanStart()?"Todos listos. Puedes iniciar.":"Esperando jugadores y confirmacion Listo",20,470,0);
    if(R->message[0]){Mg_Ink(785,884,836,32);Mg_Text(797,891,R->message,17,810,1);}
}
static void Vm_Draw(void) {
    extern void Gfx_AddDefaultEnv(void);Gfx_AddDefaultEnv();Ps_PageFull();Ps_SetShift(0);
    if(R->page==RD_ALLY)Vm_Selector();else Vm_Lobby();
}
static void Vm_Pointer(s32 x,s32 y,u32 *p) {
    s32 i,page=R->gridCursor/30;
    if(R->page==RD_ALLY) {
        for(i=0;i<5;i++)if(Mg_Hit(x,y,39+i*111,124,102,39)){Vm_List(i);return;}
        if(Mg_Hit(x,y,36,175,560,630)){i=page*30+((s32)(y/Mg_Y(1))-177)/105*5+((s32)(x/Mg_X(1))-39)/112;if(i>=0&&i<R->gridCount){R->gridCursor=i;Rd_GridChoose();Vm_ResetChoice();}return;}
        if(Mg_Hit(x,y,190,814,250,40)){R->gridCursor=Rd_Wrap(R->gridCursor+(x<Mg_X(310)?-30:30),R->gridCount?R->gridCount:1);Rd_GridChoose();Vm_ResetChoice();return;}
        if(Mg_Hit(x,y,1180,665,452,80))*p|=PADG_CROSS;
        if(Mg_Hit(x,y,640,718,455,72)){s32 f[8],n=Vm_Forms(f);i=(x/Mg_X(1)-640)/114;if(i<n){Rd_Shown()->chara=f[i];Rd_Shown()->costume=0;Vm_List(sVmFilter);Vm_ResetChoice();}return;}
        if(Mg_Hit(x,y,762,801,370,55)){Rd_Costume(Rd_Shown(),x<Mg_X(947)?-1:1);Vm_ResetChoice();}
        return;
    }
    if(Mg_Hit(x,y,978,48,402,47))R->cursor=0;
    else if(Mg_Hit(x,y,55,658,415,54))R->cursor=3;
    else if(Mg_Hit(x,y,55,726,415,54))R->cursor=1;
    else if(Mg_Hit(x,y,55,794,415,54))R->cursor=4;
    else if(Mg_Hit(x,y,1164,665,460,190)){R->cursor=2;if(R->connection==1){R->battle.stage=Port_MapStep(R->battle.stage,x<Mg_X(1394)?-1:1);gTeamSel->stage->stage=R->battle.stage;TeamSel_RequestStage();Vs_ResetReady();}return;}
    else if(Mg_Hit(x,y,575,716,525,87))R->cursor=5;else return;
    *p|=PADG_CROSS;
}
static void Vm_SelectorInput(u32 p,u32 repeat) {
    s32 delta=0;
    if(R->editing){Rd_EditInput(p);Vm_List(sVmFilter);return;}
    if(p&PADG_CIRCLE){R->page=RD_LOBBY;R->cursor=3;return;}
    if(p&PADG_TRIANGLE){Rd_EditBegin(3);return;}
    if(p&PADG_SQUARE){Vm_List(sVmFilter+1);return;}
    if(p&PADG_L1)Vm_Form(-1);if(p&PADG_R1)Vm_Form(1);
    if(p&PADG_L2)Rd_Costume(Rd_Shown(),-1);if(p&PADG_R2)Rd_Costume(Rd_Shown(),1);
    if(repeat&(PADG_LEFT|PADG_LEFT2))delta=-1;else if(repeat&(PADG_RIGHT|PADG_RIGHT2))delta=1;
    else if(repeat&(PADG_UP|PADG_UP2))delta=-5;else if(repeat&(PADG_DOWN|PADG_DOWN2))delta=5;
    if(p&PADG_R3)delta=30;if(p&PADG_L3)delta=-30;
    if(delta&&R->gridCount){R->gridCursor=Rd_Wrap(R->gridCursor+delta,R->gridCount);Rd_GridChoose();}
    Vm_ResetChoice();if(p&PADG_CROSS){R->page=RD_LOBBY;R->cursor=4;}
}

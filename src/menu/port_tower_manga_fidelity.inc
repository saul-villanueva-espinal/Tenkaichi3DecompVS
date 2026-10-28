#include "port_manga_widgets.inc"
static void Tm_Draw(void) {
    static const char tabs[5][8]={"TODOS","Z","SUPER","GT","MODS"};
    s32 i,id=Nr_Id(0),page=sNrCursor[0]/30,forms[8],n=Tm_Forms(id,forms);char text[128],name[96],form[96];
    Ps_PageFull();Ps_SetShift(0);Ps_Sprite(TBO_MANGA_TOWER_SELECT,0,0,512,448,0,0,1,1,PS_PLAIN,128);
    for(i=0;i<5;i++) {
        if(i==sTowerFilter)Mg_Rect(25+i*100,123,96,32,Ps_Rgba(187,8,20,128));
        else Mg_Ink(25+i*100,123,96,32);
        Mg_Text(40+i*100,128,tabs[i],22,75,1);
    }
    for(i=0;i<30;i++) {
        s32 idx=page*30+i;f32 x=22+(i%5)*117,y=166+(i/5)*102;
        Mg_Ink(x-1,y-1,114,99);
        if(idx<sNrCount){s32 ch=Port_PcRosterId(sNrList[idx]);if(ch>=0)Mg_Portrait(ch,x,y,111,96);else Mg_Text(x+41,y+25,"+",43,40,1);}
        Mg_Outline(x,y,111,96,idx==sNrCursor[0]);
    }
    sprintf(text,"<     %d / %d     >",page+1,(sNrCount+29)/30);Mg_Text(210,780,text,23,210,0);
    if(!sNrCount)Mg_Text(100,360,"Sin personajes",28,430,0);
    Port_RaidModelsSet(0,id,sNrCostume[0]);Port_TowerModelsDraw();Ps_PageFull();
    if(id>=0)Port_RaidRosterDetails(name,96,form,96,id);else{Ps_Copy(name,"CASILLA LIBRE",96);form[0]=0;}
    Mg_Text(716,612,name,39,355,0);Mg_Text(712,652,form,25,354,0);
    Mg_Paper(717,675,319,26);sprintf(text,"<     TRAJE %d / 8     >",sNrCostume[0]+1);Mg_Text(747,677,text,23,260,0);
    for(i=0;i<n&&i<7;i++){Mg_Portrait(forms[i],637+i*59,714,55,62);Mg_Outline(637+i*59,714,55,62,forms[i]==id);}
    sprintf(text,"PISO %d - %s",sTowerFloor+1,sTmFloors[sTowerFloor]);Mg_Text(1126,83,text,38,510,0);
    Mg_Stage(1122,139,520,186);Mg_Ink(1121,326,522,26);Mg_Text(1137,330,Mg_StageName(sNr.stage),22,480,1);
    for(i=0;i<sNr.count[1]&&i<5;i++){
        f32 x=1120+i*108;Mg_Portrait(sNr.fighter[1][i].chara,x,422,101,111);
        Port_RaidRosterDetails(name,96,form,96,sNr.fighter[1][i].chara);Mg_Text(x,548,name,18,101,0);
    }
    Mg_Text(1133,634,sTowerFloor==0?"Ki +50%":sTowerFloor==1?"Danio x1.25":sTowerFloor==2?"Rivales: regeneracion HP":sTowerFloor==3?"Kid Buu: Ki infinito":"Rivales: super armadura",22,240,0);
    Mg_Text(1392,634,"Sin limite de tiempo",21,240,0);
    for(i=0;i<5;i++){f32 x=1273+i*59;u32 color=i<(sTowerFloor==0?2:sTowerFloor==1?3:sTowerFloor==2?4:5)?Ps_Rgba(255,185,10,128):Ps_Rgba(112,111,105,128);s32 k;for(k=0;k<5;k++){f32 a=-1.5708f+k*1.2566f,b=a+.6283f,c=a+1.2566f;Ps_Quad(Mg_X(x),Mg_Y(728),Mg_X(x+cosf(a)*23),Mg_Y(728+sinf(a)*23),Mg_X(x+cosf(b)*10),Mg_Y(728+sinf(b)*10),Mg_X(x+cosf(c)*23),Mg_Y(728+sinf(c)*23),color,color,color,color);}}
    sprintf(text,"TU EQUIPO - %d / %d",Tm_Filled(),sNr.count[0]);Mg_Text(45,845,text,30,365,1);
    for(i=0;i<sNr.count[0]&&i<5;i++){
        f32 x=456+i*154;Mg_Ink(x+2,819,137,91);
        if(sNrFilled[0][i]){Mg_Portrait(sNr.fighter[0][i].chara,x+3,820,133,85);sprintf(text,"TRAJE %d",sNr.fighter[0][i].costume+1);Mg_Ink(x+3,892,133,17);Mg_Text(x+11,894,text,14,120,1);}
        else Mg_Text(x+50,839,"+",50,50,1);
        Mg_Outline(x,815,140,98,!sTowerConfirm&&i==sNrSlot[0]);
    }
    Mg_Outline(1241,817,388,44,sTowerConfirm);
    if(!Nr_Complete())Mg_Rect(1241,817,388,44,Ps_Rgba(20,15,12,90));
    Mg_Text(1262,874,"Puedes entrar con 1 a 5 guerreros",18,353,1);
    if(sNrMessage[0]){Mg_Ink(620,782,1008,23);Mg_Text(630,785,sNrMessage,17,982,1);}
}

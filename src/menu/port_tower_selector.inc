#include "menu/tbo_art.h"
#include "port_select_models.h"
extern void Port_TowerModelsDraw(void);
extern void Port_RaidRosterDetails(char *,s32,char *,s32,s32);
extern s32 Port_CharaFormTarget(s32,s32);
static s32 sTowerConfirm;
static const char *const sTmFloors[5]={"ASALTO SAIYAN","SAGA FREEZER","SAGA CELL","SAGA MAJIN","SAGA DIVINA"};
static void Tm_Text(f32 x,f32 y,const char *s,f32 scale,f32 width,s32 white) {
    f32 w=Ps_TextWidth((char *)s,scale),sx=scale;u32 ink=white?Ps_Rgba(255,249,228,128):Ps_Rgba(20,12,9,128);
    if(w>width&&width>0)sx*=width/w;
    Ps_Align(0);Ps_SetTextKind(3);Ps_TextExOuterXY(x,y,(char *)s,sx,scale,0,ink,ink,white?Ps_Rgba(65,3,7,128):Ps_Rgba(239,221,187,128),0,0);Ps_SetTextKind(0);
}
static void Tm_Box(f32 x,f32 y,f32 w,f32 h,s32 selected) {
    u32 edge=selected?Ps_Rgba(221,12,25,128):Ps_Rgba(15,10,7,128),paper=Ps_Rgba(237,222,191,128);
    Ps_Rect(x,y,x+w,y+h,edge,edge);Ps_Rect(x+1,y+1,x+w-1,y+h-1,paper,paper);
    if(selected){Ps_Rect(x-1,y-1,x+w+1,y,edge,edge);Ps_Rect(x-1,y+h,x+w+1,y+h+1,edge,edge);}
}
static s32 Tm_Filled(void){s32 i,n=0;for(i=0;i<sNr.count[0];i++)n+=sNrFilled[0][i]!=0;return n;}
static void Tm_Filter(s32 n) {
    static const s32 sagas[5]={NR_SAGA_ALL,NR_SAGA_Z,NR_SAGA_SUPER,NR_SAGA_GT,NR_SAGA_ALL};
    sTowerFilter=Ps_Wrap(n,0,4);sNrSaga=sagas[sTowerFilter];sNrCursor[0]=sNrCursor[1]=0;Nr_List();sNrCostume[0]=0;
    sNrMessage[0]=0;Snd_PlaySe(1,0);
}
static s32 Tm_Forms(s32 id,s32 *forms) {
    s32 count,k,j,link;count=Nr_GetForms(id,forms,8);
    for(k=0;k<4&&count<8;k++) {
        link=Port_CharaFormTarget(id,k);if(link<0||link==255)continue;
        for(j=0;j<count;j++)if(forms[j]==link)break;
        if(j==count)forms[count++]=link;
    }return count;
}
static void Tm_Form(s32 direction) {
    s32 forms[8],id=Nr_Id(0),n=Tm_Forms(id,forms),i;
    for(i=0;i<n;i++)if(forms[i]==id)break;
    if(n>1){s32 next=forms[Ps_Wrap(i+direction,0,n-1)];if(sTowerFilter==4){sTowerFilter=0;sNrSaga=NR_SAGA_ALL;Nr_List();}Nr_SelectForm(0,next);}
}
static void Tm_Costume(s32 direction) {
    s32 id=Nr_Id(0),n,c=sNrCostume[0];char why[160];if(id<0)return;
    for(n=0;n<8;n++){c=Ps_Wrap(c+direction,0,7);if(Port_ExtraCostumeOk(id,c,why,sizeof(why)))break;}
    if(n<8){sNrCostume[0]=c;gTeamSel->side[0]->chara=-1;Snd_PlaySe(1,0);}
}
static void Tm_Page(s32 direction) {
    s32 page;if(sNrCount<1)return;
    page=Ps_Wrap(sNrCursor[0]/30+direction,0,(sNrCount-1)/30);
    sNrCursor[0]=page*30;sNrCostume[0]=0;sNrMessage[0]=0;Snd_PlaySe(1,0);
}
static void Tm_Remove(void) {
    s32 slot=sNrSlot[0];if(!sNrFilled[0][slot]&&slot>0)slot--;
    sNrFilled[0][slot]=0;sNrSlot[0]=slot;sTowerConfirm=0;sNrMessage[0]=0;Snd_PlaySe(1,2);
}
static void Tm_Confirm(s32 *done,s32 *result) {
    if(Nr_Complete()){*result=1;*done=1;Snd_PlaySe(1,1);}
    else {Ps_Copy(sNrMessage,"Completa las cinco plazas antes de confirmar.",160);Snd_PlaySe(1,7);}
}
static void Tm_Draw(void) {
    static const char *const tabs[5]={"TODOS","Z","SUPER","GT","MODS"};
    s32 i,id=Nr_Id(0),page=sNrCursor[0]/30,forms[8],n=Tm_Forms(id,forms);char buf[128],name[96],form[96];
    Ps_PageFull();Ps_SetShift(0);Ps_Sprite(TBO_TOWER_SELECTOR,0,0,512,448,0,0,1,1,PS_PLAIN,128);
    Port_RaidModelsSet(0,id,sNrCostume[0]);Port_TowerModelsDraw();Ps_PageFull();
    for(i=0;i<5;i++) {
        f32 x=13+i*33.5f;Ps_Rect(x,53,x+31,72,Ps_Rgba(i==sTowerFilter?174:13,6,6,128),Ps_Rgba(i==sTowerFilter?150:13,5,5,128));
        Tm_Text(x+3,55,tabs[i],.69f,25,1);
    }
    for(i=0;i<30;i++) {
        s32 idx=page*30+i,ch;f32 x=13+(i%5)*32,y=78+(i/5)*40;if(idx>=sNrCount)break;
        ch=Port_PcRosterId(sNrList[idx]);Tm_Box(x,y,30,37,idx==sNrCursor[0]);
        if(ch>=0)Nr_Chip(ch,x+2,y+2,26,33,128);else {Tm_Text(x+8,y+8,"+",1,15,0);Tm_Text(x+3,y+25,"Libre",.30f,24,0);}
    }
    sprintf(buf,"<     %d / %d     >",sNrCount?page+1:0,(sNrCount+29)/30);Tm_Text(56,321,buf,.67f,90,0);
    if(!sNrCount)Tm_Text(26,171,"Sin personajes en este filtro",.62f,145,0);
    if(id>=0)Port_RaidRosterDetails(name,sizeof(name),form,sizeof(form),id);else {Ps_Copy(name,"CASILLA LIBRE",96);form[0]=0;}
    if(!Port_SelectModelsActive()&&id>=0&&(gTeamSel->side[0]->flags&TEAMSEL_SIDE_FACE_READY))Nr_Image(gTeamSel->faceRes[0],0,198,75,126,200,0,128);
    Ps_Rect(198,260,323,290,Ps_Rgba(8,7,5,128),Ps_Rgba(8,7,5,128));
    Tm_Text(201,260,name,.87f,118,1);Tm_Text(201,277,form,.56f,118,1);
    Ps_Rect(218,291,311,304,Ps_Rgba(13,22,26,128),Ps_Rgba(13,22,26,128));sprintf(buf,"<  TRAJE %d / 8  >",sNrCostume[0]+1);Tm_Text(221,291,buf,.59f,87,1);
    Tm_Text(204,306,"FORMAS",.58f,85,0);
    for(i=0;i<n&&i<5;i++){f32 x=204+i*23;Tm_Box(x,319,21,27,forms[i]==id);Nr_Chip(forms[i],x+1,320,19,25,128);}
    sprintf(buf,"PISO %d - ",sTowerFloor+1);Ps_Cat(buf,sTmFloors[sTowerFloor],sizeof(buf));Tm_Text(338,57,buf,1.00f,161,0);
    Nr_Image(MPACK_AT(gTeamSel->stagePack,sNr.stage+1),0,340,83,157,61,0,128);
    Tm_Text(340,148,"RIVALES",.75f,155,0);
    for(i=0;i<sNr.count[1];i++) {
        f32 x=340+(i%5)*31,y=169+(i/5)*30;Tm_Box(x,y,29,27,0);Nr_Chip(sNr.fighter[1][i].chara,x+1,y+1,27,25,128);
    }
    Tm_Text(340,232,"REGLAS",.75f,155,0);
    Tm_Text(340,252,sTowerFloor==0?"Ki +50%":sTowerFloor==1?"Danio x1.25":sTowerFloor==2?"Rivales: regeneracion de HP":sTowerFloor==3?"Kid Buu: Ki infinito":"Rivales: super armadura",.57f,157,0);
    Tm_Text(340,267,"Sin limite de tiempo",.57f,157,0);
    Tm_Text(340,287,"PERSONAJE",.75f,155,0);Tm_Text(340,306,name,.66f,155,0);Tm_Text(340,323,form,.57f,155,0);
    sprintf(buf,"Traje %d / 8",sNrCostume[0]+1);Tm_Text(340,337,buf,.55f,155,0);
    sprintf(buf,"TU EQUIPO - %d / %d",Tm_Filled(),sNr.count[0]);Tm_Text(13,350,buf,.80f,250,1);
    for(i=0;i<sNr.count[0];i++) {
        f32 x=13+i*65;Tm_Box(x,368,62,53,!sTowerConfirm&&i==sNrSlot[0]);
        if(sNrFilled[0][i]){Nr_Chip(sNr.fighter[0][i].chara,x+2,370,30,37,128);Port_RaidRosterDetails(name,sizeof(name),form,sizeof(form),sNr.fighter[0][i].chara);Tm_Text(x+2,406,name,.42f,58,0);sprintf(buf,"T%d",sNr.fighter[0][i].costume+1);Tm_Text(x+37,383,buf,.57f,22,0);}
        else {sprintf(buf,"%d",i+1);Tm_Text(x+4,369,buf,.60f,15,0);Tm_Text(x+24,378,"+",1.35f,20,0);Tm_Text(x+15,405,"Vacio",.45f,43,0);}
    }
    if(!Nr_Complete())Ps_Rect(352,369,500,415,Ps_Rgba(17,8,6,82),Ps_Rgba(17,8,6,82));
    if(sTowerConfirm)Tm_Box(354,368,143,44,1);
    if(sTowerConfirm)Tm_Text(359,378,"CONFIRMAR EQUIPO",1.08f,134,0);
    if(sNrMessage[0]){Ps_Rect(12,337,326,350,Ps_Rgba(233,218,183,128),Ps_Rgba(233,218,183,128));Tm_Text(14,337,sNrMessage,.48f,308,0);}
    Tm_Text(12,430,"X: anadir   Cuadrado: quitar   O: volver   START: equipo",.42f,268,1);
    Tm_Text(286,430,"L1/R1: forma  L2/R2: traje  SELECT: filtro",.39f,220,1);
    Tm_Text(142,321,"R3: pagina",.42f,46,0);
}
static void Tm_Input(u32 p,u32 repeat,s32 x,s32 y,s32 click,s32 *done,s32 *result) {
    s32 i,old=sNrCursor[0],cur=old,page=old/30;
    if(click) {
        x=(s32)((x-Ps_X(0))*512.0f/(Ps_X(512)-Ps_X(0)));y=(s32)((y-Ps_Y(0))*448.0f/(Ps_Y(448)-Ps_Y(0)));
        if(y>=53&&y<=72&&x>=13&&x<181){Tm_Filter((x-13)/33.5f);return;}
        if(y>=78&&y<318&&x>=13&&x<173){i=page*30+((y-78)/40)*5+(x-13)/32;if(i<sNrCount){sNrCursor[0]=i;sNrCostume[0]=0;sTowerConfirm=0;p|=PADG_CROSS;}}
        if(y>=319&&y<338&&x<190){Tm_Page(x<100?-1:1);return;}
        if(y>=368&&y<=421&&x>=13&&x<338){sNrSlot[0]=(x-13)/65;sTowerConfirm=0;return;}
        if(y>=365&&y<=421&&x>=350)p|=PADG_START;
        if(y>=291&&y<=304&&x>=218&&x<=311){Tm_Costume(x<265?-1:1);return;}
        if(y>=319&&y<=346&&x>=204&&x<319){s32 forms[8],n=Tm_Forms(Nr_Id(0),forms);i=(x-204)/23;if(i<n){Nr_SelectForm(0,forms[i]);return;}}
    }
    old=cur=sNrCursor[0];
    if(p&PADG_CIRCLE){*done=1;*result=0;Snd_PlaySe(1,2);return;}
    if(p&PADG_START){Tm_Confirm(done,result);return;}
    if(p&PADG_SELECT){Tm_Filter(sTowerFilter+1);return;}
    if(p&PADG_L3){Tm_Filter(sTowerFilter-1);return;}
    if(p&PADG_R3){Tm_Page(1);return;}
    if(p&PADG_TRIANGLE){Tm_Page(-1);return;}
    if(p&PADG_L1){Tm_Form(-1);return;}if(p&PADG_R1){Tm_Form(1);return;}
    if(p&PADG_L2){Tm_Costume(-1);return;}if(p&PADG_R2){Tm_Costume(1);return;}
    if(p&PADG_SQUARE){Tm_Remove();return;}
    if(sTowerConfirm) {
        if(repeat&(PADG_UP|PADG_LEFT|PADG_UP2|PADG_LEFT2))sTowerConfirm=0;
        if(p&PADG_CROSS)Tm_Confirm(done,result);return;
    }
    if(repeat&(PADG_LEFT|PADG_LEFT2))cur--;
    else if(repeat&(PADG_RIGHT|PADG_RIGHT2))cur++;
    else if(repeat&(PADG_UP|PADG_UP2))cur-=5;
    else if(repeat&(PADG_DOWN|PADG_DOWN2)){if((cur%30)/5==5)sTowerConfirm=1;else cur+=5;}
    if(sNrCount)sNrCursor[0]=Ps_Wrap(cur,0,sNrCount-1);
    if(old!=sNrCursor[0]){sNrCostume[0]=0;sNrMessage[0]=0;Snd_PlaySe(1,0);}
    if(p&PADG_CROSS){Nr_Selected(0);if(Nr_Complete())sTowerConfirm=1;}
}

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
static void Tm_SelectForm(s32 target) {
    if(sTowerFilter==4){sTowerFilter=0;sNrSaga=NR_SAGA_ALL;Nr_List();}
    Nr_SelectForm(0,target);
    /* A form can belong to another saga; keep the visible filter truthful. */
    if(sNrSaga==NR_SAGA_ALL)sTowerFilter=0;
}
static void Tm_Form(s32 direction) {
    s32 forms[8],id=Nr_Id(0),n=Tm_Forms(id,forms),i;
    for(i=0;i<n;i++)if(forms[i]==id)break;
    if(n>1){s32 next=forms[Ps_Wrap(i+direction,0,n-1)];Tm_SelectForm(next);}
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
    else {Ps_Copy(sNrMessage,"Elige al menos un guerrero antes de confirmar.",160);Snd_PlaySe(1,7);}
}
#include "port_tower_manga_fidelity.inc"
static void Tm_Input(u32 p,u32 repeat,s32 x,s32 y,s32 click,s32 *done,s32 *result) {
    s32 i,old=sNrCursor[0],cur=old,page=old/30;
    if(click) {
        if(Mg_Hit(x,y,25,123,500,32)){Tm_Filter((x/Mg_X(1)-25)/100);return;}
        if(Mg_Hit(x,y,22,166,585,612)){i=page*30+((s32)(y/Mg_Y(1))-166)/102*5+((s32)(x/Mg_X(1))-22)/117;if(i>=0&&i<sNrCount){sNrCursor[0]=i;sNrCostume[0]=0;sTowerConfirm=0;p|=PADG_CROSS;}}
        if(Mg_Hit(x,y,200,779,240,32)){Tm_Page(x<Mg_X(320)?-1:1);return;}
        if(Mg_Hit(x,y,456,815,770,98)){sNrSlot[0]=(x/Mg_X(1)-456)/154;if(sNrSlot[0]>=sNr.count[0])sNrSlot[0]=sNr.count[0]-1;sTowerConfirm=0;return;}
        if(Mg_Hit(x,y,1241,817,388,70))p|=PADG_START;
        if(Mg_Hit(x,y,717,675,319,26)){Tm_Costume(x<Mg_X(877)?-1:1);return;}
        if(Mg_Hit(x,y,637,714,413,62)){s32 forms[8],n=Tm_Forms(Nr_Id(0),forms);i=(x/Mg_X(1)-637)/59;if(i<n){Tm_SelectForm(forms[i]);return;}}
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
    if(p&PADG_CROSS){Nr_Selected(0);if(Tm_Filled()==sNr.count[0])sTowerConfirm=1;}
}

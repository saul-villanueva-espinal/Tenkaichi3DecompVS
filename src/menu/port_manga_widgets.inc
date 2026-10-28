/* Coordinates follow the approved 1672x941 artwork; input uses the same map. */
static f32 Mg_X(f32 x){return x*(512.0f/1672.0f);}
static f32 Mg_Y(f32 y){return y*(448.0f/941.0f);}
static void Mg_Rect(f32 x,f32 y,f32 w,f32 h,u32 color){Ps_Rect(Mg_X(x),Mg_Y(y),Mg_X(x+w),Mg_Y(y+h),color,color);}
static void Mg_Text(f32 x,f32 y,const char *text,f32 height,f32 width,s32 white) {
    f32 size=height*.028f,sx=size,tw=Ps_TextWidth((char *)text,size),max=Mg_X(width);
    u32 ink=white?Ps_Rgba(255,247,224,128):Ps_Rgba(15,10,8,128);
    if(tw>max&&max>0)sx*=max/tw;
    Ps_Align(0);Ps_SetTextKind(3);Ps_TextExOuterXY(Mg_X(x),Mg_Y(y),(char *)text,sx,size,0,ink,ink,0,0,0);Ps_SetTextKind(0);
}
static void Mg_Outline(f32 x,f32 y,f32 w,f32 h,s32 on){
    if(on){u32 red=Ps_Rgba(240,20,30,128);Mg_Rect(x-3,y-3,w+6,3,red);Mg_Rect(x-3,y+h,w+6,3,red);Mg_Rect(x-3,y,3,h,red);Mg_Rect(x+w,y,3,h,red);}
}
static void Mg_Paper(f32 x,f32 y,f32 w,f32 h){Mg_Rect(x,y,w,h,Ps_Rgba(238,224,193,128));}
static void Mg_Ink(f32 x,f32 y,f32 w,f32 h){Mg_Rect(x,y,w,h,Ps_Rgba(13,12,12,128));}
static s32 Mg_Hit(s32 x,s32 y,s32 left,s32 top,s32 w,s32 h){return x>=Mg_X(left)&&x<Mg_X(left+w)&&y>=Mg_Y(top)&&y<Mg_Y(top+h);}
extern void Port_TeamPortrait(s32,f32,f32,f32,f32,s32);
static void Mg_Portrait(s32 id,f32 x,f32 y,f32 w,f32 h){Port_TeamPortrait(id,Mg_X(x),Mg_Y(y),Mg_X(w),Mg_Y(h),128);}
extern void Port_MangaStage(f32,f32,f32,f32);
extern void Port_MangaStageText(char *,s32,s32);
static char sMgStageName[64];
static char *Mg_StageName(s32 stage){Port_MangaStageText(sMgStageName,sizeof(sMgStageName),stage);return sMgStageName;}
static void Mg_Stage(f32 x,f32 y,f32 w,f32 h){Port_MangaStage(Mg_X(x),Mg_Y(y),Mg_X(w),Mg_Y(h));}
static void Mg_Sprite(s32 art,f32 x,f32 y,f32 w,f32 h,f32 u0,f32 v0,f32 u1,f32 v1){Ps_Sprite(art,Mg_X(x),Mg_Y(y),Mg_X(x+w),Mg_Y(y+h),u0,v0,u1,v1,PS_PLAIN,128);}

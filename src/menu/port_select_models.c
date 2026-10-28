#include "common.h"
#include "battle/view_a.h"
#include "battle/orbit_cam.h"
#include "battle/btl_obj.h"
#include "battle/stg_b.h"
#include "sys/file.h"
#include "sys/gfx.h"
#include "port_select_models.h"
#include "port_limits.h"

#ifdef PORT
extern void BtlObjAnim_PlayModel(BtlObj *obj, s32 anim, s32 mode);
extern void BtlObj_SetPartVisible(BtlObj *obj, u32 part, s32 on);
extern BtlObjPart *BtlObj_GetNode(BtlObj *obj, s32 node);
extern void BtlObjDraw_Draw(void);
extern void Gfx_MarkPass(s32 pass);
extern void Gfx_AddDefaultEnv(void);
extern void StgAmb_Init(void);
extern int Port_ModelSize(void);
extern int Port_NetSession(void);
extern int Port_Setting(const char *name, int def);
extern void Dma_AddScissor(s32 x0, s32 x1, s32 y0, s32 y1);
extern void Port_GsDrain(void);
extern s32 ChrTbl_WrapCostume(s32 chara, s32 *costume);

typedef struct SelectModel {
    s32 wanted, costume, loaded, loadedCostume, stable, object;
    f32 angle;
    View view;
} SelectModel;
static SelectModel sSelectModel[2];
static s32 sSelectModelsOn;

int Port_SelectModelsActive(void) { return sSelectModelsOn; }

void Port_SelectModelsBegin(void) {
    s32 i;
    /* This is a local presentation. The official network's original loading
       schedule must remain deterministic on clients without this extension. */
    if (Port_NetSession() || !Port_Setting("selection_models", 1)) return;
    ChrView_Init();
    StgAmb_Init();
    for (i = 0; i < 2; i++) {
        sSelectModel[i].wanted = sSelectModel[i].loaded = -1;
        sSelectModel[i].costume = sSelectModel[i].loadedCostume = -1;
        sSelectModel[i].stable = 0;
        sSelectModel[i].object = -1;
        sSelectModel[i].angle = 3.14159265f;
    }
    sSelectModelsOn = 1;
}

void Port_SelectModelsSet(int side, int chara, int costume) {
    SelectModel *m;
    if (!sSelectModelsOn || side < 0 || side > 1) return;
    m = &sSelectModel[side];
    if (chara >= 0 && (chara < PORT_DISC_CHARAS || (chara >= PORT_CHARA_FIRST && chara < PORT_CHARA_ID_LIMIT)))
        ChrTbl_WrapCostume(chara, &costume);
    if (chara != m->wanted || costume != m->costume) {
        m->wanted = chara;
        m->costume = costume;
        m->stable = 0;
    }
}

static void SelectModel_Load(s32 side) {
    SelectModel *m = &sSelectModel[side];
    ChrViewSlot *slot = &ChrView_Get()->slot[side];
    s32 file, size;
    BtlObj *obj;
    if (m->loaded == m->wanted && m->loadedCostume == m->costume) return;
    /* Stop drawing the previous object immediately, then wait eight frames
       before reusing its buffer. Rapid cursor movement never queues models. */
    if (++m->stable < 8) return;
    if (m->object >= 0) BtlObj_Destroy(m->object);
    m->object = -1;
    m->loaded = m->wanted;
    m->loadedCostume = m->costume;
    if (m->wanted < 0 || (m->wanted >= PORT_DISC_CHARAS && m->wanted < PORT_CHARA_FIRST) || m->wanted >= PORT_CHARA_ID_LIMIT) return;
    file = ChrView_GetModelFile(m->wanted, m->costume, 0);
    size = File_GetSize(file);
    if (size <= 0 || size > Port_ModelSize()) return;
    /* File_LoadSync also finishes outstanding menu texture reads; their normal
       completion handlers still own and attach those pictures. No Job_Clear. */
    slot->file[0].id = file;
    slot->file[0].size = size;
    if (!File_LoadSync(file, slot->file[0].buf, 0)) return;
    m->object = BtlObj_Create(3, (BtlResSlot *)&slot->file, 1);
    if (m->object < 0) return;
    obj = BtlObj_Get(m->object);
    BtlObjAnim_PlayModel(obj, 7, 2);
    obj->state.flags &= ~0x02000000;
    BtlObj_SetPartVisible(obj, 0, 1);
    m->angle = 3.14159265f;
}

void Port_SelectModelsDraw(void) {
    s32 i, j;
    BtlObj *obj;
    BtlObjPart *root,*head;
    SelectModel *m;
    ChrViewInfo *info;
    f32 target,dist,height,bodyHeight,floor;
    if (!sSelectModelsOn) return;
    for (i = 0; i < 2; i++) {
        SelectModel_Load(i);
        m = &sSelectModel[i];
        if (m->object >= 0) {
            obj = BtlObj_Get(m->object);
            obj->active = m->loaded == m->wanted && m->loadedCostume == m->costume;
            obj->state.views[0].flags = obj->state.views[1].flags = 0;
        }
    }
    gBtlStage->flags &= ~1;
    BtlObj_UpdateAll();
    for(i=0;i<2;i++)if(sSelectModel[i].object>=0)BtlObj_Get(sSelectModel[i].object)->noAnim=1;
    for (i = 0; i < 2; i++) {
        m = &sSelectModel[i];
        for (j = 0; j < 2; j++)
            if (sSelectModel[j].object >= 0) BtlObj_Get(sSelectModel[j].object)->active = i == j && m->loaded == m->wanted && m->loadedCostume == m->costume;
        info = ChrView_GetCharaInfo(m->loaded);
        View_InitLayout(&gOrbitCam->view, i ? VIEW_LAYOUT_RIGHT : VIEW_LAYOUT_LEFT);
        target=info ? info->targetY : 1.2f;
        dist=info ? info->dist*1.15f : 5.75f;
        /* Added models use their own bounds rather than a borrowed base
           character's camera. Keep the original roster's camera unchanged. */
        if (m->object>=0 && m->loaded>=PORT_CHARA_FIRST) {
            obj=BtlObj_Get(m->object);
            height=obj->state.box.max[1]-obj->state.box.min[1];
            if (height>.01f && height<1000.0f) {
                /* Collision bounds differ between imported models. Use the
                   standard root/head bones to reserve anatomical headroom too:
                   otherwise Cell looks short while Broly's hair gets cropped. */
                root=BtlObj_GetNode(obj,0);
                head=BtlObj_GetNode(obj,0x2e);
                floor=obj->state.box.max[1];
                if (root && head) {
                    floor=root->mtx.m[3][1];
                    bodyHeight=(floor-head->mtx.m[3][1])*1.35f;
                    if (bodyHeight>height && bodyHeight<height*1.5f)
                        height=bodyHeight;
                }
                /* Normal roster coverage, independent of wings/tail width.
                   Keep the feet anchored while allowing the native plates to
                   cover the lower legs, as in the original giant previews. */
                dist=height*1.5f;
                /* Native 448-line canvas: keep the ground near line 367,
                   just above the bottom carousel, throughout rotation. */
                target=floor-dist*.33f;
            }
        }
        OrbitCam_SetTarget(0.0f,target,0.0f);
        OrbitCam_SetDist(dist);
        OrbitCam_SetDistRange(0.1f, 1000.0f);
        /* Classic two-sided selection keeps its original turntable. The
           manga Tower/Oracle previews below retain their fixed framing. */
        m->angle += 0.008f;
        if (m->angle > 3.14159265f) m->angle -= 6.2831853f;
        OrbitCam_SetAngles(0.0f, m->angle, 0.0f);
        OrbitCam_Update(0);
        /* Each draw list keeps its own persistent view, not the orbit camera's
           scratch view overwritten by the other panel. */
        m->view = gOrbitCam->view;
        View_Apply(&m->view, 1);
        BtlObj_UpdateVisibility(i);
    }
    for (i = 0; i < 2; i++)
        if (sSelectModel[i].object >= 0) {
            m = &sSelectModel[i];
            BtlObj_Get(m->object)->active = m->loaded == m->wanted && m->loadedCostume == m->costume;
        }
    BtlObj_FinishVisibility(1);
    Gfx_MarkPass(2);
    BtlObjDraw_Draw();
    Gfx_MarkPass(0);
    Gfx_AddDefaultEnv();
    Dma_AddScissor(0, 511, 0, 447);
}

void Port_SelectModelsEnd(void) {
    s32 i;
    if (!sSelectModelsOn) return;
    for (i = 0; i < 2; i++)
        if (sSelectModel[i].object >= 0) BtlObj_Destroy(sSelectModel[i].object);
    ChrView_Term();
    gBtlCamView = NULL;
    sSelectModelsOn = 0;
}

typedef struct RaidPreview {SelectModel model;BtlResSlot resource;} RaidPreview;
static RaidPreview sRaidPreview[PORT_RAID_BOSSES];
static s32 sRaidPreviewOn,sRaidSelector;
static volatile f32 sMenuPreviewMetrics[PORT_RAID_BOSSES][6]; /* presentation QA: angle, framing */
extern void *Heap_Alloc(s32,u32,s32,s32);
extern void Heap_Free(void *);
void Port_RaidModelsBegin(void) {
    s32 i;Port_SelectModelsBegin();if(!sSelectModelsOn) return;
    memset(sRaidPreview,0,sizeof(sRaidPreview));
    for(i=0;i<PORT_RAID_BOSSES;i++) {
        SelectModel *m=&sRaidPreview[i].model;
        m->wanted=m->loaded=m->loadedCostume=m->object=-1;m->angle=3.14159265f;
    }sRaidPreviewOn=1;
}
void Port_RaidModelsSet(s32 slot,s32 chara,s32 costume) {
    SelectModel *m;if(!sRaidPreviewOn || slot<0 || slot>=PORT_RAID_BOSSES) return;
    m=&sRaidPreview[slot].model;
    if(chara!=m->wanted || costume!=m->costume) {m->wanted=chara;m->costume=costume;m->stable=0;}
}
static void RaidPreview_Load(s32 slot) {
    RaidPreview *p=&sRaidPreview[slot];SelectModel *m=&p->model;s32 file,size;BtlObj *obj;
    if(m->loaded==m->wanted && m->loadedCostume==m->costume) return;
    if(++m->stable<8) return;
    if(m->object>=0) {Port_GsDrain(); BtlObj_Destroy(m->object);}
    m->object=-1;m->loaded=m->wanted;m->loadedCostume=m->costume;
    if(m->wanted<0) return;
    file=ChrView_GetModelFile(m->wanted,m->costume,0);size=File_GetSize(file);
    if(size<=0 || size>Port_ModelSize()) return;
    if(!p->resource.file[0].buf) p->resource.file[0].buf=Heap_Alloc(Port_ModelSize(),64,0,2);
    if(!p->resource.file[0].buf) return;
    p->resource.file[0].id=file;p->resource.file[0].size=size;
    if(!File_LoadSync(file,p->resource.file[0].buf,0)) return;
    m->object=BtlObj_Create(3,&p->resource,1);
    if(m->object<0) return;obj=BtlObj_Get(m->object);
    BtlObjAnim_PlayModel(obj,7,2);obj->state.flags&=~0x02000000;BtlObj_SetPartVisible(obj,0,1);
}
/* Each object is drawn through its own persistent full-screen camera. The
   renderer receives real model geometry; none of the five previews is a PNG. */
void Port_RaidModelsDraw(s32 count,s32 focus) {
    /* Two rear places and two front flanks leave the selected leader clear.
       Independent floor anchors keep small/giant imports on the platform. */
    static const f32 centers[5]={400,326,461,478,354};
    static const f32 feet[5]={350,350,285,350,260};
    static const f32 pixels[5]={240,155,170,150,100};
    static const s32 order[5]={4,2,1,3,0};

    s32 i,j,k,n,x0=sRaidSelector?171:292,x1=sRaidSelector?346:511;SelectModel *m;BtlObj *obj;BtlObjPart *root,*head;
    f32 height,width,depth,floor,body,target,dist,cx,fy,ph,maxWidth;Vec4 size;
    if(!sRaidPreviewOn) return;
    if(sRaidSelector==2){x0=187;x1=330;}
    if(sRaidSelector==3){x0=189;x1=344;}
    if(count<1) count=1;if(count>PORT_RAID_BOSSES) count=PORT_RAID_BOSSES;
    for(i=0;i<PORT_RAID_BOSSES;i++) {
        RaidPreview_Load(i);m=&sRaidPreview[i].model;
        if(m->object>=0) {obj=BtlObj_Get(m->object);obj->active=m->wanted>=0 && m->loaded==m->wanted && m->loadedCostume==m->costume;obj->state.views[0].flags=obj->state.views[1].flags=0;}
    }
    gBtlStage->flags&=~1;BtlObj_UpdateAll();
    /* Sample the model's presentation pose once. Imported idle animations may
       turn the entire body even with a fixed orbit camera. */
    for(i=0;i<PORT_RAID_BOSSES;i++)if(sRaidPreview[i].model.object>=0)BtlObj_Get(sRaidPreview[i].model.object)->noAnim=1;

    n=focus>=0?1:count;
    {extern void BtlObjLight_FindRes0(void);BtlObjLight_FindRes0();}
    for(k=0;k<n;k++) {
        i=focus>=0?focus:(count==5?order[k]:(k+1)%count);m=&sRaidPreview[i].model;
        for(j=0;j<PORT_RAID_BOSSES;j++) if(sRaidPreview[j].model.object>=0) BtlObj_Get(sRaidPreview[j].model.object)->active=j==i;
        if(m->object<0 || m->loaded!=m->wanted || m->loadedCostume!=m->costume) continue;
        obj=BtlObj_Get(m->object);height=obj->state.box.max[1]-obj->state.box.min[1];
        width=obj->state.box.max[0]-obj->state.box.min[0];depth=obj->state.box.max[2]-obj->state.box.min[2];
        floor=obj->state.box.max[1];root=BtlObj_GetNode(obj,0);head=BtlObj_GetNode(obj,0x2e);
        if(root && head) {floor=root->mtx.m[3][1];body=(floor-head->mtx.m[3][1])*1.35f;if(body>height && body<height*1.6f) height=body;}
        if(height<.01f || height>1000.0f) height=3.0f;
        cx=focus>=0 || count==1?399:centers[i];fy=focus>=0?354:feet[i];ph=focus>=0 || count==1?285:pixels[i];
        if(sRaidSelector) {cx=259;fy=323;ph=249;}
        if(sRaidSelector==2){cx=260;fy=286;ph=262;}
        if(sRaidSelector==3){cx=263;fy=286;ph=262;}
        dist=height*433.0f/ph;
        maxWidth=(x1-2.0f-cx)*2.0f;if((cx-x0-2.0f)*2.0f<maxWidth) maxWidth=(cx-x0-2.0f)*2.0f;
        if(!sRaidSelector&&maxWidth>(focus>=0?155.0f:i==0?112.0f:64.0f)) maxWidth=focus>=0?155.0f:i==0?112.0f:64.0f;
        /* Tail/wing collision extents are unsuitable for humanoid framing. */
        if(sRaidSelector) width=height*.48f;
        else if(width>height*.45f) width=height*.45f;
        if(depth>height*.35f) depth=height*.35f;
        if(width>0 && width*433.0f/maxWidth>dist) dist=width*433.0f/maxWidth;
        if(depth>0 && depth*433.0f/maxWidth>dist) dist=depth*433.0f/maxWidth;
        ph=height*433.0f/dist;
        target=floor-height*.5f;
        View_InitLayout(&gOrbitCam->view,VIEW_LAYOUT_FULL);size=gOrbitCam->view.screenSize;
        View_SetProjection(&gOrbitCam->view,&size,433.0f,1.0f,gOrbitCam->view.aspect,
                           1792.0f+cx,1824.0f+fy-ph*.5f,1.0f,10877000.0f,.3f,65536.0f,1.0f);
        gOrbitCam->view.scissorX0=x0;gOrbitCam->view.scissorX1=x1;gOrbitCam->view.scissorY0=40;gOrbitCam->view.scissorY1=359;
        OrbitCam_SetTarget(0,target,0);OrbitCam_SetDist(dist);OrbitCam_SetDistRange(.1f,1000.0f);
        m->angle=3.14159265f;
        sMenuPreviewMetrics[i][0]=m->angle;sMenuPreviewMetrics[i][1]=cx;
        sMenuPreviewMetrics[i][2]=fy;sMenuPreviewMetrics[i][3]=ph;
        sMenuPreviewMetrics[i][4]=height;sMenuPreviewMetrics[i][5]=dist;
        OrbitCam_SetAngles(0,m->angle,0);OrbitCam_Update(0);m->view=gOrbitCam->view;
        /* Each preview has its own camera and world scale. Depth left by the
           other cameras is meaningless here and can hide entire later models.
           Clear depth buffer only within the preview scissor bounding box. */
        Dma_AddFrame(GFX_FRAME_FBP(),GFX_FBW,-1);
        Dma_AddScissor(x0,x1,40,359);Dma_AddZbuf(GFX_ZBP,0);
        Dma_AddFillRect((GFX_OFX>>4)+x0,(GFX_OFY>>4)+40,x1-x0+1,360-40,0);
        Gfx_AddDefaultEnv();
        View_Apply(&m->view,1);BtlObj_UpdateVisibility(0);BtlObj_FinishVisibility(0);
        /* Independent menu previews must not run the full-screen battle
           post pass five times: its alpha copies overwrite the previous
           preview and the room. Draw each native lit model directly. */
        {extern void BtlObjDraw_DrawLists(s32);
         Gfx_MarkPass(2);BtlObjDraw_DrawLists(1);}
        Gfx_MarkPass(0);Gfx_AddDefaultEnv();Dma_AddScissor(0,511,0,447);
    }
}
/* Only the new Boss selector uses the centered preview camera. */
void Port_RaidModelsDrawSelector(void) {sRaidSelector=1;Port_RaidModelsDraw(1,0);sRaidSelector=0;}
void Port_MangaModelsDraw(void) {sRaidSelector=3;Port_RaidModelsDraw(1,0);sRaidSelector=0;}
void Port_TowerModelsDraw(void) {sRaidSelector=2;Port_RaidModelsDraw(1,0);sRaidSelector=0;}
void Port_RaidModelsTurn(f32 delta) {(void)delta; /* Fixed presentation: no manual or automatic rotation. */}
void Port_RaidModelsEnd(void) {
    s32 i;if(!sRaidPreviewOn) return;
    Port_GsDrain(); /* finish pending texture/geometry reads before releasing them */
    for(i=0;i<PORT_RAID_BOSSES;i++) {
        RaidPreview *p=&sRaidPreview[i];if(p->model.object>=0) BtlObj_Destroy(p->model.object);
        if(p->resource.file[0].buf) Heap_Free(p->resource.file[0].buf);
    }sRaidPreviewOn=0;Port_SelectModelsEnd();
}
/* Compatibility wrappers for earlier single-boss callers. */
void Port_SelectModelsDrawRaid(void) {Port_SelectModelsDraw();}
void Port_SelectModelsDrawRaidEx(s32 layout) {(void)layout;Port_SelectModelsDraw();}

#endif

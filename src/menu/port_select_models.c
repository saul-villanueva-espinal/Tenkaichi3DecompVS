#include "common.h"
#include "battle/view_a.h"
#include "battle/orbit_cam.h"
#include "battle/btl_obj.h"
#include "battle/stg_b.h"
#include "sys/file.h"
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
extern s32 ChrTbl_WrapCostume(s32 chara, s32 *costume);

typedef struct SelectModel {
    s32 wanted, costume, loaded, loadedCostume, stable, object;
    f32 angle;
    View view;
} SelectModel;
static SelectModel sSelectModel[2];
static s32 sSelectModelsOn;
static s32 sSelectRaidLayout;

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
    for (i = 0; i < 2; i++) {
        m = &sSelectModel[i];
        for (j = 0; j < 2; j++)
            if (sSelectModel[j].object >= 0) BtlObj_Get(sSelectModel[j].object)->active = i == j && m->loaded == m->wanted && m->loadedCostume == m->costume;
        if (m->object < 0 || m->loaded != m->wanted || (sSelectRaidLayout && i)) continue;
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
        if (sSelectRaidLayout && m->object >= 0) {
            Vec4 size;
            f32 width, depth;
            obj=BtlObj_Get(m->object);
            height=obj->state.box.max[1]-obj->state.box.min[1];
            width=obj->state.box.max[0]-obj->state.box.min[0];
            depth=obj->state.box.max[2]-obj->state.box.min[2];
            floor=obj->state.box.max[1];
            root=BtlObj_GetNode(obj,0);head=BtlObj_GetNode(obj,0x2e);
            if(root && head) {
                floor=root->mtx.m[3][1];bodyHeight=(floor-head->mtx.m[3][1])*1.35f;
                if(bodyHeight>height && bodyHeight<height*1.6f) height=bodyHeight;
            }
            if(height>.01f && height<1000.0f) {
                dist=height*2.55f;
                /* Fit wide giants as well as tall imports throughout rotation. */
                if(width>0 && width*2.6f>dist) dist=width*2.6f;
                if(depth>0 && depth*2.6f>dist) dist=depth*2.6f;
                target=floor-height*.5f;
            }
            f32 cx = (sSelectRaidLayout == 2) ? 2048.0f : 2178.0f;
            View_InitLayout(&gOrbitCam->view,VIEW_LAYOUT_FULL);
            size=gOrbitCam->view.screenSize;
            View_SetProjection(&gOrbitCam->view,&size,433.0f,1.0f,gOrbitCam->view.aspect,
                               cx,1988.0f,1.0f,10877000.0f,.3f,65536.0f,1.0f);
            if (sSelectRaidLayout == 2) {
                gOrbitCam->view.scissorX0=175;gOrbitCam->view.scissorX1=345;
                gOrbitCam->view.scissorY0=35;gOrbitCam->view.scissorY1=380;
            } else {
                gOrbitCam->view.scissorX0=262;gOrbitCam->view.scissorX1=511;
                gOrbitCam->view.scissorY0=35;gOrbitCam->view.scissorY1=269;
            }
        }
        OrbitCam_SetTarget(0.0f,target,0.0f);
        OrbitCam_SetDist(dist);
        OrbitCam_SetDistRange(0.1f, 1000.0f);
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
    BtlObj_FinishVisibility(sSelectRaidLayout ? 0 : 1);
    Gfx_MarkPass(2);
    BtlObjDraw_Draw();
    Gfx_MarkPass(0);
    Gfx_AddDefaultEnv();
    Dma_AddScissor(0, 511, 0, 447);
}

/* Independent single-boss presentation; the original two-player view keeps
   its camera, sizing and split layout. */
void Port_SelectModelsDrawRaid(void) {
    sSelectRaidLayout=1;Port_SelectModelsDraw();sSelectRaidLayout=0;
}

void Port_SelectModelsDrawRaidEx(s32 layout) {
    sSelectRaidLayout=layout>0?layout:1;Port_SelectModelsDraw();sSelectRaidLayout=0;
}

void Port_SelectModelsEnd(void) {
    s32 i;
    if (!sSelectModelsOn) return;
    for (i = 0; i < 2; i++)
        if (sSelectModel[i].object >= 0) BtlObj_Destroy(sSelectModel[i].object);
    ChrView_Term();
    if (gOrbitCam) gBtlCamView = &gOrbitCam->view;
    sSelectModelsOn = 0;
}
#endif

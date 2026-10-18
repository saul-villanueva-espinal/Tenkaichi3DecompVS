#include "common.h"
#include "battle/view_a.h"
#include "battle/orbit_cam.h"
#include "sys/pad.h"
#include "sys/game_pad.h"
#include "sys/job.h"
#include "sys/fade.h"
#include "sys/file.h"
#include "sys/heap.h"
#include "menu/port_screen.h"

#ifdef PORT
extern void Port_MenuScene3D(int on);
extern void Dma_ResetBuffers(void);
extern void Dma_Flush(void);
extern void File_Stub264D90(void);
extern void Font_FlushAll(void);
extern const char *Port_FighterName(int chara);
extern int Port_ExtraCostumeOk(int chara, int costume, char *why, int size);
extern s32 ChrTbl_WrapCostume(s32 chara, s32 *costume);
extern void Bgm_Play(s32 bgm);

/*
 * Port_ChrPreview_Run:
 * Renders the real-time 3D model of the character in the 3D battlefield scenery.
 * OrbitCam allows rotating the camera 360 around the model with the stick or D-pad.
 *
 * isConfirm:
 *   1 = confirmation on character selection (Cross/Start accepts and returns costume, Triangle cancels and returns -1)
 *   0 = browse inspection (Square/Triangle/Cross/Start exits and returns costume)
 */
s32 Port_ChrPreview_Run(s32 chara, s32 costume, s32 bgm, s32 isConfirm) {
    s32 totalCostumes, curCostume = costume;
    s32 loaded = 0, frames = 0;
    char text[64];

    if (chara < 0 || chara >= PORT_TOTAL_CHARAS || chara == MCHR_RANDOM) return costume;

    totalCostumes = ChrTbl_WrapCostume(chara, NULL);
    if (totalCostumes < 1) totalCostumes = 1;
    if (totalCostumes > 8) totalCostumes = 8;
    if (curCostume < 0 || curCostume >= totalCostumes) curCostume = 0;

    ChrView_Init();
    ChrView_Show(chara, curCostume, 0);
    Port_MenuScene3D(1);
    ColorFade_StartIn(0, 0, 0, 10);

    while (1) {
        Pad *pad = &gPad[0];
        u32 pressed, repeat, down;

        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        Job_Run();

        pressed = pad->gamePressed;
        repeat = pad->gameRepeat;
        down = pad->gameDown;

        if (ChrView_IsVisible()) {
            /* Support manual 360 rotation with D-pad or keyboard arrow keys in addition to analog stick */
            if (gOrbitCam != NULL) {
                if (down & PAD_LEFT) gOrbitCam->angles.y -= 0.04f;
                if (down & PAD_RIGHT) gOrbitCam->angles.y += 0.04f;
                if (down & PAD_UP) gOrbitCam->angles.x -= 0.02f;
                if (down & PAD_DOWN) gOrbitCam->angles.x += 0.02f;
            }
            ChrView_Update();
            loaded = 1;
            frames++;
        }

        /* Costume switching with L1/R1 */
        if (loaded) {
            s32 nextCostume = curCostume;
            if ((pressed & PADG_R1) || (repeat & PADG_R1)) {
                nextCostume++;
                if (nextCostume >= totalCostumes) nextCostume = 0;
            } else if ((pressed & PADG_L1) || (repeat & PADG_L1)) {
                nextCostume--;
                if (nextCostume < 0) nextCostume = totalCostumes - 1;
            }

            if (nextCostume != curCostume) {
                if (Port_ExtraCostumeOk(chara, nextCostume, NULL, 0)) {
                    curCostume = nextCostume;
                    ChrView_Show(chara, curCostume, 0);
                    Snd_PlaySe(1, 0);
                }
            }
        }

        /* 2D HUD text and button prompts */
        Ps_Text(24.0f, 26.0f, (char *)Port_FighterName(chara), 0.70f, 300.0f, 1);
        sprintf(text, "TRAJE %d / %d", curCostume + 1, totalCostumes);
        Ps_Text(24.0f, 52.0f, text, 0.48f, 160.0f, 3);

        Ps_PromptsClear();
        if (isConfirm) {
            Ps_Prompt(0, "Aceptar");
            Ps_Prompt(2, "Volver");
            Ps_Prompt(3, "Rotar 360");
            if (totalCostumes > 1) {
                Ps_Prompt(3, "L1/R1: Traje");
            }
        } else {
            Ps_Prompt(2, "Volver");
            Ps_Prompt(3, "Rotar 360");
            if (totalCostumes > 1) {
                Ps_Prompt(3, "L1/R1: Traje");
            }
        }
        Ps_PromptsDraw();

        Font_FlushAll();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();

        /* Input handling for exit */
        if (frames > 15) {
            if (isConfirm) {
                if (pressed & (PADG_CROSS | PADG_START)) {
                    Snd_PlaySe(1, 1);
                    break;
                }
                if (pressed & PADG_TRIANGLE) {
                    Snd_PlaySe(1, 2);
                    curCostume = -1;
                    break;
                }
            } else {
                if (pressed & (PADG_SQUARE | PADG_TRIANGLE | PADG_CIRCLE | PADG_CROSS | PADG_START)) {
                    Snd_PlaySe(1, 2);
                    break;
                }
            }
        }
    }

    ChrView_Hide();
    ChrView_Term();
    Port_MenuScene3D(0);
    Dma_ResetBuffers();

    if (bgm > 0) {
        Bgm_Play(bgm);
    }

    return curCostume;
}
#endif

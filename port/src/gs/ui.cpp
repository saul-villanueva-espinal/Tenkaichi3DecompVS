// The settings window of the PC build: Dear ImGui over the finished picture. F1 opens and closes it, or START + SELECT
// of a controller, or the key and button of its Controls list (gs_input.c: PortInput_Hotkey).
// Everything it shows lives elsewhere (ui.h): the renderer's settings in gs_gpu.c, the bindings in gs_input.c.
// It runs on the render thread, which owns the window and its events.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"
#include "imgui_impl_opengl3.h"
#include "ui.h"
#include "namefont.h"
#include "../port_release.h"
#include "../plat_fighters.h"
#include "menu_text.h"
#include "../quit_dialog.h"
#include "net_chat.h"

static int sMeter = -1; // the meter (frame rate, connection): -1 not read yet
static bool meter_on(void);
static int sGL; // 1: the OpenGL back end draws the window (ImGui's OpenGL3 backend, no SDL GPU device)
static char sGpuName[256];

extern "C" {
extern volatile int gPortNetMenuRequest;
void Port_NetOptions(int rollback, int delay); // gs/net.c
void Port_NetRulesSet(int type, int dp, int time); // gs/net.c: the host's battle type, DP limit row, time limit
void Port_NetSpeedSet(int percent);                // ... and the match's speed, 100..200 (set before the rules)
int Port_NetSpeedStandard(void);                   // what a quick match runs at
void Port_NetNameSet(const char *name);         // gs/net.c: this player's name for the match
int Port_IsWide(void);                         // plat_stub.c: the picture is wider than 4:3
int Port_AspectMilli(void);
extern "C" int gPortTboTyping;                     // gs_input.c: a keypad of Tenkaichi Budokai Online takes text
extern "C" int PortInput_TboEvent(const SDL_Event *ev);
extern "C" void PortInput_TboClipboard(void);      // ... and its "Copy Code": onto the clipboard, from this thread
void Match_UpdateAsk(void);                    // net_match.c: asks GitHub for the newest release, once
int Match_UpdateLatest(void);                  // ... its number (0.1.33 is 133), 0 while unknown
int Port_Menus43(void);                       // plat_stub.c: the menus are shown at 4:3 in a wider picture
int Port_LateInput(void);                     // plat_stub.c: "Low input lag", the blanks are waited for before the pad read
void Port_LateInputSet(int on);
void Port_Menus43Set(int on);
int Port_HudLayout(void);                      // gs_draw.c: 2 = the HUD is about the middle of the screen
int GsDraw_HudShown(void);                     // ... and 1 while the game is drawing the HUD's side panels
const char *Port_NetName(int player);          // the name of the player of side 0 or 1 ("" outside a match)
int Port_HudSwap(void);                        // the two sides of the HUD are in each other's places on this copy
int Port_NetActive(void);
int Port_NetMe(void);                          // gs/net.c: this copy's side, 0 or 1
void *BtlChar_FindByObjId(int objId);          // the game: the fighter of a side, NULL outside a fight
int Port_NetStats(int *out);                   // gs/net.c: the meter's numbers of an online match
int Port_LobbyOther(void);
extern unsigned gPortLiveBlanks;               // plat_stub.c: vertical blanks the game has gone through
int Port_RollCan(void);                        // gs/state.c
const char *Port_FileRoot(void);               // plat_file.c: the game's data folder
unsigned GsGl_StripTexture(const void *rgba, int w, int h); // gs_gl.c
void GsGl_StripTextureFree(unsigned id);
const char *PortStages_Dir(void);                          // plat_stages.c
const char *PortStages_Name(int index);
int PortStages_Count(void);
const char *PortSongs_Dir(void);                           // plat_songs.c
const char *PortSongs_Name(int index);
int PortSongs_Count(void);
int Port_Setting(const char *name, int def);   // plat_settings.c
void Port_SettingSave(const char *name, int value);
void Port_SettingText(const char *name, const char *def, char *out, int size);
void Port_SettingTextSave(const char *name, const char *value);
void Port_SettingsWrite(void);
int Port_ClashSquare(void);                    // gs_input.c: Square counts as a direction in clashes
void Port_ClashSquareSet(int on);
int Port_ClashSquareX(void);                   // what a press is worth, in quarters of a point (4, 6, 7, 8)
void Port_ClashSquareXSet(int quarters);
extern int gsInterp;                           // gs_draw.c: in-between pictures on (1) / off (0); -1 before the first frame
void Port_SettingsWrite(void);
int Port_LobbyStart(int host, const char *address, int port); // gs/net.c
int Port_LobbyStartCode(int host, const char *code);         // the same with a room code (the matchmaking service)
int Port_LobbyStartQuick(int queue);                         // a quick match: 0 single, 1 team, 2..4 DP battle of 10 / 15 / 20
void Port_LobbyQueuesAsk(void);                              // asks the service how many wait in each queue
void Port_LobbyHelloAsk(void);                               // asks it whether this release is new enough
int Port_LobbyRelease(int *min, int *latest);                // this release's number; the oldest let in and the newest (0: not known)
int Port_LobbyQueueWaiting(int queue);                       // ... as last heard, -1 if not known
const char *Port_LobbyCode(void);                            // the host's room code, "" until it is known
const char *Port_LobbyError(void);                           // why the matchmaking failed (state -3)
int Port_LobbyRelayed(void);                                 // the other player was reached through the relay
int Port_LobbyPoll(void);
void Port_LobbyCancel(void);
void Port_LobbyLaunch(void);
}
static bool sReady, sOpen;
static bool sNet;                              // the open window is the online screen (Dragon Net Battle), not the settings
static int sForceTab = -1;                     // BT3_UI_OPEN=<tab>: open at start on that tab (testing)
static int sCapKind, sCapPlayer, sCapAction;   // waiting for a key (1) or a controller button (2) to bind
static Uint64 sCapUntil;                       // ... until then: a controller has no button that is not a binding, so the wait ends by itself
static bool sQuit;                             // the question before the game closes is up
static bool sQuitNo;                           // ... and Esc answered it: no
static bool sPadHold;                          // the controller does not work the window: a binding is waited for, or its button is still down
static int sTab;                               // the settings tab that is open (the shoulder buttons go to its neighbours)
static bool sPadTab;                           // the shoulder buttons changed the tab this frame
static SDL_GPUDevice *sDevice;
// The online chat (net_chat.c): its key opens a line to type, Enter sends it, Esc puts it away.
static bool sChat;                             // a line is being typed
static bool sChatFocus;                        // ... and its field takes the keyboard next frame
static bool sChatKeyHeld;                      // ... and the key that opened it is still down (its letter is not typed)
static bool sChatSoon;                         // the last Enter came within a second of the line before: not sent
static char sChatBuf[CHAT_TEXT + 1];
static void chat_close(void);

/* Name strips (gamedata/stages/names.rgba and gamedata/songs/names.rgba): the names of the stages and tracks added
   from outside the disc, drawn by the port over the menu in the game's own lettering. Raw RGBA, a 12-byte header
   (w, h, count) and then `count` images of w x h stacked. */
static SDL_GPUTexture *sNameTex, *sSongTex;
static uint32_t sNameW, sNameH, sNameCount, sSongW, sSongH, sSongCount;

static uint8_t *read_strip(const char *path, size_t *bytes, uint32_t *w, uint32_t *h, uint32_t *count) {
    FILE *fp = fopen(path, "rb");
    uint32_t hdr[3];
    uint8_t *pix;

    if (fp == NULL) {
        return NULL;
    }
    if (fread(hdr, sizeof(hdr), 1, fp) != 1 || hdr[0] == 0 || hdr[1] == 0 || hdr[2] == 0) {
        fclose(fp);
        return NULL;
    }
    *w = hdr[0];
    *h = hdr[1];
    *count = hdr[2];
    *bytes = (size_t)hdr[0] * hdr[1] * hdr[2] * 4;
    pix = (uint8_t *)malloc(*bytes);
    if (pix == NULL || fread(pix, 1, *bytes, fp) != *bytes) {
        free(pix);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    return pix;
}

static SDL_GPUTexture *make_strip(uint8_t *pix, size_t bytes, uint32_t w, uint32_t h, uint32_t count) {
    SDL_GPUTextureCreateInfo ci;
    SDL_GPUTransferBufferCreateInfo tbi;
    SDL_GPUTransferBuffer *tb;
    SDL_GPUCommandBuffer *cmd;
    SDL_GPUCopyPass *cp;
    SDL_GPUTextureTransferInfo src;
    SDL_GPUTextureRegion dst;
    SDL_GPUTexture *tex;
    void *map;

    if (sGL) { // the OpenGL back end: a GL texture, its name carried in the pointer (ImGui's OpenGL3 backend takes it so)
        unsigned id = GsGl_StripTexture(pix, (int)w, (int)(h * count));
        free(pix);
        return (SDL_GPUTexture *)(intptr_t)id;
    }
    SDL_zero(ci);
    ci.type = SDL_GPU_TEXTURETYPE_2D;
    ci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ci.width = w;
    ci.height = h * count;
    ci.layer_count_or_depth = 1;
    ci.num_levels = 1;
    ci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tex = SDL_CreateGPUTexture(sDevice, &ci);
    if (tex == NULL) {
        free(pix);
        return NULL;
    }
    SDL_zero(tbi);
    tbi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbi.size = (Uint32)bytes;
    tb = SDL_CreateGPUTransferBuffer(sDevice, &tbi);
    map = SDL_MapGPUTransferBuffer(sDevice, tb, false);
    memcpy(map, pix, bytes);
    SDL_UnmapGPUTransferBuffer(sDevice, tb);
    free(pix);
    cmd = SDL_AcquireGPUCommandBuffer(sDevice);
    cp = SDL_BeginGPUCopyPass(cmd);
    SDL_zero(src);
    src.transfer_buffer = tb;
    src.pixels_per_row = w;
    src.rows_per_layer = h * count;
    SDL_zero(dst);
    dst.texture = tex;
    dst.w = w;
    dst.h = h * count;
    dst.d = 1;
    SDL_UploadToGPUTexture(cp, &src, &dst, false);
    SDL_EndGPUCopyPass(cp);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(sDevice, tb);
    return tex;
}

static SDL_GPUTexture *load_strip(const char *env, const char *rel, uint32_t *w, uint32_t *h, uint32_t *count,
                                  const char *what) {
    const char *paths[3];
    uint8_t *pix = NULL;
    size_t bytes = 0;
    SDL_GPUTexture *tex;
    uint32_t i;

    char inData[600];
    // in the stages / songs folder (plat_extras.c); the environment variable names another file
    snprintf(inData, sizeof(inData), "%s", rel);
    paths[0] = getenv(env);
    paths[1] = inData;
    paths[2] = NULL;
    for (i = 0; i < 3 && pix == NULL; i++) {
        if (paths[i] != NULL) {
            pix = read_strip(paths[i], &bytes, w, h, count);
        }
    }
    if (pix == NULL) {
        return NULL;
    }
    tex = make_strip(pix, bytes, *w, *h, *count);
    if (tex != NULL) {
        fprintf(stderr, "bt3: %s overlay: %u names, %ux%u each\n", what, *count, *w, *h);
    }
    return tex;
}

// ---- the side bars ----
// Where the picture is narrower than the window (a 4:3 picture, or "Menus in 4:3", in a wide window) the sides are
// black, or with "Side bars: Border" a panel with a border along the picture. Two pictures: the border (trim.png),
// kept in proportion at the picture's edge and as high as the picture, and the plain panel (fill.png), stretched
// over the rest, so a bar of any width looks right. Both are drawn as they are on the left and mirrored on the right.
// The program carries a gold set (port/assets/bars, art by the project's owner: 34 x 720 and 126 x 720). A player
// replaces it by putting a trim.png and / or a fill.png of any size into a folder "bars" next to the game.
#include "bars_png.h"
static SDL_GPUTexture *sBarTrim, *sBarFill;
static int sBars = -1; // 0 black, 1 the border, 2 the border with the fight panels; -1 not read yet
static int sBarsTried;
static int sChatSize = -1; // 0 = compact, 1 = normal, 2 = large; -1 = not read yet

static float sBarTrimShape = 34.0f / 720.0f; // the border's width over its height

static SDL_GPUTexture *bar_texture(const char *file, const unsigned char *png, size_t size, float *shape) {
    SDL_Surface *s = SDL_LoadPNG(file), *rgba; // the player's own picture, if there is one
    if (s != NULL) {
        fprintf(stderr, "bt3: side bars: %s, %d x %d\n", file, s->w, s->h);
    } else {
        s = SDL_LoadPNG_IO(SDL_IOFromConstMem(png, size), true);
    }
    uint8_t *pix;
    size_t bytes;
    if (s == NULL) {
        return NULL;
    }
    rgba = SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(s);
    if (rgba == NULL) {
        return NULL;
    }
    if (shape != NULL && rgba->h > 0) {
        *shape = (float)rgba->w / (float)rgba->h;
    }
    bytes = (size_t)rgba->w * rgba->h * 4;
    pix = (uint8_t *)malloc(bytes);
    for (int y = 0; pix != NULL && y < rgba->h; y++) {
        memcpy(pix + (size_t)y * rgba->w * 4, (const uint8_t *)rgba->pixels + (size_t)y * rgba->pitch, (size_t)rgba->w * 4);
    }
    SDL_GPUTexture *tex = pix != NULL ? make_strip(pix, bytes, (uint32_t)rgba->w, (uint32_t)rgba->h, 1) : NULL;
    SDL_DestroySurface(rgba);
    return tex;
}

static bool bars_on(void) {
    if (sBars < 0) {
        sBars = Port_Setting("side_bars", 0);
        sBars = sBars < 0 ? 0 : sBars > 2 ? 2 : sBars;
    }
    return sBars != 0 && gUiPresentX > 0 && gUiPresentW > 0 && gUiPresentH > 0;
}

// For the fight panels (fight_panels_ui.cpp): textures of their own and the display faces.
static ImFont *sFontBold;
SDL_GPUTexture *Ui_Texture(uint8_t *pix, uint32_t w, uint32_t h) {
    return make_strip(pix, (size_t)w * h * 4, w, h, 1);
}
void Ui_TextureFree(SDL_GPUTexture *tex) {
    if (tex == NULL) {
        return;
    }
    if (sGL) {
        GsGl_StripTextureFree((unsigned)(intptr_t)tex);
    } else {
        SDL_ReleaseGPUTexture(sDevice, tex);
    }
}
ImFont *Ui_FontBold(void) { return sFontBold; }
bool FightPanels_Draw(ImDrawList *dl, float lx, float rx, float y0, float y1, float bar, float trim);

// The fight panels' two faces ("Name font", "Form font" in the settings): fonts of the game's own list (fonts.tsv,
// as the Menu font and the Dialogue / UI font), checked as those are (menu_text.c) and saved by their id; "Default"
// is the first of the list that can be used. A choice is loaded into the font atlas between two frames (fonts_apply).
struct PanelFont {
    const char *setting;
    ImFont *font;
    int choice;  // in use (0: Default)
    int pending; // chosen, loaded before the next frame (-1: nothing)
};
static PanelFont sPanelFont[2] = {{"panel_name_font_id", NULL, 0, -1}, {"panel_form_font_id", NULL, 0, -1}};
ImFont *Ui_FontName(void) { return sPanelFont[0].font; }
ImFont *Ui_FontForm(void) { return sPanelFont[1].font; }

// A choice of the list as an ImGui font (NULL: none can be used).
static ImFont *panel_font_load(int choice) {
    char path[1400];
    ImGuiIO &io = ImGui::GetIO();
    for (int i = choice > 0 ? choice : 1; i < MenuText_FontCount(); i++) {
        if (MenuText_FontFile(i, path, sizeof(path))) {
            return io.Fonts->AddFontFromFileTTF(path, 48.0f);
        }
        if (choice > 0) {
            break;
        }
    }
    return NULL;
}

static void fonts_apply(void) {
    for (PanelFont &pf : sPanelFont) {
        if (pf.pending < 0) {
            continue;
        }
        ImFont *f = panel_font_load(pf.pending);
        if (f != NULL) {
            if (pf.font != NULL) {
                ImGui::GetIO().Fonts->RemoveFont(pf.font);
            }
            pf.font = f;
            pf.choice = pf.pending;
            Port_SettingSave(pf.setting, MenuText_FontId(pf.choice));
            Port_SettingsWrite();
        }
        pf.pending = -1;
    }
}

// A combo of the game's fonts; the choice is loaded before the next frame.
static void font_picker(const char *label, PanelFont &pf, const char *tip) {
    if (ImGui::BeginCombo(label, pf.choice > 0 ? MenuText_FontName(pf.choice) : "Predeterminada")) {
        if (ImGui::Selectable("Predeterminada", pf.choice == 0)) {
            pf.pending = 0;
        }
        for (int i = 1; i < MenuText_FontCount(); i++) {
            ImGui::PushID(i);
            if (ImGui::Selectable(MenuText_FontName(i), pf.choice == i)) {
                pf.pending = i;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("%s", tip);
}

static void bars_draw(void) {
    if (!sBarsTried) {
        sBarsTried = 1;
        sBarTrim = bar_texture("bars/trim.png", kBarsTrimPng, sizeof(kBarsTrimPng), &sBarTrimShape);
        sBarFill = bar_texture("bars/fill.png", kBarsFillPng, sizeof(kBarsFillPng), NULL);
    }
    if (sBarTrim == NULL || sBarFill == NULL) {
        return;
    }
    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    float y0 = (float)gUiPresentY, y1 = y0 + (float)gUiPresentH;
    float bar = (float)gUiPresentX; // the width of each side
    float trim = (float)gUiPresentH * sBarTrimShape;
    float u0 = 0.0f;
    if (trim > bar) { // a bar narrower than the border: the border's inner part
        u0 = 1.0f - bar / trim;
        trim = bar;
    }
    float lx = (float)gUiPresentX, rx = lx + (float)gUiPresentW; // the picture's two edges
    ImTextureRef tt((ImTextureID)(intptr_t)sBarTrim), ft((ImTextureID)(intptr_t)sBarFill);
    if (bar > trim) {
        dl->AddImage(ft, ImVec2(lx - bar, y0), ImVec2(lx - trim, y1), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
        dl->AddImage(ft, ImVec2(rx + trim, y0), ImVec2(rx + bar, y1), ImVec2(1.0f, 0.0f), ImVec2(0.0f, 1.0f));
        // In a fight: each side's fighter over the plain panel, with the border along the picture at half its size
        // (in proportion, twice down the height) so the fighter has more room (asked for).
        const int tiles = 2;
        float thin = trim / tiles;
        if (sBars == 2 && FightPanels_Draw(dl, lx, rx, y0, y1, bar, thin)) {
            float th = (y1 - y0) / tiles;
            for (int i = 0; i < tiles; i++) {
                float a = y0 + th * i, b = a + th;
                dl->AddImage(tt, ImVec2(lx - thin, a), ImVec2(lx, b), ImVec2(u0, 0.0f), ImVec2(1.0f, 1.0f));
                dl->AddImage(tt, ImVec2(rx, a), ImVec2(rx + thin, b), ImVec2(1.0f, 0.0f), ImVec2(u0, 1.0f));
            }
            return;
        }
    }
    dl->AddImage(tt, ImVec2(lx - trim, y0), ImVec2(lx, y1), ImVec2(u0, 0.0f), ImVec2(1.0f, 1.0f));
    dl->AddImage(tt, ImVec2(rx, y0), ImVec2(rx + trim, y1), ImVec2(1.0f, 0.0f), ImVec2(u0, 1.0f));
}

static void load_strips(void) {
    char path[600];
    // The names of added stages and songs are drawn by this overlay, over the menu: from a strip of pre-rendered
    // names if the install script made one (names.rgba in the folder), else as text (frame_build). Either way the
    // game leaves the name to the overlay, so the flags are set whenever the overlay exists.
    snprintf(path, sizeof(path), "%s/names.rgba", PortStages_Dir());
    sNameTex = PortStages_Dir()[0] != '\0' ? load_strip("BT3_STAGE_NAMES", path, &sNameW, &sNameH, &sNameCount, "stage-name") : NULL;
    snprintf(path, sizeof(path), "%s/names.rgba", PortSongs_Dir());
    sSongTex = PortSongs_Dir()[0] != '\0' ? load_strip("BT3_SONG_NAMES", path, &sSongW, &sSongH, &sSongCount, "song-name") : NULL;
    if (sNameTex == NULL) { sNameCount = 0; }
    if (sSongTex == NULL) { sSongCount = 0; }
    gUiNameReady = 1;
    gUiSongReady = 1;
}

static void style() {
    ImGuiStyle &st = ImGui::GetStyle();
    ImGui::StyleColorsDark();
    st.WindowRounding = 10.0f;
    st.FrameRounding = 6.0f;
    st.GrabRounding = 6.0f;
    st.TabRounding = 6.0f;
    st.PopupRounding = 6.0f;
    st.WindowPadding = ImVec2(18.0f, 16.0f);
    st.FramePadding = ImVec2(10.0f, 6.0f);
    st.ItemSpacing = ImVec2(10.0f, 9.0f);
    st.WindowBorderSize = 0.0f;
    st.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    ImVec4 *c = st.Colors;
    const ImVec4 accent(0.96f, 0.55f, 0.13f, 1.0f), accentDim(0.96f, 0.55f, 0.13f, 0.55f);
    c[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.08f, 0.11f, 0.96f);
    c[ImGuiCol_TitleBg] = c[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.12f, 0.17f, 1.0f);
    c[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.17f, 0.23f, 1.0f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.21f, 0.24f, 0.32f, 1.0f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.29f, 0.38f, 1.0f);
    c[ImGuiCol_Button] = ImVec4(0.17f, 0.20f, 0.27f, 1.0f);
    c[ImGuiCol_ButtonHovered] = accentDim;
    c[ImGuiCol_ButtonActive] = accent;
    c[ImGuiCol_CheckMark] = c[ImGuiCol_SliderGrab] = c[ImGuiCol_SliderGrabActive] = accent;
    c[ImGuiCol_Header] = ImVec4(0.96f, 0.55f, 0.13f, 0.30f);
    c[ImGuiCol_HeaderHovered] = accentDim;
    c[ImGuiCol_HeaderActive] = accent;
    c[ImGuiCol_Tab] = ImVec4(0.13f, 0.15f, 0.20f, 1.0f);
    c[ImGuiCol_TabHovered] = accentDim;
    c[ImGuiCol_TabSelected] = ImVec4(0.96f, 0.55f, 0.13f, 0.80f);
    c[ImGuiCol_PopupBg] = ImVec4(0.09f, 0.10f, 0.14f, 0.98f);
    c[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.03f);
}

int Ui_Init(SDL_Window *window, SDL_GPUDevice *device, void *gl_context, const char *gpu_name) {
    static const char *fonts[] = { // a proportional system font if there is one; else the library's built-in font
        "/usr/share/fonts/TTF/DejaVuSans.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/noto/NotoSans-Regular.ttf", "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf", "C:\\Windows\\Fonts\\segoeui.ttf",
    };
    ImGui_ImplSDLGPU3_InitInfo info;

    snprintf(sGpuName, sizeof(sGpuName), "%s", gpu_name && gpu_name[0] ? gpu_name : "No disponible");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = NULL; // no window-layout file next to the game
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad; // (a controller opens it too: START + SELECT)
    style();
    for (size_t i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++) {
        FILE *fp = fopen(fonts[i], "rb");
        if (fp != NULL) {
            fclose(fp);
            io.Fonts->AddFontFromFileTTF(fonts[i], 18.0f);
            break;
        }
    }
    { // the fight panels' faces: the two chosen ones (sPanelFont), and the list's first where one cannot be had
        if (io.Fonts->Fonts.Size == 0) {
            io.Fonts->AddFontDefault(); // (the window's font stays the first)
        }
        sFontBold = panel_font_load(0);
        for (PanelFont &pf : sPanelFont) {
            pf.choice = MenuText_FontChoice(Port_Setting(pf.setting, 0));
            pf.font = panel_font_load(pf.choice);
        }
    }
    if (device == NULL) { // the OpenGL back end (gs_gl.c), with its context current
        sGL = 1;
        if (!ImGui_ImplSDL3_InitForOpenGL(window, (SDL_GLContext)gl_context) || !ImGui_ImplOpenGL3_Init("#version 330 core")) {
            return 0;
        }
    } else {
        if (!ImGui_ImplSDL3_InitForSDLGPU(window)) {
            return 0;
        }
        info.Device = device;
        info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(device, window);
        info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
        if (!ImGui_ImplSDLGPU3_Init(&info)) {
            return 0;
        }
    }
    ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_AutoAll, NULL, 0); // any connected controller works the window
    sDevice = device;
    sReady = true;
    load_strips(); /* the stage- and song-name overlays; the ready flags stay 0 if there is no strip */
    if (getenv("BT3_UI_OPEN") != NULL) {
        sForceTab = atoi(getenv("BT3_UI_OPEN"));
        if (sForceTab == 9) { // testing: the online screen
            sForceTab = -1;
            gPortNetMenuRequest = 1;
        }
        if (sForceTab == 8) { // testing: the exit question
            sForceTab = -1;
            sQuit = true;
            PortQuit_Request();
        }
        if (sForceTab == 10) sForceTab = 8; // test the PC roster panel
        sOpen = !sQuit;
        gPortOverlayOpen = 1;
    }
    return 1;
}

// The game gets nothing from keyboard or controller while the window or the exit question is up; and the button
// that closed either is not the game's.
static void input_owner(void) {
    int up = sOpen || sQuit;
    if (gPortOverlayOpen && !up) {
        PortInput_Swallow();
    }
    gPortOverlayOpen = up;
}

static void set_open(bool open) {
    if (open && sChat) {
        chat_close();
    }
    sOpen = open;
    sNet = false;
    sCapKind = 0;
    gPortInputCapture = 0;
    input_owner();
}

static void capture(int kind, int player, int action) {
    sCapKind = kind; sCapPlayer = player; sCapAction = action; gPortInputCapture = 1;
    sCapUntil = SDL_GetTicks() + 5000;
    sPadHold = true;
}

// A controller button pressed in the window (not while it waits for a binding, a list is open or a field is edited).
static bool pad_pressed(ImGuiKey key) {
    return sCapKind == 0 && !sPadHold && !sQuit && !ImGui::IsAnyItemActive() &&
           !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) && ImGui::IsKeyPressed(key, false);
}

extern "C" void PortA11y_Say(const char *text, int flags);
extern "C" const char *PortA11y_Label(const char *label);
static void say_quit_button(int which, int flags);

int Ui_QuitAsk(void) {
    if (!sReady) {
        return 0;
    }
    if (sQuit) { return 1; }
    if (sOpen) { set_open(false); }
    sQuit = true;
    PortQuit_Request();
    input_owner();
    PortA11y_Say(PortA11y_Label("¿Cerrar el juego? Se perderá el progreso que no hayas guardado."), 1);
    say_quit_button(1, 0);
    PortA11y_Say(PortA11y_Label("Izquierda y derecha para elegir, Intro para confirmar, Esc para volver al juego"), 0);
    return 1;
}

// The screen reader (a11y.c): it cannot see this window's question, which is drawn and not one of the game's screens,
// so the question, the button under the keyboard's focus and the keys are said here. Nothing while it is off.
static void say_quit_button(int which, int flags) {
    char text[128], pos[32];
    snprintf(pos, sizeof(pos), PortA11y_Label(", %d de %d"), which + 1, 2);
    snprintf(text, sizeof(text), "%s%s", PortA11y_Label(which == 0 ? "Salir" : "Cancelar"), pos);
    PortA11y_Say(text, flags);
}

static void build_quit(void) {
    static int said = -1; // the button last said: 0 Exit, 1 Cancel
    ImGuiIO &io = ImGui::GetIO();
    bool stay = sQuitNo;
    int focus = -1;

    sQuitNo = false;
    if (!ImGui::IsPopupOpen("Salir del juego")) {
        ImGui::OpenPopup("Salir del juego");
    }
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Salir del juego", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
        static const char *const kAsk = "¿Cerrar el juego? Se perderá el progreso que no hayas guardado.";
        // the two buttons under the middle of the question (the window is as wide as its text)
        float line = ImGui::CalcTextSize(kAsk).x, pair = 140.0f * 2.0f + ImGui::GetStyle().ItemSpacing.x;
        ImGui::TextUnformatted(kAsk);
        ImGui::Spacing();
        if (line > pair) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (line - pair) * 0.5f);
        }
        if (ImGui::Button("Salir", ImVec2(140.0f, 0.0f))) {
            exit(0);
        }
        focus = ImGui::IsItemFocused() ? 0 : focus;
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(140.0f, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) {
            stay = true;
        }
        focus = ImGui::IsItemFocused() ? 1 : focus;
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetItemDefaultFocus(); // (on Cancel: a second press of the same button does not close the game)
            PortA11y_Say(PortA11y_Label(kAsk), 1);
            say_quit_button(1, 0);
            PortA11y_Say(PortA11y_Label("Izquierda y derecha para elegir, Intro para confirmar, Esc para volver al juego"), 0);
            said = 1;
        } else if (focus >= 0 && focus != said) {
            say_quit_button(focus, 1);
            said = focus;
        }
        if (stay) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (stay) {
        sQuit = false;
        PortQuit_Dismiss();
        input_owner();
        PortA11y_Say(PortA11y_Label("Volver al juego"), 1);
    }
}

// Dragon Net Battle: the screen behind the main menu's entry. For now only its shape: what a player fills in to
// host or join. Nothing is sent anywhere yet.
extern "C" {
extern volatile int gPortNetMenuRequest;
extern volatile int gPortNetWindowClose; // gs/state.c
}

static void net_open(void) {
    set_open(true);
    sNet = true;
    Port_LobbyHelloAsk(); // (once per opening: is there a newer release, and is this one still let in)
    gPortInputCapture = 1; // the game takes no input at all while this is open (it sits on the main menu behind it)
}

// Quick match (the third tab of the online screen): what stands under the queues.
static bool sNetBusy;   // the lobby is waiting or connecting
static int sQueue = -1; // the queue chosen: 0 single, 1 team, 2..4 DP battle of 10 / 15 / 20 DP

static void quick_status(int state, int &lastState, const char *name) {
    // A search ends by itself after three minutes: a window left open does not go on asking the service for hours,
    // and the player is told that nobody was there instead of being left to wonder. One press searches again.
    static const Uint64 kSearchMs = 180000;
    static Uint64 since;
    if (state == 3 && since != 0 && SDL_GetTicks() - since > kSearchMs) {
        Port_LobbyCancel();
        lastState = -5;
        state = 0;
    }
    if (state == 3 || state == 1) {
        ImGui::TextUnformatted(state == 1 ? "Jugador encontrado. Conectando..." : "Buscando un jugador en esta cola...");
        if (state == 3 && since != 0) {
            Uint64 left = kSearchMs - (SDL_GetTicks() - since);
            ImGui::TextDisabled("Buscando durante %d:%02d más. Deja la ventana abierta.", (int)(left / 60000), (int)(left / 1000 % 60));
        }
        if (ImGui::Button("Cancelar", ImVec2(120.0f, 0.0f))) {
            Port_LobbyCancel();
            lastState = 0;
        }
        return;
    }
    if (lastState == -3) {
        ImGui::TextWrapped("%s", Port_LobbyError());
    } else if (lastState == -4) {
        ImGui::TextWrapped("Los juegos no pudieron conectarse, ni directamente ni mediante el servidor intermediario. Busca otro jugador.");
    } else if (lastState == -2) {
        ImGui::TextWrapped("El otro jugador tiene una versión diferente. Vuelve a buscar.");
    } else if (lastState == -5) {
        ImGui::TextWrapped("No se encontraron jugadores en esta cola durante los últimos tres minutos. Vuelve a buscar o elige otra cola.");
    }
    if (ImGui::Button(lastState == -5 ? "Volver a buscar" : "Buscar partida", ImVec2(200.0f, 0.0f))) {
        since = SDL_GetTicks() + 1;
        // The same for everyone in a quick match, whoever turns out to be the host: rollback of up to 8 frames and
        // the input delay measured from the connection. (The Host tab's own choices are for matches with someone
        // you can agree them with; a stranger's saved ones were not to be inherited unseen.)
        Port_NetOptions(Port_RollCan() ? 8 : 0, -2);
        Port_NetSpeedSet(Port_NetSpeedStandard()); // (every quick match: the competitive standard)
        Port_NetRulesSet(sQueue == 0 ? 0 : sQueue == 1 ? 1 : 2, sQueue >= 2 ? sQueue - 2 : 0, 4);
        Port_NetNameSet(name);
        Port_SettingTextSave("net_name", name);
        Port_SettingSave("net_queue", sQueue);
        Port_SettingsWrite();
        lastState = 0;
        Port_LobbyStartQuick(sQueue);
    }
}

static void build_net(void) {
    static char name[17] = "Jugador", address[64] = "", port[8] = "7000";
    static int tab, roll = -1, delay = 1, battle = -1, timeLimit = 3, speed = -1;
    static bool useCode = true;       // host with a room code (no port forwarding) or on a port
    static char roomCode[8] = "";     // the code to join with
    static int lastState = 0;         // what the lobby said last (for its messages after a failure)
    ImGuiIO &io = ImGui::GetIO();
    bool open = true;

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(560.0f, 0.0f), ImGuiCond_Appearing);
    if (pad_pressed(ImGuiKey_GamepadFaceRight)) { // Circle / B: out of the window, as its close button
        open = false;
    }
    if (ImGui::Begin("Batalla Dragón en línea", &open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
        {
            int min = 0, latest = 0, mine = Port_LobbyRelease(&min, &latest);
            if (min > mine) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.55f, 0.35f, 1.0f));
                ImGui::TextWrapped("Actualización disponible. Actualiza para acceder a los servidores en línea.");
                ImGui::PopStyleColor();
                ImGui::TextDisabled("Tienes 0.%d.%d; se necesita 0.%d.%d o posterior. github.com/z3xox/Tenkaichi3Decomp/releases", mine / 100, mine % 100, min / 100, min % 100);
                ImGui::Separator();
            } else if (latest > mine) {
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Actualización disponible: versión 0.%d.%d.", latest / 100, latest % 100);
                ImGui::TextDisabled("github.com/z3xox/Tenkaichi3Decomp/releases");
                ImGui::Separator();
            }
        }
        ImGui::TextWrapped("Combate contra un jugador en otra PC. Uno crea la partida y el otro se une con la dirección del anfitrión.");
        ImGui::Spacing();
        ImGui::SetNextItemWidth(260.0f);
        {
            static bool read;
            if (!read) { // the name used last time
                read = true;
                Port_SettingText("net_name", "Jugador", name, sizeof(name));
            }
        }
        ImGui::InputText("Nombre del jugador", name, sizeof(name));
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            Port_SettingTextSave("net_name", name);
            Port_SettingsWrite();
        }
        ImGui::Spacing();
        if (ImGui::BeginTabBar("net")) {
            if (ImGui::BeginTabItem("Crear partida")) {
                static const char *const kRoll[] = {"Desactivado (esperar al otro jugador)", "Hasta 2 cuadros", "Hasta 4 cuadros", "Hasta 6 cuadros", "Hasta 8 cuadros"};
                tab = 0;
                ImGui::Checkbox("Usar código de sala (sin abrir puertos)", &useCode);
                ImGui::SetItemTooltip("Obtienes un código corto para compartir con el otro jugador. Ambos juegos se encuentran mediante un\n"
                                      "servicio de búsqueda de partidas. Funciona con la mayoría de routers domésticos. Sin él, usas un puerto\n"
                                      "que debe ser accesible para el otro jugador (apertura de puertos o una red virtual).");
                ImGui::BeginDisabled(useCode);
                ImGui::SetNextItemWidth(120.0f);
                ImGui::InputText("Puerto", port, sizeof(port), ImGuiInputTextFlags_CharsDecimal);
                ImGui::EndDisabled();
                if (roll < 0) { // the choices of last time (new names: the first ones' defaults were 4 frames and delay 1)
                    roll = Port_RollCan() ? Port_Setting("net_rollback2", 8) / 2 : 0;
                    delay = Port_Setting("net_delay2", -1) + 1; // 0 = automatic, else the delay + 1
                    if (roll < 0 || roll > 4) { roll = 4; }
                    if (delay < 0 || delay > 7) { delay = 0; }
                }
                ImGui::BeginDisabled(!Port_RollCan());
                ImGui::SetNextItemWidth(260.0f);
                ImGui::Combo("Corrección predictiva (rollback)", &roll, kRoll, 5);
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    ImGui::SetTooltip("Con rollback, tus movimientos se ejecutan al instante y el juego corrige el estado cuando llegan\n"
                                      "las entradas del otro jugador. Más cuadros permiten conexiones más lentas, pero requieren\n"
                                      "más procesamiento. Desactivado: ambos juegos se esperan en cada cuadro.");
                }
                ImGui::SetNextItemWidth(260.0f);
                {
                    static const char *const kDelay[] = {"Automático (según la conexión)", "0 cuadros", "1 cuadro", "2 cuadros", "3 cuadros",
                                                         "4 cuadros", "5 cuadros", "6 cuadros"};
                    ImGui::Combo("Retardo de entrada", &delay, kDelay, 8);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Tiempo entre pulsar un botón y ejecutar el movimiento. Una conexión lenta necesita más retardo;\n"
                                      "de lo contrario, el juego predice demasiado y puede dar tirones. El ajuste automático mide la conexión\n"
                                      "al iniciar: 1 cuadro con una buena conexión y más con una lenta.");
                }
                {
                    // The match's rules: the game's own versus menu is not shown online, so they are chosen here.
                    static const char *const kBattle[] = {"Combate individual", "Combate por equipos", "Combate PD (10 PD)", "Combate PD (15 PD)", "Combate PD (20 PD)"};
                    static const char *const kTime[] = {"60 segundos", "90 segundos", "180 segundos", "240 segundos", "Sin límite"};
                    if (battle < 0) {
                        battle = Port_Setting("net_battle", 0);
                        timeLimit = Port_Setting("net_time", 3);
                        if (battle < 0 || battle > 4) { battle = 0; }
                        if (timeLimit < 0 || timeLimit > 4) { timeLimit = 3; }
                    }
                    ImGui::Spacing();
                    ImGui::SetNextItemWidth(260.0f);
                    ImGui::Combo("Combate", &battle, kBattle, 5);
                    ImGui::SetNextItemWidth(260.0f);
                    ImGui::Combo("Límite de tiempo", &timeLimit, kTime, 5);
                    {
                        if (speed < 0) {
                            speed = Port_Setting("net_speed_host", Port_NetSpeedStandard()) /* (a new name with 0.1.31: the standard went from 120 to 114) */;
                        }
                        ImGui::SetNextItemWidth(260.0f);
                        ImGui::SliderInt("Velocidad del juego", &speed, 100, 150, "%d%%", ImGuiSliderFlags_AlwaysClamp);
                        ImGui::SetItemTooltip("Velocidad del combate para ambos jugadores; el sonido conserva su tono.\n"
                                              "100%% es la velocidad original. %d%% es el estándar competitivo y la velocidad\n"
                                              "de las partidas rápidas.\n"
                                              "Con un jugador en la versión 0.1.29 solo se admite 100%% o 120%%; con versiones anteriores,\n"
                                              "solo 100%%.", Port_NetSpeedStandard());
                    }
                }
                ImGui::TextDisabled("Las opciones del anfitrión se aplican a ambos jugadores.");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Partida rápida")) {
                static const char *const kQueue[] = {"Combate individual", "Combate por equipos", "Combate PD (10 PD)", "Combate PD (15 PD)", "Combate PD (20 PD)"};
                static Uint64 asked;
                bool busy = sNetBusy; // (a search is under way: the queue is not changed under it)
                tab = 2;
                if (sQueue < 0) {
                    sQueue = Port_Setting("net_queue", 0);
                    if (sQueue < 0 || sQueue > 4) { sQueue = 0; }
                }
                {
                    // How many wait: asked when the tab is opened and then every thirty seconds, but only while the
                    // player is doing something here (a window left open all day asks nothing after two minutes),
                    // and not during a search (the search hears from the service anyway).
                    static Uint64 active;
                    bool moved = io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f || ImGui::IsAnyItemActive();
                    for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END && !moved; k++) {
                        moved = ImGui::IsKeyDown((ImGuiKey)k);
                    }
                    if (moved || active == 0) {
                        active = SDL_GetTicks() + 1;
                    }
                    if (!sNetBusy && (asked == 0 || (SDL_GetTicks() - asked > 30000 && SDL_GetTicks() - active < 120000))) {
                        asked = SDL_GetTicks() + 1;
                        Port_LobbyQueuesAsk();
                    }
                }
                ImGui::TextWrapped("Juega con alguien que busque en la misma cola. Todas las partidas rápidas son sin límite de tiempo y a %d%% de velocidad.", Port_NetSpeedStandard());
                ImGui::BeginDisabled(busy);
                for (int i = 0; i < 5; i++) {
                    char label[80];
                    int n = Port_LobbyQueueWaiting(i);
                    if (n < 0) {
                        snprintf(label, sizeof(label), "%s", kQueue[i]);
                    } else {
                        snprintf(label, sizeof(label), "%s   (%d buscando)###q%d", kQueue[i], n, i);
                    }
                    ImGui::RadioButton(label, &sQueue, i);
                }
                ImGui::EndDisabled();
                ImGui::TextDisabled("Ambos juegos se conectan directamente cuando es posible, como en otros juegos de lucha en línea.");
                ImGui::TextDisabled("Un jugador que inspeccione su tráfico de red puede ver la dirección del otro.");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Unirse")) {
                tab = 1;
                ImGui::SetNextItemWidth(120.0f);
                ImGui::InputText("Código de sala", roomCode, sizeof(roomCode), ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_CharsNoBlank);
                ImGui::SameLine();
                if (ImGui::Button("Pegar")) { // the letters and digits of what was copied, as capitals
                    char *clip = SDL_GetClipboardText();
                    size_t n = 0;
                    for (const char *c = clip; c != NULL && *c != '\0' && n + 1 < sizeof(roomCode); c++) {
                        if ((*c >= '0' && *c <= '9') || (*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z')) {
                            roomCode[n++] = (char)(*c >= 'a' ? *c - 32 : *c);
                        }
                    }
                    roomCode[n] = '\0';
                    SDL_free(clip);
                }
                ImGui::TextDisabled("O, sin código, la dirección y el puerto del anfitrión:");
                ImGui::BeginDisabled(roomCode[0] != '\0');
                ImGui::SetNextItemWidth(260.0f);
                ImGui::InputText("Dirección del anfitrión", address, sizeof(address));
                ImGui::SetNextItemWidth(120.0f);
                ImGui::InputText("Puerto", port, sizeof(port), ImGuiInputTextFlags_CharsDecimal);
                ImGui::EndDisabled();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::Spacing();
        {
            int state = Port_LobbyPoll();
            sNetBusy = state == 1 || state == 3;
            if (state == 2) {
                Port_LobbyLaunch(); // the other player is there: the game turns into the session at its next blank
                set_open(false);
                sNet = false;
            }
            bool byCode = tab == 0 ? useCode : roomCode[0] != '\0';
            if (state != 0) {
                lastState = state;
            }
            if (tab == 2) {
                quick_status(state, lastState, name);
            } else if (state == 3 || (state == 1 && byCode)) { // a room code: asking the service, then reaching the other player
                if (tab == 0 && Port_LobbyCode()[0] != '\0') {
                    ImGui::TextUnformatted("Código de sala:");
                    ImGui::SameLine();
                    ImGui::SetWindowFontScale(1.8f);
                    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "%s", Port_LobbyCode());
                    ImGui::SetWindowFontScale(1.0f);
                    {
                        static Uint64 copiedAt;
                        ImGui::SameLine();
                        if (ImGui::Button("Copiar")) {
                            copiedAt = SDL_SetClipboardText(Port_LobbyCode()) ? SDL_GetTicks() : 0;
                        }
                        ImGui::SetItemTooltip("Copia el código para compartirlo con el otro jugador.");
                        if (copiedAt != 0 && SDL_GetTicks() - copiedAt < 2000) {
                            ImGui::SameLine();
                            ImGui::TextDisabled("Copiado");
                        }
                    }
                    ImGui::TextUnformatted(state == 3 ? "Comparte este código con el otro jugador. Esperando a que se una..." : "El otro jugador se ha unido. Conectando...");
                } else {
                    ImGui::TextUnformatted(state == 1 ? "Sala encontrada. Conectando con el anfitrión..." : tab == 0 ? "Obteniendo un código de sala..." : "Buscando la sala...");
                }
                if (ImGui::Button("Cancelar", ImVec2(120.0f, 0.0f))) {
                    Port_LobbyCancel();
                    lastState = 0;
                }
            } else if (state == 1) {
                if (tab == 0) {
                    ImGui::Text("Esperando al otro jugador en el puerto %s...", port);
                    if (Port_LobbyOther()) {
                        ImGui::TextWrapped("Un jugador con otra versión intenta unirse. Ambos necesitan la misma versión.");
                    }
                } else {
                    ImGui::Text("Buscando %s...", address);
                }
                if (ImGui::Button("Cancelar", ImVec2(120.0f, 0.0f))) {
                    Port_LobbyCancel();
                }
            } else {
                if (lastState == -3) {
                    ImGui::TextWrapped("%s", Port_LobbyError());
                } else if (lastState == -4) {
                    ImGui::TextWrapped("Los juegos no pudieron conectarse, ni directamente ni mediante el servidor intermediario. "
                                       "Puedes crear la partida abriendo un puerto o usando una red virtual como Tailscale.");
                }
                bool can = byCode ? (tab == 0 || strlen(roomCode) == 6) : atoi(port) > 0 && (tab == 0 || address[0] != '\0');
                ImGui::BeginDisabled(!can);
                if (ImGui::Button(tab == 0 ? "Crear partida" : "Unirse a la partida", ImVec2(200.0f, 0.0f))) {
                    if (tab == 0 && roll >= 0) {
                        Port_NetOptions(roll * 2, delay == 0 ? -2 : delay - 1);
                        if (battle >= 0) {
                            Port_NetSpeedSet(speed);
                            Port_SettingSave("net_speed_host", speed);
                            Port_NetRulesSet(battle == 0 ? 0 : battle == 1 ? 1 : 2, battle >= 2 ? battle - 2 : 0, timeLimit);
                            Port_SettingSave("net_battle", battle);
                            Port_SettingSave("net_time", timeLimit);
                        }
                        Port_SettingSave("net_rollback2", roll * 2);
                        Port_SettingSave("net_delay2", delay - 1);
                        Port_SettingsWrite();
                    } else {
                        Port_NetOptions(-1, -1); // joining: the host's choices arrive with its answer
                    }
                    Port_NetNameSet(name);
                    Port_SettingTextSave("net_name", name);
                    Port_SettingsWrite();
                    lastState = 0;
                    if (byCode) {
                        Port_LobbyStartCode(tab == 0, roomCode);
                    } else {
                        Port_LobbyStart(tab == 0, address, atoi(port));
                    }
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Volver", ImVec2(120.0f, 0.0f))) {
                    open = false;
                }
                if (state == -2) {
                    ImGui::TextWrapped("El anfitrión tiene otra versión. Ambos jugadores necesitan la misma versión.");
                } else if (state < 0) {
                    ImGui::TextWrapped(tab == 0 ? "No se puede usar ese puerto. Es posible que otro programa lo esté usando." : "No se reconoce esa dirección.");
                }
            }
        }
        ImGui::Spacing();
        ImGui::TextDisabled("Al conectarse, el juego inicia la partida: ambos jugadores reciben la misma plantilla,");
        ImGui::TextDisabled("con todo desbloqueado. Tu guardado se conserva. Al salir de la partida, vuelves");
        ImGui::TextDisabled("al punto donde estabas. El anfitrión también controla los menús.");
    }
    ImGui::End();
    if (!open) {
        Port_LobbyCancel();
        set_open(false);
    }
}

void Ui_Toggle(void) {
    if (sReady && !sQuit) {
        set_open(!sOpen);
    }
}

static void bind(int value) {
    if (sCapAction == PORT_ACTIONS) { // the row "Open this window"
        if (sCapKind == 1) {
            *PortInput_OverlayKey() = value > 0 ? value : SDL_SCANCODE_F1; // (cleared = F1 again: a keyboard always has a way in)
        } else if (value < PORT_PAD_LT) { // (a button, not a trigger)
            *PortInput_OverlayPad() = value;
        }
    } else if (sCapAction == PORT_ACTIONS + 1) { // the row "Open online chat"
        if (sCapKind == 1) {
            *PortInput_ChatKey() = value > 0 ? value : SDL_SCANCODE_TAB;
        }
    } else if (sCapKind == 1) {
        PortInput_Keys(sCapPlayer)[sCapAction] = value;
    } else {
        PortInput_PadButtons(sCapPlayer)[sCapAction] = value;
    }
    PortInput_Save();
}

static bool chat_can_open(void) {
    return sReady && !sOpen && !sQuit && PortChat_Enabled() && Port_NetActive();
}

static void chat_open(void) {
    sChat = true;
    sChatFocus = true;
    sChatKeyHeld = true;
    sChatSoon = false;
    gPortChatTyping = 1;
    ImGui::GetIO().ClearInputKeys(); // (it saw none of the keys while the line was put away: no key is down for it)
}

static void chat_close(void) {
    sChat = false;
    gPortChatTyping = 0;
    PortInput_SwallowKeys(); // (Enter, which sent the line, is START: not the game's until it is let go)
}

// The chat while a line is typed: the keyboard is the line's; Esc puts it away. 1 = the event was used.
static int chat_event(const SDL_Event *ev) {
    int key = *PortInput_ChatKey();
    if (ev->type == SDL_EVENT_KEY_DOWN && ev->key.scancode == SDL_SCANCODE_ESCAPE) {
        chat_close();
        return 1;
    }
    if (ev->type == SDL_EVENT_KEY_DOWN && !sChatKeyHeld && !ev->key.repeat && key > 0 && (int)ev->key.scancode == key) {
        chat_close();
        return 1;
    }
    if (ev->type == SDL_EVENT_KEY_UP && (int)ev->key.scancode == key) {
        sChatKeyHeld = false;
    }
    if (ev->type == SDL_EVENT_TEXT_INPUT && sChatKeyHeld) { // the letter of the key that opened the line
        return 1;
    }
    if (ev->type == SDL_EVENT_KEY_DOWN || ev->type == SDL_EVENT_KEY_UP || ev->type == SDL_EVENT_TEXT_INPUT || ev->type == SDL_EVENT_TEXT_EDITING ||
        ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev->type == SDL_EVENT_MOUSE_BUTTON_UP || ev->type == SDL_EVENT_MOUSE_WHEEL ||
        ev->type == SDL_EVENT_MOUSE_MOTION) {
        ImGui_ImplSDL3_ProcessEvent(ev);
    }
    return ev->type == SDL_EVENT_KEY_DOWN || ev->type == SDL_EVENT_KEY_UP || ev->type == SDL_EVENT_TEXT_INPUT ||
           ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev->type == SDL_EVENT_MOUSE_BUTTON_UP || ev->type == SDL_EVENT_MOUSE_WHEEL;
}

int Ui_Event(const SDL_Event *ev) {
    if (!sReady) {
        return 0;
    }
    if (ev->type == SDL_EVENT_GAMEPAD_ADDED || ev->type == SDL_EVENT_GAMEPAD_REMOVED) { // (also while it is closed: its list of controllers)
        ImGui_ImplSDL3_ProcessEvent(ev);
        return 0;
    }
    if (!sOpen && !sQuit && !sChat && PortInput_TboEvent(ev)) { // typed on a keypad of Tenkaichi Budokai Online
        return 1;
    }
    if (sQuit && PortQuit_Active()) { // game dialog: keyboard and controller operate its native Yes / No plates
        if (ev->type == SDL_EVENT_KEY_DOWN && !ev->key.repeat) {
            SDL_Scancode key = ev->key.scancode;
            if (key == SDL_SCANCODE_ESCAPE) { PortQuit_Action(PORT_QUIT_CANCEL); }
            else if (key == SDL_SCANCODE_RETURN || key == SDL_SCANCODE_KP_ENTER || key == SDL_SCANCODE_SPACE) {
                PortQuit_Action(PORT_QUIT_CONFIRM);
            } else if (key == SDL_SCANCODE_LEFT || key == SDL_SCANCODE_RIGHT ||
                       key == SDL_SCANCODE_UP || key == SDL_SCANCODE_DOWN) {
                if (PortQuit_Ready()) { say_quit_button(1 - PortQuit_Cursor(), 1); }
                PortQuit_Action(PORT_QUIT_MOVE);
            }
        } else if (ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            SDL_GamepadButton button = (SDL_GamepadButton)ev->gbutton.button;
            if (button == SDL_GAMEPAD_BUTTON_SOUTH) { PortQuit_Action(PORT_QUIT_CONFIRM); }
            else if (button == SDL_GAMEPAD_BUTTON_EAST) { PortQuit_Action(PORT_QUIT_CANCEL); }
            else if (button == SDL_GAMEPAD_BUTTON_DPAD_LEFT || button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT ||
                     button == SDL_GAMEPAD_BUTTON_DPAD_UP || button == SDL_GAMEPAD_BUTTON_DPAD_DOWN) {
                if (PortQuit_Ready()) { say_quit_button(1 - PortQuit_Cursor(), 1); }
                PortQuit_Action(PORT_QUIT_MOVE);
            }
        }
        return 1;
    }
    if (sChat && !sOpen && !sQuit) {
        return chat_event(ev);
    }
    if (ev->type == SDL_EVENT_KEY_DOWN && !ev->key.repeat && *PortInput_ChatKey() > 0 && (int)ev->key.scancode == *PortInput_ChatKey() &&
        chat_can_open()) {
        chat_open();
        return 1;
    }
    if (sQuit) { // fallback when the native dialog resource cannot be loaded
        if (ev->type == SDL_EVENT_KEY_DOWN && ev->key.scancode == SDL_SCANCODE_ESCAPE) {
            sQuitNo = sQuitNo || !ev->key.repeat; // (taken away where it is drawn)
            return 1;
        }
        ImGui_ImplSDL3_ProcessEvent(ev);
        return ev->type == SDL_EVENT_KEY_DOWN || ev->type == SDL_EVENT_KEY_UP || ev->type == SDL_EVENT_TEXT_INPUT ||
               ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev->type == SDL_EVENT_MOUSE_BUTTON_UP || ev->type == SDL_EVENT_MOUSE_WHEEL;
    }
    if (!sOpen) {
        return 0;
    }
    if (sCapKind != 0) { // the next key or button is the binding; Esc leaves it as it was, Delete clears it
        bool done = false;
        if (ev->type == SDL_EVENT_KEY_DOWN) {
            if (ev->key.scancode == SDL_SCANCODE_DELETE || ev->key.scancode == SDL_SCANCODE_BACKSPACE) {
                bind(sCapKind == 1 ? 0 : -1);
            } else if (ev->key.scancode != SDL_SCANCODE_ESCAPE && sCapKind == 1) {
                bind((int)ev->key.scancode);
            }
            done = ev->key.scancode == SDL_SCANCODE_ESCAPE || ev->key.scancode == SDL_SCANCODE_DELETE ||
                   ev->key.scancode == SDL_SCANCODE_BACKSPACE || sCapKind == 1;
        } else if (sCapKind == 2 && ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            bind((int)ev->gbutton.button);
            done = true;
        } else if (sCapKind == 2 && ev->type == SDL_EVENT_GAMEPAD_AXIS_MOTION && ev->gaxis.value > 20000 &&
                   (ev->gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || ev->gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)) {
            bind(ev->gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? PORT_PAD_LT : PORT_PAD_RT);
            done = true;
        }
        if (done) {
            sCapKind = 0;
            gPortInputCapture = 0;
        }
        if (ev->type == SDL_EVENT_KEY_DOWN || ev->type == SDL_EVENT_KEY_UP) {
            return 1;
        }
    } else if (ev->type == SDL_EVENT_KEY_DOWN && ev->key.scancode == SDL_SCANCODE_ESCAPE) {
        set_open(false);
        return 1;
    }
    ImGui_ImplSDL3_ProcessEvent(ev);
    return ev->type == SDL_EVENT_KEY_DOWN || ev->type == SDL_EVENT_KEY_UP || ev->type == SDL_EVENT_TEXT_INPUT ||
           ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev->type == SDL_EVENT_MOUSE_BUTTON_UP || ev->type == SDL_EVENT_MOUSE_WHEEL;
}

// SDL supplies English key names independently of the display language.
static const char *key_caption(SDL_Scancode key) {
    switch (key) {
    case SDL_SCANCODE_UP: return "Arriba";
    case SDL_SCANCODE_DOWN: return "Abajo";
    case SDL_SCANCODE_LEFT: return "Izquierda";
    case SDL_SCANCODE_RIGHT: return "Derecha";
    case SDL_SCANCODE_SPACE: return "Espacio";
    case SDL_SCANCODE_RETURN: return "Intro";
    case SDL_SCANCODE_KP_ENTER: return "Intro numérico";
    case SDL_SCANCODE_BACKSPACE: return "Retroceso";
    case SDL_SCANCODE_DELETE: return "Supr";
    case SDL_SCANCODE_INSERT: return "Insertar";
    case SDL_SCANCODE_HOME: return "Inicio";
    case SDL_SCANCODE_END: return "Fin";
    case SDL_SCANCODE_PAGEUP: return "Re Pág";
    case SDL_SCANCODE_PAGEDOWN: return "Av Pág";
    case SDL_SCANCODE_LSHIFT: return "Mayús izquierda";
    case SDL_SCANCODE_RSHIFT: return "Mayús derecha";
    case SDL_SCANCODE_LCTRL: return "Ctrl izquierdo";
    case SDL_SCANCODE_RCTRL: return "Ctrl derecho";
    case SDL_SCANCODE_LALT: return "Alt izquierdo";
    case SDL_SCANCODE_RALT: return "Alt derecho";
    case SDL_SCANCODE_CAPSLOCK: return "Bloq Mayús";
    case SDL_SCANCODE_NUMLOCKCLEAR: return "Bloq Num";
    case SDL_SCANCODE_PRINTSCREEN: return "Impr Pant";
    case SDL_SCANCODE_SCROLLLOCK: return "Bloq Despl";
    case SDL_SCANCODE_PAUSE: return "Pausa";
    default: return SDL_GetScancodeName(key);
    }
}

static const char *pad_source_name(int src) {
    static const char *names[] = {"A / Cruz", "B / Círculo", "X / Cuadrado", "Y / Triángulo", "Atrás / Select", "Guía", "Inicio",
                                  "Pulsar palanca izquierda", "Pulsar palanca derecha", "Botón superior izquierdo", "Botón superior derecho", "Cruceta arriba",
                                  "Cruceta abajo", "Cruceta izquierda", "Cruceta derecha", "Adicional 1", "Palanca trasera derecha 1", "Palanca trasera izquierda 1",
                                  "Palanca trasera derecha 2", "Palanca trasera izquierda 2", "Panel táctil"};
    static char other[24];
    if (src == PORT_PAD_LT) { return "Gatillo izquierdo"; }
    if (src == PORT_PAD_RT) { return "Gatillo derecho"; }
    if (src < 0) { return "-"; }
    if (src < (int)(sizeof(names) / sizeof(names[0]))) { return names[src]; }
    snprintf(other, sizeof(other), "Botón %d", src);
    return other;
}

static bool tab(const char *label, int index) {
    ImGuiTabItemFlags flags = sForceTab == index ? ImGuiTabItemFlags_SetSelected : 0;
    bool on = ImGui::BeginTabItem(label, NULL, flags);
    if (on && sForceTab < 0) {
        sTab = index;
    }
    return on;
}

static void video_tab(PortVideo &v) {
    static const struct { const char *name; int milli; } aspects[] = {{"4:3 (original)", 1333}, {"16:10", 1600}, {"16:9", 1778}, {"21:9", 2389}, {"32:9", 3556}};
    char label[64];
    int best = 0, count = 0;

    snprintf(label, sizeof(label), "%dx  (%d x %d)", v.scale, 512 * v.scale, 448 * v.scale);
    if (ImGui::BeginCombo("Resolución interna", label)) {
        for (int n = 1; n <= 8; n++) {
            snprintf(label, sizeof(label), "%dx  (%d x %d)", n, 512 * n, 448 * n);
            if (ImGui::Selectable(label, n == v.scale)) { v.scale = n; }
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("Nivel de detalle del renderizado. 1x es la resolución original de PlayStation 2.\nUna resolución mayor mejora la nitidez y consume más memoria de video.");
    for (int i = 1; i < (int)(sizeof(aspects) / sizeof(aspects[0])); i++) {
        if (abs(aspects[i].milli - v.aspectMilli) < abs(aspects[best].milli - v.aspectMilli)) { best = i; }
    }
    if (ImGui::BeginCombo("Relación de aspecto", aspects[best].name)) {
        for (int i = 0; i < (int)(sizeof(aspects) / sizeof(aspects[0])); i++) {
            if (ImGui::Selectable(aspects[i].name, i == best)) { v.aspectMilli = aspects[i].milli; }
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("Una relación más ancha que 4:3 muestra más combate a los lados. Los menús se estiran.");
    if (ImGui::BeginCombo("Filtrado 2D", v.smooth2d ? "Suave" : "Nítido (consola)")) {
        if (ImGui::Selectable("Nítido (consola)", !v.smooth2d)) { v.smooth2d = 0; }
        if (ImGui::Selectable("Suave", v.smooth2d != 0)) { v.smooth2d = 1; }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("Cómo se dibujan el HUD, los menús y el texto con una resolución interna superior a 1x.\n"
                          "Nítido: conserva los píxeles de PlayStation 2. Suave: aplica un filtrado similar al de un emulador.\n"
                          "Sin diferencias a 1x.");
    bool full = v.fullscreen != 0;
    if (ImGui::Checkbox("Pantalla completa", &full)) { v.fullscreen = full; }
    ImGui::SameLine();
    ImGui::TextDisabled("(F11)");
    {
        bool vs = v.vsync != 0, fs = v.frameSkip != 0;
        if (ImGui::Checkbox("Sincronización vertical", &vs)) { v.vsync = vs; }
        ImGui::SetItemTooltip("Cada imagen espera la actualización de la pantalla. Actívalo si observas cortes en la imagen.\n"
                              "Desactivado por defecto, ya que añade algo de retardo.");
        if (ImGui::Checkbox("Omitir imágenes para mantener la velocidad", &fs)) { v.frameSkip = fs; }
        ImGui::SetItemTooltip("Si la PC no puede mostrar una imagen a tiempo, el juego normalmente se ralentiza.\n"
                              "Con esta opción mantiene la velocidad omitiendo imágenes (menos fluidez,\n"
                              "sin ralentizarse). Ayuda cuando la GPU limita el rendimiento. Activado por defecto.");
        {
            bool late = Port_LateInput() != 0;
            if (ImGui::Checkbox("Baja latencia de entrada (experimental)", &late)) { Port_LateInputSet(late); }
            ImGui::SetItemTooltip("El juego lee el mando, calcula y espera antes de mostrar la imagen:\n"
                                  "unos 33 ms en combate. Esta opción mueve la espera al inicio, para que la imagen responda\n"
                                  "a tu pulsación en los pocos milisegundos que necesita la PC. La lógica del juego se conserva.\n"
                                  "No se usa en línea ni con interpolación. Desactivado por defecto: si la PC no puede\n"
                                  "mantener el ritmo, puede producir tirones que no ocurren con el orden original.");
        }
    }
    SDL_DisplayID *displays = SDL_GetDisplays(&count);
    if (v.display > count) { v.display = 0; }
    snprintf(label, sizeof(label), "%s", v.display == 0 ? "Elegida por el escritorio" : SDL_GetDisplayName(displays[v.display - 1]));
    if (ImGui::BeginCombo("Pantalla", label)) {
        if (ImGui::Selectable("Elegida por el escritorio", v.display == 0)) { v.display = 0; }
        for (int i = 0; i < count; i++) {
            snprintf(label, sizeof(label), "%d: %s", i + 1, SDL_GetDisplayName(displays[i]));
            if (ImGui::Selectable(label, v.display == i + 1)) { v.display = i + 1; }
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("Se aplica al reiniciar el juego.");
    SDL_free(displays);
    {   // which renderer draws the game: Vulkan (the default) or OpenGL 3.3, for machines without a working Vulkan
        static const char *const kApi[] = {"Vulkan", "OpenGL"};
        static int api = -1;
        if (api < 0) {
            api = Port_Setting("gpu_api", 0) == 1 ? 1 : 0;
        }
        if (ImGui::Combo("Renderizador", &api, kApi, 2)) {
            Port_SettingSave("gpu_api", api);
            Port_SettingsWrite();
        }
        ImGui::SetItemTooltip("Se aplica al reiniciar. Ambos dibujan la misma imagen; Vulkan es más rápido.\n"
                              "Si el elegido no puede iniciarse, el juego prueba el otro.");
        if (api != (sGL ? 1 : 0)) {
            ImGui::TextDisabled("En uso: %s. Se usará %s al reiniciar.", kApi[sGL ? 1 : 0], kApi[api]);
        }
        ImGui::TextWrapped("GPU activa: %s", sGpuName);
    }
    {
        // In-between pictures (gs_draw.c): the fight is computed 30 times a second, as on the console; with this
        // on one more picture is drawn between each two. The game itself, its replays and online play are the same.
        static const char *kSmooth[] = { "Desactivado (30, como la consola)", "60 imágenes por segundo", "120 imágenes por segundo", "240 imágenes por segundo" };
        int smooth = gsInterp < 0 ? 0 : gsInterp > 3 ? 3 : gsInterp;
        ImGui::SetNextItemWidth(416.0f);
        if (ImGui::Combo("Interpolación de movimiento", &smooth, kSmooth, 4)) {
            gsInterp = smooth;
            Port_SettingSave("interp", gsInterp);
            Port_SettingsWrite();
        }
        ImGui::SetItemTooltip("Experimental. El juego calcula el combate 30 veces por segundo. Esta opción dibuja imágenes\n"
                              "intermedias. La lógica del combate y de las partidas en línea se conserva.\n"
                              "La imagen más reciente llega después: 17 ms a 60, 25 ms a 120 y 29 ms a 240.\n"
                              "Elige una frecuencia compatible con tu pantalla. F9 lo activa y desactiva.");
    }
    {
        bool on = meter_on();
        if (ImGui::Checkbox("Mostrar FPS y datos de conexión en partidas en línea", &on)) {
            sMeter = on;
            Port_SettingSave("meter", sMeter);
            Port_SettingsWrite();
        }
    }
    ImGui::Spacing();
    if (v.texPackCount > 0) {
        bool on = v.texPack != 0;
        snprintf(label, sizeof(label), "Paquete de texturas (%d texturas)", v.texPackCount);
        if (ImGui::Checkbox(label, &on)) { v.texPack = on; }
        ImGui::SetItemTooltip("Texturas de reemplazo de la carpeta \"textures\" junto al juego.");
    } else {
        ImGui::BeginDisabled();
        bool off = false;
        ImGui::Checkbox("Paquete de texturas (ninguno encontrado)", &off);
        ImGui::EndDisabled();
        ImGui::SetItemTooltip("Coloca los archivos del paquete (nombres PCSX2, .dds o .png) en la carpeta \"textures\" junto al juego.");
    }
}

static void effects_tab(PortVideo &v) {
    static const struct { const char *name, *tip; } fx[6] = {
        {"Contorno", "La línea negra que rodea a los luchadores."},
        {"Silueta tras obstáculos", "Color que permite ver al luchador detrás del escenario."},
        {"Niebla de profundidad", "La neblina que colorea los elementos lejanos."},
        {"Destellos y resplandor", "Resplandor de los objetos brillantes y destellos del cielo."},
        {"Desenfoque de distancia", "Desenfoque de las zonas lejanas del escenario."},
        {"Desenfoque de movimiento", "Desenfoque del escenario al girar o mover rápido la cámara."},
    };
    for (int i = 0; i < 6; i++) {
        bool on = !((v.fxOff >> i) & 1);
        if (ImGui::Checkbox(fx[i].name, &on)) { v.fxOff = (v.fxOff & ~(1 << i)) | (on ? 0 : 1 << i); }
        ImGui::SetItemTooltip("%s", fx[i].tip);
    }
    ImGui::Spacing();
    ImGui::SliderInt("Intensidad del resplandor", &v.glow, 0, 200, "%d%%", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetItemTooltip("100%% es la intensidad original de PlayStation 2; aquí se usa 60%% por defecto.");
}

// ---- "an update is available" ----
// Once per run the newest published release's number is asked of GitHub (net_match.c), unless "Check for updates" is
// off. If it is newer than this program: a line over the picture for ten seconds, and a line with a button to the
// release page at the top of this window for the rest of the run. Nothing is downloaded by the game.
// Not when nobody is looking (the offscreen and dummy video drivers); BT3_UPDATE_CHECK=1 / 0 says otherwise, and
// BT3_UPDATE_LATEST=<number> is taken as the answer without asking (testing).
static int sUpdCheck = -1; // the setting; -1 not read yet
static bool sUpdAsked, sUpdTold;

static int update_latest(void) {
    const char *fake = SDL_getenv("BT3_UPDATE_LATEST");
    if (sUpdCheck < 0) {
        sUpdCheck = Port_Setting("update_check", 1) != 0;
    }
    if (fake != NULL) {
        return SDL_atoi(fake);
    }
    if (!sUpdAsked) {
        const char *drv = SDL_GetCurrentVideoDriver(), *env = SDL_getenv("BT3_UPDATE_CHECK");
        bool seen = drv == NULL || (SDL_strcmp(drv, "offscreen") != 0 && SDL_strcmp(drv, "dummy") != 0);
        sUpdAsked = true;
        if (env != NULL ? SDL_atoi(env) != 0 : (sUpdCheck != 0 && seen)) {
            Match_UpdateAsk();
        }
    }
    return sUpdCheck != 0 || SDL_getenv("BT3_UPDATE_CHECK") != NULL ? Match_UpdateLatest() : 0;
}

static void misc_tab(void) {
    {
        bool chk = sUpdCheck != 0;
        if (ImGui::Checkbox("Buscar actualizaciones", &chk)) {
            sUpdCheck = chk;
            Port_SettingSave("update_check", sUpdCheck);
            Port_SettingsWrite();
        }
        ImGui::SetItemTooltip("Consulta una vez al iniciar si hay una versión más reciente en GitHub. Si existe, muestra un aviso\n"
                              "y un botón para abrir su página. No descarga archivos automáticamente.");
    }
    // the start of the game (PR #50): read by FirstRun_Main, so they count from the next start
    static int sSkipLogos = -1, sSkipIntro = -1, sSkipCopy = -1;
    if (sSkipLogos < 0) {
        sSkipLogos = Port_Setting("skip_logos", 0) != 0;
        sSkipIntro = Port_Setting("skip_intro", 0) != 0;
        sSkipCopy = Port_Setting("skip_copyright", 0) != 0;
    }
    bool logos = sSkipLogos != 0, intro = sSkipIntro != 0, copy = sSkipCopy != 0;
    ImGui::Spacing();
    if (ImGui::Checkbox("Omitir logotipos", &logos)) {
        sSkipLogos = logos;
        Port_SettingSave("skip_logos", sSkipLogos);
        Port_SettingsWrite();
    }
    ImGui::SetItemTooltip("Omite los logotipos del distribuidor y del desarrollador. Se aplica al reiniciar.");
    if (ImGui::Checkbox("Omitir video de introducción", &intro)) {
        sSkipIntro = intro;
        Port_SettingSave("skip_intro", sSkipIntro);
        Port_SettingsWrite();
    }
    ImGui::SetItemTooltip("Inicia sin el video de introducción. Se aplica al reiniciar.");
    if (ImGui::Checkbox("Omitir pantalla de derechos de autor", &copy)) {
        sSkipCopy = copy;
        Port_SettingSave("skip_copyright", sSkipCopy);
        Port_SettingsWrite();
    }
    ImGui::SetItemTooltip("Omite el texto legal que aparece cinco segundos antes de la pantalla del título.");
    ImGui::Spacing();
    bool chat = PortChat_Enabled() != 0;
    if (ImGui::Checkbox("Chat en línea", &chat)) {
        PortChat_EnabledSet(chat);
    }
    ImGui::SetItemTooltip("Chatea con el otro jugador. Tab abre el campo (ajuste key_chat), Intro envía el mensaje\n"
                          "y Esc lo cierra. Mientras escribes, el teclado no controla el juego; el mando sí. Desactivado: no se\n"
                          "muestran ni envían mensajes. Un jugador que lo tenga desactivado no verá el chat.");
    if (chat) {
        if (sChatSize < 0) {
            sChatSize = Port_Setting("chat_size", 1);
            if (sChatSize < 0 || sChatSize > 2) sChatSize = 1;
        }
        static const char *const kChatSizes[] = {"Compacto", "Normal", "Grande"};
        int size = sChatSize;
        if (ImGui::Combo("Tamaño del chat", &size, kChatSizes, 3)) {
            sChatSize = size;
            Port_SettingSave("chat_size", sChatSize);
            Port_SettingsWrite();
        }
        ImGui::SetItemTooltip("Ancho del chat y cantidad de líneas visibles (al escribir / el resto del tiempo).\n"
                              "Compacto: 4 / 2, más estrecho.\n"
                              "Normal: 6 / 4.\n"
                              "Grande: 8 / 6, más ancho, para pantallas grandes.");
    }
}

static void ui_tab(PortVideo &v) {
    if (ImGui::BeginCombo("Fuente de los menús", MenuText_FontName(v.menuFont))) {
        for (int i = 0; i < MenuText_FontCount(); i++) {
            if (!MenuText_FontAvailable(i, MENU_TEXT_MENU)) { continue; }
            ImGui::PushID(i);
            if (ImGui::Selectable(MenuText_FontName(i), v.menuFont == i)) { v.menuFont = i; }
            if (v.menuFont == i) { ImGui::SetItemDefaultFocus(); }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("Original usa los gráficos del juego. Otras fuentes reemplazan los textos compatibles al instante.\nLos anuncios de combate tienen su propio ajuste más abajo.");
    if (ImGui::BeginCombo("Fuente del combate", MenuText_FontName(v.battleFont))) {
        for (int i = 0; i < MenuText_FontCount(); i++) {
            if (!MenuText_FontAvailable(i, MENU_TEXT_BATTLE)) { continue; }
            ImGui::PushID(i);
            if (ImGui::Selectable(MenuText_FontName(i), v.battleFont == i)) { v.battleFont = i; }
            if (v.battleFont == i) { ImGui::SetItemDefaultFocus(); }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("Elige los gráficos originales para los anuncios y el HUD, o una fuente de combate de fonts.tsv.");
    if (ImGui::BeginCombo("Fuente de diálogos e interfaz", MenuText_FontName(v.textFont))) {
        for (int i = 0; i < MenuText_FontCount(); i++) {
            if (!MenuText_FontAvailable(i, MENU_TEXT_DIALOGUE)) { continue; }
            ImGui::PushID(i);
            if (ImGui::Selectable(MenuText_FontName(i), v.textFont == i)) { v.textFont = i; }
            if (v.textFont == i) { ImGui::SetItemDefaultFocus(); }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("Reemplaza la fuente compartida de diálogos, subtítulos y textos de interfaz.\n"
                          "Original conserva los caracteres del juego. Se mantienen las posiciones, colores, sombras e iconos.");
    static const char *const kHud[] = {"En los bordes de la pantalla", "En los bordes, barras de salud largas", "En el centro, como en 4:3"};
    ImGui::Combo("Diseño del HUD", &v.hudLayout, kHud, 3);
    ImGui::SetItemTooltip("Posición de las barras de salud e indicadores en pantallas más anchas que 4:3.\n"
                          "Barras largas: llegan desde los bordes hasta el reloj.\n"
                          "En el centro: disposición original 4:3, junto al reloj.");
    {
        bool m43 = Port_Menus43() != 0;
        if (ImGui::Checkbox("Menús en 4:3", &m43)) {
            Port_Menus43Set(m43);
            Port_SettingSave("menus_43", m43);
            Port_SettingsWrite();
        }
        ImGui::SetItemTooltip("En pantallas más anchas que 4:3, muestra los menús con sus proporciones originales y bandas negras\n"
                              "a los lados. Los combates conservan la imagen panorámica.");
    }
    {
        static const char *const kBars[] = {"Negro", "Borde", "Paneles de luchadores"};
        int b = sBars < 0 ? 0 : sBars;
        if (ImGui::Combo("Bandas laterales", &b, kBars, 3)) {
            sBars = b;
            Port_SettingSave("side_bars", sBars);
            Port_SettingsWrite();
        }
        ImGui::SetItemTooltip("Contenido de los laterales cuando la imagen es más estrecha que la ventana:\n"
                              "una imagen 4:3 o los menús con \"Menús en 4:3\", dentro de una ventana panorámica.\n"
                              "Paneles de luchadores: borde y, en combate, un retrato con el nombre y la forma de cada luchador.\n"
                              "Personalizados: carpeta \"bars\" junto al juego, con trim.png (franja junto a la imagen,\n"
                              "conservando su proporción) y fill.png (estirado en el resto). Se carga al iniciar.");
    }
    if (sBars == 2) { // the fighter panels' faces
        font_picker("Fuente del nombre", sPanelFont[0], "Fuente del nombre bajo el retrato. Usa las mismas fuentes que los menús.\n"
                                                "Para añadir una fuente, regístrala en fonts/fonts.tsv. Por defecto: la primera de la lista.");
        font_picker("Fuente de la transformación", sPanelFont[1], "Fuente de la transformación bajo el retrato. Usa las mismas fuentes que los menús.\n"
                                                "Por defecto: la primera de la lista.");
    }
}

extern "C" int gPortMusicFollows; // snd_adx.c: 1 = looping music is sped up or slowed down with the game

static void audio_tab(PortVideo &v) {
    ImGui::SliderInt("Música", &v.music, 0, 200, "%d%%", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderInt("Efectos de sonido", &v.effects, 0, 200, "%d%%", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetItemTooltip("Golpes, ráfagas y sonidos de menús. Se aplica al siguiente sonido que se reproduzca.");
    ImGui::SliderInt("Voces", &v.voices, 0, 200, "%d%%", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetItemTooltip("Voces de los luchadores en combate y diálogos fuera de él.");
    ImGui::SliderInt("Sonidos del escenario", &v.stage, 0, 200, "%d%%", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetItemTooltip("Sonido ambiente del escenario (viento, agua) y sus efectos.");
    {
        bool follows = gPortMusicFollows > 0;
        ImGui::Spacing();
        if (ImGui::Checkbox("Adaptar música a la velocidad del juego", &follows)) {
            gPortMusicFollows = follows;
            Port_SettingSave("music_follows_speed", follows);
        }
        ImGui::SetItemTooltip("Cuando el juego usa una velocidad distinta de 100%% (partidas en línea o pestaña Trucos):\n"
                              "desactivado, la música suena a su velocidad normal; activado, sigue la velocidad del juego,\n"
                              "conservando su tono, como en un emulador. Las voces siempre siguen al juego.");
    }
}

// Cheats. "Unlock everything" is the game's own leftover debug function (Save_UnlockAll, which nothing in the game
// calls): the request is set here and carried out on the game's side at the next vertical blank (headless.c).
extern "C" {
extern volatile int gPortUnlockAll, gPortUnlockDone;
}

extern "C" {
#include "../../include/plat_charas.h"
int Port_NetSession(void);
}

// Character packages (the `characters` folder next to the game; docs/port/character_packages.md): what is
// installed, what each folder does, and what was found wrong with it.
// Translate loader diagnostics at the UI boundary, keeping logs and package parsing unchanged.
static const char *package_issue_es(const char *text) {
    static char out[2048];
    static const struct { const char *en, *es; } parts[] = {
        {"base has no character id from 0 to 160; 0 is used", "base no contiene un ID de 0 a 160; se usa 0"},
        {"replaces has no character id from 0 to 160", "replaces no contiene un ID de 0 a 160"},
        {"made for package format ", "creado para el formato de paquete "},
        {"; this game knows format ", "; el juego admite el formato "},
        {" and reads what it knows", " y lee los campos compatibles"},
        {" is not a number from ", " no es un número entre "},
        {" to ", " y "},
        {"; the character's own is used", "; se usa el valor original del personaje"},
        {" cannot be read; that file is left out", " no se puede leer; se omite ese archivo"},
        {" is empty; that file is left out", " está vacío; se omite ese archivo"},
        {" is ", " tiene "},
        {" bytes, more than the ", " bytes, por encima del límite de "},
        {"1.5 MB a model may have; the character is left out", "1,5 MB por modelo; se omite el personaje"},
        {" the game has room for (", " bytes del juego ("},
        {"); that file is left out", "); se omite ese archivo"},
        {"chip.bin is not a chip of the game (one entry of the select's chip pack, 5504 bytes); the chip is left out", "chip.bin no es un retrato válido de selección de 5504 bytes; se omite"},
        {"replaces character ", "reemplaza al personaje "},
        {" but holds nothing to put in its place", " pero no contiene archivos de reemplazo"},
        {"character ", "personaje "},
        {" is already replaced by ", " ya está reemplazado por "},
        {" (the first by name counts)", " (se usa el primero por nombre)"},
        {"no model of the first costume (*_1p.pak) and no `replaces`: not a character", "falta el modelo del primer traje (*_1p.pak) y replaces: no es un personaje válido"},
        {"there are already ", "ya hay "},
        {" added characters, the most the game takes", " personajes añadidos, el máximo admitido"},
        {", named as a link, is not installed; that ", ", indicado como enlace, no está instalado; se omite esa "},
        {"transformation", "transformación"}, {"fusion", "fusión"},
        {" is left out", ""},
        {"there is no character ", "no existe el personaje "},
        {"its model file (", "el archivo de modelo ("},
        {"its animation file (", "el archivo de animación ("},
        {") is empty in the game data", ") está vacío en los datos del juego"},
        {") is not in the game data", ") no se encuentra en los datos del juego"}
    };
    size_t used = 0;
    while (*text && used + 1 < sizeof(out)) {
        const char *replacement = NULL; size_t matched = 0;
        for (const auto &part : parts) {
            size_t n = strlen(part.en);
            if (n > matched && strncmp(text, part.en, n) == 0) { matched = n; replacement = part.es; }
        }
        if (replacement) {
            size_t n = strlen(replacement);
            if (n >= sizeof(out) - used) break;
            memcpy(out + used, replacement, n); used += n; text += matched;
        } else { out[used++] = *text++; }
    }
    out[used] = 0;
    return out;
}

static void mods_tab(void) {
    int count = Port_CharaPkgCount(), added = 0, replaced = 0, off = 0;
    for (int i = 0; i < count; i++) {
        const PortCharaPkg *p = Port_CharaPkg(i);
        added += p->kind == PORT_PKG_ADDS;
        replaced += p->kind == PORT_PKG_REPLACES;
        off += p->kind == PORT_PKG_OFF;
    }
    if (Port_NetSession()) {
        ImGui::TextWrapped("El online clásico de dos jugadores usa solo personajes originales; "
                           "no carga la carpeta \"characters\". El Raid cooperativo sí permite paquetes idénticos entre participantes.");
        ImGui::Spacing();
    } else if (count == 0) {
        ImGui::TextWrapped("No hay paquetes de personajes. Cada carpeta dentro de \"characters\" junto al juego añade un personaje "
                           "o reemplaza uno original.");
        return;
    }
    ImGui::Text("Paquetes de personajes: %d añadidos, %d reemplazos", added, replaced);
    if (off > 0) {
        ImGui::SameLine();
        ImGui::Text("(%d sin usar)", off);
    }
    ImGui::Spacing();
    if (!Port_NetSession()) {
        bool pool = Port_CharaPool() != 0;
        if (ImGui::Checkbox("Incluir personajes añadidos en la selección aleatoria", &pool)) {
            Port_CharaPoolSet(pool);
        }
        ImGui::SetItemTooltip("Desactivado: la casilla aleatoria y los participantes CPU de los torneos eligen personajes\n"
                              "originales. Activado: los personajes añadidos también participan en la selección.\n"
                              "La CPU usa el comportamiento del personaje en el que se basa cada uno.");
        ImGui::Spacing();
    }
    {
        for (int i = 0; i < count; i++) {
            ImGui::Separator();
            const PortCharaPkg *p = Port_CharaPkg(i);
            ImGui::PushID(i);
            ImGui::TextUnformatted(p->name[0] != '\0' ? p->name : p->folder);
            ImGui::SameLine();
            if (p->kind == PORT_PKG_ADDS) {
                ImGui::TextDisabled("añade un personaje (ID %d)", p->id);
            } else if (p->kind == PORT_PKG_REPLACES) {
                ImGui::TextDisabled("reemplaza al personaje %d", p->id);
            } else {
                ImGui::TextDisabled("sin usar");
            }
            if (p->author[0] != '\0' || p->version[0] != '\0') {
                ImGui::SameLine();
                ImGui::TextDisabled("- %s%s%s", p->author, p->author[0] != '\0' && p->version[0] != '\0' ? ", " : "", p->version);
            }
            ImGui::Indent();
            ImGui::TextDisabled("carpeta: %s", p->folder);
            if (p->about[0] != '\0') {
                ImGui::TextWrapped("%s", p->about);
            }
            if (p->problem[0] != '\0') {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.35f, 1.0f));
                ImGui::TextWrapped("%s", package_issue_es(p->problem));
                ImGui::PopStyleColor();
            }
            ImGui::Unindent();
            ImGui::PopID();
        }
    }
}

extern "C" int Port_GameSpeed(void);          // plat_stub.c: the player's own game speed, percent; never used online
extern "C" void Port_GameSpeedSet(int percent);
extern "C" void Port_AudioRefresh(void);

static void cheats_tab(void) {
    {
        int speed = Port_GameSpeed();
        if (ImGui::SliderInt("Velocidad del juego", &speed, 50, 200, "%d%%", ImGuiSliderFlags_AlwaysClamp)) {
            Port_GameSpeedSet(speed);
            Port_AudioRefresh();
        }
        ImGui::SetItemTooltip("Velocidad fuera de las partidas en línea (el sonido conserva su tono). 100%% es la velocidad original;\n"
                              "114%% es la usada en competitivo. Las partidas en línea tienen su propia velocidad,\n"
                              "elegida por el anfitrión o la cola de búsqueda.");
        ImGui::SameLine();
        if (ImGui::Button("Restablecer")) {
            Port_GameSpeedSet(100);
            Port_AudioRefresh();
        }
        ImGui::Separator();
        ImGui::Spacing();
    }
    ImGui::TextWrapped("Desbloquea todos los personajes, escenarios, música, objetos y la cantidad máxima de zenis. "
                       "Es una función de depuración de los desarrolladores originales.");
    ImGui::Spacing();
    ImGui::TextWrapped("También vacía tus récords y se vuelve permanente al guardar la partida. "
                       "Úsala después de cargar tu partida, desde el menú principal.");
    ImGui::Spacing();
    if (ImGui::Button("Desbloquear todo...")) {
        ImGui::OpenPopup("¿Desbloquear todo?");
    }
    if (gPortUnlockDone) {
        ImGui::SameLine();
        ImGui::TextUnformatted("Hecho.");
    }
    if (ImGui::BeginPopupModal("¿Desbloquear todo?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Se vaciará tu lista de récords. No podrás deshacerlo después de guardar la partida.");
        ImGui::Spacing();
        if (ImGui::Button("Desbloquear", ImVec2(140.0f, 0.0f))) {
            gPortUnlockAll = 1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(140.0f, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// The Experiments tab: things that are not finished, each off until switched on, none of them in online play.
// Extra fighters: a table with a row for each (one today; the settings carry an index, plat_fighters.c).
static void chara_label(char *out, size_t size, int chara) {
    const char *name = Port_FighterName(chara);
    if (name[0] != '\0') {
        snprintf(out, size, "%s  (%d)", name, chara);
    } else {
        snprintf(out, size, "Personaje %d", chara);
    }
}

#include "pc_battle_ui.h"

static void experiments_tab(void) {
    pc_battle_ui();
    if (Port_CoopEnabled()) { ImGui::TextWrapped("Sala Raid online activa: reglas compartidas y modo foto desactivado."); return; }
    if (!ImGui::CollapsingHeader("Opciones avanzadas de Tag Team y Entrenamiento")) return;
    static const char *const kTeams[] = {"Equipo 1 (Jugador 1)", "Equipo 2 (Jugador 2)", "Por su cuenta (Contra todos)"};
    static const char *const kLast[] = {NULL,
                                        "El último combate tuvo el tercer luchador activo.",
                                        "El último combate fue original: el luchador extra de esta opcion solo se aplica en Entrenamiento.",
                                        "El último combate fue original: un bando tenía más de un integrante.",
                                        "El último combate fue original: la pantalla ya estaba dividida para dos jugadores.",
                                        "El último combate fue original: era una repetición.",
                                        "El último combate fue original: combate de historia con guión.",
                                        "El último combate fue original: las partidas online no permiten luchador extra.",
                                        "El último combate fue original: no se encontraron los archivos del personaje."};
    static int checked = -1, ok, age; // the character whose files were looked for, what came of it, frames since
    static char why[160], refused[320];
    int count = Port_ExtraCount(), chara = Port_ExtraChara(0), team = Port_ExtraTeam(0), level = Port_ExtraLevel(0);
    int wasCount = count, wasChara = chara, wasTeam = team, wasLevel = level, last = Port_ExtraLast();
    bool on = count > 0;
    char label[96];

    ImGui::TextWrapped("Duelo y los modos extra son independientes. Elige Tag Team, Todos vs Todos o Raid desde el menu principal con SELECT.");
    ImGui::Spacing();
    ImGui::SeparatorText("Jugadores Locales (Multijugador en esta PC)");
    {
        static const char *const kLocal[] = {"1 Jugador", "2 Jugadores", "3 Jugadores", "4 Jugadores"};
        int local = Port_LocalPlayers() - 1;
        ImGui::SetNextItemWidth(260.0f);
        if (ImGui::Combo("Jugadores en esta PC", &local, kLocal, 4)) {
            Port_LocalPlayersSet(local + 1);
        }
        ImGui::TextWrapped("Para Batalla por Equipos o Todos Contra Todos (pulsa SELECT en el Menú Principal), con "
                           "dos o más luchadores por equipo: el mando 3 controla al 2º luchador del Equipo 1, y el mando 4 al 2º del Equipo 2. "
                           "La pantalla se divide en 4 cuadrantes (uno para cada jugador).");
    }
    ImGui::Spacing();
    ImGui::SeparatorText("Luchador Extra en Combate (Tercer Luchador CPU)");
    if (ImGui::Checkbox("Activar luchador extra solo en Entrenamiento", &on)) {
        count = on ? 1 : 0;
    }
    ImGui::SetItemTooltip("Un luchador CPU adicional para Entrenamiento.\n"
                          "Duelo conserva sus reglas y sus equipos originales.\n"
                          "Tag Team, Todos vs Todos y Raid se eligen por separado en el menu principal con SELECT.");
    if (Port_NetActive()) {
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Desactivado durante partidas online.");
    }
    ImGui::BeginDisabled(!on);
    if (ImGui::BeginTable("extra", 4, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 20.0f);
        ImGui::TableSetupColumn("Personaje", ImGuiTableColumnFlags_WidthStretch, 2.6f);
        ImGui::TableSetupColumn("Equipo", ImGuiTableColumnFlags_WidthStretch, 2.3f);
        ImGui::TableSetupColumn("Nivel CPU", ImGuiTableColumnFlags_WidthStretch, 1.5f);
        ImGui::TableHeadersRow();
        ImGui::TableNextRow();
        ImGui::PushID(0);
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("3");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        chara_label(label, sizeof(label), chara);
        if (ImGui::BeginCombo("##chara", label, ImGuiComboFlags_HeightLarge)) {
            extern int Port_CharaCount(void);
            for (int c = 0; c < 0xC6 + Port_CharaCount(); c++) {
                if (c > 160 && c < 0xC6) {
                    continue;
                }
                chara_label(label, sizeof(label), c);
                if (ImGui::Selectable(label, c == chara)) {
                    // a character whose files the game data does not have is not taken: the fight could not load it
                    if (Port_ExtraCharaOk(c, why, sizeof(why))) {
                        chara = c;
                        refused[0] = '\0';
                    } else {
                        snprintf(refused, sizeof(refused), "%s no se pudo usar: %s.", label, package_issue_es(why));
                    }
                }
                if (c == chara) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        {
            int at = team < 0 ? 2 : team;
            if (ImGui::Combo("##team", &at, kTeams, 3)) {
                team = at == 2 ? -1 : at;
            }
        }
        ImGui::SetItemTooltip("De qué bando pelea. Los luchadores del mismo equipo no se golpean entre sí.\n"
                              "Por su cuenta: pelea contra ambos jugadores (todos contra todos).");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderInt("##level", &level, 0, 29, "%d", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SetItemTooltip("Dificultad de la CPU de 0 a 29.\n"
                              "Los cinco niveles estándar del juego son: 0, 6, 13 (Normal), 21 y 29 (Muy difícil).");
        ImGui::PopID();
        ImGui::EndTable();
    }
    ImGui::EndDisabled();
    if (count != wasCount || chara != wasChara || team != wasTeam || level != wasLevel) {
        Port_ExtraSet(count, 0, chara, team, level);
    }
    if (checked != chara || ++age > 120) { // (again now and then: the data folder is the player's to change)
        checked = chara;
        age = 0;
        ok = Port_ExtraCharaOk(chara, why, sizeof(why));
    }
    if (refused[0] != '\0') {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.4f, 1.0f));
        ImGui::TextWrapped("%s", refused);
        ImGui::PopStyleColor();
    }
    if (on && !ok) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.4f, 1.0f));
        ImGui::TextWrapped("Este personaje no se puede usar: %s. Los combates siguen normales hasta que elijas otro.", package_issue_es(why));
        ImGui::PopStyleColor();
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "INSTRUCCIONES PARA ACTIVAR MODOS EXTRA (TAG TEAM / EXTRA FIGHTERS):");
    ImGui::TextWrapped("1. En la pantalla del MENÚ PRINCIPAL del juego, pulsa la tecla o botón asignado a SELECT.\n"
                       "2. El menú cambiará y mostrará: 'Batalla por equipos' (simultánea), 'Todos contra todos' y 'Entrenamiento'.\n"
                       "3. Elige el número de jugadores (1 a 4) y selecciona tus luchadores en la pantalla de equipos.\n"
                       "4. ¡En el combate lucharán todos los personajes simultáneamente en la misma arena!");
    if (Port_ModesFight() != PORT_MODE_NONE) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
        ImGui::TextWrapped("Modo actual activo: %s (se mantendrá hasta que vuelvas al Menú Principal).",
                           Port_ModesName(Port_ModesFight()));
        ImGui::PopStyleColor();
    }
    if (last > 0 && last < (int)(sizeof(kLast) / sizeof(kLast[0])) && (on || last == 1)) {
        ImGui::Spacing();
        ImGui::TextDisabled("%s", kLast[last]);
    }
}

// The Controls tab's part for the sticks: each stick of the player's controller as it is now (the dot), its deadzone
// (the shaded middle: a circle, or the game's own square), what the game is given for it (the ring, and in numbers),
// the triggers, and the settings for these and for vibration. For seeing a stick that drifts and choosing a deadzone
// that covers it.
static void sticks_panel(int player) {
    static const char *const kStick[2] = {"Palanca izquierda", "Palanca derecha"}, *const kTrigger[2] = {"Gatillo izquierdo", "Gatillo derecho"};
    static const char *const kDir[5] = {"", "arriba", "abajo", "izquierda", "derecha"};
    const float R = 42.0f, box = 2.0f * R + 8.0f;
    const ImU32 colZone = ImGui::GetColorU32(ImGuiCol_Text, 0.22f);
    const ImU32 colRaw = ImGui::GetColorU32(ImGuiCol_Text), colGame = ImGui::GetColorU32(ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
    int *dead = PortInput_Deadzone(player), *vib = PortInput_Vibration(player);
    PortStickView view;
    char label[64];

    PortInput_StickView(player, &view);
    if (!ImGui::BeginTable("sticks", 2)) {
        return;
    }
    float top = 0.0f;
    for (int s = 0; s < 2; s++) {
        ImGui::TableNextColumn();
        if (s == 0) {
            top = ImGui::GetCursorScreenPos().y;
        } else { // (the second cell would start lower by the text offset of the first one's last line)
            ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, top));
        }
        ImDrawList *dl = ImGui::GetWindowDrawList();
        ImVec2 at = ImGui::GetCursorScreenPos(), c(at.x + box * 0.5f, at.y + box * 0.5f);
        ImGui::Dummy(ImVec2(box, box));
        dl->AddCircleFilled(c, R, ImGui::GetColorU32(ImGuiCol_FrameBg), 48);
        if (dead[s] < 0) { // the game's own: each axis by itself, 50 of 127.5
            float h = R * 50.0f / 127.5f;
            dl->AddRectFilled(ImVec2(c.x - h, c.y - h), ImVec2(c.x + h, c.y + h), colZone);
        } else if (dead[s] > 0) {
            dl->AddCircleFilled(c, R * (float)dead[s] / 100.0f, colZone, 48);
        }
        dl->AddLine(ImVec2(c.x - R, c.y), ImVec2(c.x + R, c.y), ImGui::GetColorU32(ImGuiCol_Border));
        dl->AddLine(ImVec2(c.x, c.y - R), ImVec2(c.x, c.y + R), ImGui::GetColorU32(ImGuiCol_Border));
        dl->AddCircle(c, R, ImGui::GetColorU32(ImGuiCol_Border), 48);
        if (view.connected) {
            dl->AddCircle(ImVec2(c.x + view.game[s][0] * R, c.y + view.game[s][1] * R), 5.5f, colGame, 16, 1.5f);
            dl->AddCircleFilled(ImVec2(c.x + view.raw[s][0] * R, c.y + view.raw[s][1] * R), 3.0f, colRaw, 12);
        }
        ImGui::SameLine();
        ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, top));
        ImGui::BeginGroup();
        ImGui::AlignTextToFramePadding(); // (in both cells: the second one's lines start that much lower anyway)
        if (view.connected) {
            ImGui::Text(view.dir[s] != 0 ? "%s: %s" : "%s", kStick[s], kDir[view.dir[s]]);
            ImGui::Text("palanca %+.2f %+.2f", view.raw[s][0], view.raw[s][1]);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(colGame), "juego   %+.2f %+.2f", view.game[s][0], view.game[s][1]);
        } else {
            ImGui::TextUnformatted(kStick[s]);
            ImGui::TextDisabled("sin mando");
            ImGui::TextDisabled(" ");
        }
        ImGui::SetNextItemWidth(100.0f);
        snprintf(label, sizeof(label), "Zona muerta##%d", s);
        ImGui::SliderInt(label, &dead[s], -1, PORT_DEADZONE_MAX, dead[s] < 0 ? "Original" : "%d%%", ImGuiSliderFlags_AlwaysClamp);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            PortInput_Save();
        }
        ImGui::SetItemTooltip("Distancia que debe moverse la palanca antes de registrar una entrada (zona central sombreada).\n"
                              "Original: 39%% en cada eje por separado, formando un cuadrado.\n"
                              "De 0%% a 90%%: zona circular de ese tamaño. Desde su borde, el valor aumenta de forma uniforme\n"
                              "hasta el máximo al inclinar por completo. La dirección se registra a mitad del recorrido restante.\n"
                              "El punto blanco muestra la palanca y el anillo la entrada enviada al juego. Tras el nombre aparece\n"
                              "la dirección que interpreta el juego. Elige un tamaño que cubra la posición del punto\n"
                              "cuando sueltas la palanca.");
        ImGui::EndGroup();
    }
    for (int s = 0; s < 2; s++) {
        ImGui::TableNextColumn();
        snprintf(label, sizeof(label), "%d%%%s", (int)(view.trigger[s] * 100.0f + 0.5f), view.triggerHeld[s] ? " pulsado" : "");
        ImGui::ProgressBar(view.trigger[s], ImVec2(144.0f, 0.0f), label);
        ImGui::SetItemTooltip("El gatillo cuenta como pulsado a partir de %d%%. El juego solo distingue entre pulsado y suelto.",
                              (PORT_TRIGGER_AT * 100 + 16383) / 32767);
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::TextUnformatted(kTrigger[s]);
    }
    ImGui::EndTable();
    bool on = vib[0] != 0;
    if (ImGui::Checkbox("Vibración", &on)) {
        vib[0] = on;
        PortInput_Save();
    }
    ImGui::SetItemTooltip("Vibración del juego (golpes, choques y carga de energía) en el mando de este jugador.\n"
                          "También debe estar activada en Opciones > Mando > Vibración dentro del juego.\n"
                          "No hay vibración en repeticiones ni para un bando controlado por la CPU.");
    ImGui::BeginDisabled(!on);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderInt("Intensidad", &vib[1], 0, 100, "%d%%", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        PortInput_Save();
    }
    ImGui::SameLine();
    if (ImGui::Button("Probar")) {
        PortInput_VibrationTest(player);
    }
    ImGui::SetItemTooltip("Activa ambos motores durante medio segundo con esta intensidad.");
    ImGui::EndDisabled();
    if (view.connected && !view.canVibrate) {
        ImGui::SameLine();
        ImGui::TextDisabled("(este mando no tiene vibración)");
    }
}

static void controls_tab(void) {
    static int player;
    char label[96];
    int count = 0;

    ImGui::RadioButton("Jugador 1", &player, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Jugador 2", &player, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Jugador 3", &player, 2); // (players 3 and 4: four local players, Experiments tab)
    ImGui::SameLine();
    ImGui::RadioButton("Jugador 4", &player, 3);
    int *slot = PortInput_PadSlot(player), *keys = PortInput_Keys(player), *pads = PortInput_PadButtons(player);
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    if (*slot < 0) {
        snprintf(label, sizeof(label), "Ninguno");
    } else if (*slot < count) {
        snprintf(label, sizeof(label), "%d: %s", *slot + 1, SDL_GetGamepadNameForID(ids[*slot]));
    } else {
        snprintf(label, sizeof(label), "Mando %d (desconectado)", *slot + 1);
    }
    ImGui::SetNextItemWidth(300.0f);
    if (ImGui::BeginCombo("Mando", label)) {
        if (ImGui::Selectable("Ninguno", *slot < 0)) { *slot = -1; PortInput_Save(); }
        for (int i = 0; i < (count > 4 ? count : 4); i++) {
            if (i < count) {
                snprintf(label, sizeof(label), "%d: %s", i + 1, SDL_GetGamepadNameForID(ids[i]));
            } else {
                snprintf(label, sizeof(label), "Mando %d (desconectado)", i + 1);
            }
            if (ImGui::Selectable(label, *slot == i)) { *slot = i; PortInput_Save(); }
        }
        ImGui::EndCombo();
    }
    SDL_free(ids);
    ImGui::SameLine();
    if (ImGui::Button("Restablecer valores predeterminados")) {
        PortInput_ResetDefaults(player);
        PortInput_Save();
    }
    if (ImGui::CollapsingHeader("Palancas, gatillos y vibración", ImGuiTreeNodeFlags_DefaultOpen)) {
        sticks_panel(player);
    }
    ImGui::TextDisabled("Elige una asignación y pulsa la nueva tecla o botón. Esc conserva la actual; Supr la borra.");
    ImGui::TextDisabled("Con un mando, espera cinco segundos sin pulsar nada para conservarla.");
    // (never lower than a few rows: in a small window the tab scrolls as a whole instead)
    if (ImGui::BeginTable("bindings", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV,
                          ImVec2(0.0f, ImGui::GetContentRegionAvail().y > 140.0f ? ImGui::GetContentRegionAvail().y : 140.0f))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Acción");
        ImGui::TableSetupColumn("Teclado");
        ImGui::TableSetupColumn("Mando");
        ImGui::TableHeadersRow();
        {   // what opens and closes this window (not a player's: one for the program)
            const int a = PORT_ACTIONS;
            bool waitKey = sCapKind == 1 && sCapAction == a, waitPad = sCapKind == 2 && sCapAction == a;
            int key = *PortInput_OverlayKey(), pad = *PortInput_OverlayPad();
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Abrir este menú");
            ImGui::TableNextColumn();
            snprintf(label, sizeof(label), "%s##k%d", waitKey ? "pulsa una tecla..." : key > 0 ? key_caption((SDL_Scancode)key) : "-", a);
            if (ImGui::Button(label, ImVec2(-FLT_MIN, 0.0f))) {
                capture(1, player, a);
            }
            ImGui::TableNextColumn();
            snprintf(label, sizeof(label), "%s##p%d", waitPad ? "pulsa un botón..." : pad >= 0 ? pad_source_name(pad) : "START + SELECT", a);
            if (ImGui::Button(label, ImVec2(-FLT_MIN, 0.0f))) {
                capture(2, player, a);
            }
        }
        {   // what opens the online chat (not a player's: one for the program)
            const int a = PORT_ACTIONS + 1;
            bool waitKey = sCapKind == 1 && sCapAction == a;
            int key = *PortInput_ChatKey();
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Abrir chat en línea");
            ImGui::TableNextColumn();
            snprintf(label, sizeof(label), "%s##k%d", waitKey ? "pulsa una tecla..." : key > 0 ? key_caption((SDL_Scancode)key) : "Tab", a);
            if (ImGui::Button(label, ImVec2(-FLT_MIN, 0.0f))) {
                capture(1, player, a);
            }
            ImGui::TableNextColumn();
            ImGui::TextDisabled("-");
        }
        for (int a = 0; a < PORT_ACTIONS; a++) {
            bool waitKey = sCapKind == 1 && sCapPlayer == player && sCapAction == a;
            bool waitPad = sCapKind == 2 && sCapPlayer == player && sCapAction == a;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(PortInput_ActionLabel(a));
            ImGui::TableNextColumn();
            snprintf(label, sizeof(label), "%s##k%d", waitKey ? "pulsa una tecla..." : keys[a] > 0 ? key_caption((SDL_Scancode)keys[a]) : "-", a);
            if (ImGui::Button(label, ImVec2(-FLT_MIN, 0.0f))) {
                capture(1, player, a);
            }
            ImGui::TableNextColumn();
            if (a < PORT_PAD_BUTTONS) {
                snprintf(label, sizeof(label), "%s##p%d", waitPad ? "pulsa un botón..." : pad_source_name(pads[a]), a);
                if (ImGui::Button(label, ImVec2(-FLT_MIN, 0.0f))) {
                    capture(2, player, a);
                }
            } else {
                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled(a < 20 ? "Palanca izquierda" : "Palanca derecha");
            }
        }
        ImGui::EndTable();
    }
}

static void build(void) {
    PortVideo v, was;
    ImGuiIO &io = ImGui::GetIO();
    bool open = true;

    GsGpu_GetSettings(&v);
    was = v;
    if (sCapKind == 2 && SDL_GetTicks() > sCapUntil) { // nothing was pressed: the binding stays
        sCapKind = 0;
        gPortInputCapture = 0;
    }
    if (pad_pressed(ImGuiKey_GamepadFaceRight)) { // Circle / B: out of the window
        open = false;
    }
    if (sForceTab < 0 && (pad_pressed(ImGuiKey_GamepadL1) || pad_pressed(ImGuiKey_GamepadR1))) {
        static const int tabs[] = {0, 1, 2, 3, 4, 5, 7, 8};
        int at = 0;
        while (at < 7 && tabs[at] != sTab) at++;
        sForceTab = tabs[(at + (ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false) ? 7 : 1)) % 8];
        sPadTab = true;
    }
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    // (760 high where the picture has the room: the Controls tab's sticks and its list of bindings together)
    // (and 690 wide: the eight tabs' names side by side)
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x >= 1000.0f ? 900.0f : io.DisplaySize.x * .94f, io.DisplaySize.y >= 800.0f ? 760.0f : 600.0f), ImGuiCond_Appearing);
    if (ImGui::Begin("Ajustes", &open, ImGuiWindowFlags_NoCollapse)) {
        {
            int latest = update_latest();
            if (latest > PORT_RELEASE) {
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Actualización disponible: 0.%d.%d (versión actual: 0.%d.%d).", latest / 100, latest % 100,
                                   PORT_RELEASE / 100, PORT_RELEASE % 100);
                ImGui::SameLine();
                if (ImGui::Button("Página de descarga")) {
                    SDL_OpenURL("https://github.com/" PORT_UPDATE_REPO "/releases/latest");
                }
                ImGui::SetItemTooltip("Abre la página de la versión en tu navegador. Para actualizar, reemplaza los archivos anteriores por los nuevos.");
                ImGui::Separator();
            }
        }
        // the tabs in a region of their own, above a last line that is always there
        ImGui::BeginChild("body", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing() - ImGui::GetStyle().ItemSpacing.y), ImGuiChildFlags_NavFlattened);
        if (ImGui::BeginTabBar("tabs")) {
            if (tab("Video", 0)) { video_tab(v); ImGui::EndTabItem(); }
            if (tab("Efectos", 1)) { effects_tab(v); ImGui::EndTabItem(); }
            if (tab("Audio", 2)) { audio_tab(v); ImGui::EndTabItem(); }
            if (tab("Interfaz", 3)) { ui_tab(v); ImGui::EndTabItem(); }
            if (tab("Controles", 4)) {
                bool cs = Port_ClashSquare() != 0;
                if (ImGui::Checkbox("Choques: Cuadrado cuenta como una dirección", &cs)) {
                    Port_ClashSquareSet(cs);
                }
                ImGui::SetItemTooltip("En un choque de rayos o acometidas, el juego cuenta nuevas pulsaciones de dirección:\n"
                                      "giros de la palanca o pulsaciones de la cruceta. Esta opción permite que cada pulsación de Cuadrado\n"
                                      "cuente como una dirección mientras dure el choque, igual que la cruceta.\n"
                                      "Solo modifica tu mando, por lo que puede usarse en partidas en línea.");
                if (cs) {
                    static const int kQuarters[] = {4, 6, 7, 8};
                    static const char *const kWorth[] = {"1", "1.5", "1.75", "2"};
                    int at = 2;
                    for (int i = 0; i < 4; i++) {
                        if (kQuarters[i] == Port_ClashSquareX()) {
                            at = i;
                        }
                    }
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(80.0f);
                    if (ImGui::Combo("por pulsación", &at, kWorth, 4)) {
                        Port_ClashSquareXSet(kQuarters[at]);
                    }
                    ImGui::SetItemTooltip("Valor de cada pulsación de Cuadrado. La cruceta tiene cuatro botones y la palanca cuatro direcciones\n"
                                          "por giro; Cuadrado es un solo botón. 1 equivale a una pulsación de la cruceta.\n"
                                          "Nunca se cuenta más de una por cuadro, el máximo admitido para cualquier mando.");
                }
                ImGui::Separator();
                controls_tab();
                ImGui::EndTabItem();
            }
            if (tab("Trucos", 5)) { ImGui::BeginDisabled(Port_CoopEnabled()); cheats_tab(); ImGui::EndDisabled(); ImGui::EndTabItem(); }
            if (tab("Modificaciones", 7)) { mods_tab(); ImGui::EndTabItem(); }
            if (tab("Equipos / Modos extra", 8)) { experiments_tab(); ImGui::EndTabItem(); }
            ImGui::EndTabBar();
        }
        ImGui::EndChild();
        if (sPadTab) { // (a tab chosen with the shoulder buttons)
            sPadTab = false;
            sTab = sForceTab;
            sForceTab = -1;
        } else if (ImGui::GetFrameCount() > 3) {
            sForceTab = -1;
        }
        ImGui::Separator();
        if (ImGui::Button("Salir del juego")) {
            Ui_QuitAsk();
        }
        if ((io.BackendFlags & ImGuiBackendFlags_HasGamepad) != 0) {
            ImGui::SameLine();
            ImGui::TextDisabled("Cruceta: mover   Cruz / A: elegir   Círculo / B: volver   L1 / R1: pestaña");
        }
    }
    ImGui::End();
    if (memcmp(&v, &was, sizeof(v)) != 0) {
        GsGpu_SetSettings(&v);
    }
    if (!open) {
        set_open(false);
    }
}

// ---- the hint of the extra modes -----------------------------------------------------------------------------------
// SELECT on the game's main menu switches the menu to the extra modes' entries and back (src/menu/menu_a.c,
// plat_modes.c; the entries are the menu's own plates, their names drawn by menu_text.c). Here only the small plate
// in the corner of the menu's message window that says so. It is drawn into the game's picture, behind the settings
// window, in the picture's own 512 x 448 units so that it is the same at every window size.
struct ModesBox { float x, y, kx, ky; }; // the 4:3 page in the window: its corner, and window pixels per unit

static ImVec2 modes_at(const ModesBox &b, float x, float y) {
    return ImVec2(b.x + x * b.kx, b.y + y * b.ky);
}

// A line of text; align 0 from x, 1 centred on x. Made narrower when it is wider than `fit` units.
static void modes_text(ImDrawList *dl, const ModesBox &b, float size, float x, float y, ImU32 colour, const char *text, int align, float fit) {
    ImFont *font = ImGui::GetFont();
    float px = size * b.ky;
    ImVec2 ext = font->CalcTextSizeA(px, FLT_MAX, 0.0f, text);
    if (fit > 0.0f && ext.x > fit * b.kx) {
        px *= fit * b.kx / ext.x;
        ext = font->CalcTextSizeA(px, FLT_MAX, 0.0f, text);
    }
    ImVec2 at = modes_at(b, x, y);
    at.x -= align == 1 ? ext.x * 0.5f : 0.0f;
    at.y += (size * b.ky - px) * 0.5f;
    dl->AddText(font, px, at, colour, text);
}

static void modes_draw(const PortModesView &v) {
    static int said = -1; // the entry the screen reader was told last, -1: the set is not showing
    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    ModesBox b;
    float pw = (float)gUiPresentW, ph = (float)gUiPresentH;

    if (pw <= 0.0f || ph <= 0.0f) {
        return;
    }
    {   // the 4:3 page in the middle of the picture (the menus are that page, also in a wide picture)
        float w = pw < ph * 4.0f / 3.0f ? pw : ph * 4.0f / 3.0f;
        b.x = (float)gUiPresentX + (pw - w) * 0.5f;
        b.y = (float)gUiPresentY;
        b.kx = w / 512.0f;
        b.ky = ph / 448.0f;
    }
    if (v.hint) {
        ImVec2 p0 = modes_at(b, 275.0f, 414.0f), p1 = modes_at(b, 505.0f, 442.0f), k0 = modes_at(b, 281.0f, 418.0f), k1 = modes_at(b, 345.0f, 438.0f);
        dl->AddRectFilled(p0, p1, IM_COL32(6, 14, 40, 230), 12.0f * b.ky);
        dl->AddRect(p0, p1, IM_COL32(246, 150, 30, 255), 12.0f * b.ky, 0, 1.8f * b.ky);
        dl->AddRectFilled(k0, k1, IM_COL32(230, 235, 245, 255), 8.0f * b.ky);
        modes_text(dl, b, 12.0f, 313.0f, 421.5f, IM_COL32(20, 24, 40, 255), "SELECT", 1, 60.0f);
        modes_text(dl, b, 14.0f, 352.0f, 420.5f, IM_COL32(255, 240, 200, 255), v.set ? "VOLVER AL MENU" : "MODO TAG TEAM", 0, 145.0f);
    }
    if (!v.set || !v.hint) { // the screen reader: the set when it comes up, then the entry under the cursor when it changes
        said = -1;
    } else if (v.players && v.cursor + 10 != said && v.cursor >= 0 && v.cursor < 4) { // (the number of players)
        char speak[96];
        snprintf(speak, sizeof(speak), "%s, %d de 4", Port_ModesPlayersName(v.cursor + 1), v.cursor + 1);
        if (said < 10) {
            PortA11y_Say(PortA11y_Label("Jugadores"), 1);
        }
        PortA11y_Say(speak, said < 10 ? 0 : 1);
        said = v.cursor + 10;
    } else if (!v.players && v.cursor != said && v.cursor >= 0 && v.cursor < PORT_MODE_COUNT - 1) {
        char speak[96];
        snprintf(speak, sizeof(speak), "%s, %d de %d", Port_ModesName(v.cursor + 1), v.cursor + 1, PORT_MODE_COUNT - 1);
        if (said < 0 || said >= 10) {
            PortA11y_Say(PortA11y_Label("Modos extra"), 1);
        }
        PortA11y_Say(speak, said < 0 || said >= 10 ? 0 : 1);
        said = v.cursor;
    }
}

static bool sBuilt; // this frame has something to draw: the window, or the line over the picture

void Ui_DrawAgain(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *target) {
    ImDrawData *dd = sReady && sBuilt ? ImGui::GetDrawData() : NULL; // (sBuilt: Ui_Draw made a picture this frame)
    SDL_GPUColorTargetInfo ti;
    SDL_GPURenderPass *pass;

    if (dd == NULL || dd->CmdListsCount == 0 || dd->DisplaySize.x <= 0.0f || dd->DisplaySize.y <= 0.0f) {
        return;
    }
    ImGui_ImplSDLGPU3_PrepareDrawData(dd, cmd);
    SDL_zero(ti);
    ti.texture = target;
    ti.load_op = SDL_GPU_LOADOP_LOAD;
    ti.store_op = SDL_GPU_STOREOP_STORE;
    pass = SDL_BeginGPURenderPass(cmd, &ti, 1, NULL);
    ImGui_ImplSDLGPU3_RenderDrawData(dd, cmd, pass);
    SDL_EndGPURenderPass(pass);
}

static char sNotice[160];
static Uint64 sNoticeUntil;
static volatile int sNoticeNew;

extern "C" void Port_UiNotice(const char *text) {
    SDL_strlcpy(sNotice, text != NULL ? text : "", sizeof(sNotice));
    sNoticeNew = 1;
}

// ============================================================================
// Battle feature overlays and cinematic photo mode
// ============================================================================
// The battle and renderer run on different threads. Share only small locked snapshots.
static SDL_SpinLock sFeatureLock;
static int sBattleActive, sBossHealth = -1;
struct FormHint { int x, y, count; Uint64 until; };
static FormHint sFormHints[2];
extern "C" {
int gPortPhotoMode = 0;
int gPortHideHud = 0;
float gPortPhotoYaw = 0.0f, gPortPhotoPitch = 0.0f, gPortPhotoDist = 1.0f;
}

extern "C" int Port_PhotoActive(void) {
    return __atomic_load_n(&gPortPhotoMode, __ATOMIC_ACQUIRE);
}
extern "C" int Port_HudHidden(void) {
    return __atomic_load_n(&gPortHideHud, __ATOMIC_ACQUIRE);
}
extern "C" void Port_PhotoCamera(float *yaw, float *pitch, float *distance) {
    SDL_LockSpinlock(&sFeatureLock);
    *yaw = gPortPhotoYaw; *pitch = gPortPhotoPitch; *distance = gPortPhotoDist;
    SDL_UnlockSpinlock(&sFeatureLock);
}
extern "C" void Port_SetBattleOverlay(int active, int health) {
    SDL_LockSpinlock(&sFeatureLock);
    sBattleActive = active;
    sBossHealth = health;
    if (!active) {
        __atomic_store_n(&gPortPhotoMode, 0, __ATOMIC_RELEASE);
        __atomic_store_n(&gPortHideHud, 0, __ATOMIC_RELEASE);
    }
    SDL_UnlockSpinlock(&sFeatureLock);
}
extern "C" void Port_FormHint(int side, int x, int y, int count) {
    if ((unsigned)side >= 2) return;
    SDL_LockSpinlock(&sFeatureLock);
    sFormHints[side] = {x, y, count, SDL_GetTicks() + 150};
    SDL_UnlockSpinlock(&sFeatureLock);
}
static bool feature_overlay_on(void) {
    SDL_LockSpinlock(&sFeatureLock);
    bool on = sBattleActive || sFormHints[0].until > SDL_GetTicks() || sFormHints[1].until > SDL_GetTicks();
    SDL_UnlockSpinlock(&sFeatureLock);
    return on || gPortPcTeamReady;
}
static void Port_DrawFeatureHints(ImDrawList *dl) {
    if (Port_HudHidden() || gUiPresentW <= 0 || gUiPresentH <= 0) return;
    SDL_LockSpinlock(&sFeatureLock);
    int hp = sBossHealth;
    bool active = sBattleActive;
    FormHint hints[2] = {sFormHints[0], sFormHints[1]};
    SDL_UnlockSpinlock(&sFeatureLock);
    float sx = (float)gUiPresentW / 512.0f, sy = (float)gUiPresentH / 448.0f;
    ImFont *font = ImGui::GetFont();
    if (hp >= 0 && GsDraw_HudShown()) {
        char text[128];
        snprintf(text, sizeof(text), "[JEFE COLOSAL: RAID BOSS]  %d/6 barras", (hp + 9999) / 10000);
        float size = 11.0f * sy;
        ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
        float x = gUiPresentX + gUiPresentW - ts.x - 8.0f * sx;
        float y = gUiPresentY + 3.0f * sy;
        dl->AddRectFilled(ImVec2(x - 4 * sx, y), ImVec2(x + ts.x + 4 * sx, y + ts.y + 3 * sy), IM_COL32(45, 8, 12, 220), 3 * sy);
        dl->AddText(font, size, ImVec2(x, y), IM_COL32(255, 205, 75, 255), text);
    }
    if (active && GsDraw_HudShown()) {
        int a = Port_PcRelayRemaining(0), b = Port_PcRelayRemaining(1);
        char text[100] = "";
        if (a || b) snprintf(text, sizeof(text), "RELEVOS  Equipo 1: %d restantes  |  Equipo 2: %d restantes", a, b);
        if (Port_CoopEnabled()) snprintf(text, sizeof(text), "RAID ONLINE  Jugador %d (mando local 1)", Port_CoopMe() + 1);
        if (text[0]) dl->AddText(font, 11.0f * sy, ImVec2(gUiPresentX + 8 * sx, gUiPresentY + 22 * sy), IM_COL32(245,220,150,255), text);
    }
    for (const FormHint &h : hints) {
        if (h.count <= 1 || h.until <= SDL_GetTicks()) continue;
        const char *text = "[< L1] Forma [R1 >]";
        float size = 11.0f * sy;
        ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
        float x = gUiPresentX + h.x * sx, y = gUiPresentY + h.y * sy;
        dl->AddRectFilled(ImVec2(x - 3 * sx, y), ImVec2(x + ts.x + 3 * sx, y + ts.y), IM_COL32(10, 15, 25, 200), 3 * sy);
        dl->AddText(font, size, ImVec2(x, y), IM_COL32(255, 220, 95, 255), text);
    }
}

// Cinematic photo mode and clean HUD toggle
static float sPhotoToastTimer = 0.0f;
static const char *sPhotoToast = "";

extern "C" void Port_TogglePhotoMode(void) {
    SDL_LockSpinlock(&sFeatureLock);
    if (!sBattleActive || Port_NetSession() || Port_NetActive() || Port_CoopEnabled()) {
        SDL_UnlockSpinlock(&sFeatureLock);
        return;
    }
    gPortPhotoYaw = gPortPhotoPitch = 0.0f;
    gPortPhotoDist = 1.0f;
    __atomic_store_n(&gPortPhotoMode, !Port_PhotoActive(), __ATOMIC_RELEASE);
    SDL_UnlockSpinlock(&sFeatureLock);
    sPhotoToast = Port_PhotoActive() ? "MODO FOTO ACTIVADO (Cámara libre 360)" : "MODO FOTO DESACTIVADO";
    sPhotoToastTimer = 2.5f;
}

extern "C" void Port_ToggleHud(void) {
    __atomic_store_n(&gPortHideHud, !Port_HudHidden(), __ATOMIC_RELEASE);
    sPhotoToast = Port_HudHidden() ? "INTERFAZ OCULTA (Foto Limpia)" : "INTERFAZ VISIBLE";
    sPhotoToastTimer = 2.0f;
}

static void Port_UpdateAndDrawPhotoMode(ImDrawList *dl) {
    float dt = ImGui::GetIO().DeltaTime;
    if (dt <= 0.0f || dt > 0.1f) dt = 1.0f / 60.0f;

    SDL_LockSpinlock(&sFeatureLock);
    bool inBattle = sBattleActive;
    SDL_UnlockSpinlock(&sFeatureLock);
    if (!inBattle || sOpen || sChat || ImGui::GetIO().WantTextInput) return;
    // Read the real SDL gamepad: ImGui navigation may be disabled while playing.
    bool combo = false;
    float stickX = 0.0f, stickY = 0.0f;
    bool zoomIn = false, zoomOut = false;
    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    for (int i = 0; i < count; i++) {
        SDL_Gamepad *pad = SDL_GetGamepadFromID(ids[i]);
        if (!pad) continue;
        combo |= SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_LEFT_STICK) && SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_RIGHT_STICK);
        if (i != 0) continue;
        stickX = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX) / 32768.0f;
        stickY = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY) / 32768.0f;
        if (fabsf(stickX) < 0.18f) stickX = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX) / 32768.0f;
        if (fabsf(stickY) < 0.18f) stickY = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY) / 32768.0f;
        if (fabsf(stickX) < 0.18f) stickX = 0.0f;
        if (fabsf(stickY) < 0.18f) stickY = 0.0f;
        zoomIn = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
        zoomOut = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    }
    SDL_free(ids);
    static bool comboWas;
    bool comboPressed = combo && !comboWas;
    comboWas = combo;
    if (ImGui::IsKeyPressed(ImGuiKey_P, false) || comboPressed) {
        Port_TogglePhotoMode();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_H, false)) {
        Port_ToggleHud();
    }

    SDL_LockSpinlock(&sFeatureLock);
    if (Port_PhotoActive()) {
        gPortPhotoYaw += stickX * 2.0f * dt;
        gPortPhotoPitch -= stickY * 1.5f * dt;
        // Orbit Controls
        if (ImGui::IsKeyDown(ImGuiKey_LeftArrow) || ImGui::IsKeyDown(ImGuiKey_A) || ImGui::IsKeyDown(ImGuiKey_GamepadDpadLeft)) {
            gPortPhotoYaw -= 2.0f * dt;
        }
        if (ImGui::IsKeyDown(ImGuiKey_RightArrow) || ImGui::IsKeyDown(ImGuiKey_D) || ImGui::IsKeyDown(ImGuiKey_GamepadDpadRight)) {
            gPortPhotoYaw += 2.0f * dt;
        }
        if (ImGui::IsKeyDown(ImGuiKey_UpArrow) || ImGui::IsKeyDown(ImGuiKey_W) || ImGui::IsKeyDown(ImGuiKey_GamepadDpadUp)) {
            gPortPhotoPitch += 1.5f * dt;
        }
        if (ImGui::IsKeyDown(ImGuiKey_DownArrow) || ImGui::IsKeyDown(ImGuiKey_S) || ImGui::IsKeyDown(ImGuiKey_GamepadDpadDown)) {
            gPortPhotoPitch -= 1.5f * dt;
        }
        if (zoomIn || ImGui::IsKeyDown(ImGuiKey_Q) || ImGui::IsKeyDown(ImGuiKey_GamepadL1)) {
            gPortPhotoDist *= (1.0f - 1.2f * dt);
            if (gPortPhotoDist < 0.25f) gPortPhotoDist = 0.25f;
        }
        if (zoomOut || ImGui::IsKeyDown(ImGuiKey_E) || ImGui::IsKeyDown(ImGuiKey_GamepadR1)) {
            gPortPhotoDist *= (1.0f + 1.2f * dt);
            if (gPortPhotoDist > 3.5f) gPortPhotoDist = 3.5f;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
            gPortPhotoYaw = 0.0f;
            gPortPhotoPitch = 0.0f;
            gPortPhotoDist = 1.0f;
        }

        gPortPhotoYaw = fmodf(gPortPhotoYaw, 6.2831853f);
        if (gPortPhotoPitch > 1.5f) gPortPhotoPitch = 1.5f;
        if (gPortPhotoPitch < -1.5f) gPortPhotoPitch = -1.5f;
        // Draw sleek photo mode controls bar if HUD is not hidden
        if (!Port_HudHidden() && gUiPresentW > 0 && gUiPresentH > 0) {
            float sy = (float)gUiPresentH / 448.0f;
            float font_size = 14.0f * sy;
            const char *barText = "[MODO FOTO 360] Orbitar: Flechas/WASD | Zoom: Q/E (L1/R1) | Restablecer: [R] | Ocultar HUD: [H] | Salir: [P]";
            ImFont *font = ImGui::GetFont();
            ImVec2 tsz = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, barText);
            float bx = (float)gUiPresentX + ((float)gUiPresentW - tsz.x) * 0.5f;
            float by = (float)gUiPresentY + (float)gUiPresentH - 32.0f * sy;

            dl->AddRectFilled(ImVec2(bx - 14.0f, by - 4.0f), ImVec2(bx + tsz.x + 14.0f, by + tsz.y + 4.0f), IM_COL32(15, 20, 30, 220), 8.0f);
            dl->AddRect(ImVec2(bx - 14.0f, by - 4.0f), ImVec2(bx + tsz.x + 14.0f, by + tsz.y + 4.0f), IM_COL32(255, 180, 0, 200), 8.0f, 0, 1.5f);
            dl->AddText(font, font_size, ImVec2(bx, by), IM_COL32(255, 255, 255, 255), barText);
        }
    }

    // Toast notification
    if (!Port_HudHidden() && sPhotoToastTimer > 0.0f && gUiPresentW > 0 && gUiPresentH > 0) {
        sPhotoToastTimer -= dt;
        float alpha = sPhotoToastTimer > 0.5f ? 1.0f : (sPhotoToastTimer / 0.5f);
        int a_byte = (int)(alpha * 255.0f);
        if (a_byte > 10) {
            float sy = (float)gUiPresentH / 448.0f;
            float font_size = 16.0f * sy;
            ImFont *font = ImGui::GetFont();
            ImVec2 tsz = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, sPhotoToast);
            float tx = (float)gUiPresentX + ((float)gUiPresentW - tsz.x) * 0.5f;
            float ty = (float)gUiPresentY + 28.0f * sy;

            dl->AddRectFilled(ImVec2(tx - 18.0f, ty - 6.0f), ImVec2(tx + tsz.x + 18.0f, ty + tsz.y + 6.0f), IM_COL32(10, 12, 18, (int)(a_byte * 0.9f)), 10.0f);
            dl->AddRect(ImVec2(tx - 18.0f, ty - 6.0f), ImVec2(tx + tsz.x + 18.0f, ty + tsz.y + 6.0f), IM_COL32(255, 190, 40, a_byte), 10.0f, 0, 1.5f);
            dl->AddText(font, font_size, ImVec2(tx, ty), IM_COL32(255, 255, 255, a_byte), sPhotoToast);
        }
    }
    SDL_UnlockSpinlock(&sFeatureLock);
}

// The meter: frames shown and the game's speed, and in an online match the connection. A setting (Video tab).
static bool meter_on(void) {
    if (sMeter < 0) {
        sMeter = getenv("BT3_METER") != NULL ? atoi(getenv("BT3_METER")) != 0 : Port_Setting("meter", 0) != 0;
    }
    return sMeter != 0;
}
static float sFps, sSpeed; // frames shown per second; the game's vertical blanks per second as a share of 60

static void meter_count(void) { // once per frame shown
    static Uint64 t0;
    static unsigned frames, blanks0;
    Uint64 now = SDL_GetTicks();
    frames++;
    if (t0 == 0) {
        t0 = now;
        blanks0 = gPortLiveBlanks;
    } else if (now - t0 >= 500) {
        sFps = (float)frames * 1000.0f / (float)(now - t0);
        sSpeed = (float)(gPortLiveBlanks - blanks0) * 1000.0f / (float)(now - t0) / 60.0f * 100.0f;
        frames = 0;
        blanks0 = gPortLiveBlanks;
        t0 = now;
    }
}

static void meter_draw(void) {
    int n[8];
    ImGui::SetNextWindowPos(ImVec2(8.0f, 8.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.6f);
    if (ImGui::Begin("##meter", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                                          ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings)) {
        // a fight shows 30 frames a second and the menus 60, as on the console; the speed is what should stay at 100%
        ImGui::Text("%.0f FPS   velocidad %.0f%%", sFps, sSpeed);
        if (Port_NetStats(n)) {
            if (n[0] >= 0) {
                ImGui::Text("ping %d ms%s", n[0], Port_LobbyRelayed() ? " (intermediario)" : "");
            } else {
                ImGui::TextUnformatted("ping ...");
            }
            if (n[5] > 0) {
                ImGui::Text("correcciones %d/s, %.1f cuadros", n[1], (float)n[2] / 10.0f);
            } else {
                ImGui::TextUnformatted("predicción desactivada");
            }
            ImGui::Text("espera %d ms/s (%d veces)", n[4], n[3]);
            ImGui::Text("retardo %d%s, predicción hasta %d", n[6], n[7] ? " (auto)" : "", n[5]);
        }
    }
    ImGui::End();
}

// The name of an added stage or song as text, in the rectangle the menu gives (the strip images' place): cream
// letters with a dark outline, as tall as the rectangle allows, centred for a stage and from the left for a song.
static void name_text(ImDrawList *dl, const char *text, float x, float y, float w, float h, bool centre) {
    ImFont *font = ImGui::GetFont();
    float size = h * (centre ? 0.42f : 0.72f);
    ImVec2 ext = font->CalcTextSizeA(size, 10000.0f, 0.0f, text);
    if (ext.x > w * 0.96f && ext.x > 0.0f) { // a long name: smaller, to fit
        size *= w * 0.96f / ext.x;
        ext = font->CalcTextSizeA(size, 10000.0f, 0.0f, text);
    }
    float tx = centre ? x + (w - ext.x) * 0.5f : x + h * 0.15f, ty = y + (h - ext.y) * 0.5f, o = size * 0.07f + 1.0f;
    for (int k = 0; k < 8; k++) {
        static const float dx[8] = {-1, 0, 1, -1, 1, -1, 0, 1}, dy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
        dl->AddText(font, size, ImVec2(tx + dx[k] * o, ty + dy[k] * o), IM_COL32(40, 20, 0, 255), text);
    }
    dl->AddText(font, size, ImVec2(tx, ty), IM_COL32(255, 236, 170, 255), text);
}

/* Names in the game's own lettering (namefont.c): a name with no picture in a strip is put together from the letters
   of the disc's name pictures, once, and kept as a texture while it is the one shown (a few are kept, for going up
   and down a list). */
struct FontName {
    SDL_GPUTexture *tex; // NULL: this name cannot be drawn that way (a character the disc's names do not have)
    int style, w, h, choice, resolution;
    char text[64];
    bool used;
};
static FontName sFontNames[12];
static unsigned sFontNext;

static const FontName *font_name(int style, const char *text) {
    int choice = MenuText_GetFont();
    int resolution = choice ? 4 : 1; // (a font's strip: 4 times the disc's, for a window larger than the game's picture)
    if (text == NULL || text[0] == '\0' || (choice == 0 && !NameFont_Ready())) { // (not ready: asked again next frame)
        return NULL;
    }
    for (FontName &n : sFontNames) {
        if (n.used && n.style == style && n.choice == choice && n.resolution == resolution && strncmp(n.text, text, sizeof(n.text) - 1) == 0) {
            return n.tex != NULL ? &n : NULL;
        }
    }
    FontName &n = sFontNames[sFontNext++ % (sizeof(sFontNames) / sizeof(sFontNames[0]))];
    if (n.used && n.tex != NULL) {
        if (sGL) {
            GsGl_StripTextureFree((unsigned)(intptr_t)n.tex);
        } else {
            SDL_ReleaseGPUTexture(sDevice, n.tex);
        }
    }
    n.tex = NULL;
    n.w = n.h = 0;
    n.used = true;
    n.style = style;
    n.choice = choice; n.resolution = resolution;
    SDL_strlcpy(n.text, text, sizeof(n.text));
    unsigned char *pix;
    if (choice) {
        pix = style == NF_STAGE ? MenuText_RenderStage(n.text, resolution, &n.w, &n.h) :
                                  MenuText_RenderSong(n.text, style == NF_SONG_LIT, resolution, &n.w, &n.h);
    } else {
        pix = NameFont_Compose(style, n.text, &n.w, &n.h);
    }
    if (pix != NULL) {
        n.tex = make_strip(pix, (size_t)n.w * n.h * 4, (uint32_t)n.w, (uint32_t)n.h, 1); // (frees pix)
    }
    if (choice) { n.w /= resolution; n.h /= resolution; }
    return n.tex != NULL ? &n : NULL;
}

/* Draws such a name over the place of the disc's name picture: (x, y) is the corner of that picture's row in the
   window, kx and ky are window pixels per pixel of the row (512 wide). A name longer than the row is made smaller
   to fit, about the row's middle line; a stage name stays in the middle, a song name keeps its left edge. */
static void font_draw(ImDrawList *dl, const FontName *n, float x, float y, float kx, float ky, bool centre) {
    float k = n->w > 512 ? 512.0f / (float)n->w : 1.0f;
    float w = (float)n->w * kx * k, h = (float)n->h * ky * k;
    float x0 = centre ? x + (512.0f * kx - w) * 0.5f : x;
    float y0 = y + ((float)n->h * ky - h) * 0.5f;
    dl->AddImage(ImTextureRef((ImTextureID)(intptr_t)n->tex), ImVec2(x0, y0), ImVec2(x0 + w, y0 + h));
}

// The online chat, in the lower left corner. Put away, it shows the newest lines for a few seconds each in a dark
// rounded card, fading out, and takes no input. While a line is typed it shows the last ones, input field and
// allows selecting/copying text with mouse.
enum { CHAT_SHOWN_MS = 8000, CHAT_FADE_MS = 2000 };

static bool chat_recent(void) {
    PortChatLine l;
    unsigned n = PortChat_Count();
    return n > 0 && PortChat_Line(n - 1, &l) && SDL_GetTicks() - l.atMs < CHAT_SHOWN_MS;
}

static void chat_draw(void) {
    ImGuiIO &io = ImGui::GetIO();
    if (sChatSize < 0) {
        sChatSize = Port_Setting("chat_size", 1);
        if (sChatSize < 0 || sChatSize > 2) sChatSize = 1;
    }
    int rowsTyping = 6, rowsPassive = 4; // Normal: as the chat always was
    float wFactor = 0.42f, minW = 320.0f, maxW = 100000.0f;
    if (sChatSize == 0) { // Compact
        rowsTyping = 4;
        rowsPassive = 2;
        wFactor = 0.28f;
        minW = 240.0f;
        maxW = 380.0f;
    } else if (sChatSize == 2) { // Large
        rowsTyping = 8;
        rowsPassive = 6;
        wFactor = 0.52f;
        minW = 400.0f;
    }
    float w = io.DisplaySize.x * wFactor;
    if (w < minW) w = minW;
    if (w > maxW) w = maxW;
    unsigned n = PortChat_Count(), rows = sChat ? rowsTyping : rowsPassive, first = n > rows ? n - rows : 0;
    Uint64 now = SDL_GetTicks();
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;
    if (!sChat) {
        flags |= ImGuiWindowFlags_NoInputs;
    }

    // In the lower left corner (growing upwards) until it is dragged; then where it was left (off the screen: back).
    static ImVec2 sChatPos;
    static bool sChatMoved;
    if (sChatMoved && (sChatPos.x < 0.0f || sChatPos.y < 0.0f || sChatPos.x > io.DisplaySize.x - 50.0f ||
                       sChatPos.y > io.DisplaySize.y - 50.0f)) {
        sChatMoved = false;
    }
    if (!sChatMoved) {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.02f, io.DisplaySize.y * 0.86f), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
    } else if (!sChat) {
        ImGui::SetNextWindowPos(sChatPos, ImGuiCond_Always);
    }

    float bgAlpha = 0.85f;
    if (!sChat) {
        float newestAlpha = 1.0f;
        PortChatLine lastLine;
        if (n > 0 && PortChat_Line(n - 1, &lastLine)) {
            Uint64 age = now - lastLine.atMs;
            if (age >= CHAT_SHOWN_MS) {
                return;
            }
            if (age > CHAT_SHOWN_MS - CHAT_FADE_MS) {
                newestAlpha = (float)(CHAT_SHOWN_MS - age) / (float)CHAT_FADE_MS;
            }
        }
        bgAlpha = 0.65f * newestAlpha;
    }
    ImGui::SetNextWindowBgAlpha(bgAlpha);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, sChat ? 1.0f : 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 2.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.75f, 0.2f, 0.5f));

    if (sChat && sChatFocus) {
        ImGui::SetNextWindowFocus();
    }
    if (ImGui::Begin("##chat", NULL, flags)) {
        if (sChat && ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            sChatMoved = true;
        }
        if (sChat && sChatMoved) {
            sChatPos = ImGui::GetWindowPos();
        }
        const char *names[2] = {Port_NetName(Port_NetMe()), Port_NetName(!Port_NetMe())};
        if (sChat) {
            ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.28f, 1.0f), "Chat (Tab)");
            ImGui::SameLine();
            ImGui::TextDisabled("::");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Arrastra para mover el chat. Haz clic en un mensaje para copiarlo.");
            }
            ImGui::SameLine(w - 70.0f);
            bool mute = PortChat_Muted() != 0;
            if (ImGui::Checkbox("Silenciar", &mute)) {
                PortChat_MutedSet(mute);
                sChatFocus = true;
            }
            ImGui::Separator();
        }

        for (unsigned i = first; i < n; i++) {
            PortChatLine l;
            float alpha = 1.0f;
            if (!PortChat_Line(i, &l)) {
                continue;
            }
            if (!sChat) {
                Uint64 age = now - l.atMs;
                if (age >= CHAT_SHOWN_MS) {
                    continue;
                }
                if (age > CHAT_SHOWN_MS - CHAT_FADE_MS) {
                    alpha = (float)(CHAT_SHOWN_MS - age) / (float)CHAT_FADE_MS;
                }
            }
            const char *sender = l.sender[0] != '\0' ? l.sender : (names[l.mine ? 0 : 1][0] != '\0' ? names[l.mine ? 0 : 1] : (l.mine ? "Tú" : "Rival"));
            ImGui::PushID((int)i);
            ImVec4 nameCol = l.mine ? ImVec4(0.35f, 0.85f, 1.0f, alpha) : ImVec4(1.0f, 0.82f, 0.28f, alpha);
            ImGui::TextColored(nameCol, "%s:", sender);
            ImGui::SameLine(0.0f, 4.0f);

            ImVec2 at = ImGui::GetCursorScreenPos();
            if (!sChat) {
                ImGui::GetWindowDrawList()->AddText(NULL, 0.0f, ImVec2(at.x + 1.0f, at.y + 1.0f), IM_COL32(0, 0, 0, (int)(230.0f * alpha)), l.text, NULL, w - (at.x - ImGui::GetWindowPos().x));
            }
            ImGui::PushTextWrapPos(w);
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, alpha), "%s", l.text);
            ImGui::PopTextWrapPos();
            if (sChat) { // a click copies the line (a room code or an address is passed on this way); a drag does not
                static unsigned sCopiedIdx;
                static Uint64 sCopiedMs;
                bool copied = sCopiedIdx == i + 1 && now - sCopiedMs < 1500;
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(copied ? "Copiado" : "Haz clic para copiar");
                    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] < 16.0f) {
                        SDL_SetClipboardText(l.text);
                        sCopiedIdx = i + 1;
                        sCopiedMs = now;
                        sChatFocus = true;
                    }
                }
            }
            ImGui::PopID();
        }

        if (sChat) {
            if (sChatFocus) {
                ImGui::SetKeyboardFocusHere();
                sChatFocus = false;
            }
            ImGui::SetNextItemWidth(w);
            if (ImGui::InputText("##chatline", sChatBuf, sizeof(sChatBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                const char *p = sChatBuf;
                while (*p == ' ') {
                    p++;
                }
                if (*p == '\0') {
                    chat_close();
                } else if (PortChat_Say(sChatBuf)) {
                    sChatBuf[0] = '\0';
                    chat_close();
                } else { // (said within a second of the line before: kept, to be sent again)
                    sChatSoon = true;
                    sChatFocus = true;
                }
            }
            ImGui::TextDisabled(sChatSoon ? "1 mensaje/s: Intro para enviar" : "Intro: enviar   Esc/Tab: cerrar   Ctrl+V: pegar");
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(4);
}

// Builds this frame's picture of the overlay (the settings or the online window, the line over the picture).
// false: there is nothing to draw. The same for both back ends; each then draws ImGui's data its own way.
static bool frame_build(void) {
    PortInput_TboClipboard();
    { // a keypad of Tenkaichi Budokai Online takes text: the system's text input is on for it (letters as typed)
        static SDL_Window *on = NULL;
        SDL_Window *want = gPortTboTyping && !sOpen && !sQuit && !sChat ? SDL_GetKeyboardFocus() : NULL;
        if (want != on) {
            if (on != NULL) { SDL_StopTextInput(on); }
            if (want != NULL) { SDL_StartTextInput(want); }
            on = want;
        }
    }
    if (sQuit && !PortQuit_Active() && !PortQuit_Failed()) {
        sQuit = false;
        input_owner();
        PortA11y_Say(PortA11y_Label("Volver al juego"), 1);
    }
    if (sReady && gPortNetWindowClose) { // back from an online session: the window it was started from closes
        gPortNetWindowClose = 0;
        gPortNetMenuRequest = 0;
        set_open(false);
    } else if (sReady && gPortNetMenuRequest) { // Dragon Net Battle was chosen in the game's main menu
        gPortNetMenuRequest = 0;
        net_open();
    }
    if (sReady && !sUpdTold && update_latest() > PORT_RELEASE && SDL_GetTicks() >= sNoticeUntil) {
        int latest = update_latest(); // (told once, and not over another notice)
        sUpdTold = true;
        SDL_snprintf(sNotice, sizeof(sNotice), "Actualización disponible: 0.%d.%d. F1 para abrir la página de descarga.", latest / 100, latest % 100);
        sNoticeUntil = SDL_GetTicks() + 10000;
    }
    int coopKnown=0, coopTotal=0;
    int coopLobby=Port_CoopLobbyStatus(&coopKnown,&coopTotal);
    bool notice = sNotice[0] != '\0' && SDL_GetTicks() < sNoticeUntil;
    if (sNoticeNew) { // (set from the game's side: the time starts when it is first drawn)
        sNoticeNew = 0;
        sNoticeUntil = SDL_GetTicks() + 6000;
        notice = sNotice[0] != '\0';
    }
    // the name of a stage added from outside the disc, over the stage select (a strip of pre-rendered names)
    bool nameOn = gUiNameReady != 0 && gUiNameIdx >= 0 && gUiNameIdx < PortStages_Count();
    bool songOn = gUiSongReady != 0 && gUiSongIdx >= 0 && gUiSongIdx < PortSongs_Count(); // (the same for an added song)
    bool overlay = nameOn || songOn;
    bool meter = sReady && meter_on();
    // the players' names under the gauges of an online fight
    bool names = sReady && Port_NetActive() && BtlChar_FindByObjId(0) != NULL && (Port_NetName(0)[0] != '\0' || Port_NetName(1)[0] != '\0');
    bool bars = sReady && bars_on();
    PortModesView modes; // the main menu's hint of the extra modes
    Port_ModesView(&modes);
    bool modesOn = sReady && modes.hint;
    bool online = sReady && Port_NetActive() && PortChat_Enabled();
    if (sChat && !online) { // (the match ended while a line was typed)
        chat_close();
    }
    bool chat = online && (sChat || chat_recent());
    meter_count();
    sBuilt = false;
    if (!sReady || (!coopLobby && !sOpen && !sQuit && !notice && !overlay && !meter && !names && !bars && !modesOn && !chat && !feature_overlay_on())) {
        return false;
    }
    if (sPadHold && sCapKind != 2) { // a binding was taken: the controller works the window again once nothing is held
        int count = 0;
        SDL_JoystickID *ids = SDL_GetGamepads(&count);
        bool held = false;
        for (int i = 0; i < count; i++) {
            SDL_Gamepad *g = SDL_GetGamepadFromID(ids[i]);
            for (int b = 0; g != NULL && b < SDL_GAMEPAD_BUTTON_COUNT; b++) {
                held = held || SDL_GetGamepadButton(g, (SDL_GamepadButton)b);
            }
        }
        SDL_free(ids);
        sPadHold = held;
    }
    // (the button that becomes the binding would also press the row it is bound in; while a chat line is typed the
    // controller is the game's)
    if (sPadHold || (sChat && !sOpen)) {
        ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    } else {
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    }
    sBuilt = true;
    if (sGL) {
        ImGui_ImplOpenGL3_NewFrame();
    } else {
        ImGui_ImplSDLGPU3_NewFrame();
    }
    ImGui_ImplSDL3_NewFrame();
    fonts_apply(); // (a face chosen for the fight panels, between two frames)
    ImGui::NewFrame();
    Port_UpdateAndDrawPhotoMode(ImGui::GetForegroundDrawList());
    if (!sOpen) pc_teams_preview();
    if (coopLobby) {
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * .5f, ImGui::GetIO().DisplaySize.y * .5f), ImGuiCond_Always, ImVec2(.5f,.5f));
        ImGui::SetNextWindowBgAlpha(.96f);
        if (ImGui::Begin("Sala Raid online", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
            if (coopLobby==1) ImGui::TextUnformatted("Comprobando datos del juego y paquetes...");
            else ImGui::Text("Esperando jugadores: %d / %d",coopKnown,coopTotal);
            ImGui::TextUnformatted("La partida comienza cuando todos estan conectados.");
            ImGui::TextDisabled("Cierra la ventana del juego para cancelar.");
        }
        ImGui::End();
    }
    if (bars && !Port_HudHidden()) {
        bars_draw();
    }
    if (modesOn) {
        modes_draw(modes);
    }
    if (sOpen) {
        if (sNet) {
            build_net();
        } else {
            build();
        }
    }
    if (sQuit && PortQuit_Failed()) {
        build_quit();
    }
    if (meter && !Port_HudHidden()) {
        meter_draw();
    }
    if (chat && !Port_HudHidden()) {
        chat_draw();
    }
    Port_DrawFeatureHints(ImGui::GetForegroundDrawList());
    if (names && !Port_HudHidden() && GsDraw_HudShown() && gUiPresentW > 0 && gUiPresentH > 0) { // (with the HUD: not over an intro)
        // Under each side's health bar (the game's 512 x 448 page: the bars end at about y 50). The left place is
        // side 0's, or this player's own side when the HUD's sides are swapped.
        ImDrawList *dl = ImGui::GetBackgroundDrawList();
        float sx = (float)gUiPresentW / 512.0f, sy = (float)gUiPresentH / 448.0f;
        float size = 15.0f * sy;
        ImFont *font = ImGui::GetFont();
        for (int place = 0; place < 2; place++) {
            const char *text = Port_NetName(place ^ Port_HudSwap());
            if (text[0] == '\0') {
                continue;
            }
            ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
            // 106 of the HUD's own pixels in from its outer edge: clear of the panel of a team's waiting fighters,
            // which reaches to about 100 (the name sat on it at 62: reported). The HUD is narrowed in a wide
            // picture and sits at the edges or about the middle (gs_draw.c), and the name goes with it.
            float narrow = Port_IsWide() ? 1333.333f / (float)Port_AspectMilli() : 1.0f;
            float in = Port_HudLayout() == 2 ? 256.0f + (106.0f - 256.0f) * narrow : 106.0f * narrow;
            float x = place == 0 ? (float)gUiPresentX + in * sx : (float)gUiPresentX + (512.0f - in) * sx - ts.x;
            float y = (float)gUiPresentY + 56.0f * sy;
            for (int ox = -1; ox <= 1; ox++) {
                for (int oy = -1; oy <= 1; oy++) {
                    if (ox != 0 || oy != 0) {
                        dl->AddText(font, size, ImVec2(x + (float)ox * 1.5f, y + (float)oy * 1.5f), IM_COL32(0, 0, 0, 220), text);
                    }
                }
            }
            dl->AddText(font, size, ImVec2(x, y), IM_COL32(255, 255, 255, 255), text);
        }
    }
    if (notice && !Port_HudHidden()) { // a line over the picture for a few seconds (the end of an online match)
        ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.12f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.85f);
        if (ImGui::Begin("##notice", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                                               ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::TextUnformatted(sNotice);
        }
        ImGui::End();
    }
    if (overlay && !Port_HudHidden() && gUiPresentW > 0 && gUiPresentH > 0) {
        /* Each name's rectangle is in the game's 512x448 pixels: map it through the picture's rectangle in the
           window (the letterbox gs_gpu.c just blitted into). */
        float sx = (float)gUiPresentW / 512.0f;
        float sy = (float)gUiPresentH / 448.0f;
        // With a menu font every name is drawn here, so the names belong to the game picture, behind the settings
        // and other windows. With "Original" only an added stage or song is, in front of everything as before.
        ImDrawList *dl = MenuText_GetFont() != 0 ? ImGui::GetBackgroundDrawList() : ImGui::GetForegroundDrawList();

        if (nameOn && MenuText_GetFont() != 0) {
            const FontName *fn = font_name(NF_STAGE, PortStages_Name(gUiNameIdx));
            float x = (float)gUiPresentX + (float)gUiNameX * sx, y = (float)gUiPresentY + (float)gUiNameY * sy;
            if (fn != NULL) {
                float kx = (float)gUiNameW * sx / 384.0f, ky = (float)gUiNameH * sy / 48.0f;
                font_draw(dl, fn, x - 64.0f * kx, y + (1.5f - 14.0f) * ky, kx, ky, true);
            } else {
                name_text(dl, PortStages_Name(gUiNameIdx), x, y, (float)gUiNameW * sx, (float)gUiNameH * sy, true);
            }
        } else if (nameOn && gUiNameIdx >= (int)sNameCount) { // no pre-rendered name for this one
            /* In the game's lettering if the disc's names have all its characters, else as plain text. The menu's
               rectangle is the strip image's (384x48, set 64 right and 14 down in the 512x64 row of the disc's own
               name picture); the lettering goes where that row is. The menu's movie draws the row a little below
               the clip's position: kNameDy, measured by drawing a disc name both ways (the same for the songs). */
            static const float kNameDy = 1.5f;
            const FontName *fn = font_name(NF_STAGE, PortStages_Name(gUiNameIdx));
            if (fn != NULL) {
                float kx = (float)gUiNameW * sx / 384.0f, ky = (float)gUiNameH * sy / 48.0f;
                font_draw(dl, fn, (float)gUiPresentX + (float)gUiNameX * sx - 64.0f * kx, (float)gUiPresentY + (float)gUiNameY * sy + (kNameDy - 14.0f) * ky, kx, ky, true);
            } else {
                name_text(dl, PortStages_Name(gUiNameIdx), (float)gUiPresentX + (float)gUiNameX * sx, (float)gUiPresentY + (float)gUiNameY * sy,
                          (float)gUiNameW * sx, (float)gUiNameH * sy, true);
            }
        } else if (nameOn) {
            float x = (float)gUiPresentX + (float)gUiNameX * sx;
            float y = (float)gUiPresentY + (float)gUiNameY * sy;
            float w = (float)gUiNameW * sx;
            float h = (float)gUiNameH * sy;
            dl->AddImage(ImTextureRef((ImTextureID)(intptr_t)sNameTex), ImVec2(x, y), ImVec2(x + w, y + h),
                         ImVec2(0.0f, (float)gUiNameIdx / (float)sNameCount),
                         ImVec2(1.0f, (float)(gUiNameIdx + 1) / (float)sNameCount));
        }
        if (songOn && MenuText_GetFont() != 0) {
            const FontName *fn = font_name(gUiSongLit ? NF_SONG_LIT : NF_SONG, PortSongs_Name(gUiSongIdx));
            float x = (float)gUiPresentX + (float)gUiSongX * sx, y = (float)gUiPresentY + (float)gUiSongY * sy;
            if (fn != NULL) {
                font_draw(dl, fn, x, y, (float)gUiSongW * sx / 512.0f, (float)gUiSongH * sy / 32.0f, false);
            } else {
                name_text(dl, PortSongs_Name(gUiSongIdx), x, y, (float)gUiSongW * sx, (float)gUiSongH * sy, false);
            }
        } else if (songOn && gUiSongIdx >= (int)sSongCount) {
            // (the rectangle is the 512x32 row of the disc's song-name picture itself)
            const FontName *fn = font_name(gUiSongLit ? NF_SONG_LIT : NF_SONG, PortSongs_Name(gUiSongIdx));
            if (fn != NULL) {
                static const float kSongDx = -0.2f, kSongDy = 2.35f;
                float kx = (float)gUiSongW * sx / 512.0f, ky = (float)gUiSongH * sy / 32.0f;
                font_draw(dl, fn, (float)gUiPresentX + (float)gUiSongX * sx + kSongDx * kx, (float)gUiPresentY + (float)gUiSongY * sy + kSongDy * ky, kx, ky, false);
            } else {
                name_text(dl, PortSongs_Name(gUiSongIdx), (float)gUiPresentX + (float)gUiSongX * sx, (float)gUiPresentY + (float)gUiSongY * sy,
                          (float)gUiSongW * sx, (float)gUiSongH * sy, false);
            }
        } else if (songOn) {
            float x = (float)gUiPresentX + (float)gUiSongX * sx;
            float y = (float)gUiPresentY + (float)gUiSongY * sy;
            float w = (float)gUiSongW * sx;
            float h = (float)gUiSongH * sy;
            dl->AddImage(ImTextureRef((ImTextureID)(intptr_t)sSongTex), ImVec2(x, y), ImVec2(x + w, y + h),
                         ImVec2(0.0f, (float)gUiSongIdx / (float)sSongCount),
                         ImVec2(1.0f, (float)(gUiSongIdx + 1) / (float)sSongCount));
        }
    }
    ImGui::Render();
    return true;
}

void Ui_Draw(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *target) {
    if (!sGL && frame_build()) {
        Ui_DrawAgain(cmd, target);
    }
}

// The OpenGL back end's turn: over the presented picture, just before the buffers are swapped.
void Ui_DrawGL(void) {
    if (sGL && frame_build()) {
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
}

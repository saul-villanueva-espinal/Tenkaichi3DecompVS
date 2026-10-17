#ifndef SYS_LOAD_SPRITE_H
#define SYS_LOAD_SPRITE_H
#include "types.h"
/* LoadSprite.flags */
#define LOAD_SPR_DRAW 1   /* textured quad; 0 = skip this entry */
#define LOAD_SPR_END 2    /* end of a sprite run */
#define LOAD_SPR_CLEAR 5  /* full-screen quad in a flat colour (drawn with a NULL resource) */

/* One entry of the list drawn by Sprite_DrawList (0x38 bytes). */
typedef struct LoadSprite {
    /* 0x00 */ s32 flags; /* LOAD_SPR_* */
    /* 0x04 */ s32 x0;
    /* 0x08 */ s32 y0;
    /* 0x0C */ s32 x1;
    /* 0x10 */ s32 y1;
    /* 0x14 */ s32 u0;
    /* 0x18 */ s32 v0;
    /* 0x1C */ s32 u1;
    /* 0x20 */ s32 v1;
    /* 0x24 */ s32 tex;   /* index of the 0x40-byte texture entry in the resource */
    /* 0x28 */ s32 r;
    /* 0x2C */ s32 g;
    /* 0x30 */ s32 b;
    /* 0x34 */ s32 a;
} LoadSprite;

#endif

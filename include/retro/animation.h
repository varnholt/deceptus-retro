#ifndef RETRO_ANIMATION_H
#define RETRO_ANIMATION_H
#include "retro/types.h"

/* a target-independent clock for baked scenery, sprites and interface clips. */
typedef struct retro_animation {
    retro_u16 frame, elapsed;
} retro_animation;

void retro_animation_reset(retro_animation *animation);
/* invalid clips stay at frame zero; returns one if the displayed frame changes. */
retro_u8 retro_animation_advance(retro_animation *animation, retro_u16 frames, retro_u16 frame_ticks, retro_u16 ticks,
                                 retro_u8 loop);
#endif

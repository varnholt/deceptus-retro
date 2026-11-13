#ifndef DECEPTUS_ANIMATION_H
#define DECEPTUS_ANIMATION_H
#include "deceptus/types.h"

/* a target-independent clock for baked scenery, sprites and interface clips. */
typedef struct deceptus_animation {
    deceptus_u16 frame, elapsed;
} deceptus_animation;

void deceptus_animation_reset(deceptus_animation *animation);
/* invalid clips stay at frame zero; returns one if the displayed frame changes. */
deceptus_u8 deceptus_animation_advance(deceptus_animation *animation, deceptus_u16 frames, deceptus_u16 frame_ticks,
                                       deceptus_u16 ticks, deceptus_u8 loop);
deceptus_u8 deceptus_animation_advance_timed(deceptus_animation *animation, deceptus_u16 frames,
                                             const deceptus_u16 *durations, deceptus_u16 ticks, deceptus_u8 loop);
#endif

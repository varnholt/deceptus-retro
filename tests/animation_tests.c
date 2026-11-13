#include "retro/animation.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    static const retro_u16 durations[] = {120, 2, 1};
    retro_animation animation;
    retro_u16 index;
    retro_animation_reset(&animation);
    assert(!retro_animation_advance(&animation, 12, 6, 5, 1));
    assert(retro_animation_advance(&animation, 12, 6, 1, 1));
    assert(animation.frame == 1 && animation.elapsed == 0);
    assert(!retro_animation_advance(&animation, 12, 6, 0, 1));
    retro_animation_advance(&animation, 12, 6, 71, 1);
    assert(animation.frame == 0 && animation.elapsed == 5);
    /* repeated playback must remain stable beyond the engine's 16-bit tick wrap. */
    retro_animation_reset(&animation);
    for (index = 0; index < 40000; ++index) {
        retro_animation_advance(&animation, 12, 6, 2, 1);
    }
    assert(animation.frame == (80000UL / 6) % 12 && animation.elapsed == 80000UL % 6);
    retro_animation_advance(&animation, 12, 6, 65535, 0);
    assert(animation.frame == 11 && animation.elapsed == 0);
    assert(!retro_animation_advance(&animation, 12, 6, 65535, 0));
    assert(retro_animation_advance(&animation, 0, 6, 10, 1));
    assert(animation.frame == 0 && animation.elapsed == 0);
    retro_animation_advance(&animation, 65535, 65535, 65535, 1);
    assert(animation.frame == 1 && animation.elapsed == 0);
    retro_animation_reset(&animation);
    retro_animation_advance_timed(&animation, 3, durations, 119, 1);
    assert(animation.frame == 0 && animation.elapsed == 119);
    retro_animation_advance_timed(&animation, 3, durations, 1, 1);
    assert(animation.frame == 1 && animation.elapsed == 0);
    retro_animation_advance_timed(&animation, 3, durations, 3, 1);
    assert(animation.frame == 0 && animation.elapsed == 0);
    puts("animation playback checks passed");
    return 0;
}

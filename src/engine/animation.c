#include "retro/animation.h"

/* bounded integer division without a platform compiler-runtime dependency. */
static retro_u32 divide(retro_u32 value, retro_u16 divisor, retro_u16 *remainder)
{
    retro_u32 denominator, bit, result;
    denominator = divisor;
    bit = 1;
    result = 0;
    while (denominator <= (value >> 1)) {
        denominator <<= 1;
        bit <<= 1;
    }
    while (bit) {
        if (value >= denominator) {
            value -= denominator;
            result |= bit;
        }
        denominator >>= 1;
        bit >>= 1;
    }
    *remainder = (retro_u16)value;
    return result;
}

void retro_animation_reset(retro_animation *animation)
{
    animation->frame = 0;
    animation->elapsed = 0;
}

retro_u8 retro_animation_advance_timed(retro_animation *animation, retro_u16 frames, const retro_u16 *durations,
                                       retro_u16 ticks, retro_u8 loop)
{
    retro_u16 previous, remaining, step;
    previous = animation->frame;
    if (!frames || !durations || animation->frame >= frames) {
        retro_animation_reset(animation);
        return previous != 0;
    }
    while (ticks) {
        if (!durations[animation->frame] || animation->elapsed >= durations[animation->frame]) {
            retro_animation_reset(animation);
            return previous != 0;
        }
        remaining = durations[animation->frame] - animation->elapsed;
        step = ticks < remaining ? ticks : remaining;
        animation->elapsed += step;
        ticks -= step;
        if (animation->elapsed == durations[animation->frame]) {
            animation->elapsed = 0;
            if (animation->frame + 1 < frames) {
                ++animation->frame;
            } else if (loop) {
                animation->frame = 0;
            } else {
                return previous != animation->frame;
            }
        }
    }
    return previous != animation->frame;
}

retro_u8 retro_animation_advance(retro_animation *animation, retro_u16 frames, retro_u16 frame_ticks, retro_u16 ticks,
                                 retro_u8 loop)
{
    retro_u32 elapsed, frame;
    retro_u16 previous, remainder;
    previous = animation->frame;
    if (!frames || !frame_ticks) {
        retro_animation_reset(animation);
        return previous != 0;
    }
    elapsed = (retro_u32)animation->elapsed + ticks;
    frame = (retro_u32)animation->frame + divide(elapsed, frame_ticks, &remainder);
    if (loop) {
        animation->elapsed = remainder;
        divide(frame, frames, &remainder);
        animation->frame = remainder;
    } else if (frame >= frames - 1U) {
        animation->frame = frames - 1;
        animation->elapsed = 0;
    } else {
        animation->frame = (retro_u16)frame;
        animation->elapsed = remainder;
    }
    return previous != animation->frame;
}

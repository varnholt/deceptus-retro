#include "deceptus/animation.h"

/* bounded integer division without a platform compiler-runtime dependency. */
static deceptus_u32 divide(deceptus_u32 value, deceptus_u16 divisor, deceptus_u16 *remainder)
{
    deceptus_u32 denominator, bit, result;
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
    *remainder = (deceptus_u16)value;
    return result;
}

void deceptus_animation_reset(deceptus_animation *animation)
{
    animation->frame = 0;
    animation->elapsed = 0;
}

deceptus_u8 deceptus_animation_advance_timed(deceptus_animation *animation, deceptus_u16 frames, const deceptus_u16 *durations,
                                       deceptus_u16 ticks, deceptus_u8 loop)
{
    deceptus_u16 previous, remaining, step;
    previous = animation->frame;
    if (!frames || !durations || animation->frame >= frames) {
        deceptus_animation_reset(animation);
        return previous != 0;
    }
    while (ticks) {
        if (!durations[animation->frame] || animation->elapsed >= durations[animation->frame]) {
            deceptus_animation_reset(animation);
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

deceptus_u8 deceptus_animation_advance(deceptus_animation *animation, deceptus_u16 frames, deceptus_u16 frame_ticks, deceptus_u16 ticks,
                                 deceptus_u8 loop)
{
    deceptus_u32 elapsed, frame;
    deceptus_u16 previous, remainder;
    previous = animation->frame;
    if (!frames || !frame_ticks) {
        deceptus_animation_reset(animation);
        return previous != 0;
    }
    elapsed = (deceptus_u32)animation->elapsed + ticks;
    frame = (deceptus_u32)animation->frame + divide(elapsed, frame_ticks, &remainder);
    if (loop) {
        animation->elapsed = remainder;
        divide(frame, frames, &remainder);
        animation->frame = remainder;
    } else if (frame >= frames - 1U) {
        animation->frame = frames - 1;
        animation->elapsed = 0;
    } else {
        animation->frame = (deceptus_u16)frame;
        animation->elapsed = remainder;
    }
    return previous != animation->frame;
}

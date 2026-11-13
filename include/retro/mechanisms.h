#ifndef RETRO_MECHANISMS_H
#define RETRO_MECHANISMS_H

#include "retro/types.h"

#define RETRO_MECHANISM_CAPACITY 32
#define RETRO_MECHANISM_EVENT_CAPACITY 32
#define RETRO_MECH_CHECKPOINT 1
#define RETRO_MECH_SENSOR 2
#define RETRO_MECH_LEVER 3
#define RETRO_MECH_BLOCK 4
#define RETRO_MECH_DAMAGE 5
#define RETRO_MECH_FLAG 6
#define RETRO_MECH_SPIKES 7
#define RETRO_MECH_COLLAPSING 8
#define RETRO_TIMED_ANIMATION 1
#define RETRO_TIMED_INTERVAL 2
#define RETRO_COLLAPSE_ONCE_TOUCHED 4
#define RETRO_MECH_INVERTED 1
#define RETRO_MECH_MISSING_HANDLE 2
#define RETRO_MECH_ENTER 1
#define RETRO_MECH_LEAVE 2
#define RETRO_MECH_ENABLED 3
#define RETRO_MECH_REACHED 4
#define RETRO_MECH_HIT 5
#define RETRO_MECH_COLLAPSED 6
#define RETRO_MECH_RESTORED 7

typedef struct retro_rect {
    retro_i16 x;
    retro_i16 y;
    retro_i16 width;
    retro_i16 height;
} retro_rect;

/* source_id is the original Tiled object id, not its position in an array. */
typedef struct retro_mechanism_def {
    retro_u16 source_id;
    retro_rect bounds;
    retro_u16 target_first;
    retro_u16 target_count;
    retro_u16 value;
    retro_u8 kind;
    retro_u8 enabled;
    retro_u8 flags;
} retro_mechanism_def;

typedef struct retro_mechanism_timing {
    retro_u16 on_ticks;
    retro_u16 off_ticks;
    retro_u16 collapse_ticks;
    retro_u16 respawn_ticks;
    retro_u16 fade_ticks;
    retro_u8 flags;
} retro_mechanism_timing;

typedef struct retro_mechanism_state {
    retro_u8 enabled;
    retro_u8 visible;
    retro_u8 inside;
    retro_u8 triggered;
    retro_u8 active;
    retro_u8 frame;
    retro_u8 target_frame;
    retro_u8 fraction;
    retro_u8 progress;
    retro_u8 phase;
    retro_u8 pending;
    retro_u8 pending_count;
    retro_u16 timer;
    retro_u16 cooldown;
    retro_u16 idle_ticks;
} retro_mechanism_state;

typedef struct retro_mechanism_event {
    retro_u16 source_id;
    retro_u16 value;
    retro_u8 kind;
} retro_mechanism_event;

typedef struct retro_mechanism_set {
    const retro_mechanism_def *definitions;
    const retro_u16 *targets;
    const retro_mechanism_timing *timings;
    retro_mechanism_state states[RETRO_MECHANISM_CAPACITY];
    retro_mechanism_event events[RETRO_MECHANISM_EVENT_CAPACITY];
    retro_u16 target_count;
    retro_u8 count;
    retro_u8 event_count;
    retro_u8 overflow;
} retro_mechanism_set;

/* invalid or oversized definitions fail without leaving a partially bound set. */
retro_u8 retro_mechanisms_init(retro_mechanism_set *set, const retro_mechanism_def *definitions, retro_u16 count,
                               const retro_u16 *targets, retro_u16 target_count);
void retro_mechanisms_configure(retro_mechanism_set *set, const retro_mechanism_timing *timings);
retro_i16 retro_mechanism_find(const retro_mechanism_set *set, retro_u16 source_id);
retro_u8 retro_mechanism_enable(retro_mechanism_set *set, retro_u16 source_id, retro_u8 enabled);
/* clears the transient event queue and evaluates one frame of rectangle interactions. */
void retro_mechanisms_tick(retro_mechanism_set *set, const retro_rect *player, retro_u8 interact_pressed);
retro_u8 retro_mechanism_solid(const retro_mechanism_set *set, const retro_rect *bounds);

#endif

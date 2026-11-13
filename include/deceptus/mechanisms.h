#ifndef DECEPTUS_MECHANISMS_H
#define DECEPTUS_MECHANISMS_H

#include "deceptus/types.h"

#define DECEPTUS_MECHANISM_CAPACITY 32
#define DECEPTUS_MECHANISM_EVENT_CAPACITY 32
#define DECEPTUS_MECH_CHECKPOINT 1
#define DECEPTUS_MECH_SENSOR 2
#define DECEPTUS_MECH_LEVER 3
#define DECEPTUS_MECH_BLOCK 4
#define DECEPTUS_MECH_DAMAGE 5
#define DECEPTUS_MECH_FLAG 6
#define DECEPTUS_MECH_SPIKES 7
#define DECEPTUS_MECH_COLLAPSING 8
#define DECEPTUS_TIMED_ANIMATION 1
#define DECEPTUS_TIMED_INTERVAL 2
#define DECEPTUS_COLLAPSE_ONCE_TOUCHED 4
#define DECEPTUS_MECH_INVERTED 1
#define DECEPTUS_MECH_MISSING_HANDLE 2
#define DECEPTUS_MECH_ENTER 1
#define DECEPTUS_MECH_LEAVE 2
#define DECEPTUS_MECH_ENABLED 3
#define DECEPTUS_MECH_REACHED 4
#define DECEPTUS_MECH_HIT 5
#define DECEPTUS_MECH_COLLAPSED 6
#define DECEPTUS_MECH_RESTORED 7

typedef struct deceptus_rect {
    deceptus_i16 x;
    deceptus_i16 y;
    deceptus_i16 width;
    deceptus_i16 height;
} deceptus_rect;

/* source_id is the original Tiled object id, not its position in an array. */
typedef struct deceptus_mechanism_def {
    deceptus_u16 source_id;
    deceptus_rect bounds;
    deceptus_u16 target_first;
    deceptus_u16 target_count;
    deceptus_u16 value;
    deceptus_u8 kind;
    deceptus_u8 enabled;
    deceptus_u8 flags;
} deceptus_mechanism_def;

typedef struct deceptus_mechanism_timing {
    deceptus_u16 on_ticks;
    deceptus_u16 off_ticks;
    deceptus_u16 collapse_ticks;
    deceptus_u16 respawn_ticks;
    deceptus_u16 fade_ticks;
    deceptus_u8 flags;
} deceptus_mechanism_timing;

typedef struct deceptus_mechanism_state {
    deceptus_u8 enabled;
    deceptus_u8 visible;
    deceptus_u8 inside;
    deceptus_u8 triggered;
    deceptus_u8 active;
    deceptus_u8 frame;
    deceptus_u8 target_frame;
    deceptus_u8 fraction;
    deceptus_u8 progress;
    deceptus_u8 phase;
    deceptus_u8 pending;
    deceptus_u8 pending_count;
    deceptus_u8 handle_available;
    deceptus_u16 timer;
    deceptus_u16 cooldown;
    deceptus_u16 idle_ticks;
} deceptus_mechanism_state;

typedef struct deceptus_mechanism_event {
    deceptus_u16 source_id;
    deceptus_u16 value;
    deceptus_u8 kind;
} deceptus_mechanism_event;

typedef struct deceptus_mechanism_set {
    const deceptus_mechanism_def *definitions;
    const deceptus_u16 *targets;
    const deceptus_mechanism_timing *timings;
    deceptus_mechanism_state states[DECEPTUS_MECHANISM_CAPACITY];
    deceptus_mechanism_event events[DECEPTUS_MECHANISM_EVENT_CAPACITY];
    deceptus_u16 target_count;
    deceptus_u8 count;
    deceptus_u8 event_count;
    deceptus_u8 overflow;
} deceptus_mechanism_set;

/* invalid or oversized definitions fail without leaving a partially bound set. */
deceptus_u8 deceptus_mechanisms_init(deceptus_mechanism_set *set, const deceptus_mechanism_def *definitions,
                                     deceptus_u16 count, const deceptus_u16 *targets, deceptus_u16 target_count);
void deceptus_mechanisms_configure(deceptus_mechanism_set *set, const deceptus_mechanism_timing *timings);
deceptus_i16 deceptus_mechanism_find(const deceptus_mechanism_set *set, deceptus_u16 source_id);
deceptus_u8 deceptus_mechanism_enable(deceptus_mechanism_set *set, deceptus_u16 source_id, deceptus_u8 enabled);
/* clears the transient event queue and evaluates one frame of rectangle interactions. */
void deceptus_mechanisms_tick(deceptus_mechanism_set *set, const deceptus_rect *player, deceptus_u8 interact_pressed);
deceptus_u8 deceptus_mechanism_solid(const deceptus_mechanism_set *set, const deceptus_rect *bounds);

#endif

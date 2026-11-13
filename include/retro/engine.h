#ifndef RETRO_ENGINE_H
#define RETRO_ENGINE_H

#include "retro/types.h"

#define RETRO_ENTITY_CAPACITY 12
#define RETRO_EVENT_CAPACITY 8
#define RETRO_DRAW_CAPACITY 12
#define RETRO_CELL_SIZE 8

#define RETRO_LEFT 1
#define RETRO_RIGHT 2
#define RETRO_JUMP 4
#define RETRO_UP 8
#define RETRO_PAUSE 16
#define RETRO_RESTART 32

#define RETRO_EMPTY 0
#define RETRO_SOLID 1
#define RETRO_ONE_WAY 2
#define RETRO_HAZARD 4

#define RETRO_PLAYER 1
#define RETRO_PATROL 2
#define RETRO_DOOR 3

#define RETRO_EVENT_JUMP 1
#define RETRO_EVENT_HURT 2
#define RETRO_EVENT_DOOR 3

typedef struct retro_input {
    retro_u16 held;
    retro_u16 pressed;
    retro_u16 released;
} retro_input;

typedef struct retro_room {
    const retro_u8 *collision;
    retro_u16 columns;
    retro_u16 rows;
    retro_i16 spawn_x;
    retro_i16 spawn_y;
    retro_i16 patrol_x;
    retro_i16 patrol_y;
    retro_i16 door_x;
    retro_i16 door_y;
} retro_room;

typedef struct retro_entity {
    retro_fixed x;
    retro_fixed y;
    retro_fixed velocity_x;
    retro_fixed velocity_y;
    retro_u8 kind;
    retro_u8 active;
    retro_u8 grounded;
    retro_u8 facing_left;
    retro_u8 coyote_ticks;
    retro_u8 jump_buffer_ticks;
    retro_u8 animation_ticks;
    retro_u8 frame;
} retro_entity;

typedef struct retro_event {
    retro_u8 kind;
    retro_u8 entity;
} retro_event;

typedef struct retro_draw {
    retro_i16 x;
    retro_i16 y;
    retro_u8 frame;
    retro_u8 palette;
    retro_u8 flip_x;
} retro_draw;

/* backend capabilities are explicit; gameplay does not assume a 256px screen. */
typedef struct retro_video_caps {
    retro_u16 width;
    retro_u16 height;
    retro_u8 max_draws;
} retro_video_caps;

typedef struct retro_frame {
    retro_draw draws[RETRO_DRAW_CAPACITY];
    retro_u8 count;
    retro_i16 camera_x;
    retro_i16 camera_y;
} retro_frame;

typedef struct retro_engine {
    const retro_room *room;
    retro_entity entities[RETRO_ENTITY_CAPACITY];
    retro_event events[RETRO_EVENT_CAPACITY];
    retro_video_caps video;
    retro_u16 previous_buttons;
    retro_u16 ticks;
    retro_u16 door_entries;
    retro_i16 camera_x;
    retro_u8 event_count;
    retro_u8 event_overflow;
    retro_u8 paused;
    retro_u8 health;
    retro_u8 hurt_ticks;
} retro_engine;

/* initializes caller-owned state; room and collision storage must outlive it. */
void retro_init(retro_engine *engine, const retro_room *room, const retro_video_caps *video);
/* advances one 60Hz simulation tick; replaces the transient event queue. */
void retro_tick(retro_engine *engine, retro_u16 buttons);
/* builds a bounded list of logical metasprites without modifying simulation. */
void retro_render(const retro_engine *engine, retro_frame *frame);
/* returns solid outside horizontal/top boundaries, empty below the room. */
retro_u8 retro_cell(const retro_room *room, retro_i16 column, retro_i16 row);
/* claims a zeroed entity slot; returns null when the pool is full. */
retro_entity *retro_spawn(retro_engine *engine, retro_u8 kind, retro_i16 x, retro_i16 y);

#endif

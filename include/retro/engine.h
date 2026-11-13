#ifndef RETRO_ENGINE_H
#define RETRO_ENGINE_H

#include "retro/mechanisms.h"
#include "retro/types.h"

#define RETRO_ENTITY_CAPACITY 12
#define RETRO_EVENT_CAPACITY 8
#define RETRO_DRAW_CAPACITY 24
#define RETRO_CELL_SIZE 8

#define RETRO_LEFT 1
#define RETRO_RIGHT 2
#define RETRO_JUMP 4
#define RETRO_UP 8
#define RETRO_PAUSE 16
#define RETRO_RESTART 32
#define RETRO_DOWN 64
#define RETRO_ACCEPT 128
#define RETRO_BACK 256
#define RETRO_SKILL_WALL_JUMP 1
#define RETRO_SKILL_CRAWL 2
#define RETRO_PLAYER_HEIGHT(player) ((player)->crouched ? 18 : 30)

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
#define RETRO_EVENT_CHECKPOINT 4
#define RETRO_EVENT_ITEM 5
#define RETRO_EVENT_LEVER 6
#define RETRO_EVENT_LEVER_INSERT 7
#define RETRO_EVENT_DIALOGUE 8
#define RETRO_EVENT_STEP 9
#define RETRO_EVENT_LAND 10

typedef struct retro_input {
    retro_u16 held;
    retro_u16 pressed;
    retro_u16 released;
} retro_input;

struct retro_enemy_def;
struct retro_path_point;

typedef struct retro_room {
    const retro_u8 *collision;
    retro_u16 columns;
    retro_u16 rows;
    retro_i16 spawn_x;
    retro_i16 spawn_y;
    /* negative x omits the prototype patrol or portal. */
    retro_i16 patrol_x;
    retro_i16 patrol_y;
    retro_i16 door_x;
    retro_i16 door_y;
    const retro_mechanism_def *mechanisms;
    retro_u16 mechanism_count;
    const retro_u16 *mechanism_targets;
    retro_u16 mechanism_target_count;
    const retro_mechanism_timing *mechanism_timings;
    const struct retro_enemy_def *enemy_definitions;
    retro_u16 enemy_count;
    const struct retro_path_point *enemy_paths;
    retro_u16 enemy_path_count;
    retro_u8 max_health;
    retro_u8 skills;
    retro_u8 player_gravity;
    retro_u8 player_gravity_fraction;
    retro_u8 player_speed;
    retro_u8 player_fall_gravity;
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
    retro_u8 drop_ticks;
    retro_u8 gravity_fraction;
    retro_u8 crouched, bend_ticks;
    retro_u8 frame;
    retro_u8 definition, state, target, inverse, arrived, fired;
    retro_u16 timer, pause_ticks, random_seed;
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
    retro_u8 kind;
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

#define RETRO_PROJECTILE_CAPACITY 16
typedef struct retro_projectile {
    retro_fixed x, y, velocity_x, velocity_y;
    retro_u16 age, damage;
    retro_u8 active, alignment, gravity;
} retro_projectile;

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
    retro_mechanism_set mechanisms;
    retro_i16 checkpoint_x;
    retro_i16 checkpoint_y;
    retro_u16 checkpoint_id;
    retro_u8 content_error;
    retro_i16 camera_y;
    retro_projectile projectiles[RETRO_PROJECTILE_CAPACITY];
    retro_u16 deaths, projectiles_fired;
} retro_engine;

/* initializes caller-owned state; room and collision storage must outlive it. */
void retro_init(retro_engine *engine, const retro_room *room, const retro_video_caps *video);
/* advances one 60Hz simulation tick; replaces the transient event queue. */
void retro_tick(retro_engine *engine, retro_u16 buttons);
/* builds a bounded list of logical metasprites without modifying simulation. */
void retro_render(const retro_engine *engine, retro_frame *frame);
/* move to a room entrance and immediately reframe the camera. */
void retro_place_player(retro_engine *engine, retro_i16 x, retro_i16 y);
void retro_event_push(retro_engine *engine, retro_u8 kind);
/* returns solid outside horizontal/top boundaries, empty below the room. */
retro_u8 retro_cell(const retro_room *room, retro_i16 column, retro_i16 row);
/* claims a zeroed entity slot; returns null when the pool is full. */
retro_entity *retro_spawn(retro_engine *engine, retro_u8 kind, retro_i16 x, retro_i16 y);

#endif

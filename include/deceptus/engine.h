#ifndef DECEPTUS_ENGINE_H
#define DECEPTUS_ENGINE_H

#include "deceptus/mechanisms.h"
#include "deceptus/types.h"

#define DECEPTUS_ENTITY_CAPACITY 12
#define DECEPTUS_EVENT_CAPACITY 8
#define DECEPTUS_DRAW_CAPACITY 24
#define DECEPTUS_CELL_SIZE 8

#define DECEPTUS_LEFT 1
#define DECEPTUS_RIGHT 2
#define DECEPTUS_JUMP 4
#define DECEPTUS_UP 8
#define DECEPTUS_PAUSE 16
#define DECEPTUS_RESTART 32
#define DECEPTUS_DOWN 64
#define DECEPTUS_ACCEPT 128
#define DECEPTUS_BACK 256
#define DECEPTUS_SKILL_WALL_JUMP 1
#define DECEPTUS_SKILL_CRAWL 2
#define DECEPTUS_PLAYER_HEIGHT(player) ((player)->crouched ? 18 : 30)

#define DECEPTUS_EMPTY 0
#define DECEPTUS_SOLID 1
#define DECEPTUS_ONE_WAY 2
#define DECEPTUS_HAZARD 4
#define DECEPTUS_WATER 8

#define DECEPTUS_PLAYER 1
#define DECEPTUS_PATROL 2
#define DECEPTUS_DOOR 3

#define DECEPTUS_EVENT_JUMP 1
#define DECEPTUS_EVENT_HURT 2
#define DECEPTUS_EVENT_DOOR 3
#define DECEPTUS_EVENT_CHECKPOINT 4
#define DECEPTUS_EVENT_ITEM 5
#define DECEPTUS_EVENT_LEVER 6
#define DECEPTUS_EVENT_LEVER_INSERT 7
#define DECEPTUS_EVENT_DIALOGUE 8
#define DECEPTUS_EVENT_STEP 9
#define DECEPTUS_EVENT_LAND 10

typedef struct deceptus_input {
    deceptus_u16 held;
    deceptus_u16 pressed;
    deceptus_u16 released;
} deceptus_input;

struct deceptus_enemy_def;
struct deceptus_path_point;

typedef struct deceptus_water_configuration {
    deceptus_u8 speed, rise_speed, sink_speed, stroke_delay_ticks, gravity, stroke_force;
} deceptus_water_configuration;

typedef struct deceptus_room {
    const deceptus_u8 *collision;
    deceptus_u16 columns;
    deceptus_u16 rows;
    deceptus_i16 spawn_x;
    deceptus_i16 spawn_y;
    /* negative x omits the prototype patrol or portal. */
    deceptus_i16 patrol_x;
    deceptus_i16 patrol_y;
    deceptus_i16 door_x;
    deceptus_i16 door_y;
    const deceptus_mechanism_def *mechanisms;
    deceptus_u16 mechanism_count;
    const deceptus_u16 *mechanism_targets;
    deceptus_u16 mechanism_target_count;
    const deceptus_mechanism_timing *mechanism_timings;
    const struct deceptus_enemy_def *enemy_definitions;
    deceptus_u16 enemy_count;
    const struct deceptus_path_point *enemy_paths;
    deceptus_u16 enemy_path_count;
    deceptus_u8 max_health;
    deceptus_u8 skills;
    deceptus_u8 player_gravity;
    deceptus_u8 player_gravity_fraction;
    deceptus_u8 player_speed;
    deceptus_u8 player_fall_gravity;
    const deceptus_water_configuration *water;
} deceptus_room;

typedef struct deceptus_entity {
    deceptus_fixed x;
    deceptus_fixed y;
    deceptus_fixed velocity_x;
    deceptus_fixed velocity_y;
    deceptus_u8 kind;
    deceptus_u8 active;
    deceptus_u8 grounded;
    deceptus_u8 facing_left;
    deceptus_u8 coyote_ticks;
    deceptus_u8 jump_buffer_ticks;
    deceptus_u8 animation_ticks;
    deceptus_u8 drop_ticks;
    deceptus_u8 gravity_fraction;
    deceptus_u8 crouched, bend_ticks;
    deceptus_u8 in_water, water_ticks;
    deceptus_u8 frame;
    deceptus_u8 definition, state, target, inverse, arrived, fired;
    deceptus_u16 timer, pause_ticks, random_seed;
} deceptus_entity;

typedef struct deceptus_event {
    deceptus_u8 kind;
    deceptus_u8 entity;
} deceptus_event;

typedef struct deceptus_draw {
    deceptus_i16 x;
    deceptus_i16 y;
    deceptus_u8 frame;
    deceptus_u8 palette;
    deceptus_u8 flip_x;
    deceptus_u8 kind;
} deceptus_draw;

/* backend capabilities are explicit; gameplay does not assume a 256px screen. */
typedef struct deceptus_video_caps {
    deceptus_u16 width;
    deceptus_u16 height;
    deceptus_u8 max_draws;
} deceptus_video_caps;

typedef struct deceptus_frame {
    deceptus_draw draws[DECEPTUS_DRAW_CAPACITY];
    deceptus_u8 count;
    deceptus_i16 camera_x;
    deceptus_i16 camera_y;
} deceptus_frame;

#define DECEPTUS_PROJECTILE_CAPACITY 16
typedef struct deceptus_projectile {
    deceptus_fixed x, y, velocity_x, velocity_y;
    deceptus_u16 age, damage;
    deceptus_u8 active, alignment, gravity;
} deceptus_projectile;

typedef struct deceptus_engine {
    const deceptus_room *room;
    deceptus_entity entities[DECEPTUS_ENTITY_CAPACITY];
    deceptus_event events[DECEPTUS_EVENT_CAPACITY];
    deceptus_video_caps video;
    deceptus_u16 previous_buttons;
    deceptus_u16 ticks;
    deceptus_u16 door_entries;
    deceptus_i16 camera_x;
    deceptus_u8 event_count;
    deceptus_u8 event_overflow;
    deceptus_u8 paused;
    deceptus_u8 health;
    deceptus_u8 hurt_ticks;
    deceptus_mechanism_set mechanisms;
    deceptus_i16 checkpoint_x;
    deceptus_i16 checkpoint_y;
    deceptus_u16 checkpoint_id;
    deceptus_u8 content_error;
    deceptus_i16 camera_y;
    deceptus_projectile projectiles[DECEPTUS_PROJECTILE_CAPACITY];
    deceptus_u16 deaths, projectiles_fired;
} deceptus_engine;

/* initializes caller-owned state; room and collision storage must outlive it. */
void deceptus_init(deceptus_engine *engine, const deceptus_room *room, const deceptus_video_caps *video);
/* advances one 60Hz simulation tick; replaces the transient event queue. */
void deceptus_tick(deceptus_engine *engine, deceptus_u16 buttons);
/* builds a bounded list of logical metasprites without modifying simulation. */
void deceptus_render(const deceptus_engine *engine, deceptus_frame *frame);
/* move to a room entrance and immediately reframe the camera. */
void deceptus_place_player(deceptus_engine *engine, deceptus_i16 x, deceptus_i16 y);
void deceptus_event_push(deceptus_engine *engine, deceptus_u8 kind);
/* returns solid outside horizontal/top boundaries, empty below the room. */
deceptus_u8 deceptus_cell(const deceptus_room *room, deceptus_i16 column, deceptus_i16 row);
/* claims a zeroed entity slot; returns null when the pool is full. */
deceptus_entity *deceptus_spawn(deceptus_engine *engine, deceptus_u8 kind, deceptus_i16 x, deceptus_i16 y);

#endif

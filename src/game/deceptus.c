#include "deceptus/enemies.h"
#include "deceptus/engine.h"
#include "deceptus/physics.h"
#include "deceptus/playerwater.h"
#include <string.h>

#define PLAYER_HALF_WIDTH 8
#define PLAYER_HEIGHT 30
#define GRAVITY 4
#define MAX_FALL 80
#define JUMP_SPEED 88
#define MAX_SPEED 35
#define ACCELERATION 4
#define FRICTION 6

void deceptus_event_push(deceptus_engine *engine, deceptus_u8 kind)
{
    if (engine->event_count == DECEPTUS_EVENT_CAPACITY) {
        engine->event_overflow = 1;
        return;
    }
    engine->events[engine->event_count].kind = kind;
    engine->events[engine->event_count].entity = 0;
    ++engine->event_count;
}

static void respawn(deceptus_engine *engine)
{
    deceptus_entity *player;
    player = &engine->entities[0];
    player->x = DECEPTUS_FIXED(engine->checkpoint_x);
    player->y = DECEPTUS_FIXED(engine->checkpoint_y);
    player->velocity_x = 0;
    player->velocity_y = 0;
    player->grounded = 0;
    player->coyote_ticks = 0;
    player->jump_buffer_ticks = 0;
    player->crouched = 0;
    player->bend_ticks = 0;
    player->drop_ticks = 0;
}

static void update_camera(deceptus_engine *engine)
{
    deceptus_i16 maximum_camera, target;
    const deceptus_entity *player;
    player = &engine->entities[0];
    target = deceptus_floor(player->x) - engine->video.width / 2;
    maximum_camera = engine->room->columns * DECEPTUS_CELL_SIZE - engine->video.width;
    if (maximum_camera < 0) {
        maximum_camera = 0;
    }
    if (target < 0) {
        target = 0;
    }
    if (target > maximum_camera) {
        target = maximum_camera;
    }
    engine->camera_x = target;
    target = deceptus_floor(player->y) - (deceptus_i16)(engine->video.height * 3 / 4);
    maximum_camera = engine->room->rows * DECEPTUS_CELL_SIZE - engine->video.height;
    if (maximum_camera < 0) {
        maximum_camera = 0;
    }
    if (target < 0) {
        target = 0;
    }
    if (target > maximum_camera) {
        target = maximum_camera;
    }
    engine->camera_y = target;
}

void deceptus_init(deceptus_engine *engine, const deceptus_room *room, const deceptus_video_caps *video)
{
    deceptus_video_caps saved_video;
    saved_video = *video;
    memset(engine, 0, sizeof(*engine));
    engine->room = room;
    engine->video = saved_video;
    engine->health = room->max_health ? room->max_health : 3;
    engine->checkpoint_x = room->spawn_x;
    engine->checkpoint_y = room->spawn_y;
    engine->content_error = !deceptus_mechanisms_init(&engine->mechanisms, room->mechanisms, room->mechanism_count,
                                                      room->mechanism_targets, room->mechanism_target_count);
    deceptus_mechanisms_configure(&engine->mechanisms, room->mechanism_timings);
    deceptus_spawn(engine, DECEPTUS_PLAYER, room->spawn_x, room->spawn_y);
    if (room->patrol_x >= 0) {
        deceptus_spawn(engine, DECEPTUS_PATROL, room->patrol_x, room->patrol_y);
    }
    if (room->door_x >= 0) {
        deceptus_spawn(engine, DECEPTUS_DOOR, room->door_x, room->door_y);
    }
    deceptus_enemies_init(engine);
    update_camera(engine);
}

static deceptus_fixed approach(deceptus_fixed value, deceptus_fixed target, deceptus_i16 amount)
{
    if (value < target) {
        value += amount;
        return value > target ? target : value;
    }
    if (value > target) {
        value -= amount;
        return value < target ? target : value;
    }
    return value;
}

static void update_player(deceptus_engine *engine, const deceptus_input *input)
{
    static const deceptus_u8 bend_frames[18] = {0, 0, 0, 1, 1, 2, 2, 2, 3, 3, 4, 4, 5, 5, 5, 6, 6, 6};
    deceptus_entity *player;
    deceptus_fixed target, speed;
    deceptus_i16 direction, wall;
    deceptus_u16 gravity;
    deceptus_u8 height, was_grounded;
    player = &engine->entities[0];
    if (deceptus_player_water_tick(engine, input)) {
        return;
    }
    was_grounded = player->grounded;
    if ((input->held & DECEPTUS_DOWN) && player->grounded) {
        player->crouched = 1;
    } else if (player->crouched && deceptus_body_clear(engine->room, &engine->mechanisms, player->x, player->y,
                                                       PLAYER_HALF_WIDTH, PLAYER_HEIGHT)) {
        player->crouched = 0;
    }
    if (player->crouched) {
        if (player->bend_ticks < 17) {
            ++player->bend_ticks;
        }
    } else if (player->bend_ticks) {
        --player->bend_ticks;
    }
    wall = 0;
    if (engine->room->skills & DECEPTUS_SKILL_WALL_JUMP) {
        wall = deceptus_wall_side(engine->room, player, &engine->mechanisms);
    }
    direction = 0;
    if (input->held & DECEPTUS_LEFT) {
        --direction;
    }
    if (input->held & DECEPTUS_RIGHT) {
        ++direction;
    }
    if (player->crouched && !(engine->room->skills & DECEPTUS_SKILL_CRAWL) && !(input->pressed & DECEPTUS_JUMP)) {
        direction = 0;
        player->velocity_x = 0;
    }
    speed = engine->room->player_speed ? engine->room->player_speed : MAX_SPEED;
    target = direction * speed;
    player->velocity_x = approach(player->velocity_x, target, direction ? ACCELERATION : FRICTION);
    if (direction) {
        player->facing_left = direction < 0;
    }
    if (player->grounded) {
        player->coyote_ticks = 6;
    } else if (player->coyote_ticks) {
        --player->coyote_ticks;
    }
    if (player->jump_buffer_ticks) {
        --player->jump_buffer_ticks;
    }
    if (player->drop_ticks) {
        --player->drop_ticks;
    }
    if ((input->pressed & DECEPTUS_JUMP) && (input->held & DECEPTUS_DOWN) && player->grounded &&
        (deceptus_cell(engine->room, deceptus_position_cell(player->x), deceptus_position_cell(player->y)) &
         DECEPTUS_ONE_WAY)) {
        player->drop_ticks = 20;
        player->grounded = 0;
        player->coyote_ticks = 0;
        player->jump_buffer_ticks = 0;
        player->velocity_y = DECEPTUS_ONE;
        player->crouched = 0;
        player->bend_ticks = 0;
    } else if ((input->pressed & DECEPTUS_JUMP) && !player->crouched) {
        player->jump_buffer_ticks = 6;
    }
    if (player->jump_buffer_ticks && player->coyote_ticks) {
        player->velocity_y = -JUMP_SPEED;
        player->jump_buffer_ticks = 0;
        player->coyote_ticks = 0;
        player->grounded = 0;
        deceptus_event_push(engine, DECEPTUS_EVENT_JUMP);
    }
    if (player->jump_buffer_ticks && !player->grounded && !player->coyote_ticks && wall) {
        player->velocity_y = -JUMP_SPEED;
        player->velocity_x = -wall * speed;
        player->jump_buffer_ticks = 0;
        deceptus_event_push(engine, DECEPTUS_EVENT_JUMP);
    }
    /* a released jump cuts ascent; buffered landings still get an initial jump. */
    if ((input->released & DECEPTUS_JUMP) && player->velocity_y < -28) {
        player->velocity_y = -28;
    }
    if (engine->room->player_gravity) {
        gravity = player->gravity_fraction + (deceptus_u16)engine->room->player_gravity_fraction;
        player->gravity_fraction = (deceptus_u8)gravity;
        player->velocity_y +=
            (player->velocity_y >= 0 && engine->room->player_fall_gravity ? engine->room->player_fall_gravity
                                                                          : engine->room->player_gravity) +
            (gravity >> 8);
    } else {
        player->velocity_y += GRAVITY;
    }
    if (wall && direction == wall && !player->grounded && player->velocity_y > 32) {
        player->velocity_y = 32;
    }
    if (player->velocity_y > MAX_FALL) {
        player->velocity_y = MAX_FALL;
    }
    height = DECEPTUS_PLAYER_HEIGHT(player);
    deceptus_move_horizontal_dynamic(engine->room, player, PLAYER_HALF_WIDTH, height, &engine->mechanisms);
    deceptus_move_vertical_dynamic(engine->room, player, PLAYER_HALF_WIDTH, height, &engine->mechanisms);
    if (player->grounded && !was_grounded) {
        deceptus_event_push(engine, DECEPTUS_EVENT_LAND);
    }
    ++player->animation_ticks;
    if (player->grounded && player->velocity_x && !(player->animation_ticks & 15)) {
        deceptus_event_push(engine, DECEPTUS_EVENT_STEP);
    }
    if (!player->grounded) {
        player->frame = player->velocity_y < 0 ? 16 : 17;
    } else if (player->crouched || player->bend_ticks) {
        player->frame = 19 + bend_frames[player->bend_ticks];
    } else if (direction) {
        player->frame = 8 + (player->animation_ticks / 4) % 8;
    } else {
        player->frame = (player->animation_ticks / 8) % 8;
    }
}

static void hurt(deceptus_engine *engine, deceptus_u16 damage)
{
    if (engine->hurt_ticks || !damage) {
        return;
    }
    engine->hurt_ticks = 60;
    engine->health = damage >= engine->health ? 0 : (deceptus_u8)(engine->health - damage);
    if (!engine->health) {
        ++engine->deaths;
        engine->health = engine->room->max_health ? engine->room->max_health : 3;
        respawn(engine);
    } else if (!engine->room->max_health) {
        respawn(engine);
    }
    deceptus_event_push(engine, DECEPTUS_EVENT_HURT);
}

static void update_actors(deceptus_engine *engine, const deceptus_input *input)
{
    deceptus_u16 index;
    deceptus_entity *player, *entity;
    deceptus_i16 distance_x, distance_y, ahead, below;
    player = &engine->entities[0];
    for (index = 1; index < DECEPTUS_ENTITY_CAPACITY; ++index) {
        entity = &engine->entities[index];
        if (!entity->active) {
            continue;
        }
        if (entity->kind == DECEPTUS_PATROL) {
            entity->velocity_x = entity->facing_left ? -9 : 9;
            ahead = deceptus_position_cell(entity->x + DECEPTUS_FIXED(entity->facing_left ? -12 : 12));
            below = deceptus_position_cell(entity->y + DECEPTUS_ONE);
            if (!(deceptus_cell(engine->room, ahead, below) & (DECEPTUS_SOLID | DECEPTUS_ONE_WAY))) {
                entity->facing_left = !entity->facing_left;
                entity->velocity_x = -entity->velocity_x;
            }
            entity->velocity_y += GRAVITY;
            if (entity->velocity_y > MAX_FALL) {
                entity->velocity_y = MAX_FALL;
            }
            deceptus_move_horizontal_dynamic(engine->room, entity, PLAYER_HALF_WIDTH, PLAYER_HEIGHT,
                                             &engine->mechanisms);
            if (!entity->velocity_x) {
                entity->facing_left = !entity->facing_left;
            }
            deceptus_move_vertical_dynamic(engine->room, entity, PLAYER_HALF_WIDTH, PLAYER_HEIGHT, &engine->mechanisms);
            entity->frame = 8 + (engine->ticks / 6) % 8;
        }
        distance_x = deceptus_floor(player->x - entity->x);
        distance_y = deceptus_floor(player->y - entity->y);
        if (distance_x > -18 && distance_x < 18 && distance_y > -24 && distance_y < 24) {
            if (entity->kind == DECEPTUS_PATROL) {
                if (player->velocity_y > 0 && distance_y < -12) {
                    entity->active = 0;
                    player->velocity_y = -50;
                    deceptus_event_push(engine, DECEPTUS_EVENT_JUMP);
                } else {
                    hurt(engine, 1);
                }
            } else if (entity->kind == DECEPTUS_DOOR && (input->pressed & DECEPTUS_UP)) {
                ++engine->door_entries;
                respawn(engine);
                deceptus_event_push(engine, DECEPTUS_EVENT_DOOR);
            }
        }
    }
}

void deceptus_place_player(deceptus_engine *engine, deceptus_i16 x, deceptus_i16 y)
{
    engine->entities[0].x = DECEPTUS_FIXED(x);
    engine->entities[0].y = DECEPTUS_FIXED(y);
    engine->entities[0].velocity_x = 0;
    engine->entities[0].velocity_y = 0;
    engine->entities[0].grounded = 0;
    update_camera(engine);
}

void deceptus_tick(deceptus_engine *engine, deceptus_u16 buttons)
{
    deceptus_input input;
    deceptus_u16 mechanism_index;
    deceptus_i16 mechanism_slot;
    deceptus_entity *player;
    deceptus_rect player_rect;
    const deceptus_mechanism_event *mechanism_event;
    const deceptus_mechanism_def *mechanism;
    if (engine->content_error) {
        return;
    }
    input.held = buttons;
    input.pressed = buttons & ~engine->previous_buttons;
    input.released = engine->previous_buttons & ~buttons;
    engine->previous_buttons = buttons;
    engine->event_count = 0;
    engine->event_overflow = 0;
    if (input.pressed & DECEPTUS_RESTART) {
        deceptus_init(engine, engine->room, &engine->video);
        /* init clears state, so preserve held buttons to avoid repeat edges. */
        engine->previous_buttons = buttons;
        return;
    }
    if (input.pressed & DECEPTUS_PAUSE) {
        engine->paused = !engine->paused;
    }
    if (engine->paused) {
        return;
    }
    ++engine->ticks;
    if (engine->hurt_ticks) {
        --engine->hurt_ticks;
    }
    update_player(engine, &input);
    player = &engine->entities[0];
    player_rect.x = deceptus_floor(player->x) - PLAYER_HALF_WIDTH;
    player_rect.y = deceptus_floor(player->y) - DECEPTUS_PLAYER_HEIGHT(player);
    player_rect.width = PLAYER_HALF_WIDTH * 2;
    player_rect.height = DECEPTUS_PLAYER_HEIGHT(player);
    deceptus_mechanisms_tick(&engine->mechanisms, &player_rect, (input.pressed & DECEPTUS_UP) != 0);
    for (mechanism_index = 0; mechanism_index < engine->mechanisms.event_count; ++mechanism_index) {
        mechanism_event = &engine->mechanisms.events[mechanism_index];
        if (mechanism_event->kind == DECEPTUS_MECH_REACHED) {
            mechanism_slot = deceptus_mechanism_find(&engine->mechanisms, mechanism_event->source_id);
            mechanism = &engine->mechanisms.definitions[mechanism_slot];
            engine->checkpoint_x = mechanism->bounds.x + mechanism->bounds.width / 2;
            engine->checkpoint_y = mechanism->bounds.y + mechanism->bounds.height;
            engine->checkpoint_id = mechanism->source_id;
            deceptus_event_push(engine, DECEPTUS_EVENT_CHECKPOINT);
        } else if (mechanism_event->kind == DECEPTUS_MECH_FORCE) {
            player->velocity_x += (deceptus_i8)(mechanism_event->value & 255);
            player->velocity_y += (deceptus_i8)(mechanism_event->value >> 8);
            if (player->velocity_x > 127) {
                player->velocity_x = 127;
            }
            if (player->velocity_x < -127) {
                player->velocity_x = -127;
            }
            if (player->velocity_y > 127) {
                player->velocity_y = 127;
            }
            if (player->velocity_y < -127) {
                player->velocity_y = -127;
            }
        } else if (mechanism_event->kind == DECEPTUS_MECH_HIT) {
            hurt(engine, mechanism_event->value);
        }
    }
    hurt(engine, deceptus_enemies_tick(engine));
    update_actors(engine, &input);
    player = &engine->entities[0];
    if (deceptus_floor(player->y) > engine->room->rows * DECEPTUS_CELL_SIZE + PLAYER_HEIGHT ||
        (deceptus_cell(engine->room, deceptus_position_cell(player->x), deceptus_position_cell(player->y - 1)) &
         DECEPTUS_HAZARD)) {
        /* falling must recover even during the contact invulnerability interval. */
        engine->hurt_ticks = 0;
        hurt(engine, engine->room->max_health ? engine->health : 1);
    }
    update_camera(engine);
}

void deceptus_render(const deceptus_engine *engine, deceptus_frame *frame)
{
    deceptus_u16 index;
    deceptus_i16 screen_x, screen_y;
    const deceptus_entity *entity;
    deceptus_draw *draw;
    frame->count = 0;
    frame->camera_x = engine->camera_x;
    frame->camera_y = engine->camera_y;
    deceptus_player_water_render(engine, frame);
    for (index = 0; index < DECEPTUS_ENTITY_CAPACITY; ++index) {
        entity = &engine->entities[index];
        if (entity->kind >= DECEPTUS_RAT || !entity->active ||
            (index == 0 && engine->hurt_ticks && ((engine->hurt_ticks / 4) & 1))) {
            continue;
        }
        screen_x = deceptus_floor(entity->x) - engine->camera_x - 16;
        screen_y = deceptus_floor(entity->y) - engine->camera_y - 48;
        if (screen_x <= -32 || screen_x >= engine->video.width || screen_y <= -48 || screen_y >= engine->video.height) {
            continue;
        }
        if (frame->count >= DECEPTUS_DRAW_CAPACITY || frame->count >= engine->video.max_draws) {
            break;
        }
        draw = &frame->draws[frame->count++];
        draw->x = screen_x;
        draw->y = screen_y;
        draw->frame = entity->kind == DECEPTUS_DOOR ? 18 : entity->frame;
        draw->palette = entity->kind == DECEPTUS_PATROL ? 1 : 0;
        draw->flip_x = entity->facing_left;
        draw->kind = entity->kind;
        if (index == 0 && entity->in_water) {
            draw->kind = DECEPTUS_PLAYER_SWIM;
            draw->x -= 20;
            draw->frame += entity->facing_left ? 13 : 0;
            draw->flip_x = 0;
        }
    }
    deceptus_enemies_render(engine, frame);
}

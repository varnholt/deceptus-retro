#include "retro/engine.h"
#include "retro/physics.h"
#include <string.h>

#define PLAYER_HALF_WIDTH 8
#define PLAYER_HEIGHT 30
#define GRAVITY 4
#define MAX_FALL 80
#define JUMP_SPEED 88
#define MAX_SPEED 35
#define ACCELERATION 4
#define FRICTION 6

static void event_push(retro_engine *engine, retro_u8 kind)
{
    if (engine->event_count == RETRO_EVENT_CAPACITY) {
        engine->event_overflow = 1;
        return;
    }
    engine->events[engine->event_count].kind = kind;
    engine->events[engine->event_count].entity = 0;
    ++engine->event_count;
}

static void respawn(retro_engine *engine)
{
    retro_entity *player;
    player = &engine->entities[0];
    player->x = RETRO_FIXED(engine->checkpoint_x);
    player->y = RETRO_FIXED(engine->checkpoint_y);
    player->velocity_x = 0;
    player->velocity_y = 0;
    player->grounded = 0;
    player->coyote_ticks = 0;
    player->jump_buffer_ticks = 0;
}

void retro_init(retro_engine *engine, const retro_room *room, const retro_video_caps *video)
{
    retro_video_caps saved_video;
    saved_video = *video;
    memset(engine, 0, sizeof(*engine));
    engine->room = room;
    engine->video = saved_video;
    engine->health = 3;
    engine->checkpoint_x = room->spawn_x;
    engine->checkpoint_y = room->spawn_y;
    engine->content_error = !retro_mechanisms_init(&engine->mechanisms, room->mechanisms, room->mechanism_count,
                                                   room->mechanism_targets, room->mechanism_target_count);
    retro_spawn(engine, RETRO_PLAYER, room->spawn_x, room->spawn_y);
    if (room->patrol_x >= 0) {
        retro_spawn(engine, RETRO_PATROL, room->patrol_x, room->patrol_y);
    }
    if (room->door_x >= 0) {
        retro_spawn(engine, RETRO_DOOR, room->door_x, room->door_y);
    }
}

static retro_fixed approach(retro_fixed value, retro_fixed target, retro_i16 amount)
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

static void update_player(retro_engine *engine, const retro_input *input)
{
    retro_entity *player;
    retro_fixed target;
    retro_i16 direction;
    player = &engine->entities[0];
    direction = 0;
    if (input->held & RETRO_LEFT) {
        --direction;
    }
    if (input->held & RETRO_RIGHT) {
        ++direction;
    }
    target = direction * (retro_fixed)MAX_SPEED;
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
    if (input->pressed & RETRO_JUMP) {
        player->jump_buffer_ticks = 6;
    }
    if (player->jump_buffer_ticks && player->coyote_ticks) {
        player->velocity_y = -JUMP_SPEED;
        player->jump_buffer_ticks = 0;
        player->coyote_ticks = 0;
        player->grounded = 0;
        event_push(engine, RETRO_EVENT_JUMP);
    }
    /* a released jump cuts ascent; buffered landings still get an initial jump. */
    if ((input->released & RETRO_JUMP) && player->velocity_y < -28) {
        player->velocity_y = -28;
    }
    player->velocity_y += GRAVITY;
    if (player->velocity_y > MAX_FALL) {
        player->velocity_y = MAX_FALL;
    }
    retro_move_horizontal_dynamic(engine->room, player, PLAYER_HALF_WIDTH, PLAYER_HEIGHT, &engine->mechanisms);
    retro_move_vertical_dynamic(engine->room, player, PLAYER_HALF_WIDTH, PLAYER_HEIGHT, &engine->mechanisms);
    ++player->animation_ticks;
    if (!player->grounded) {
        player->frame = player->velocity_y < 0 ? 16 : 17;
    } else if (direction) {
        player->frame = 8 + (player->animation_ticks / 4) % 8;
    } else {
        player->frame = (player->animation_ticks / 8) % 8;
    }
}

static void hurt(retro_engine *engine, retro_u16 damage)
{
    if (engine->hurt_ticks || !damage) {
        return;
    }
    engine->hurt_ticks = 60;
    engine->health = damage >= engine->health ? 0 : (retro_u8)(engine->health - damage);
    if (!engine->health) {
        engine->health = 3;
    }
    respawn(engine);
    event_push(engine, RETRO_EVENT_HURT);
}

static void update_actors(retro_engine *engine, const retro_input *input)
{
    retro_u16 index;
    retro_entity *player, *entity;
    retro_i16 distance_x, distance_y, ahead, below;
    player = &engine->entities[0];
    for (index = 1; index < RETRO_ENTITY_CAPACITY; ++index) {
        entity = &engine->entities[index];
        if (!entity->active) {
            continue;
        }
        if (entity->kind == RETRO_PATROL) {
            entity->velocity_x = entity->facing_left ? -9 : 9;
            ahead = retro_position_cell(entity->x + RETRO_FIXED(entity->facing_left ? -12 : 12));
            below = retro_position_cell(entity->y + RETRO_ONE);
            if (!(retro_cell(engine->room, ahead, below) & (RETRO_SOLID | RETRO_ONE_WAY))) {
                entity->facing_left = !entity->facing_left;
                entity->velocity_x = -entity->velocity_x;
            }
            entity->velocity_y += GRAVITY;
            if (entity->velocity_y > MAX_FALL) {
                entity->velocity_y = MAX_FALL;
            }
            retro_move_horizontal_dynamic(engine->room, entity, PLAYER_HALF_WIDTH, PLAYER_HEIGHT, &engine->mechanisms);
            if (!entity->velocity_x) {
                entity->facing_left = !entity->facing_left;
            }
            retro_move_vertical_dynamic(engine->room, entity, PLAYER_HALF_WIDTH, PLAYER_HEIGHT, &engine->mechanisms);
            entity->frame = 8 + (engine->ticks / 6) % 8;
        }
        distance_x = retro_floor(player->x - entity->x);
        distance_y = retro_floor(player->y - entity->y);
        if (distance_x > -18 && distance_x < 18 && distance_y > -24 && distance_y < 24) {
            if (entity->kind == RETRO_PATROL) {
                if (player->velocity_y > 0 && distance_y < -12) {
                    entity->active = 0;
                    player->velocity_y = -50;
                    event_push(engine, RETRO_EVENT_JUMP);
                } else {
                    hurt(engine, 1);
                }
            } else if (entity->kind == RETRO_DOOR && (input->pressed & RETRO_UP)) {
                ++engine->door_entries;
                respawn(engine);
                event_push(engine, RETRO_EVENT_DOOR);
            }
        }
    }
}

void retro_tick(retro_engine *engine, retro_u16 buttons)
{
    retro_input input;
    retro_i16 maximum_camera, target;
    retro_u16 mechanism_index;
    retro_i16 mechanism_slot;
    retro_entity *player;
    retro_rect player_rect;
    const retro_mechanism_event *mechanism_event;
    const retro_mechanism_def *mechanism;
    if (engine->content_error) {
        return;
    }
    input.held = buttons;
    input.pressed = buttons & ~engine->previous_buttons;
    input.released = engine->previous_buttons & ~buttons;
    engine->previous_buttons = buttons;
    engine->event_count = 0;
    engine->event_overflow = 0;
    if (input.pressed & RETRO_RESTART) {
        retro_init(engine, engine->room, &engine->video);
        /* init clears state, so preserve held buttons to avoid repeat edges. */
        engine->previous_buttons = buttons;
        return;
    }
    if (input.pressed & RETRO_PAUSE) {
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
    player_rect.x = retro_floor(player->x) - PLAYER_HALF_WIDTH;
    player_rect.y = retro_floor(player->y) - PLAYER_HEIGHT;
    player_rect.width = PLAYER_HALF_WIDTH * 2;
    player_rect.height = PLAYER_HEIGHT;
    retro_mechanisms_tick(&engine->mechanisms, &player_rect, (input.pressed & RETRO_UP) != 0);
    for (mechanism_index = 0; mechanism_index < engine->mechanisms.event_count; ++mechanism_index) {
        mechanism_event = &engine->mechanisms.events[mechanism_index];
        if (mechanism_event->kind == RETRO_MECH_REACHED) {
            mechanism_slot = retro_mechanism_find(&engine->mechanisms, mechanism_event->source_id);
            mechanism = &engine->mechanisms.definitions[mechanism_slot];
            engine->checkpoint_x = mechanism->bounds.x + mechanism->bounds.width / 2;
            engine->checkpoint_y = mechanism->bounds.y + mechanism->bounds.height;
            engine->checkpoint_id = mechanism->source_id;
            event_push(engine, RETRO_EVENT_CHECKPOINT);
        } else if (mechanism_event->kind == RETRO_MECH_HIT) {
            hurt(engine, mechanism_event->value);
        }
    }
    update_actors(engine, &input);
    player = &engine->entities[0];
    if (retro_floor(player->y) > engine->room->rows * RETRO_CELL_SIZE + PLAYER_HEIGHT ||
        (retro_cell(engine->room, retro_position_cell(player->x), retro_position_cell(player->y - 1)) & RETRO_HAZARD)) {
        /* falling must recover even during the contact invulnerability interval. */
        engine->hurt_ticks = 0;
        hurt(engine, 1);
    }
    target = retro_floor(player->x) - engine->video.width / 2;
    maximum_camera = engine->room->columns * RETRO_CELL_SIZE - engine->video.width;
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
    target = retro_floor(player->y) - (retro_i16)(engine->video.height * 3 / 4);
    maximum_camera = engine->room->rows * RETRO_CELL_SIZE - engine->video.height;
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

void retro_render(const retro_engine *engine, retro_frame *frame)
{
    retro_u16 index;
    retro_i16 screen_x, screen_y;
    const retro_entity *entity;
    retro_draw *draw;
    frame->count = 0;
    frame->camera_x = engine->camera_x;
    frame->camera_y = engine->camera_y;
    for (index = 0; index < RETRO_ENTITY_CAPACITY; ++index) {
        entity = &engine->entities[index];
        if (!entity->active || (index == 0 && engine->hurt_ticks && ((engine->hurt_ticks / 4) & 1))) {
            continue;
        }
        screen_x = retro_floor(entity->x) - engine->camera_x - 16;
        screen_y = retro_floor(entity->y) - engine->camera_y - 48;
        if (screen_x <= -32 || screen_x >= engine->video.width || screen_y <= -48 || screen_y >= engine->video.height) {
            continue;
        }
        if (frame->count >= RETRO_DRAW_CAPACITY || frame->count >= engine->video.max_draws) {
            break;
        }
        draw = &frame->draws[frame->count++];
        draw->x = screen_x;
        draw->y = screen_y;
        draw->frame = entity->kind == RETRO_DOOR ? 18 : entity->frame;
        draw->palette = entity->kind == RETRO_PATROL ? 1 : 0;
        draw->flip_x = entity->facing_left;
    }
}

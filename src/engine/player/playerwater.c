/* original counterparts: Player::updateAtmosphere and PlayerJump::updateJump. */
#include "deceptus/playerwater.h"
#include "deceptus/physics.h"

deceptus_u8 deceptus_player_water_tick(deceptus_engine *engine, const deceptus_input *input)
{
    deceptus_entity *player;
    const deceptus_water_configuration *water;
    deceptus_i16 direction, target;
    deceptus_u8 wet;
    player = &engine->entities[0];
    water = engine->room->water;
    wet = water && (deceptus_cell(engine->room, deceptus_position_cell(player->x),
                                  deceptus_position_cell(player->y - 15 * DECEPTUS_ONE)) &
                    DECEPTUS_WATER);
    if (!wet) {
        if (player->in_water) {
            player->velocity_x = 0;
            player->velocity_y = (input->held & DECEPTUS_JUMP) ? -40 : 0;
        }
        player->in_water = 0;
        player->water_ticks = 0;
        return 0;
    }
    if (!player->in_water) {
        player->water_ticks = 0;
        player->animation_ticks = 0;
        player->frame = 0;
    }
    player->in_water = 1;
    if (player->water_ticks < 255) {
        ++player->water_ticks;
    }
    player->crouched = 0;
    player->bend_ticks = 0;
    player->jump_buffer_ticks = 0;
    player->coyote_ticks = 0;
    direction = ((input->held & DECEPTUS_RIGHT) != 0) - ((input->held & DECEPTUS_LEFT) != 0);
    target = direction * water->speed;
    if (player->velocity_x < target) {
        ++player->velocity_x;
    } else if (player->velocity_x > target) {
        --player->velocity_x;
    }
    if (direction) {
        player->facing_left = direction < 0;
    }
    player->velocity_y += water->gravity;
    if ((input->held & DECEPTUS_JUMP) && player->water_ticks > water->stroke_delay_ticks) {
        player->velocity_y -= water->stroke_force;
    }
    if (player->velocity_y > water->sink_speed) {
        player->velocity_y = water->sink_speed;
    }
    if (player->velocity_y < -(deceptus_i16)water->rise_speed) {
        player->velocity_y = -(deceptus_i16)water->rise_speed;
    }
    deceptus_move_horizontal_dynamic(engine->room, player, 8, 30, &engine->mechanisms);
    deceptus_move_vertical_dynamic(engine->room, player, 8, 30, &engine->mechanisms);
    if (direction) {
        if (player->frame < 5 || player->frame > 12) {
            player->frame = 5;
            player->animation_ticks = 0;
        }
        player->animation_ticks += 5;
        if (player->animation_ticks >= 42) {
            player->animation_ticks -= 42;
            if (++player->frame >= 13) {
                player->frame = 5;
            }
        }
    } else {
        if (player->frame >= 5) {
            player->frame = 0;
            player->animation_ticks = 0;
        }
        player->animation_ticks += 2;
        if (player->animation_ticks >= 9) {
            player->animation_ticks -= 9;
            if (++player->frame >= 5) {
                player->frame = 0;
            }
        }
    }
    return 1;
}

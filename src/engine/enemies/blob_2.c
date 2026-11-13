/* original counterpart: data/scripts/enemies/blob_2.lua, normal ground patrol. */
static void blob_2_sprite(deceptus_entity *blob, deceptus_u8 row, deceptus_u8 column)
{
    static const deceptus_u8 first[6] = {0, 15, 31, 0, 0, 47};
    blob->inverse = row;
    blob->arrived = column;
    blob->frame = first[row] + column;
}

static void blob_2_tick(deceptus_engine *engine, deceptus_entity *blob, const deceptus_enemy_def *definition)
{
    /* sin(sprite_index / 16 * tau) + 1, quantized to sixteenths. */
    static const deceptus_u8 speed_wave[16] = {16, 22, 27, 31, 32, 31, 27, 22, 16, 10, 5, 1, 0, 1, 5, 10};
    deceptus_i16 distance, target_speed;
    deceptus_u8 column, row;
    ++blob->timer;
    target_speed = 0;
    if (blob->state == 3) {
        column = (deceptus_u8)(blob->timer / 3);
        if (column >= 14) {
            blob->active = 0;
            return;
        }
        if (column != blob->arrived) {
            blob_2_sprite(blob, 5, column);
        }
    } else if (blob->state == 1) {
        if (blob->inverse == 1 || blob->inverse == 2) {
            column = (deceptus_u8)((blob->timer / 3) % 16);
            if (column) {
                if (column != blob->arrived) {
                    blob_2_sprite(blob, blob->inverse, column);
                }
            } else {
                blob->inverse = 0;
                blob->timer = 0;
            }
        } else {
            column = (deceptus_u8)((blob->timer / 3) % 15);
            if (column != blob->arrived) {
                blob_2_sprite(blob, 0, column);
                if (!column && ++blob->pause_ticks >= 3) {
                    blob->state = 0;
                    blob->pause_ticks = 0;
                    blob->timer = 0;
                }
            }
        }
    } else {
        distance = engine->room->enemy_paths[definition->path_first + blob->target].x - deceptus_floor(blob->x);
        if (distance >= -1 && distance <= 1) {
            blob->state = 1;
            if (++blob->target >= definition->path_count) {
                blob->target = 0;
            }
        } else {
            target_speed = definition->speed * speed_wave[blob->arrived & 15] / 16;
            blob->facing_left = distance < 0;
            if (blob->facing_left) {
                target_speed = -target_speed;
            }
            column = (deceptus_u8)((blob->timer / 3) % 16);
            row = blob->facing_left ? 1 : 2;
            if (column != blob->arrived) {
                blob_2_sprite(blob, row, column);
            }
        }
    }
    blob->velocity_x = approach(blob->velocity_x, target_speed, definition->acceleration);
    blob->velocity_y += 4;
    if (blob->velocity_y > 80) {
        blob->velocity_y = 80;
    }
    deceptus_move_horizontal_dynamic(engine->room, blob, 14, 24, &engine->mechanisms);
    deceptus_move_vertical_dynamic(engine->room, blob, 14, 24, &engine->mechanisms);
}

static deceptus_u16 blob_2_contact(deceptus_engine *engine, deceptus_entity *blob, const deceptus_enemy_def *definition)
{
    deceptus_entity *player;
    deceptus_i16 dx, dy;
    if (blob->state == 3 || !blob->active) {
        return 0;
    }
    player = &engine->entities[0];
    dx = deceptus_floor(blob->x) - deceptus_floor(player->x);
    dy = deceptus_floor(blob->y) - deceptus_floor(player->y);
    if (dx <= -26 || dx >= 26 || dy <= -DECEPTUS_PLAYER_HEIGHT(player) - 6 || dy >= 30) {
        return 0;
    }
    if (player->velocity_y > 0 && dy > 12) {
        blob->state = 3;
        blob->timer = 0;
        blob->velocity_x = 0;
        player->velocity_y = -50;
        return 0;
    }
    return definition->damage;
}

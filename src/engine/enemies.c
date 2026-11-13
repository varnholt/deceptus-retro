#include "deceptus/enemies.h"
#include "deceptus/physics.h"

static deceptus_fixed approach(deceptus_fixed current, deceptus_fixed target, deceptus_fixed step)
{
    if (current < target) {
        current += step;
        return current > target ? target : current;
    }
    if (current > target) {
        current -= step;
        return current < target ? target : current;
    }
    return current;
}

#include "enemies/blob_2.c"

void deceptus_enemies_init(deceptus_engine *engine)
{
    deceptus_u16 index, point;
    deceptus_i16 row;
    const deceptus_enemy_def *definition;
    deceptus_entity *entity;
    if (engine->room->enemy_count > 8 || (engine->room->enemy_count && !engine->room->enemy_definitions)) {
        engine->content_error = 6;
        return;
    }
    for (index = 0; index < engine->room->enemy_count; ++index) {
        definition = &engine->room->enemy_definitions[index];
        if (definition->path_first > engine->room->enemy_path_count ||
            definition->path_count > engine->room->enemy_path_count - definition->path_first ||
            (definition->path_count && !engine->room->enemy_paths) || definition->alignment > 3 ||
            (definition->kind != DECEPTUS_RAT && definition->kind != DECEPTUS_ARROW_TRAP &&
             definition->kind != DECEPTUS_BLOB_2) ||
            definition->animation_fps != 20 || definition->speed <= 0 || definition->speed >= DECEPTUS_FIXED(8)) {
            engine->content_error = 6;
            return;
        }
        if (definition->kind == DECEPTUS_RAT) {
            if (!definition->path_count || definition->path_count > 8 || !definition->acceleration) {
                engine->content_error = 6;
                return;
            }
            for (point = 0; point < 5; ++point) {
                if (!definition->cycles[point] || definition->cycles[point] > 7) {
                    engine->content_error = 6;
                    return;
                }
            }
        } else if (definition->kind == DECEPTUS_BLOB_2) {
            if (!definition->path_count || !definition->acceleration) {
                engine->content_error = 6;
                return;
            }
        } else if (!definition->period || definition->period >= 65000) {
            engine->content_error = 6;
            return;
        }
        entity = deceptus_spawn(engine, definition->kind, definition->x, definition->y);
        if (!entity) {
            engine->content_error = 6;
            return;
        }
        entity->definition = (deceptus_u8)index;
        entity->random_seed = definition->source_id;
        entity->timer = definition->time_offset;
        if (definition->kind == DECEPTUS_BLOB_2) {
            entity->target = definition->path_count > 1 ? 1 : 0;
            entity->y += DECEPTUS_FIXED(12);
        }
        if (definition->kind == DECEPTUS_RAT) {
            entity->state = 1;
            entity->target = definition->path_count > 1 ? 1 : 0;
            entity->y += DECEPTUS_FIXED(14);
            row = deceptus_position_cell(entity->y);
            while (row > 0 && (deceptus_cell(engine->room, deceptus_position_cell(entity->x), row) & DECEPTUS_SOLID)) {
                --row;
            }
            if (row < deceptus_position_cell(entity->y)) {
                entity->y = DECEPTUS_FIXED((row + 1) * 8);
            }
        }
    }
}

static void rat_tick(deceptus_engine *engine, deceptus_entity *rat, const deceptus_enemy_def *definition)
{
    deceptus_i16 distance;
    deceptus_u8 column, choice;
    const deceptus_path_point *point;
    if (rat->pause_ticks) {
        --rat->pause_ticks;
    } else {
        if (++rat->timer >= definition->cycles[rat->state] * 3) {
            rat->timer = 0;
            rat->random_seed = (deceptus_u16)(rat->random_seed * 17U + 31U);
            choice = (deceptus_u8)((rat->random_seed >> 8) & 3);
            if (rat->state == 0) {
                if (rat->arrived) {
                    rat->state = 4;
                    rat->inverse = 0;
                }
            } else if (rat->state < 4) {
                rat->state = choice == 3 ? 4 : choice + 1;
                rat->inverse = choice == 3;
                rat->pause_ticks = definition->idle_ticks;
            } else if (rat->inverse) {
                rat->state = 0;
                rat->arrived = 0;
            } else {
                rat->state = choice == 3 ? 1 : choice + 1;
                rat->pause_ticks = definition->idle_ticks;
            }
        }
    }
    if (rat->state != 0 || rat->arrived) {
        rat->velocity_x = 0;
    }
    if (rat->state == 0 && !rat->arrived) {
        point = &engine->room->enemy_paths[definition->path_first + rat->target];
        distance = point->x - deceptus_floor(rat->x);
        if (distance >= -1 && distance <= 1) {
            rat->arrived = 1;
            rat->velocity_x = 0;
            if (++rat->target >= definition->path_count) {
                rat->target = 0;
            }
        } else {
            rat->facing_left = distance < 0;
            rat->velocity_x = approach(rat->velocity_x, rat->facing_left ? -definition->speed : definition->speed,
                                       definition->acceleration);
        }
    }
    rat->velocity_y += 4;
    if (rat->velocity_y > 80) {
        rat->velocity_y = 80;
    }
    deceptus_move_horizontal_dynamic(engine->room, rat, 7, 14, &engine->mechanisms);
    deceptus_move_vertical_dynamic(engine->room, rat, 7, 14, &engine->mechanisms);
    column = (deceptus_u8)(rat->timer / 3);
    if (column >= definition->cycles[rat->state]) {
        column = 0;
    }
    if (rat->state == 4 && rat->inverse) {
        column = definition->cycles[4] - column - 1;
    }
    rat->frame = rat->state * 14 + (rat->facing_left ? 0 : 7) + column;
}

static void fire_arrow(deceptus_engine *engine, const deceptus_entity *trap, const deceptus_enemy_def *definition,
                       deceptus_u8 muzzle)
{
    deceptus_u16 index;
    deceptus_projectile *arrow;
    for (index = 0; index < DECEPTUS_PROJECTILE_CAPACITY; ++index) {
        arrow = &engine->projectiles[index];
        if (arrow->active) {
            continue;
        }
        arrow->active = 1;
        arrow->alignment = definition->alignment;
        arrow->age = 0;
        arrow->gravity = 0;
        arrow->damage = definition->damage;
        arrow->velocity_x = 0;
        arrow->velocity_y = 0;
        arrow->x = trap->x;
        arrow->y = trap->y;
        if (definition->alignment < 2) {
            arrow->x += DECEPTUS_FIXED(definition->alignment ? 36 : -16);
            arrow->y += DECEPTUS_FIXED(3 + muzzle * 8);
            arrow->velocity_x = definition->alignment ? definition->speed : -definition->speed;
        } else {
            arrow->x += DECEPTUS_FIXED(4 + muzzle * 8);
            arrow->y += DECEPTUS_FIXED(definition->alignment == 2 ? 40 : -16);
            arrow->velocity_y = definition->alignment == 2 ? definition->speed : -definition->speed;
        }
        ++engine->projectiles_fired;
        return;
    }
}

deceptus_u16 deceptus_enemies_tick(deceptus_engine *engine)
{
    deceptus_u16 index, muzzle, damage;
    deceptus_i16 x, y, dx, dy;
    deceptus_entity *enemy;
    deceptus_projectile *arrow;
    const deceptus_enemy_def *definition;
    damage = 0;
    for (index = 1; index < DECEPTUS_ENTITY_CAPACITY; ++index) {
        enemy = &engine->entities[index];
        if (!enemy->active ||
            (enemy->kind != DECEPTUS_RAT && enemy->kind != DECEPTUS_ARROW_TRAP && enemy->kind != DECEPTUS_BLOB_2)) {
            continue;
        }
        definition = &engine->room->enemy_definitions[enemy->definition];
        if (enemy->kind == DECEPTUS_BLOB_2) {
            deceptus_u16 contact;
            blob_2_tick(engine, enemy, definition);
            contact = blob_2_contact(engine, enemy, definition);
            if (contact > damage) {
                damage = contact;
            }
        } else if (enemy->kind == DECEPTUS_RAT) {
            rat_tick(engine, enemy, definition);
            dx = deceptus_floor(enemy->x) - deceptus_floor(engine->entities[0].x);
            dy = deceptus_floor(enemy->y) - deceptus_floor(engine->entities[0].y);
            if (dx > -15 && dx < 15 && dy > -DECEPTUS_PLAYER_HEIGHT(&engine->entities[0]) && dy < 14 &&
                definition->damage > damage) {
                damage = definition->damage;
            }
        } else {
            if (++enemy->timer > definition->period) {
                enemy->timer = 0;
                enemy->fired = 0;
            }
            for (muzzle = 0; muzzle < 3; ++muzzle) {
                if (!(enemy->fired & (1 << muzzle)) && enemy->timer >= definition->delays[muzzle] + 12) {
                    fire_arrow(engine, enemy, definition, (deceptus_u8)muzzle);
                    enemy->fired |= (deceptus_u8)(1 << muzzle);
                }
            }
            enemy->frame = definition->alignment;
        }
    }
    for (index = 0; index < DECEPTUS_PROJECTILE_CAPACITY; ++index) {
        arrow = &engine->projectiles[index];
        if (!arrow->active) {
            continue;
        }
        if (++arrow->age > 180) {
            arrow->active = 0;
            continue;
        }
        if (arrow->gravity > 202) {
            arrow->gravity -= 203;
            ++arrow->velocity_y;
        } else {
            arrow->gravity += 53;
        }
        if (arrow->velocity_y >= DECEPTUS_FIXED(8)) {
            arrow->velocity_y = DECEPTUS_FIXED(8) - 1;
        }
        arrow->x += arrow->velocity_x;
        arrow->y += arrow->velocity_y;
        x = deceptus_floor(arrow->x);
        y = deceptus_floor(arrow->y);
        if (x < 0 || y < 0 || x >= engine->room->columns * 8 || y >= engine->room->rows * 8 ||
            (deceptus_cell(engine->room, deceptus_position_cell(arrow->x), deceptus_position_cell(arrow->y)) & DECEPTUS_SOLID)) {
            arrow->active = 0;
            continue;
        }
        dx = x - deceptus_floor(engine->entities[0].x);
        dy = y - deceptus_floor(engine->entities[0].y);
        if (dx >= -9 && dx <= 9 && dy >= -DECEPTUS_PLAYER_HEIGHT(&engine->entities[0]) && dy <= 0) {
            if (arrow->damage > damage) {
                damage = arrow->damage;
            }
            arrow->active = 0;
        }
    }
    return damage;
}

static void draw_enemy(const deceptus_engine *engine, deceptus_frame *frame, deceptus_u8 kind, deceptus_u8 image, deceptus_i16 x,
                       deceptus_i16 y, deceptus_i16 width, deceptus_i16 height)
{
    deceptus_draw *draw;
    x -= engine->camera_x;
    y -= engine->camera_y;
    if (x <= -width || y <= -height || x >= engine->video.width || y >= engine->video.height ||
        frame->count >= DECEPTUS_DRAW_CAPACITY || frame->count >= engine->video.max_draws) {
        return;
    }
    draw = &frame->draws[frame->count++];
    draw->kind = kind;
    draw->frame = image;
    draw->x = x;
    draw->y = y;
    draw->palette = 0;
    draw->flip_x = 0;
}

void deceptus_enemies_render(const deceptus_engine *engine, deceptus_frame *frame)
{
    deceptus_u16 index, muzzle, image, elapsed;
    deceptus_i16 x, y;
    const deceptus_entity *enemy;
    const deceptus_enemy_def *definition;
    const deceptus_projectile *arrow;
    for (index = 0; index < DECEPTUS_PROJECTILE_CAPACITY; ++index) {
        arrow = &engine->projectiles[index];
        if (arrow->active) {
            draw_enemy(engine, frame, DECEPTUS_ARROW, (deceptus_u8)(arrow->alignment * 4 + (arrow->age / 5) % 4),
                       deceptus_floor(arrow->x) - 12, deceptus_floor(arrow->y) - 12, 32, 32);
        }
    }
    for (index = 1; index < DECEPTUS_ENTITY_CAPACITY; ++index) {
        enemy = &engine->entities[index];
        if (!enemy->active) {
            continue;
        }
        x = deceptus_floor(enemy->x);
        y = deceptus_floor(enemy->y);
        if (enemy->kind == DECEPTUS_BLOB_2) {
            draw_enemy(engine, frame, DECEPTUS_BLOB_2, enemy->frame, x - 36, y - 48, 80, 80);
        } else if (enemy->kind == DECEPTUS_RAT) {
            draw_enemy(engine, frame, DECEPTUS_RAT, enemy->frame, x - 12, y - 26, 32, 32);
        } else if (enemy->kind == DECEPTUS_ARROW_TRAP) {
            definition = &engine->room->enemy_definitions[enemy->definition];
            draw_enemy(engine, frame, DECEPTUS_ARROW_TRAP, definition->alignment, x - 24, y - 24, 80, 80);
            for (muzzle = 0; muzzle < 3; ++muzzle) {
                elapsed = enemy->timer;
                if (elapsed <= definition->delays[muzzle]) {
                    image = elapsed / 3;
                    if (image > 5) {
                        image = 5;
                    }
                } else {
                    elapsed = (deceptus_u16)(elapsed - definition->delays[muzzle]);
                    image = 5 + elapsed / 3;
                    if (image > 9) {
                        image = 9;
                    }
                }
                draw_enemy(engine, frame, DECEPTUS_ARROW_MUZZLE, (deceptus_u8)(definition->alignment * 10 + image),
                           x + (definition->alignment < 2 ? (definition->alignment ? 24 : -24) : muzzle * 8),
                           y + (definition->alignment < 2 ? muzzle * 8 : (definition->alignment == 2 ? 24 : -24)), 32,
                           32);
            }
        }
    }
}

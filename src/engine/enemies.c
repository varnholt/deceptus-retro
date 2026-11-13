#include "retro/enemies.h"
#include "retro/physics.h"

static retro_fixed approach(retro_fixed current, retro_fixed target, retro_fixed step)
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

void retro_enemies_init(retro_engine *engine)
{
    retro_u16 index, point;
    retro_i16 row;
    const retro_enemy_def *definition;
    retro_entity *entity;
    if (engine->room->enemy_count > 8 || (engine->room->enemy_count && !engine->room->enemy_definitions)) {
        engine->content_error = 6;
        return;
    }
    for (index = 0; index < engine->room->enemy_count; ++index) {
        definition = &engine->room->enemy_definitions[index];
        if (definition->path_first > engine->room->enemy_path_count ||
            definition->path_count > engine->room->enemy_path_count - definition->path_first ||
            (definition->path_count && !engine->room->enemy_paths) || definition->alignment > 3 ||
            (definition->kind != RETRO_RAT && definition->kind != RETRO_ARROW_TRAP) ||
            definition->animation_fps != 20 || definition->speed <= 0 || definition->speed >= RETRO_FIXED(8)) {
            engine->content_error = 6;
            return;
        }
        if (definition->kind == RETRO_RAT) {
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
        } else if (!definition->period || definition->period >= 65000) {
            engine->content_error = 6;
            return;
        }
        entity = retro_spawn(engine, definition->kind, definition->x, definition->y);
        if (!entity) {
            engine->content_error = 6;
            return;
        }
        entity->definition = (retro_u8)index;
        entity->random_seed = definition->source_id;
        entity->timer = definition->time_offset;
        if (definition->kind == RETRO_RAT) {
            entity->state = 1;
            entity->target = definition->path_count > 1 ? 1 : 0;
            entity->y += RETRO_FIXED(14);
            row = retro_position_cell(entity->y);
            while (row > 0 && (retro_cell(engine->room, retro_position_cell(entity->x), row) & RETRO_SOLID)) {
                --row;
            }
            if (row < retro_position_cell(entity->y)) {
                entity->y = RETRO_FIXED((row + 1) * 8);
            }
        }
    }
}

static void rat_tick(retro_engine *engine, retro_entity *rat, const retro_enemy_def *definition)
{
    retro_i16 distance;
    retro_u8 column, choice;
    const retro_path_point *point;
    if (rat->pause_ticks) {
        --rat->pause_ticks;
    } else {
        if (++rat->timer >= definition->cycles[rat->state] * 3) {
            rat->timer = 0;
            rat->random_seed = (retro_u16)(rat->random_seed * 17U + 31U);
            choice = (retro_u8)((rat->random_seed >> 8) & 3);
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
        distance = point->x - retro_floor(rat->x);
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
    retro_move_horizontal_dynamic(engine->room, rat, 7, 14, &engine->mechanisms);
    retro_move_vertical_dynamic(engine->room, rat, 7, 14, &engine->mechanisms);
    column = (retro_u8)(rat->timer / 3);
    if (column >= definition->cycles[rat->state]) {
        column = 0;
    }
    if (rat->state == 4 && rat->inverse) {
        column = definition->cycles[4] - column - 1;
    }
    rat->frame = rat->state * 14 + (rat->facing_left ? 0 : 7) + column;
}

static void fire_arrow(retro_engine *engine, const retro_entity *trap, const retro_enemy_def *definition,
                       retro_u8 muzzle)
{
    retro_u16 index;
    retro_projectile *arrow;
    for (index = 0; index < RETRO_PROJECTILE_CAPACITY; ++index) {
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
            arrow->x += RETRO_FIXED(definition->alignment ? 36 : -16);
            arrow->y += RETRO_FIXED(3 + muzzle * 8);
            arrow->velocity_x = definition->alignment ? definition->speed : -definition->speed;
        } else {
            arrow->x += RETRO_FIXED(4 + muzzle * 8);
            arrow->y += RETRO_FIXED(definition->alignment == 2 ? 40 : -16);
            arrow->velocity_y = definition->alignment == 2 ? definition->speed : -definition->speed;
        }
        ++engine->projectiles_fired;
        return;
    }
}

retro_u16 retro_enemies_tick(retro_engine *engine)
{
    retro_u16 index, muzzle, damage;
    retro_i16 x, y, dx, dy;
    retro_entity *enemy;
    retro_projectile *arrow;
    const retro_enemy_def *definition;
    damage = 0;
    for (index = 1; index < RETRO_ENTITY_CAPACITY; ++index) {
        enemy = &engine->entities[index];
        if (!enemy->active || (enemy->kind != RETRO_RAT && enemy->kind != RETRO_ARROW_TRAP)) {
            continue;
        }
        definition = &engine->room->enemy_definitions[enemy->definition];
        if (enemy->kind == RETRO_RAT) {
            rat_tick(engine, enemy, definition);
            dx = retro_floor(enemy->x) - retro_floor(engine->entities[0].x);
            dy = retro_floor(enemy->y) - retro_floor(engine->entities[0].y);
            if (dx > -15 && dx < 15 && dy > -30 && dy < 14 && definition->damage > damage) {
                damage = definition->damage;
            }
        } else {
            if (++enemy->timer > definition->period) {
                enemy->timer = 0;
                enemy->fired = 0;
            }
            for (muzzle = 0; muzzle < 3; ++muzzle) {
                if (!(enemy->fired & (1 << muzzle)) && enemy->timer >= definition->delays[muzzle] + 12) {
                    fire_arrow(engine, enemy, definition, (retro_u8)muzzle);
                    enemy->fired |= (retro_u8)(1 << muzzle);
                }
            }
            enemy->frame = definition->alignment;
        }
    }
    for (index = 0; index < RETRO_PROJECTILE_CAPACITY; ++index) {
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
        if (arrow->velocity_y >= RETRO_FIXED(8)) {
            arrow->velocity_y = RETRO_FIXED(8) - 1;
        }
        arrow->x += arrow->velocity_x;
        arrow->y += arrow->velocity_y;
        x = retro_floor(arrow->x);
        y = retro_floor(arrow->y);
        if (x < 0 || y < 0 || x >= engine->room->columns * 8 || y >= engine->room->rows * 8 ||
            (retro_cell(engine->room, retro_position_cell(arrow->x), retro_position_cell(arrow->y)) & RETRO_SOLID)) {
            arrow->active = 0;
            continue;
        }
        dx = x - retro_floor(engine->entities[0].x);
        dy = y - retro_floor(engine->entities[0].y);
        if (dx >= -9 && dx <= 9 && dy >= -30 && dy <= 0) {
            if (arrow->damage > damage) {
                damage = arrow->damage;
            }
            arrow->active = 0;
        }
    }
    return damage;
}

static void draw_enemy(const retro_engine *engine, retro_frame *frame, retro_u8 kind, retro_u8 image, retro_i16 x,
                       retro_i16 y, retro_i16 width, retro_i16 height)
{
    retro_draw *draw;
    x -= engine->camera_x;
    y -= engine->camera_y;
    if (x <= -width || y <= -height || x >= engine->video.width || y >= engine->video.height ||
        frame->count >= RETRO_DRAW_CAPACITY || frame->count >= engine->video.max_draws) {
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

void retro_enemies_render(const retro_engine *engine, retro_frame *frame)
{
    retro_u16 index, muzzle, image, elapsed;
    retro_i16 x, y;
    const retro_entity *enemy;
    const retro_enemy_def *definition;
    const retro_projectile *arrow;
    for (index = 0; index < RETRO_PROJECTILE_CAPACITY; ++index) {
        arrow = &engine->projectiles[index];
        if (arrow->active) {
            draw_enemy(engine, frame, RETRO_ARROW, (retro_u8)(arrow->alignment * 4 + (arrow->age / 5) % 4),
                       retro_floor(arrow->x) - 12, retro_floor(arrow->y) - 12, 32, 32);
        }
    }
    for (index = 1; index < RETRO_ENTITY_CAPACITY; ++index) {
        enemy = &engine->entities[index];
        if (!enemy->active) {
            continue;
        }
        x = retro_floor(enemy->x);
        y = retro_floor(enemy->y);
        if (enemy->kind == RETRO_RAT) {
            draw_enemy(engine, frame, RETRO_RAT, enemy->frame, x - 12, y - 26, 32, 32);
        } else if (enemy->kind == RETRO_ARROW_TRAP) {
            definition = &engine->room->enemy_definitions[enemy->definition];
            draw_enemy(engine, frame, RETRO_ARROW_TRAP, definition->alignment, x - 24, y - 24, 80, 80);
            for (muzzle = 0; muzzle < 3; ++muzzle) {
                elapsed = enemy->timer;
                if (elapsed <= definition->delays[muzzle]) {
                    image = elapsed / 3;
                    if (image > 5) {
                        image = 5;
                    }
                } else {
                    elapsed = (retro_u16)(elapsed - definition->delays[muzzle]);
                    image = 5 + elapsed / 3;
                    if (image > 9) {
                        image = 9;
                    }
                }
                draw_enemy(engine, frame, RETRO_ARROW_MUZZLE, (retro_u8)(definition->alignment * 10 + image),
                           x + (definition->alignment < 2 ? (definition->alignment ? 24 : -24) : muzzle * 8),
                           y + (definition->alignment < 2 ? muzzle * 8 : (definition->alignment == 2 ? 24 : -24)), 32,
                           32);
            }
        }
    }
}

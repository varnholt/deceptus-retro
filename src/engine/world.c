#include "retro/physics.h"

retro_i16 retro_floor(retro_fixed value)
{
    if (value >= 0) {
        return (retro_i16)(value / RETRO_ONE);
    }
    return (retro_i16)(-1 - ((-1 - value) / RETRO_ONE));
}

retro_u8 retro_cell(const retro_room *room, retro_i16 column, retro_i16 row)
{
    if (column < 0 || column >= room->columns || row < 0) {
        return RETRO_SOLID;
    }
    if (row >= room->rows) {
        return RETRO_EMPTY;
    }
    return room->collision[(retro_u16)row * room->columns + column];
}

retro_i16 retro_position_cell(retro_fixed position)
{
    retro_i16 pixel;
    pixel = retro_floor(position);
    if (pixel >= 0) {
        return pixel / RETRO_CELL_SIZE;
    }
    return -1 - ((-1 - pixel) / RETRO_CELL_SIZE);
}

static retro_u8 collision_cell(const retro_room *room, const retro_mechanism_set *mechanisms, retro_i16 column,
                               retro_i16 row)
{
    retro_rect bounds;
    retro_u8 value;
    value = retro_cell(room, column, row);
    if (!mechanisms || !mechanisms->count || (value & RETRO_SOLID)) {
        return value;
    }
    bounds.x = column * RETRO_CELL_SIZE;
    bounds.y = row * RETRO_CELL_SIZE;
    bounds.width = RETRO_CELL_SIZE;
    bounds.height = RETRO_CELL_SIZE;
    if (retro_mechanism_solid(mechanisms, &bounds)) {
        value |= RETRO_SOLID;
    }
    return value;
}

void retro_move_horizontal_dynamic(const retro_room *room, retro_entity *entity, retro_u8 half_width, retro_u8 height,
                                   const retro_mechanism_set *mechanisms)
{
    retro_fixed next_x;
    retro_i16 column, top, bottom, row;
    next_x = entity->x + entity->velocity_x;
    top = retro_position_cell(entity->y - RETRO_FIXED(height));
    bottom = retro_position_cell(entity->y - 1);
    if (entity->velocity_x == 0) {
        return;
    }
    column = retro_position_cell(next_x + RETRO_FIXED(entity->velocity_x > 0 ? half_width - 1 : -half_width));
    for (row = top; row <= bottom; ++row) {
        if (collision_cell(room, mechanisms, column, row) & RETRO_SOLID) {
            if (entity->velocity_x > 0) {
                next_x = RETRO_FIXED(column * RETRO_CELL_SIZE - half_width);
            } else {
                next_x = RETRO_FIXED((column + 1) * RETRO_CELL_SIZE + half_width);
            }
            entity->velocity_x = 0;
            break;
        }
    }
    entity->x = next_x;
}

void retro_move_vertical_dynamic(const retro_room *room, retro_entity *entity, retro_u8 half_width, retro_u8 height,
                                 const retro_mechanism_set *mechanisms)
{
    retro_fixed next_y;
    retro_i16 row, left, right, column, old_bottom;
    retro_u8 cell, blocked;
    next_y = entity->y + entity->velocity_y;
    left = retro_position_cell(entity->x - RETRO_FIXED(half_width));
    right = retro_position_cell(entity->x + RETRO_FIXED(half_width - 1));
    old_bottom = retro_floor(entity->y);
    row = retro_position_cell(entity->velocity_y >= 0 ? next_y - 1 : next_y - RETRO_FIXED(height));
    entity->grounded = 0;
    for (column = left; column <= right; ++column) {
        cell = collision_cell(room, mechanisms, column, row);
        blocked = (cell & RETRO_SOLID) != 0;
        if ((cell & RETRO_ONE_WAY) && entity->velocity_y >= 0 && old_bottom <= row * RETRO_CELL_SIZE) {
            blocked = 1;
        }
        if (blocked) {
            if (entity->velocity_y >= 0) {
                next_y = RETRO_FIXED(row * RETRO_CELL_SIZE);
                entity->grounded = 1;
            } else {
                next_y = RETRO_FIXED((row + 1) * RETRO_CELL_SIZE + height);
            }
            entity->velocity_y = 0;
            break;
        }
    }
    entity->y = next_y;
}

void retro_move_horizontal(const retro_room *room, retro_entity *entity, retro_u8 half_width, retro_u8 height)
{
    retro_move_horizontal_dynamic(room, entity, half_width, height, 0);
}

void retro_move_vertical(const retro_room *room, retro_entity *entity, retro_u8 half_width, retro_u8 height)
{
    retro_move_vertical_dynamic(room, entity, half_width, height, 0);
}

retro_i16 retro_wall_side(const retro_room *room, const retro_entity *entity, const retro_mechanism_set *mechanisms)
{
    retro_i16 side, column, first, last, row;
    first = retro_position_cell(entity->y - RETRO_FIXED(24));
    last = retro_position_cell(entity->y - RETRO_FIXED(8));
    for (side = -1; side <= 1; side += 2) {
        column = retro_position_cell(entity->x + RETRO_FIXED(side * 9));
        for (row = first; row <= last; ++row) {
            if (collision_cell(room, mechanisms, column, row) & RETRO_SOLID) {
                return side;
            }
        }
    }
    return 0;
}

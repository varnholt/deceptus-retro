#include "deceptus/physics.h"

deceptus_i16 deceptus_floor(deceptus_fixed value)
{
    if (value >= 0) {
        return (deceptus_i16)(value / DECEPTUS_ONE);
    }
    return (deceptus_i16)(-1 - ((-1 - value) / DECEPTUS_ONE));
}

deceptus_u8 deceptus_cell(const deceptus_room *room, deceptus_i16 column, deceptus_i16 row)
{
    if (column < 0 || column >= room->columns || row < 0) {
        return DECEPTUS_SOLID;
    }
    if (row >= room->rows) {
        return DECEPTUS_EMPTY;
    }
    return room->collision[(deceptus_u16)row * room->columns + column];
}

deceptus_i16 deceptus_position_cell(deceptus_fixed position)
{
    deceptus_i16 pixel;
    pixel = deceptus_floor(position);
    if (pixel >= 0) {
        return pixel / DECEPTUS_CELL_SIZE;
    }
    return -1 - ((-1 - pixel) / DECEPTUS_CELL_SIZE);
}

static deceptus_u8 collision_cell(const deceptus_room *room, const deceptus_mechanism_set *mechanisms, deceptus_i16 column,
                               deceptus_i16 row)
{
    deceptus_rect bounds;
    deceptus_u8 value;
    value = deceptus_cell(room, column, row);
    if (!mechanisms || !mechanisms->count || (value & DECEPTUS_SOLID)) {
        return value;
    }
    bounds.x = column * DECEPTUS_CELL_SIZE;
    bounds.y = row * DECEPTUS_CELL_SIZE;
    bounds.width = DECEPTUS_CELL_SIZE;
    bounds.height = DECEPTUS_CELL_SIZE;
    if (deceptus_mechanism_solid(mechanisms, &bounds)) {
        value |= DECEPTUS_SOLID;
    }
    return value;
}

deceptus_u8 deceptus_body_clear(const deceptus_room *room, const deceptus_mechanism_set *mechanisms, deceptus_fixed x, deceptus_fixed y,
                          deceptus_u8 half_width, deceptus_u8 height)
{
    deceptus_i16 column, row, left, right, top, bottom;
    left = deceptus_position_cell(x - DECEPTUS_FIXED(half_width));
    right = deceptus_position_cell(x + DECEPTUS_FIXED(half_width - 1));
    top = deceptus_position_cell(y - DECEPTUS_FIXED(height));
    bottom = deceptus_position_cell(y - 1);
    for (row = top; row <= bottom; ++row) {
        for (column = left; column <= right; ++column) {
            if (collision_cell(room, mechanisms, column, row) & DECEPTUS_SOLID) {
                return 0;
            }
        }
    }
    return 1;
}

void deceptus_move_horizontal_dynamic(const deceptus_room *room, deceptus_entity *entity, deceptus_u8 half_width, deceptus_u8 height,
                                   const deceptus_mechanism_set *mechanisms)
{
    deceptus_fixed next_x;
    deceptus_i16 column, top, bottom, row;
    next_x = entity->x + entity->velocity_x;
    top = deceptus_position_cell(entity->y - DECEPTUS_FIXED(height));
    bottom = deceptus_position_cell(entity->y - 1);
    if (entity->velocity_x == 0) {
        return;
    }
    column = deceptus_position_cell(next_x + DECEPTUS_FIXED(entity->velocity_x > 0 ? half_width - 1 : -half_width));
    for (row = top; row <= bottom; ++row) {
        if (collision_cell(room, mechanisms, column, row) & DECEPTUS_SOLID) {
            if (entity->velocity_x > 0) {
                next_x = DECEPTUS_FIXED(column * DECEPTUS_CELL_SIZE - half_width);
            } else {
                next_x = DECEPTUS_FIXED((column + 1) * DECEPTUS_CELL_SIZE + half_width);
            }
            entity->velocity_x = 0;
            break;
        }
    }
    entity->x = next_x;
}

void deceptus_move_vertical_dynamic(const deceptus_room *room, deceptus_entity *entity, deceptus_u8 half_width, deceptus_u8 height,
                                 const deceptus_mechanism_set *mechanisms)
{
    deceptus_fixed next_y;
    deceptus_i16 row, left, right, column, old_bottom;
    deceptus_u8 cell, blocked;
    next_y = entity->y + entity->velocity_y;
    left = deceptus_position_cell(entity->x - DECEPTUS_FIXED(half_width));
    right = deceptus_position_cell(entity->x + DECEPTUS_FIXED(half_width - 1));
    old_bottom = deceptus_floor(entity->y);
    row = deceptus_position_cell(entity->velocity_y >= 0 ? next_y - 1 : next_y - DECEPTUS_FIXED(height));
    entity->grounded = 0;
    for (column = left; column <= right; ++column) {
        cell = collision_cell(room, mechanisms, column, row);
        blocked = (cell & DECEPTUS_SOLID) != 0;
        if ((cell & DECEPTUS_ONE_WAY) && !entity->drop_ticks && entity->velocity_y >= 0 &&
            old_bottom <= row * DECEPTUS_CELL_SIZE) {
            blocked = 1;
        }
        if (blocked) {
            if (entity->velocity_y >= 0) {
                next_y = DECEPTUS_FIXED(row * DECEPTUS_CELL_SIZE);
                entity->grounded = 1;
            } else {
                next_y = DECEPTUS_FIXED((row + 1) * DECEPTUS_CELL_SIZE + height);
            }
            entity->velocity_y = 0;
            break;
        }
    }
    entity->y = next_y;
}

void deceptus_move_horizontal(const deceptus_room *room, deceptus_entity *entity, deceptus_u8 half_width, deceptus_u8 height)
{
    deceptus_move_horizontal_dynamic(room, entity, half_width, height, 0);
}

void deceptus_move_vertical(const deceptus_room *room, deceptus_entity *entity, deceptus_u8 half_width, deceptus_u8 height)
{
    deceptus_move_vertical_dynamic(room, entity, half_width, height, 0);
}

deceptus_i16 deceptus_wall_side(const deceptus_room *room, const deceptus_entity *entity, const deceptus_mechanism_set *mechanisms)
{
    deceptus_i16 side, column, first, last, row;
    first = deceptus_position_cell(entity->y - DECEPTUS_FIXED(24));
    last = deceptus_position_cell(entity->y - DECEPTUS_FIXED(8));
    for (side = -1; side <= 1; side += 2) {
        column = deceptus_position_cell(entity->x + DECEPTUS_FIXED(side * 9));
        for (row = first; row <= last; ++row) {
            if (collision_cell(room, mechanisms, column, row) & DECEPTUS_SOLID) {
                return side;
            }
        }
    }
    return 0;
}

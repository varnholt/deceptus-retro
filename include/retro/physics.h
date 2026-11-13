#ifndef RETRO_PHYSICS_H
#define RETRO_PHYSICS_H

#include "retro/engine.h"

/* signed room-local fixed position to collision-cell coordinate. */
retro_i16 retro_position_cell(retro_fixed position);
/* axis-separated swept body movement. velocities must be less than one cell
 * per tick; the caller supplies its body size and applies acceleration. */
void retro_move_horizontal(const retro_room *room, retro_entity *entity, retro_u8 half_width, retro_u8 height);
void retro_move_vertical(const retro_room *room, retro_entity *entity, retro_u8 half_width, retro_u8 height);

#endif

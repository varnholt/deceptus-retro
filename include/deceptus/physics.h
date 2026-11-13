#ifndef DECEPTUS_PHYSICS_H
#define DECEPTUS_PHYSICS_H

#include "deceptus/engine.h"

/* signed room-local fixed position to collision-cell coordinate. */
deceptus_i16 deceptus_position_cell(deceptus_fixed position);
deceptus_u8 deceptus_body_clear(const deceptus_room *room, const deceptus_mechanism_set *mechanisms, deceptus_fixed x,
                                deceptus_fixed y, deceptus_u8 half_width, deceptus_u8 height);
/* axis-separated swept body movement. velocities must be less than one cell
 * per tick; the caller supplies its body size and applies acceleration. */
void deceptus_move_horizontal(const deceptus_room *room, deceptus_entity *entity, deceptus_u8 half_width,
                              deceptus_u8 height);
void deceptus_move_vertical(const deceptus_room *room, deceptus_entity *entity, deceptus_u8 half_width,
                            deceptus_u8 height);
void deceptus_move_horizontal_dynamic(const deceptus_room *room, deceptus_entity *entity, deceptus_u8 half_width,
                                      deceptus_u8 height, const deceptus_mechanism_set *mechanisms);
void deceptus_move_vertical_dynamic(const deceptus_room *room, deceptus_entity *entity, deceptus_u8 half_width,
                                    deceptus_u8 height, const deceptus_mechanism_set *mechanisms);

deceptus_i16 deceptus_wall_side(const deceptus_room *room, const deceptus_entity *entity,
                                const deceptus_mechanism_set *mechanisms);

#endif

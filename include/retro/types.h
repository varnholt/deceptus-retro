#ifndef RETRO_TYPES_H
#define RETRO_TYPES_H

#include <stdint.h>

typedef int8_t retro_i8;
typedef uint8_t retro_u8;
typedef int16_t retro_i16;
typedef uint16_t retro_u16;
typedef int32_t retro_i32;
typedef uint32_t retro_u32;

/* signed 12.4 room-local positions: -2048..2047px, with 1/16px precision.
 * room/chunk coordinates are separate from simulation positions. */
typedef retro_i16 retro_fixed;
#define RETRO_ONE 16
#define RETRO_FIXED(pixel) ((retro_fixed)(pixel) * RETRO_ONE)

/* floor division, including negative coordinates at room boundaries. */
retro_i16 retro_floor(retro_fixed value);

#endif

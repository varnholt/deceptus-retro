#ifndef DECEPTUS_TYPES_H
#define DECEPTUS_TYPES_H

#include <stdint.h>

typedef int8_t deceptus_i8;
typedef uint8_t deceptus_u8;
typedef int16_t deceptus_i16;
typedef uint16_t deceptus_u16;
typedef int32_t deceptus_i32;
typedef uint32_t deceptus_u32;

/* signed 12.4 room-local positions: -2048..2047px, with 1/16px precision.
 * room/chunk coordinates are separate from simulation positions. */
typedef deceptus_i16 deceptus_fixed;
#define DECEPTUS_ONE 16
#define DECEPTUS_FIXED(pixel) ((deceptus_fixed)(pixel) * DECEPTUS_ONE)

/* floor division, including negative coordinates at room boundaries. */
deceptus_i16 deceptus_floor(deceptus_fixed value);

#endif

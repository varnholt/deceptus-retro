#ifndef RETRO_ENEMIES_H
#define RETRO_ENEMIES_H
#include "retro/engine.h"
#define RETRO_RAT 4
#define RETRO_ARROW_TRAP 5
#define RETRO_ARROW 6
#define RETRO_ARROW_MUZZLE 7

typedef struct retro_path_point {
    retro_i16 x, y;
} retro_path_point;
typedef struct retro_enemy_def {
    retro_u16 source_id;
    retro_i16 x, y;
    retro_u16 path_first, path_count, time_offset;
    retro_fixed speed, acceleration;
    retro_u16 damage, period, delays[3];
    retro_u8 kind, alignment, cycles[5], animation_fps, idle_ticks;
} retro_enemy_def;

void retro_enemies_init(retro_engine *engine);
retro_u16 retro_enemies_tick(retro_engine *engine);
void retro_enemies_render(const retro_engine *engine, retro_frame *frame);
#endif

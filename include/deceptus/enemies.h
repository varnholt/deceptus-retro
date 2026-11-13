#ifndef DECEPTUS_ENEMIES_H
#define DECEPTUS_ENEMIES_H
#include "deceptus/engine.h"
#define DECEPTUS_RAT 4
#define DECEPTUS_ARROW_TRAP 5
#define DECEPTUS_ARROW 6
#define DECEPTUS_ARROW_MUZZLE 7
#define DECEPTUS_PLAYER_SWIM 8
#define DECEPTUS_BLOB_2 9

typedef struct deceptus_path_point {
    deceptus_i16 x, y;
} deceptus_path_point;
typedef struct deceptus_enemy_def {
    deceptus_u16 source_id;
    deceptus_i16 x, y;
    deceptus_u16 path_first, path_count, time_offset;
    deceptus_fixed speed, acceleration;
    deceptus_u16 damage, period, delays[3];
    deceptus_u8 kind, alignment, cycles[5], animation_fps, idle_ticks;
} deceptus_enemy_def;

void deceptus_enemies_init(deceptus_engine *engine);
deceptus_u16 deceptus_enemies_tick(deceptus_engine *engine);
void deceptus_enemies_render(const deceptus_engine *engine, deceptus_frame *frame);
#endif

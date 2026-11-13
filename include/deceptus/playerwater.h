#ifndef DECEPTUS_PLAYER_WATER_H
#define DECEPTUS_PLAYER_WATER_H
#include "deceptus/engine.h"
deceptus_u8 deceptus_player_water_tick(deceptus_engine *engine, const deceptus_input *input);
void deceptus_player_water_render(const deceptus_engine *engine, deceptus_frame *frame);
#endif

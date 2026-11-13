#include "deceptus/enemies.h"
#include "deceptus/engine.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static deceptus_u8 cells[32 * 28];
static deceptus_engine engine;
static deceptus_room room;
static const deceptus_video_caps video = {240, 160, DECEPTUS_DRAW_CAPACITY};
static const deceptus_water_configuration water = {19, 19, 10, 30, 2, 4};
int main(void)
{
    deceptus_u16 x, y, tick;
    deceptus_u8 exited;
    deceptus_frame frame;
    memset(&room, 0, sizeof(room));
    for (y = 12; y < 24; ++y) {
        for (x = 0; x < 32; ++x) {
            cells[y * 32 + x] = DECEPTUS_WATER;
        }
    }
    for (x = 0; x < 32; ++x) {
        cells[24 * 32 + x] = DECEPTUS_SOLID;
    }
    room.collision = cells;
    room.columns = 32;
    room.rows = 28;
    room.spawn_x = 128;
    room.spawn_y = 160;
    room.patrol_x = -1;
    room.door_x = -1;
    room.max_health = 100;
    room.water = &water;
    deceptus_init(&engine, &room, &video);
    for (tick = 0; tick < 30; ++tick) {
        deceptus_tick(&engine, DECEPTUS_JUMP);
        assert(engine.entities[0].in_water && engine.entities[0].velocity_y >= 0);
        assert(engine.entities[0].velocity_y <= 10);
    }
    deceptus_render(&engine, &frame);
    assert(frame.count && frame.draws[0].kind == DECEPTUS_PLAYER_SWIM);
    exited = 0;
    for (tick = 0; tick < 180; ++tick) {
        deceptus_tick(&engine, DECEPTUS_JUMP);
        if (!engine.entities[0].in_water) {
            exited = 1;
            break;
        }
        assert(engine.entities[0].velocity_y >= -19);
    }
    assert(exited && engine.health == 100 && !engine.content_error);
    deceptus_place_player(&engine, 128, 160);
    for (tick = 0; tick < 150; ++tick) {
        deceptus_tick(&engine, DECEPTUS_RIGHT);
    }
    assert(engine.entities[0].in_water && engine.entities[0].grounded);
    assert(engine.entities[0].velocity_x <= 19 && !engine.entities[0].crouched);
    puts("source water detection contract, stroke delay, velocity bounds, swimming draw and surface exit passed");
    return 0;
}

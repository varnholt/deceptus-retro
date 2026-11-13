#include "retro/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                                               \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            fprintf(stderr, "failure at line %d: %s\n", __LINE__, #condition);                                         \
            exit(1);                                                                                                   \
        }                                                                                                              \
    } while (0)

static retro_u8 cells[64 * 28];
static retro_room room = {cells, 64, 28, 40, 192, 300, 192, 464, 192};
static retro_video_caps video = {256, 224, RETRO_DRAW_CAPACITY};
static retro_engine engine;

static void reset(void)
{
    retro_u16 column;
    memset(cells, 0, sizeof(cells));
    for (column = 0; column < 64; ++column) {
        cells[24 * 64 + column] = RETRO_SOLID;
    }
    retro_init(&engine, &room, &video);
    engine.entities[1].active = 0;
    retro_tick(&engine, 0);
}

static void run(retro_u16 ticks, retro_u16 buttons)
{
    while (ticks) {
        retro_tick(&engine, buttons);
        --ticks;
    }
}

int main(void)
{
    retro_fixed held_height, released_height;
    retro_u16 index, ticks;
    retro_frame frame;
    CHECK(sizeof(retro_i8) == 1 && sizeof(retro_i16) == 2 && sizeof(retro_i32) == 4);
    CHECK(retro_floor(-1) == -1 && retro_floor(-16) == -1 && retro_floor(-17) == -2);
    CHECK(retro_floor(-32768) == -2048 && retro_floor(32767) == 2047);
    reset();
    CHECK(engine.entities[0].grounded);
    CHECK(retro_floor(engine.entities[0].y) == 192);
    run(30, RETRO_RIGHT);
    CHECK(retro_floor(engine.entities[0].x) > 85);
    CHECK(retro_floor(engine.entities[0].y) == 192);
    run(20, RETRO_LEFT | RETRO_RIGHT);
    CHECK(engine.entities[0].velocity_x == 0);

    /* held jump must rise higher than a tapped jump. */
    reset();
    run(12, RETRO_JUMP);
    held_height = engine.entities[0].y;
    reset();
    run(1, RETRO_JUMP);
    run(11, 0);
    released_height = engine.entities[0].y;
    CHECK(held_height < released_height - RETRO_FIXED(8));
    run(90, 0);
    CHECK(engine.entities[0].grounded && retro_floor(engine.entities[0].y) == 192);

    /* wall collisions stop motion and leave the body outside the solid cell. */
    reset();
    for (index = 16; index < 24; ++index) {
        cells[index * 64 + 10] = RETRO_SOLID;
    }
    run(90, RETRO_RIGHT);
    CHECK(retro_floor(engine.entities[0].x) == 72);

    /* one-way platforms catch downward motion, and allow upward passage. */
    reset();
    for (index = 0; index < 64; ++index) {
        cells[16 * 64 + index] = RETRO_ONE_WAY;
    }
    engine.entities[0].y = RETRO_FIXED(120);
    engine.entities[0].grounded = 0;
    run(30, 0);
    CHECK(retro_floor(engine.entities[0].y) == 128);
    engine.entities[0].y = RETRO_FIXED(160);
    engine.entities[0].velocity_y = -62;
    engine.entities[0].grounded = 0;
    run(8, RETRO_JUMP);
    CHECK(retro_floor(engine.entities[0].y) < 140);

    /* six-frame coyote time and landing jump buffering. */
    reset();
    engine.entities[0].grounded = 0;
    engine.entities[0].coyote_ticks = 4;
    engine.entities[0].y = RETRO_FIXED(180);
    retro_tick(&engine, RETRO_JUMP);
    CHECK(engine.entities[0].velocity_y < 0);
    reset();
    engine.entities[0].grounded = 0;
    engine.entities[0].coyote_ticks = 0;
    engine.entities[0].y = RETRO_FIXED(190);
    engine.entities[0].velocity_y = 25;
    retro_tick(&engine, RETRO_JUMP);
    run(2, RETRO_JUMP);
    CHECK(engine.entities[0].velocity_y < 0);

    reset();
    retro_tick(&engine, RETRO_PAUSE);
    ticks = engine.ticks;
    run(30, RETRO_RIGHT);
    CHECK(engine.ticks == ticks && engine.paused);
    retro_tick(&engine, RETRO_PAUSE);
    CHECK(!engine.paused);
    retro_tick(&engine, RETRO_RESTART);
    CHECK(engine.video.width == 256 && engine.video.height == 224);
    CHECK(retro_floor(engine.entities[0].x) == room.spawn_x);

    /* contact damage returns to the checkpoint and has an invulnerability window. */
    reset();
    engine.entities[1].active = 1;
    engine.entities[1].x = engine.entities[0].x;
    engine.entities[1].y = engine.entities[0].y;
    retro_tick(&engine, 0);
    CHECK(engine.health == 2 && engine.hurt_ticks > 0);
    CHECK(engine.events[0].kind == RETRO_EVENT_HURT);
    retro_tick(&engine, 0);
    CHECK(engine.health == 2);

    reset();
    engine.entities[0].x = RETRO_FIXED(room.door_x);
    retro_tick(&engine, RETRO_UP);
    CHECK(engine.door_entries == 1);
    CHECK(engine.event_count == 1 && engine.events[0].kind == RETRO_EVENT_DOOR);
    CHECK(retro_floor(engine.entities[0].x) == room.spawn_x);

    /* fixed pools and render queues must never overflow. */
    reset();
    engine.entities[1].active = 1;
    for (index = 3; index < RETRO_ENTITY_CAPACITY; ++index) {
        CHECK(retro_spawn(&engine, RETRO_PATROL, 80, 192) != 0);
    }
    CHECK(retro_spawn(&engine, RETRO_PATROL, 80, 192) == 0);
    engine.video.max_draws = 2;
    retro_render(&engine, &frame);
    CHECK(frame.count == 2);
    engine.entities[0].x = RETRO_FIXED(510);
    retro_tick(&engine, 0);
    CHECK(engine.camera_x == 256);
    /* the smaller GBA viewport follows vertically without rescaling the world. */
    reset();
    engine.video.width = 240;
    engine.video.height = 160;
    retro_tick(&engine, 0);
    CHECK(engine.camera_y == 64);
    retro_render(&engine, &frame);
    CHECK(frame.camera_y == 64 && frame.draws[0].y == 80);
    printf("engine checks passed; state uses %lu bytes on this host\n", (unsigned long)sizeof(engine));
    return 0;
}

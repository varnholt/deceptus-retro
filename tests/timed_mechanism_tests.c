#include "retro/engine.h"
#include "retro/logic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                                               \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            fprintf(stderr, "line %d: %s\n", __LINE__, #condition);                                                    \
            exit(1);                                                                                                   \
        }                                                                                                              \
    } while (0)

static retro_mechanism_set mechanisms;
static const retro_mechanism_def switches[] = {{1, {0, 0, 72, 48}, 0, 1, 0, RETRO_MECH_LEVER, 1, 0},
                                               {2, {80, 0, 24, 24}, 0, 0, 100, RETRO_MECH_SPIKES, 1, 0}};
static const retro_u16 targets[] = {2};
static const retro_mechanism_timing switch_times[] = {{240, 180, 60, 240, 60, RETRO_TIMED_ANIMATION},
                                                      {240, 180, 60, 240, 60, RETRO_TIMED_ANIMATION}};
static const retro_mechanism_def platforms[] = {{3, {80, 160, 24, 24}, 0, 0, 0, RETRO_MECH_COLLAPSING, 1, 0}};
static const retro_mechanism_timing platform_times[] = {{0, 0, 60, 240, 60, 0}};

static void tick_many(retro_u16 count, const retro_rect *body)
{
    while (count--) {
        retro_mechanisms_tick(&mechanisms, body, 0);
    }
}

int main(void)
{
    retro_rect lever = {24, 0, 16, 30};
    retro_rect trap = {80, 0, 16, 30};
    retro_rect away = {0, 100, 16, 30};
    retro_rect feet = {80, 130, 16, 30};
    retro_rect overlap = {80, 150, 16, 30};
    retro_u16 index;
    retro_u8 cells[32 * 28];
    retro_room room;
    retro_engine engine;
    retro_video_caps video = {240, 160, 12};
    retro_mechanism_timing once = {0, 0, 2, 3, 2, RETRO_COLLAPSE_ONCE_TOUCHED};
    const retro_mechanism_def block[] = {{4, {80, 160, 24, 24}, 0, 0, 0, RETRO_MECH_BLOCK, 1, 0}};
    const retro_mechanism_def inverted[] = {{4, {80, 160, 24, 24}, 0, 0, 0, RETRO_MECH_BLOCK, 1, RETRO_MECH_INVERTED}};
    const retro_mechanism_timing interval[] = {{3, 100, 0, 0, 0, RETRO_TIMED_ANIMATION | RETRO_TIMED_INTERVAL}};
    const retro_u8 program[] = {1, 7, 1, 0, 2, 7, 2, 0, 0};

    CHECK(retro_mechanisms_init(&mechanisms, switches, 2, targets, 1));
    retro_mechanisms_configure(&mechanisms, switch_times);
    retro_mechanisms_tick(&mechanisms, &lever, 1);
    CHECK(!mechanisms.states[0].enabled && !mechanisms.states[1].enabled);
    retro_mechanisms_tick(&mechanisms, &trap, 0);
    CHECK(mechanisms.event_count == 1 && mechanisms.events[0].kind == RETRO_MECH_HIT);
    CHECK(mechanisms.states[1].frame == 32);
    tick_many(4, &trap);
    CHECK(mechanisms.states[1].frame == 35 && mechanisms.event_count == 1);
    tick_many(1, &trap);
    CHECK(mechanisms.states[1].frame == 36 && mechanisms.event_count == 0);
    tick_many(5, &away);
    CHECK(mechanisms.states[1].frame == 0 && mechanisms.states[0].frame == 21);
    retro_mechanisms_tick(&mechanisms, &lever, 1);
    CHECK(!mechanisms.states[0].enabled);
    tick_many(60, &away);
    retro_mechanisms_tick(&mechanisms, &lever, 1);
    CHECK(mechanisms.states[0].enabled);
    tick_many(50, &away);
    CHECK(mechanisms.states[1].frame == 32);
    for (index = 0; index < 8; ++index) {
        CHECK(retro_mechanism_enable(&mechanisms, 2, index & 1));
    }
    CHECK(mechanisms.states[1].pending_count == 8);
    CHECK(!retro_mechanism_enable(&mechanisms, 2, 0));
    CHECK(mechanisms.overflow && mechanisms.states[1].enabled);
    mechanisms.states[0].cooldown = 0;
    retro_mechanisms_tick(&mechanisms, &lever, 1);
    CHECK(mechanisms.overflow && mechanisms.states[0].enabled && mechanisms.states[1].enabled);
    mechanisms.states[1].pending_count = 8;
    mechanisms.states[1].enabled = 0;
    CHECK(retro_logic_execute(&mechanisms, program, sizeof(program), 0) == RETRO_LOGIC_BUDGET);
    CHECK(mechanisms.states[0].enabled && !mechanisms.states[1].enabled);

    CHECK(retro_mechanisms_init(&mechanisms, block, 1, 0, 0));
    retro_mechanisms_configure(&mechanisms, interval);
    CHECK(retro_mechanism_solid(&mechanisms, &overlap));
    tick_many(3, &away);
    CHECK(!mechanisms.states[0].enabled && !retro_mechanism_solid(&mechanisms, &overlap));
    tick_many(100, &away);
    CHECK(mechanisms.states[0].enabled && retro_mechanism_solid(&mechanisms, &overlap));

    CHECK(retro_mechanisms_init(&mechanisms, inverted, 1, 0, 0));
    retro_mechanisms_configure(&mechanisms, switch_times);
    CHECK(!retro_mechanism_solid(&mechanisms, &overlap));
    CHECK(retro_mechanism_enable(&mechanisms, 4, 0));
    tick_many(1, &away);
    CHECK(retro_mechanism_solid(&mechanisms, &overlap));
    CHECK(retro_mechanism_enable(&mechanisms, 4, 1));
    tick_many(25, &away);
    CHECK(!retro_mechanism_solid(&mechanisms, &overlap));

    CHECK(retro_mechanisms_init(&mechanisms, platforms, 1, 0, 0));
    retro_mechanisms_configure(&mechanisms, platform_times);
    tick_many(60, &feet);
    CHECK(mechanisms.states[0].active && mechanisms.states[0].timer == 60);
    tick_many(1, &away);
    CHECK(mechanisms.states[0].timer == 0);
    tick_many(61, &feet);
    CHECK(mechanisms.states[0].phase == 1 && !retro_mechanism_solid(&mechanisms, &overlap));
    tick_many(250, &overlap);
    CHECK(mechanisms.states[0].phase == 1);
    tick_many(1, &away);
    CHECK(mechanisms.states[0].phase == 2);
    tick_many(60, &away);
    CHECK(!mechanisms.states[0].active);
    tick_many(1, &away);
    CHECK(mechanisms.states[0].active && mechanisms.states[0].phase == 0);
    CHECK(mechanisms.events[0].kind == RETRO_MECH_RESTORED);
    CHECK(retro_mechanisms_init(&mechanisms, platforms, 1, 0, 0));
    retro_mechanisms_configure(&mechanisms, &once);
    tick_many(1, &feet);
    tick_many(2, &away);
    CHECK(mechanisms.states[0].phase == 1);

    memset(&room, 0, sizeof(room));
    memset(cells, 0, sizeof(cells));
    room.collision = cells;
    room.columns = 32;
    room.rows = 28;
    room.spawn_x = 92;
    room.spawn_y = 160;
    room.patrol_x = -1;
    room.door_x = -1;
    room.mechanisms = platforms;
    room.mechanism_count = 1;
    room.mechanism_timings = platform_times;
    retro_init(&engine, &room, &video);
    for (index = 0; index < 60; ++index) {
        retro_tick(&engine, 0);
    }
    CHECK(engine.entities[0].grounded && retro_floor(engine.entities[0].y) == 160);
    for (index = 0; index < 12; ++index) {
        retro_tick(&engine, 0);
    }
    CHECK(!engine.entities[0].grounded && retro_floor(engine.entities[0].y) > 160);
    retro_tick(&engine, RETRO_RESTART);
    CHECK(engine.mechanisms.states[0].active && !engine.mechanisms.states[0].phase);
    puts("timed switches, deadly animation frames, bounded queues and collapsing platforms passed");
    return 0;
}

#include "deceptus/engine.h"
#include "deceptus/logic.h"
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

static deceptus_mechanism_set mechanisms;
static const deceptus_mechanism_def switches[] = {{1, {0, 0, 72, 48}, 0, 1, 0, DECEPTUS_MECH_LEVER, 1, 0},
                                               {2, {80, 0, 24, 24}, 0, 0, 100, DECEPTUS_MECH_SPIKES, 1, 0}};
static const deceptus_u16 targets[] = {2};
static const deceptus_mechanism_timing switch_times[] = {{240, 180, 60, 240, 60, DECEPTUS_TIMED_ANIMATION},
                                                      {240, 180, 60, 240, 60, DECEPTUS_TIMED_ANIMATION}};
static const deceptus_mechanism_def platforms[] = {{3, {80, 160, 24, 24}, 0, 0, 0, DECEPTUS_MECH_COLLAPSING, 1, 0}};
static const deceptus_mechanism_timing platform_times[] = {{0, 0, 60, 240, 60, 0}};

static void tick_many(deceptus_u16 count, const deceptus_rect *body)
{
    while (count--) {
        deceptus_mechanisms_tick(&mechanisms, body, 0);
    }
}

int main(void)
{
    deceptus_rect lever = {24, 0, 16, 30};
    deceptus_rect trap = {80, 0, 16, 30};
    deceptus_rect away = {0, 100, 16, 30};
    deceptus_rect feet = {80, 130, 16, 30};
    deceptus_rect overlap = {80, 150, 16, 30};
    deceptus_u16 index;
    deceptus_u8 cells[32 * 28];
    deceptus_room room;
    deceptus_engine engine;
    deceptus_video_caps video = {240, 160, 12};
    deceptus_mechanism_timing once = {0, 0, 2, 3, 2, DECEPTUS_COLLAPSE_ONCE_TOUCHED};
    const deceptus_mechanism_def block[] = {{4, {80, 160, 24, 24}, 0, 0, 0, DECEPTUS_MECH_BLOCK, 1, 0}};
    const deceptus_mechanism_def inverted[] = {{4, {80, 160, 24, 24}, 0, 0, 0, DECEPTUS_MECH_BLOCK, 1, DECEPTUS_MECH_INVERTED}};
    const deceptus_mechanism_timing interval[] = {{3, 100, 0, 0, 0, DECEPTUS_TIMED_ANIMATION | DECEPTUS_TIMED_INTERVAL}};
    const deceptus_u8 program[] = {1, 7, 1, 0, 2, 7, 2, 0, 0};

    CHECK(deceptus_mechanisms_init(&mechanisms, switches, 2, targets, 1));
    deceptus_mechanisms_configure(&mechanisms, switch_times);
    deceptus_mechanisms_tick(&mechanisms, &lever, 1);
    CHECK(!mechanisms.states[0].enabled && !mechanisms.states[1].enabled);
    deceptus_mechanisms_tick(&mechanisms, &trap, 0);
    CHECK(mechanisms.event_count == 1 && mechanisms.events[0].kind == DECEPTUS_MECH_HIT);
    CHECK(mechanisms.states[1].frame == 32);
    tick_many(4, &trap);
    CHECK(mechanisms.states[1].frame == 35 && mechanisms.event_count == 1);
    tick_many(1, &trap);
    CHECK(mechanisms.states[1].frame == 36 && mechanisms.event_count == 0);
    tick_many(5, &away);
    CHECK(mechanisms.states[1].frame == 0 && mechanisms.states[0].frame == 21);
    deceptus_mechanisms_tick(&mechanisms, &lever, 1);
    CHECK(!mechanisms.states[0].enabled);
    tick_many(60, &away);
    deceptus_mechanisms_tick(&mechanisms, &lever, 1);
    CHECK(mechanisms.states[0].enabled);
    tick_many(50, &away);
    CHECK(mechanisms.states[1].frame == 32);
    for (index = 0; index < 8; ++index) {
        CHECK(deceptus_mechanism_enable(&mechanisms, 2, index & 1));
    }
    CHECK(mechanisms.states[1].pending_count == 8);
    CHECK(!deceptus_mechanism_enable(&mechanisms, 2, 0));
    CHECK(mechanisms.overflow && mechanisms.states[1].enabled);
    mechanisms.states[0].cooldown = 0;
    deceptus_mechanisms_tick(&mechanisms, &lever, 1);
    CHECK(mechanisms.overflow && mechanisms.states[0].enabled && mechanisms.states[1].enabled);
    mechanisms.states[1].pending_count = 8;
    mechanisms.states[1].enabled = 0;
    CHECK(deceptus_logic_execute(&mechanisms, program, sizeof(program), 0) == DECEPTUS_LOGIC_BUDGET);
    CHECK(mechanisms.states[0].enabled && !mechanisms.states[1].enabled);

    CHECK(deceptus_mechanisms_init(&mechanisms, block, 1, 0, 0));
    deceptus_mechanisms_configure(&mechanisms, interval);
    CHECK(deceptus_mechanism_solid(&mechanisms, &overlap));
    tick_many(3, &away);
    CHECK(!mechanisms.states[0].enabled && !deceptus_mechanism_solid(&mechanisms, &overlap));
    tick_many(100, &away);
    CHECK(mechanisms.states[0].enabled && deceptus_mechanism_solid(&mechanisms, &overlap));

    CHECK(deceptus_mechanisms_init(&mechanisms, inverted, 1, 0, 0));
    deceptus_mechanisms_configure(&mechanisms, switch_times);
    CHECK(!deceptus_mechanism_solid(&mechanisms, &overlap));
    CHECK(deceptus_mechanism_enable(&mechanisms, 4, 0));
    tick_many(1, &away);
    CHECK(deceptus_mechanism_solid(&mechanisms, &overlap));
    CHECK(deceptus_mechanism_enable(&mechanisms, 4, 1));
    tick_many(25, &away);
    CHECK(!deceptus_mechanism_solid(&mechanisms, &overlap));

    CHECK(deceptus_mechanisms_init(&mechanisms, platforms, 1, 0, 0));
    deceptus_mechanisms_configure(&mechanisms, platform_times);
    tick_many(60, &feet);
    CHECK(mechanisms.states[0].active && mechanisms.states[0].timer == 60);
    tick_many(1, &away);
    CHECK(mechanisms.states[0].timer == 0);
    tick_many(61, &feet);
    CHECK(mechanisms.states[0].phase == 1 && !deceptus_mechanism_solid(&mechanisms, &overlap));
    tick_many(250, &overlap);
    CHECK(mechanisms.states[0].phase == 1);
    tick_many(1, &away);
    CHECK(mechanisms.states[0].phase == 2);
    tick_many(60, &away);
    CHECK(!mechanisms.states[0].active);
    tick_many(1, &away);
    CHECK(mechanisms.states[0].active && mechanisms.states[0].phase == 0);
    CHECK(mechanisms.events[0].kind == DECEPTUS_MECH_RESTORED);
    CHECK(deceptus_mechanisms_init(&mechanisms, platforms, 1, 0, 0));
    deceptus_mechanisms_configure(&mechanisms, &once);
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
    deceptus_init(&engine, &room, &video);
    for (index = 0; index < 60; ++index) {
        deceptus_tick(&engine, 0);
    }
    CHECK(engine.entities[0].grounded && deceptus_floor(engine.entities[0].y) == 160);
    for (index = 0; index < 12; ++index) {
        deceptus_tick(&engine, 0);
    }
    CHECK(!engine.entities[0].grounded && deceptus_floor(engine.entities[0].y) > 160);
    deceptus_tick(&engine, DECEPTUS_RESTART);
    CHECK(engine.mechanisms.states[0].active && !engine.mechanisms.states[0].phase);
    puts("timed switches, deadly animation frames, bounded queues and collapsing platforms passed");
    return 0;
}

#include "deceptus/engine.h"
#include "deceptus/logic.h"
#include "deceptus/physics.h"
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

static const deceptus_mechanism_def definitions[] = {{10, {24, 144, 32, 48}, 0, 1, 0, DECEPTUS_MECH_LEVER, 1, 0},
                                                  {20, {80, 144, 8, 48}, 0, 0, 0, DECEPTUS_MECH_BLOCK, 1, 0},
                                                  {30, {120, 144, 32, 48}, 0, 0, 1, DECEPTUS_MECH_CHECKPOINT, 1, 0},
                                                  {40, {0, 0, 16, 16}, 0, 0, 0, DECEPTUS_MECH_SENSOR, 1, 0},
                                                  {50, {176, 144, 8, 48}, 0, 0, 1, DECEPTUS_MECH_DAMAGE, 1, 0}};
static const deceptus_u16 targets[] = {20};

int main(void)
{
    deceptus_mechanism_set mechanisms;
    deceptus_rect player = {32, 160, 16, 30};
    deceptus_rect block = {80, 160, 8, 8};
    deceptus_u8 cells[32 * 28];
    deceptus_room room;
    deceptus_entity body;
    deceptus_engine engine;
    deceptus_video_caps video = {256, 224, DECEPTUS_DRAW_CAPACITY};
    const deceptus_u8 malformed[] = {1, 7, 20, 0, 1, 7, 255, 255, 0};
    const deceptus_u8 looping[] = {11, 0, 0};
    const deceptus_u8 branch[] = {3, 0, 10, 12, 0, 2, 7, 20, 0, 11, 16, 0, 1, 7, 20, 0, 0};
    const deceptus_u8 boolean_actions[] = {3, 0, 3, 1, 4, 5, 7, 20, 0, 3, 0, 3, 1, 6, 8, 20, 0, 9, 10, 0, 0};
    deceptus_u16 index;

    CHECK(deceptus_mechanisms_init(&mechanisms, definitions, 5, targets, 1));
    CHECK(deceptus_mechanism_solid(&mechanisms, &block));
    deceptus_mechanisms_tick(&mechanisms, &player, 1);
    CHECK(!mechanisms.states[0].enabled && !mechanisms.states[1].enabled);
    CHECK(!deceptus_mechanism_solid(&mechanisms, &block));
    deceptus_mechanisms_tick(&mechanisms, &player, 0);
    CHECK(!mechanisms.states[1].enabled);
    deceptus_mechanisms_tick(&mechanisms, &player, 1);
    CHECK(mechanisms.states[1].enabled);
    CHECK(deceptus_logic_execute(&mechanisms, malformed, sizeof(malformed), 0) == DECEPTUS_LOGIC_MISSING_TARGET);
    CHECK(mechanisms.states[1].enabled);
    CHECK(deceptus_logic_execute(&mechanisms, looping, sizeof(looping), 0) == DECEPTUS_LOGIC_BUDGET);
    for (index = 0; index < 4; ++index) {
        mechanisms.states[0].enabled = 0;
        CHECK(deceptus_logic_execute(&mechanisms, boolean_actions, sizeof(boolean_actions), (deceptus_u8)index) ==
              DECEPTUS_LOGIC_OK);
        CHECK(mechanisms.states[1].enabled == ((index & 1) && !(index & 2)));
        CHECK(mechanisms.states[1].visible == ((index & 1) || (index & 2)));
        CHECK(mechanisms.states[0].enabled);
    }
    CHECK(deceptus_logic_execute(&mechanisms, branch, sizeof(branch), 0) == DECEPTUS_LOGIC_OK);
    CHECK(!mechanisms.states[1].enabled);
    CHECK(deceptus_logic_execute(&mechanisms, branch, sizeof(branch), 1) == DECEPTUS_LOGIC_OK);
    CHECK(mechanisms.states[1].enabled);
    player.x = 0;
    player.y = 0;
    deceptus_mechanisms_tick(&mechanisms, &player, 0);
    CHECK(mechanisms.event_count == 1 && mechanisms.events[0].kind == DECEPTUS_MECH_ENTER);
    deceptus_mechanisms_tick(&mechanisms, &player, 0);
    CHECK(mechanisms.event_count == 0);
    player.x = 64;
    deceptus_mechanisms_tick(&mechanisms, &player, 0);
    CHECK(mechanisms.event_count == 1 && mechanisms.events[0].kind == DECEPTUS_MECH_LEAVE);

    /* a live mechanism participates in collision, not just an enabled flag. */
    memset(cells, 0, sizeof(cells));
    memset(&room, 0, sizeof(room));
    memset(&body, 0, sizeof(body));
    room.collision = cells;
    room.columns = 32;
    room.rows = 28;
    body.x = DECEPTUS_FIXED(71);
    body.y = DECEPTUS_FIXED(192);
    body.velocity_x = DECEPTUS_FIXED(2);
    deceptus_move_horizontal_dynamic(&room, &body, 8, 30, &mechanisms);
    CHECK(deceptus_floor(body.x) == 72 && body.velocity_x == 0);
    deceptus_mechanism_enable(&mechanisms, 20, 0);
    body.velocity_x = DECEPTUS_FIXED(2);
    deceptus_move_horizontal_dynamic(&room, &body, 8, 30, &mechanisms);
    CHECK(deceptus_floor(body.x) == 74);

    /* imported checkpoints become the respawn point through actual overlap. */
    for (index = 0; index < 32; ++index) {
        cells[24 * 32 + index] = DECEPTUS_SOLID;
    }
    room.spawn_x = 40;
    room.spawn_y = 192;
    room.patrol_x = 220;
    room.patrol_y = 192;
    room.door_x = 240;
    room.door_y = 192;
    room.mechanisms = definitions;
    room.mechanism_count = 5;
    room.mechanism_targets = targets;
    room.mechanism_target_count = 1;
    deceptus_init(&engine, &room, &video);
    engine.entities[1].active = 0;
    engine.entities[0].x = DECEPTUS_FIXED(128);
    deceptus_tick(&engine, 0);
    CHECK(engine.checkpoint_id == 30 && engine.checkpoint_x == 136 && engine.checkpoint_y == 192);
    engine.entities[0].x = DECEPTUS_FIXED(240);
    deceptus_tick(&engine, DECEPTUS_UP);
    CHECK(deceptus_floor(engine.entities[0].x) == 136);
    engine.entities[0].x = DECEPTUS_FIXED(180);
    deceptus_tick(&engine, 0);
    CHECK(engine.health == 2 && deceptus_floor(engine.entities[0].x) == 136);
    CHECK(!deceptus_mechanisms_init(&mechanisms, definitions, DECEPTUS_MECHANISM_CAPACITY + 1, targets, 1));
    CHECK(mechanisms.count == 0);
    {
        static const deceptus_mechanism_def trap[] = {{90, {80, 160, 24, 24}, 0, 0, 100, DECEPTUS_MECH_SPIKES, 1, 0}};
        CHECK(deceptus_mechanisms_init(&mechanisms, trap, 1, 0, 0));
        player.x = 80;
        player.y = 160;
        deceptus_mechanisms_tick(&mechanisms, &player, 0);
        CHECK(mechanisms.event_count == 1 && mechanisms.events[0].kind == DECEPTUS_MECH_HIT);
        CHECK(mechanisms.events[0].value == 100);
        CHECK(!deceptus_mechanism_solid(&mechanisms, &player));
        deceptus_mechanism_enable(&mechanisms, 90, 0);
        deceptus_mechanisms_tick(&mechanisms, &player, 0);
        CHECK(mechanisms.event_count == 0);
    }
    puts("mechanism, dynamic collision, checkpoint and bounded logic checks passed");
    return 0;
}

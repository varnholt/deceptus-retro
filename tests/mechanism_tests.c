#include "retro/engine.h"
#include "retro/logic.h"
#include "retro/physics.h"
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

static const retro_mechanism_def definitions[] = {{10, {24, 144, 32, 48}, 0, 1, 0, RETRO_MECH_LEVER, 1, 0},
                                                  {20, {80, 144, 8, 48}, 0, 0, 0, RETRO_MECH_BLOCK, 1, 0},
                                                  {30, {120, 144, 32, 48}, 0, 0, 1, RETRO_MECH_CHECKPOINT, 1, 0},
                                                  {40, {0, 0, 16, 16}, 0, 0, 0, RETRO_MECH_SENSOR, 1, 0},
                                                  {50, {176, 144, 8, 48}, 0, 0, 1, RETRO_MECH_DAMAGE, 1, 0}};
static const retro_u16 targets[] = {20};

int main(void)
{
    retro_mechanism_set mechanisms;
    retro_rect player = {32, 160, 16, 30};
    retro_rect block = {80, 160, 8, 8};
    retro_u8 cells[32 * 28];
    retro_room room;
    retro_entity body;
    retro_engine engine;
    retro_video_caps video = {256, 224, RETRO_DRAW_CAPACITY};
    const retro_u8 malformed[] = {1, 7, 20, 0, 1, 7, 255, 255, 0};
    const retro_u8 looping[] = {11, 0, 0};
    const retro_u8 branch[] = {3, 0, 10, 12, 0, 2, 7, 20, 0, 11, 16, 0, 1, 7, 20, 0, 0};
    const retro_u8 boolean_actions[] = {3, 0, 3, 1, 4, 5, 7, 20, 0, 3, 0, 3, 1, 6, 8, 20, 0, 9, 10, 0, 0};
    retro_u16 index;

    CHECK(retro_mechanisms_init(&mechanisms, definitions, 5, targets, 1));
    CHECK(retro_mechanism_solid(&mechanisms, &block));
    retro_mechanisms_tick(&mechanisms, &player, 1);
    CHECK(!mechanisms.states[0].enabled && !mechanisms.states[1].enabled);
    CHECK(!retro_mechanism_solid(&mechanisms, &block));
    retro_mechanisms_tick(&mechanisms, &player, 0);
    CHECK(!mechanisms.states[1].enabled);
    retro_mechanisms_tick(&mechanisms, &player, 1);
    CHECK(mechanisms.states[1].enabled);
    CHECK(retro_logic_execute(&mechanisms, malformed, sizeof(malformed), 0) == RETRO_LOGIC_MISSING_TARGET);
    CHECK(mechanisms.states[1].enabled);
    CHECK(retro_logic_execute(&mechanisms, looping, sizeof(looping), 0) == RETRO_LOGIC_BUDGET);
    for (index = 0; index < 4; ++index) {
        mechanisms.states[0].enabled = 0;
        CHECK(retro_logic_execute(&mechanisms, boolean_actions, sizeof(boolean_actions), (retro_u8)index) ==
              RETRO_LOGIC_OK);
        CHECK(mechanisms.states[1].enabled == ((index & 1) && !(index & 2)));
        CHECK(mechanisms.states[1].visible == ((index & 1) || (index & 2)));
        CHECK(mechanisms.states[0].enabled);
    }
    CHECK(retro_logic_execute(&mechanisms, branch, sizeof(branch), 0) == RETRO_LOGIC_OK);
    CHECK(!mechanisms.states[1].enabled);
    CHECK(retro_logic_execute(&mechanisms, branch, sizeof(branch), 1) == RETRO_LOGIC_OK);
    CHECK(mechanisms.states[1].enabled);
    player.x = 0;
    player.y = 0;
    retro_mechanisms_tick(&mechanisms, &player, 0);
    CHECK(mechanisms.event_count == 1 && mechanisms.events[0].kind == RETRO_MECH_ENTER);
    retro_mechanisms_tick(&mechanisms, &player, 0);
    CHECK(mechanisms.event_count == 0);
    player.x = 64;
    retro_mechanisms_tick(&mechanisms, &player, 0);
    CHECK(mechanisms.event_count == 1 && mechanisms.events[0].kind == RETRO_MECH_LEAVE);

    /* a live mechanism participates in collision, not just an enabled flag. */
    memset(cells, 0, sizeof(cells));
    memset(&room, 0, sizeof(room));
    memset(&body, 0, sizeof(body));
    room.collision = cells;
    room.columns = 32;
    room.rows = 28;
    body.x = RETRO_FIXED(71);
    body.y = RETRO_FIXED(192);
    body.velocity_x = RETRO_FIXED(2);
    retro_move_horizontal_dynamic(&room, &body, 8, 30, &mechanisms);
    CHECK(retro_floor(body.x) == 72 && body.velocity_x == 0);
    retro_mechanism_enable(&mechanisms, 20, 0);
    body.velocity_x = RETRO_FIXED(2);
    retro_move_horizontal_dynamic(&room, &body, 8, 30, &mechanisms);
    CHECK(retro_floor(body.x) == 74);

    /* imported checkpoints become the respawn point through actual overlap. */
    for (index = 0; index < 32; ++index) {
        cells[24 * 32 + index] = RETRO_SOLID;
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
    retro_init(&engine, &room, &video);
    engine.entities[1].active = 0;
    engine.entities[0].x = RETRO_FIXED(128);
    retro_tick(&engine, 0);
    CHECK(engine.checkpoint_id == 30 && engine.checkpoint_x == 136 && engine.checkpoint_y == 192);
    engine.entities[0].x = RETRO_FIXED(240);
    retro_tick(&engine, RETRO_UP);
    CHECK(retro_floor(engine.entities[0].x) == 136);
    engine.entities[0].x = RETRO_FIXED(180);
    retro_tick(&engine, 0);
    CHECK(engine.health == 2 && retro_floor(engine.entities[0].x) == 136);
    CHECK(!retro_mechanisms_init(&mechanisms, definitions, RETRO_MECHANISM_CAPACITY + 1, targets, 1));
    CHECK(mechanisms.count == 0);
    puts("mechanism, dynamic collision, checkpoint and bounded logic checks passed");
    return 0;
}

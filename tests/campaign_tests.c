#include "deceptus/campaign.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(value)                                                                                                   \
    do {                                                                                                               \
        if (!(value)) {                                                                                                \
            fprintf(stderr, "line %d: %s\n", __LINE__, #value);                                                        \
            exit(1);                                                                                                   \
        }                                                                                                              \
    } while (0)
int main(void)
{
    static deceptus_u8 cells[32 * 28];
    static deceptus_room first, second;
    static const deceptus_room *rooms[2];
    static const deceptus_campaign_goal goals[2] = {{{32, 144, 48, 64}, 77}, {{32, 144, 48, 64}, 0}};
    static const deceptus_mechanism_def flag = {77, {100, 100, 24, 24}, 0, 0, 0, DECEPTUS_MECH_FLAG, 1, 0};
    deceptus_campaign_def definition;
    deceptus_campaign game;
    deceptus_video_caps video = {240, 160, 24};
    deceptus_u16 index;
    for (index = 0; index < 32; ++index) {
        cells[24 * 32 + index] = DECEPTUS_SOLID;
    }
    memset(&first, 0, sizeof(first));
    first.collision = cells;
    first.columns = 32;
    first.rows = 28;
    first.spawn_x = 40;
    first.spawn_y = 192;
    first.patrol_x = -1;
    first.door_x = -1;
    first.max_health = 100;
    first.mechanisms = &flag;
    first.mechanism_count = 1;
    second = first;
    second.mechanisms = 0;
    second.mechanism_count = 0;
    rooms[0] = &first;
    rooms[1] = &second;
    definition.rooms = rooms;
    definition.goals = goals;
    definition.room_count = 2;
    deceptus_campaign_init(&game, &definition, &video);
    CHECK(game.mode == DECEPTUS_CAMPAIGN_TITLE);
    deceptus_campaign_tick(&game, DECEPTUS_UP);
    CHECK(game.room_index == 0 && game.elapsed_ticks == 0);
    deceptus_campaign_tick(&game, DECEPTUS_PAUSE);
    CHECK(game.mode == DECEPTUS_CAMPAIGN_PLAYING);
    CHECK(!deceptus_campaign_at_goal(&game));
    deceptus_campaign_tick(&game, DECEPTUS_UP);
    CHECK(game.room_index == 0);
    deceptus_mechanism_enable(&game.engine.mechanisms, 77, 0);
    CHECK(deceptus_campaign_at_goal(&game));
    deceptus_campaign_tick(&game, 0);
    CHECK(deceptus_campaign_tick(&game, DECEPTUS_UP) == 1 && game.room_index == 1);
    deceptus_campaign_tick(&game, DECEPTUS_UP);
    CHECK(game.mode == DECEPTUS_CAMPAIGN_READY);
    deceptus_campaign_tick(&game, DECEPTUS_PAUSE);
    CHECK(game.mode == DECEPTUS_CAMPAIGN_PLAYING);
    deceptus_campaign_tick(&game, 0);
    deceptus_campaign_tick(&game, DECEPTUS_UP);
    CHECK(game.mode == DECEPTUS_CAMPAIGN_FINISHED);
    index = game.seconds;
    deceptus_campaign_tick(&game, 0);
    deceptus_campaign_tick(&game, DECEPTUS_RIGHT);
    CHECK(game.seconds == index);
    CHECK(deceptus_campaign_tick(&game, DECEPTUS_PAUSE) == 1 && game.mode == DECEPTUS_CAMPAIGN_PLAYING && game.room_index == 0);
    CHECK(game.engine.video.width == 240 && game.video.height == 160 && game.elapsed_ticks == 0);
    deceptus_campaign_tick(&game, 0);
    deceptus_campaign_tick(&game, DECEPTUS_RESTART);
    CHECK(game.restarts == 1 && game.room_index == 0);
    deceptus_campaign_tick(&game, 0);
    deceptus_campaign_tick(&game, DECEPTUS_PAUSE);
    CHECK(game.engine.paused);
    index = (deceptus_u16)game.elapsed_ticks;
    deceptus_campaign_tick(&game, DECEPTUS_RIGHT);
    CHECK(game.elapsed_ticks == index);
    deceptus_campaign_init(&game, 0, &video);
    CHECK(game.engine.content_error == 7);
    puts("campaign title, goal prerequisites, room transitions, finish, replay and pause checks passed");
    return 0;
}

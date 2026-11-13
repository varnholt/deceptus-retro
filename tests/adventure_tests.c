#include "deceptus/adventure.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    static deceptus_u8 cells[32 * 28];
    static deceptus_room rooms[2];
    static const deceptus_room *room_pointers[2];
    static const deceptus_campaign_goal goals[2] = {{{0, 0, 1, 1}, 0}, {{0, 0, 1, 1}, 0}};
    static const char *const pages[] = {"Original dialogue.", "Second page."};
    static const char *const names[] = {"", "Key", "Handle"};
    static const deceptus_mechanism_def lever = {581, {144, 144, 48, 48}, 0, 0,
                                              0,   DECEPTUS_MECH_LEVER,   1, DECEPTUS_MECH_MISSING_HANDLE};
    static const deceptus_interaction actions[] = {
        {1385, {24, 144, 32, 48}, pages, 0, 2, 2, 0, 2, 0, 1, 0, 0},
        {1432, {88, 144, 32, 48}, pages, 0, 1, 1, 0, 2, 1, 2, 0, 0},
        {581, {144, 144, 48, 48}, pages, 1, 8, 8, 581, 2, 2, 0, DECEPTUS_ACTION_HANDLE, 0}};
    static const deceptus_portal right = {{248, 144, 8, 64}, -256, 0, 1, DECEPTUS_PORTAL_RIGHT, 0, 0, 0};
    static const deceptus_portal left = {{0, 144, 8, 64}, 256, 0, 0, DECEPTUS_PORTAL_LEFT, 0, 0, 0};
    deceptus_adventure_room settings[2];
    deceptus_campaign_def campaign;
    deceptus_adventure_def definition;
    deceptus_adventure game;
    deceptus_video_caps video = {240, 160, 24};
    deceptus_u16 index, tick;
    for (index = 0; index < 32; ++index) {
        cells[24 * 32 + index] = DECEPTUS_SOLID;
    }
    memset(rooms, 0, sizeof(rooms));
    for (index = 0; index < 2; ++index) {
        rooms[index].collision = cells;
        rooms[index].columns = 32;
        rooms[index].rows = 28;
        rooms[index].spawn_x = 40;
        rooms[index].spawn_y = 192;
        rooms[index].patrol_x = -1;
        rooms[index].door_x = -1;
        rooms[index].max_health = 100;
        room_pointers[index] = &rooms[index];
    }
    rooms[0].mechanisms = &lever;
    rooms[0].mechanism_count = 1;
    settings[0].portals = &right;
    settings[0].portal_count = 1;
    settings[0].interactions = actions;
    settings[0].interaction_count = 3;
    settings[1].portals = &left;
    settings[1].portal_count = 1;
    settings[1].interactions = 0;
    settings[1].interaction_count = 0;
    campaign.rooms = room_pointers;
    campaign.goals = goals;
    campaign.room_count = 2;
    definition.campaign = &campaign;
    definition.rooms = settings;
    definition.item_names = names;
    definition.item_count = 2;
    deceptus_adventure_init(&game, &definition, &video);
    deceptus_adventure_tick(&game, DECEPTUS_PAUSE);
    deceptus_adventure_tick(&game, 0);
    /* an item cannot be used away from its interaction, or before acquiring it. */
    deceptus_place_player(&game.campaign.engine, 100, 192);
    deceptus_adventure_tick(&game, DECEPTUS_UP);
    assert(!game.flags && !game.inventory.counts[1]);
    deceptus_adventure_tick(&game, 0);
    deceptus_place_player(&game.campaign.engine, 40, 192);
    deceptus_adventure_tick(&game, DECEPTUS_UP);
    assert(game.flags == 2 && game.inventory.counts[1] == 1 && game.campaign.mode == DECEPTUS_DIALOGUE);
    tick = game.campaign.engine.ticks;
    deceptus_adventure_tick(&game, DECEPTUS_RIGHT);
    assert(game.campaign.engine.ticks == tick);
    deceptus_adventure_tick(&game, DECEPTUS_ACCEPT);
    deceptus_adventure_tick(&game, 0);
    deceptus_adventure_tick(&game, DECEPTUS_ACCEPT);
    assert(game.campaign.mode == DECEPTUS_CAMPAIGN_PLAYING);
    deceptus_adventure_tick(&game, 0);
    deceptus_place_player(&game.campaign.engine, 100, 192);
    deceptus_adventure_tick(&game, DECEPTUS_UP);
    assert(game.flags == 3 && !game.inventory.counts[1] && game.inventory.counts[2] == 1);
    deceptus_adventure_tick(&game, DECEPTUS_ACCEPT);
    deceptus_adventure_tick(&game, 0);
    deceptus_adventure_tick(&game, DECEPTUS_ACCEPT);
    deceptus_adventure_tick(&game, 0);
    deceptus_place_player(&game.campaign.engine, 160, 192);
    deceptus_adventure_tick(&game, DECEPTUS_UP);
    assert(game.flags == 11 && !game.inventory.counts[2]);
    assert(game.campaign.engine.mechanisms.states[0].handle_available);
    deceptus_adventure_tick(&game, DECEPTUS_ACCEPT);
    deceptus_adventure_tick(&game, 0);
    deceptus_adventure_tick(&game, DECEPTUS_ACCEPT);
    deceptus_adventure_tick(&game, 0);
    deceptus_adventure_tick(&game, DECEPTUS_UP);
    assert(!game.campaign.engine.mechanisms.states[0].enabled);
    /* entering and returning must retain the fitted handle and operated lever. */
    deceptus_place_player(&game.campaign.engine, 245, 192);
    assert(deceptus_adventure_tick(&game, DECEPTUS_RIGHT));
    assert(game.campaign.room_index == 1);
    for (index = 0; index < 12; ++index) {
        deceptus_adventure_tick(&game, 0);
    }
    deceptus_place_player(&game.campaign.engine, 10, 192);
    assert(deceptus_adventure_tick(&game, DECEPTUS_LEFT));
    assert(game.campaign.room_index == 0 && game.flags == 11);
    assert(game.campaign.engine.mechanisms.states[0].handle_available &&
           !game.campaign.engine.mechanisms.states[0].enabled);
    deceptus_adventure_tick(&game, DECEPTUS_RESTART);
    assert(game.campaign.mode == DECEPTUS_MENU_INVENTORY);
    deceptus_adventure_tick(&game, 0);
    deceptus_adventure_tick(&game, DECEPTUS_BACK);
    deceptus_adventure_tick(&game, DECEPTUS_PAUSE);
    deceptus_adventure_tick(&game, 0);
    tick = game.campaign.engine.ticks;
    deceptus_adventure_tick(&game, DECEPTUS_RIGHT);
    assert(game.campaign.engine.ticks == tick);
    assert(!deceptus_inventory_add(&game.inventory, 0));
    assert(!deceptus_inventory_remove(&game.inventory, 15));
    game.inventory.counts[1] = 255;
    assert(!deceptus_inventory_add(&game.inventory, 1));
    puts("adventure item use, dialogue, menus and persistent room state checks passed");
    return 0;
}

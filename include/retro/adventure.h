#ifndef RETRO_ADVENTURE_H
#define RETRO_ADVENTURE_H
#include "retro/campaign.h"

#define RETRO_ADVENTURE_ROOMS 12
#define RETRO_INVENTORY_ITEMS 16
#define RETRO_DIALOGUE 4
#define RETRO_MENU_PAUSE 5
#define RETRO_MENU_INVENTORY 6
#define RETRO_MENU_CONTROLS 7
#define RETRO_MENU_EXIT 8
#define RETRO_MENU_OPTIONS 9
#define RETRO_MENU_CONFIRM 10
#define RETRO_PORTAL_LEFT 1
#define RETRO_PORTAL_RIGHT 2
#define RETRO_PORTAL_DOWN 3
#define RETRO_ACTION_HANDLE 1

typedef struct retro_inventory {
    retro_u8 counts[RETRO_INVENTORY_ITEMS];
} retro_inventory;
retro_u8 retro_inventory_add(retro_inventory *inventory, retro_u8 item);
retro_u8 retro_inventory_remove(retro_inventory *inventory, retro_u8 item);

typedef struct retro_portal {
    retro_rect bounds;
    retro_i16 offset_x, offset_y;
    retro_u8 destination, direction;
    retro_i16 arrival_x, arrival_y;
    retro_u8 has_arrival;
} retro_portal;

typedef struct retro_interaction {
    retro_u16 source_id;
    retro_rect bounds;
    const char *const *pages;
    retro_u16 required_flags, forbidden_flags, set_flags, mechanism_id;
    retro_u8 page_count, cost_item, grant_item, effect, automatic;
} retro_interaction;

typedef struct retro_adventure_room {
    const retro_portal *portals;
    const retro_interaction *interactions;
    retro_u8 portal_count, interaction_count;
} retro_adventure_room;

typedef struct retro_adventure_def {
    const retro_campaign_def *campaign;
    const retro_adventure_room *rooms;
    const char *const *item_names;
    retro_u8 item_count;
} retro_adventure_def;

typedef struct retro_adventure {
    retro_campaign campaign;
    const retro_adventure_def *definition;
    retro_inventory inventory;
    retro_mechanism_state saved[RETRO_ADVENTURE_ROOMS][RETRO_MECHANISM_CAPACITY];
    retro_u8 visited[RETRO_ADVENTURE_ROOMS];
    retro_u16 flags;
    const retro_interaction *dialogue;
    retro_u8 page, selection, prompt, portal_cooldown, room_label_ticks, menu_return, options_return;
} retro_adventure;

void retro_adventure_init(retro_adventure *game, const retro_adventure_def *definition, const retro_video_caps *video);
retro_u8 retro_adventure_tick(retro_adventure *game, retro_u16 buttons);
retro_u8 retro_interaction_available(const retro_adventure *game, const retro_interaction *interaction);
#endif

#ifndef DECEPTUS_ADVENTURE_H
#define DECEPTUS_ADVENTURE_H
#include "deceptus/campaign.h"

#define DECEPTUS_ADVENTURE_ROOMS 12
#define DECEPTUS_INVENTORY_ITEMS 16
#define DECEPTUS_DIALOGUE 4
#define DECEPTUS_MENU_PAUSE 5
#define DECEPTUS_MENU_INVENTORY 6
#define DECEPTUS_MENU_CONTROLS 7
#define DECEPTUS_MENU_EXIT 8
#define DECEPTUS_MENU_OPTIONS 9
#define DECEPTUS_MENU_CONFIRM 10
#define DECEPTUS_PORTAL_LEFT 1
#define DECEPTUS_PORTAL_RIGHT 2
#define DECEPTUS_PORTAL_DOWN 3
#define DECEPTUS_ACTION_HANDLE 1

typedef struct deceptus_inventory {
    deceptus_u8 counts[DECEPTUS_INVENTORY_ITEMS];
} deceptus_inventory;
deceptus_u8 deceptus_inventory_add(deceptus_inventory *inventory, deceptus_u8 item);
deceptus_u8 deceptus_inventory_remove(deceptus_inventory *inventory, deceptus_u8 item);

typedef struct deceptus_portal {
    deceptus_rect bounds;
    deceptus_i16 offset_x, offset_y;
    deceptus_u8 destination, direction;
    deceptus_i16 arrival_x, arrival_y;
    deceptus_u8 has_arrival;
} deceptus_portal;

typedef struct deceptus_interaction {
    deceptus_u16 source_id;
    deceptus_rect bounds;
    const char *const *pages;
    deceptus_u16 required_flags, forbidden_flags, set_flags, mechanism_id;
    deceptus_u8 page_count, cost_item, grant_item, effect, automatic;
} deceptus_interaction;

typedef struct deceptus_adventure_room {
    const deceptus_portal *portals;
    const deceptus_interaction *interactions;
    deceptus_u8 portal_count, interaction_count;
} deceptus_adventure_room;

typedef struct deceptus_adventure_def {
    const deceptus_campaign_def *campaign;
    const deceptus_adventure_room *rooms;
    const char *const *item_names;
    deceptus_u8 item_count;
} deceptus_adventure_def;

typedef struct deceptus_adventure {
    deceptus_campaign campaign;
    const deceptus_adventure_def *definition;
    deceptus_inventory inventory;
    deceptus_mechanism_state saved[DECEPTUS_ADVENTURE_ROOMS][DECEPTUS_MECHANISM_CAPACITY];
    deceptus_u8 visited[DECEPTUS_ADVENTURE_ROOMS];
    deceptus_u16 flags;
    const deceptus_interaction *dialogue;
    deceptus_u8 page, selection, prompt, portal_cooldown, room_label_ticks, menu_return, options_return;
} deceptus_adventure;

void deceptus_adventure_init(deceptus_adventure *game, const deceptus_adventure_def *definition,
                             const deceptus_video_caps *video);
deceptus_u8 deceptus_adventure_tick(deceptus_adventure *game, deceptus_u16 buttons);
deceptus_u8 deceptus_interaction_available(const deceptus_adventure *game, const deceptus_interaction *interaction);
#endif

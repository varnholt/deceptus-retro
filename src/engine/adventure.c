#include "retro/adventure.h"
#include <string.h>

retro_u8 retro_inventory_add(retro_inventory *inventory, retro_u8 item)
{
    if (!item || item >= RETRO_INVENTORY_ITEMS || inventory->counts[item] == 255) {
        return 0;
    }
    ++inventory->counts[item];
    return 1;
}

retro_u8 retro_inventory_remove(retro_inventory *inventory, retro_u8 item)
{
    if (!item || item >= RETRO_INVENTORY_ITEMS || !inventory->counts[item]) {
        return 0;
    }
    --inventory->counts[item];
    return 1;
}

static retro_u8 overlaps(const retro_rect *bounds, retro_i16 x, retro_i16 y)
{
    return x + 8 > bounds->x && x - 8 < bounds->x + bounds->width && y > bounds->y &&
           y - 30 < bounds->y + bounds->height;
}

retro_u8 retro_interaction_available(const retro_adventure *game, const retro_interaction *interaction)
{
    return (game->flags & interaction->required_flags) == interaction->required_flags &&
           !(game->flags & interaction->forbidden_flags) &&
           (!interaction->cost_item ||
            (interaction->cost_item < RETRO_INVENTORY_ITEMS && game->inventory.counts[interaction->cost_item]));
}

void retro_adventure_init(retro_adventure *game, const retro_adventure_def *definition, const retro_video_caps *video)
{
    retro_video_caps saved_video;
    saved_video = *video;
    memset(game, 0, sizeof(*game));
    game->definition = definition;
    game->prompt = 255;
    game->room_label_ticks = 120;
    if (!definition || !definition->campaign || !definition->rooms || !definition->campaign->rooms ||
        !definition->campaign->room_count || definition->campaign->room_count > RETRO_ADVENTURE_ROOMS ||
        definition->item_count >= RETRO_INVENTORY_ITEMS) {
        game->campaign.engine.content_error = 8;
        return;
    }
    game->campaign.definition = definition->campaign;
    game->campaign.video = saved_video;
    retro_init(&game->campaign.engine, definition->campaign->rooms[0], &saved_video);
    retro_tick(&game->campaign.engine, 0);
}

static void save_room(retro_adventure *game)
{
    retro_u8 room;
    room = game->campaign.room_index;
    memcpy(game->saved[room], game->campaign.engine.mechanisms.states, sizeof(game->saved[room]));
    game->visited[room] = 1;
}

static void load_room(retro_adventure *game, retro_u8 room, retro_u16 buttons)
{
    retro_engine *engine;
    retro_u8 health;
    engine = &game->campaign.engine;
    health = engine->health;
    game->campaign.room_index = room;
    retro_init(engine, game->definition->campaign->rooms[room], &game->campaign.video);
    engine->health = health;
    if (game->visited[room]) {
        memcpy(engine->mechanisms.states, game->saved[room], sizeof(game->saved[room]));
    }
    engine->previous_buttons = buttons;
    game->portal_cooldown = 12;
    game->room_label_ticks = 120;
    game->prompt = 255;
}

static retro_u8 interact(retro_adventure *game, const retro_interaction *action)
{
    retro_i16 slot;
    slot = -1;
    if (action->effect == RETRO_ACTION_HANDLE) {
        slot = retro_mechanism_find(&game->campaign.engine.mechanisms, action->mechanism_id);
        if (slot < 0 || game->campaign.engine.mechanisms.definitions[slot].kind != RETRO_MECH_LEVER) {
            return 0;
        }
    }
    if (action->grant_item &&
        (action->grant_item >= RETRO_INVENTORY_ITEMS || game->inventory.counts[action->grant_item] == 255)) {
        return 0;
    }
    if (action->cost_item && !retro_inventory_remove(&game->inventory, action->cost_item)) {
        return 0;
    }
    if (action->grant_item) {
        retro_inventory_add(&game->inventory, action->grant_item);
    }
    if (slot >= 0) {
        game->campaign.engine.mechanisms.states[slot].handle_available = 1;
    }
    game->flags |= action->set_flags;
    retro_event_push(&game->campaign.engine, action->grant_item  ? RETRO_EVENT_ITEM
                                             : action->effect    ? RETRO_EVENT_LEVER_INSERT
                                             : action->cost_item ? RETRO_EVENT_DOOR
                                                                 : RETRO_EVENT_DIALOGUE);
    if (action->page_count) {
        game->dialogue = action;
        game->page = 0;
        game->campaign.mode = RETRO_DIALOGUE;
    }
    return 1;
}

retro_u8 retro_adventure_tick(retro_adventure *game, retro_u16 buttons)
{
    retro_campaign *campaign;
    retro_engine *engine;
    const retro_adventure_room *room;
    const retro_interaction *action;
    const retro_portal *portal;
    retro_u16 pressed, deaths;
    retro_i16 x, y;
    retro_fixed velocity_x, velocity_y;
    retro_u8 index, changed, direction;
    campaign = &game->campaign;
    engine = &campaign->engine;
    engine->event_count = 0;
    engine->event_overflow = 0;
    pressed = buttons & ~campaign->previous_buttons;
    campaign->previous_buttons = buttons;
    if (engine->content_error) {
        return 0;
    }
    if (campaign->mode == RETRO_CAMPAIGN_TITLE) {
        if (pressed & RETRO_DOWN) {
            if (++game->selection >= 3) {
                game->selection = 0;
            }
        }
        if (pressed & RETRO_UP) {
            game->selection = game->selection ? game->selection - 1 : 2;
        }
        if (pressed & (RETRO_ACCEPT | RETRO_PAUSE | RETRO_JUMP)) {
            campaign->mode = game->selection == 0   ? RETRO_CAMPAIGN_PLAYING
                             : game->selection == 1 ? RETRO_MENU_OPTIONS
                                                    : RETRO_MENU_EXIT;
            game->options_return = RETRO_CAMPAIGN_TITLE;
            game->selection = 0;
        }
        engine->previous_buttons = buttons;
        return 0;
    }
    if (campaign->mode == RETRO_MENU_EXIT) {
        if (pressed & (RETRO_BACK | RETRO_PAUSE | RETRO_ACCEPT)) {
            campaign->mode = RETRO_CAMPAIGN_TITLE;
            game->selection = 0;
        }
        return 0;
    }
    if (campaign->mode == RETRO_MENU_OPTIONS) {
        if (pressed & (RETRO_UP | RETRO_DOWN)) {
            game->selection ^= 1;
        }
        if ((pressed & RETRO_BACK) || ((pressed & (RETRO_ACCEPT | RETRO_JUMP | RETRO_PAUSE)) && game->selection)) {
            campaign->mode = game->options_return;
            game->selection = 0;
        } else if (pressed & (RETRO_ACCEPT | RETRO_JUMP | RETRO_PAUSE)) {
            campaign->mode = RETRO_MENU_CONTROLS;
            game->menu_return = RETRO_MENU_OPTIONS;
        }
        engine->previous_buttons = buttons;
        return 0;
    }
    if (campaign->mode == RETRO_MENU_CONFIRM) {
        if (pressed & (RETRO_UP | RETRO_DOWN)) {
            game->selection ^= 1;
        }
        if (pressed & RETRO_BACK) {
            campaign->mode = RETRO_MENU_PAUSE;
            game->selection = 0;
        } else if (pressed & (RETRO_ACCEPT | RETRO_JUMP | RETRO_PAUSE)) {
            if (game->selection) {
                retro_adventure_init(game, game->definition, &campaign->video);
                game->campaign.previous_buttons = buttons;
                return 1;
            }
            campaign->mode = RETRO_MENU_PAUSE;
            game->selection = 0;
        }
        engine->previous_buttons = buttons;
        return 0;
    }
    if (campaign->mode == RETRO_DIALOGUE) {
        if (pressed & (RETRO_ACCEPT | RETRO_JUMP | RETRO_UP | RETRO_PAUSE)) {
            if (++game->page >= game->dialogue->page_count) {
                campaign->mode = RETRO_CAMPAIGN_PLAYING;
                game->dialogue = 0;
                engine->previous_buttons = buttons;
            }
        }
        return 0;
    }
    if (campaign->mode == RETRO_MENU_CONTROLS || campaign->mode == RETRO_MENU_INVENTORY) {
        if (pressed & (RETRO_BACK | RETRO_RESTART | RETRO_PAUSE | RETRO_ACCEPT | RETRO_JUMP)) {
            campaign->mode = game->menu_return;
            engine->previous_buttons = buttons;
        }
        return 0;
    }
    if (campaign->mode == RETRO_MENU_PAUSE) {
        if (pressed & RETRO_DOWN) {
            if (++game->selection >= 3) {
                game->selection = 0;
            }
        }
        if (pressed & RETRO_UP) {
            game->selection = game->selection ? game->selection - 1 : 2;
        }
        if (pressed & RETRO_BACK) {
            campaign->mode = RETRO_CAMPAIGN_PLAYING;
        } else if (pressed & (RETRO_ACCEPT | RETRO_JUMP | RETRO_PAUSE)) {
            if (game->selection == 0) {
                campaign->mode = RETRO_CAMPAIGN_PLAYING;
            } else if (game->selection == 1) {
                campaign->mode = RETRO_MENU_OPTIONS;
                game->options_return = RETRO_MENU_PAUSE;
                game->selection = 0;
            } else {
                campaign->mode = RETRO_MENU_CONFIRM;
                game->selection = 0;
            }
        }
        engine->previous_buttons = buttons;
        return 0;
    }
    if (pressed & RETRO_PAUSE) {
        campaign->mode = RETRO_MENU_PAUSE;
        game->selection = 0;
        return 0;
    }
    if (pressed & RETRO_RESTART) {
        campaign->mode = RETRO_MENU_INVENTORY;
        game->menu_return = RETRO_CAMPAIGN_PLAYING;
        return 0;
    }
    room = &game->definition->rooms[campaign->room_index];
    if (game->room_label_ticks) {
        --game->room_label_ticks;
    }
    x = retro_floor(engine->entities[0].x);
    y = retro_floor(engine->entities[0].y);
    game->prompt = 255;
    for (index = 0; index < room->interaction_count; ++index) {
        action = &room->interactions[index];
        if (retro_interaction_available(game, action) && overlaps(&action->bounds, x, y)) {
            game->prompt = index;
            if ((action->page_count || action->cost_item || action->grant_item || action->set_flags ||
                 action->effect) &&
                (action->automatic || (pressed & RETRO_UP)) && interact(game, action)) {
                /* consuming interaction input prevents the newly fitted lever toggling on the same press. */
                engine->previous_buttons = buttons;
                return 0;
            }
            break;
        }
    }
    deaths = engine->deaths;
    retro_tick(engine, buttons & ~(RETRO_PAUSE | RETRO_RESTART));
    campaign->deaths += engine->deaths - deaths;
    ++campaign->elapsed_ticks;
    if (++campaign->subsecond >= 60) {
        campaign->subsecond = 0;
        ++campaign->seconds;
    }
    if (game->portal_cooldown) {
        --game->portal_cooldown;
        return 0;
    }
    x = retro_floor(engine->entities[0].x);
    y = retro_floor(engine->entities[0].y);
    changed = 0;
    for (index = 0; index < room->portal_count; ++index) {
        portal = &room->portals[index];
        direction = (portal->direction == RETRO_PORTAL_LEFT && (buttons & RETRO_LEFT)) ||
                    (portal->direction == RETRO_PORTAL_RIGHT && (buttons & RETRO_RIGHT)) ||
                    (portal->direction == RETRO_PORTAL_DOWN && engine->entities[0].velocity_y > 0);
        if (!direction || !overlaps(&portal->bounds, x, y) ||
            (portal->direction != RETRO_PORTAL_DOWN &&
             (y < portal->bounds.y || y >= portal->bounds.y + portal->bounds.height))) {
            continue;
        }
        if (portal->destination >= game->definition->campaign->room_count) {
            engine->content_error = 8;
            return 0;
        }
        velocity_x = engine->entities[0].velocity_x;
        velocity_y = engine->entities[0].velocity_y;
        save_room(game);
        load_room(game, portal->destination, buttons);
        x += portal->offset_x;
        y += portal->offset_y;
        if (portal->direction == RETRO_PORTAL_LEFT) {
            x = engine->room->columns * 8 - 20;
        } else if (portal->direction == RETRO_PORTAL_RIGHT) {
            x = 20;
        } else {
            y = 32;
        }
        if (portal->has_arrival) {
            retro_place_player(engine, portal->arrival_x, portal->arrival_y);
        } else {
            retro_place_player(engine, x, y);
            engine->entities[0].velocity_x = velocity_x;
            engine->entities[0].velocity_y = velocity_y;
        }
        changed = 1;
        break;
    }
    return changed;
}

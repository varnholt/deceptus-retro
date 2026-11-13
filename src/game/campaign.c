#include "retro/campaign.h"
#include <string.h>

void retro_campaign_init(retro_campaign *campaign, const retro_campaign_def *definition, const retro_video_caps *video)
{
    retro_video_caps saved_video;
    saved_video = *video;
    memset(campaign, 0, sizeof(*campaign));
    campaign->definition = definition;
    campaign->video = saved_video;
    if (!definition || !definition->room_count || !definition->rooms || !definition->goals) {
        campaign->engine.content_error = 7;
        return;
    }
    retro_init(&campaign->engine, definition->rooms[0], &saved_video);
    retro_tick(&campaign->engine, 0);
}

retro_u8 retro_campaign_at_goal(const retro_campaign *campaign)
{
    const retro_rect *goal;
    retro_i16 x, y, slot;
    if (campaign->mode != RETRO_CAMPAIGN_PLAYING) {
        return 0;
    }
    goal = &campaign->definition->goals[campaign->room_index].bounds;
    x = retro_floor(campaign->engine.entities[0].x);
    y = retro_floor(campaign->engine.entities[0].y);
    if (x + 8 <= goal->x || x - 8 >= goal->x + goal->width || y <= goal->y || y - 30 >= goal->y + goal->height) {
        return 0;
    }
    if (campaign->definition->goals[campaign->room_index].disabled_mechanism) {
        slot = retro_mechanism_find(&campaign->engine.mechanisms,
                                    campaign->definition->goals[campaign->room_index].disabled_mechanism);
        if (slot < 0 || campaign->engine.mechanisms.states[slot].enabled) {
            return 0;
        }
    }
    return 1;
}

retro_u8 retro_campaign_tick(retro_campaign *campaign, retro_u16 buttons)
{
    retro_u16 pressed, deaths;
    pressed = buttons & ~campaign->previous_buttons;
    campaign->previous_buttons = buttons;
    if (campaign->engine.content_error) {
        return 0;
    }
    if (campaign->mode == RETRO_CAMPAIGN_TITLE || campaign->mode == RETRO_CAMPAIGN_READY) {
        if (pressed & (RETRO_PAUSE | RETRO_JUMP)) {
            campaign->mode = RETRO_CAMPAIGN_PLAYING;
            campaign->engine.previous_buttons = buttons;
        }
        return 0;
    }
    if (campaign->mode == RETRO_CAMPAIGN_FINISHED) {
        if (pressed & (RETRO_PAUSE | RETRO_RESTART)) {
            retro_campaign_init(campaign, campaign->definition, &campaign->video);
            campaign->previous_buttons = buttons;
            campaign->mode = RETRO_CAMPAIGN_PLAYING;
            campaign->engine.previous_buttons = buttons;
            return 1;
        }
        return 0;
    }
    deaths = campaign->engine.deaths;
    if (pressed & RETRO_RESTART) {
        ++campaign->restarts;
    }
    retro_tick(&campaign->engine, buttons);
    if (campaign->engine.deaths > deaths) {
        campaign->deaths += campaign->engine.deaths - deaths;
    }
    if (!campaign->engine.paused) {
        ++campaign->elapsed_ticks;
        if (++campaign->subsecond >= 60) {
            campaign->subsecond = 0;
            ++campaign->seconds;
        }
    }
    if (!campaign->engine.paused && (pressed & RETRO_UP) && retro_campaign_at_goal(campaign)) {
        if (campaign->room_index + 1 >= campaign->definition->room_count) {
            campaign->mode = RETRO_CAMPAIGN_FINISHED;
        } else {
            ++campaign->room_index;
            campaign->mode = RETRO_CAMPAIGN_READY;
            retro_init(&campaign->engine, campaign->definition->rooms[campaign->room_index], &campaign->video);
            campaign->engine.previous_buttons = buttons;
            return 1;
        }
    }
    return 0;
}

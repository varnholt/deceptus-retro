#ifndef RETRO_CAMPAIGN_H
#define RETRO_CAMPAIGN_H
#include "retro/engine.h"
#define RETRO_CAMPAIGN_TITLE 0
#define RETRO_CAMPAIGN_PLAYING 1
#define RETRO_CAMPAIGN_FINISHED 2
#define RETRO_CAMPAIGN_READY 3

typedef struct retro_campaign_goal {
    retro_rect bounds;
    retro_u16 disabled_mechanism;
} retro_campaign_goal;
typedef struct retro_campaign_def {
    const retro_room *const *rooms;
    const retro_campaign_goal *goals;
    retro_u8 room_count;
} retro_campaign_def;
typedef struct retro_campaign {
    retro_engine engine;
    const retro_campaign_def *definition;
    retro_video_caps video;
    retro_u32 elapsed_ticks;
    retro_u16 deaths, restarts, previous_buttons, seconds;
    retro_u8 subsecond;
    retro_u8 room_index, mode;
} retro_campaign;

void retro_campaign_init(retro_campaign *campaign, const retro_campaign_def *definition, const retro_video_caps *video);
/* returns one when platform resources must change to the current room. */
retro_u8 retro_campaign_tick(retro_campaign *campaign, retro_u16 buttons);
retro_u8 retro_campaign_at_goal(const retro_campaign *campaign);
#endif

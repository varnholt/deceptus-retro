#ifndef DECEPTUS_CAMPAIGN_H
#define DECEPTUS_CAMPAIGN_H
#include "deceptus/engine.h"
#define DECEPTUS_CAMPAIGN_TITLE 0
#define DECEPTUS_CAMPAIGN_PLAYING 1
#define DECEPTUS_CAMPAIGN_FINISHED 2
#define DECEPTUS_CAMPAIGN_READY 3

typedef struct deceptus_campaign_goal {
    deceptus_rect bounds;
    deceptus_u16 disabled_mechanism;
} deceptus_campaign_goal;
typedef struct deceptus_campaign_def {
    const deceptus_room *const *rooms;
    const deceptus_campaign_goal *goals;
    deceptus_u8 room_count;
} deceptus_campaign_def;
typedef struct deceptus_campaign {
    deceptus_engine engine;
    const deceptus_campaign_def *definition;
    deceptus_video_caps video;
    deceptus_u32 elapsed_ticks;
    deceptus_u16 deaths, restarts, previous_buttons, seconds;
    deceptus_u8 subsecond;
    deceptus_u8 room_index, mode;
} deceptus_campaign;

void deceptus_campaign_init(deceptus_campaign *campaign, const deceptus_campaign_def *definition,
                            const deceptus_video_caps *video);
/* returns one when platform resources must change to the current room. */
deceptus_u8 deceptus_campaign_tick(deceptus_campaign *campaign, deceptus_u16 buttons);
deceptus_u8 deceptus_campaign_at_goal(const deceptus_campaign *campaign);
#endif

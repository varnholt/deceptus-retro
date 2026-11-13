#if GBA_CAMPAIGN
static retro_u16 hud_map[1024] __attribute__((aligned(4)));

static void hud_text(retro_u16 row, retro_u16 column, const char *text)
{
    retro_u8 character;
    while (*text && column < 30 && row < 20) {
        character = (retro_u8)*text++;
        if (character < 32 || character > 127) {
            character = 32;
        }
        hud_map[row * 32 + column++] = (retro_u16)(character - 31) | (15 << 12);
    }
}
static void hud_number(retro_u16 row, retro_u16 column, retro_u16 value, retro_u8 digits)
{
    char text[6];
    retro_u16 quotient;
    retro_u8 index;
    if (digits > 5) {
        digits = 5;
    }
    text[digits] = 0;
    for (index = digits; index > 0; --index) {
        quotient = value / 10;
        text[index - 1] = (char)('0' + value - quotient * 10);
        value = quotient;
    }
    hud_text(row, column, text);
}
static void hud_panel(retro_u16 first, retro_u16 last)
{
    retro_u16 row;
    for (row = first; row <= last; ++row) {
        hud_text(row, 0, "                              ");
    }
}
static void hud_prepare(void)
{
    retro_u16 index, minutes;
    for (index = 0; index < 1024; ++index) {
        hud_map[index] = 0;
    }
    if (campaign.mode == RETRO_CAMPAIGN_TITLE) {
        hud_panel(4, 18);
        hud_text(5, 10, "DECEPTUS");
        hud_text(7, 5, "GBA PLAYABLE PREVIEW");
        hud_text(9, 6, "FOUR CATACOMB AREAS");
        hud_text(11, 4, "ARROWS MOVE. A/B JUMP");
        hud_text(12, 3, "UP INTERACT. START PAUSE");
        hud_text(13, 3, "SELECT RESTARTS THE ROOM");
        hud_text(16, 9, "PRESS START");
        return;
    }
    if (campaign.mode == RETRO_CAMPAIGN_READY) {
        hud_panel(4, 18);
        hud_text(5, 10, "AREA");
        hud_number(5, 15, campaign.room_index + 1, 1);
        hud_text(7, 3, active_stage->title);
        hud_text(10, 1, active_stage->hint1);
        hud_text(12, 1, active_stage->hint2);
        hud_text(16, 9, "PRESS START");
        return;
    }
    minutes = campaign.seconds / 60;
    if (campaign.mode == RETRO_CAMPAIGN_FINISHED) {
        hud_panel(4, 18);
        hud_text(5, 7, "PREVIEW COMPLETE");
        hud_text(7, 6, "THANKS FOR PLAYING");
        hud_text(10, 8, "TIME");
        hud_number(10, 15, minutes, 2);
        hud_text(10, 17, ":");
        hud_number(10, 18, campaign.seconds - minutes * 60, 2);
        hud_text(12, 8, "DEATHS");
        hud_number(12, 17, campaign.deaths, 3);
        hud_text(13, 8, "RESTARTS");
        hud_number(13, 17, campaign.restarts, 3);
        hud_text(16, 6, "START TO PLAY AGAIN");
        return;
    }
    hud_panel(0, 2);
    hud_text(0, 0, active_stage->title);
    hud_text(1, 0, "AREA");
    hud_number(1, 5, campaign.room_index + 1, 1);
    hud_text(1, 6, "/4");
    hud_text(1, 9, "HP");
    hud_number(1, 11, engine.health, 3);
    hud_number(1, 16, minutes, 2);
    hud_text(1, 18, ":");
    hud_number(1, 19, campaign.seconds - minutes * 60, 2);
    hud_text(1, 23, "D");
    hud_number(1, 25, campaign.deaths, 2);
    hud_text(2, 0,
             engine.paused                       ? "PAUSED - START TO RESUME"
             : retro_campaign_at_goal(&campaign) ? "UP TO CONTINUE"
                                                 : active_stage->instruction);
    if (engine.content_error) {
        hud_text(2, 0, "CONTENT LIMIT ");
        hud_number(2, 15, engine.content_error, 2);
    }
}
static void hud_commit(void) { dma_copy(hud_map, (void *)0x0600d000, sizeof(hud_map)); }
#else
static void hud_prepare(void) {}
static void hud_commit(void) {}
#endif

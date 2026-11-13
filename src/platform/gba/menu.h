#if GBA_TUTORIAL
static retro_u8 menu_active;
static retro_u16 menu_angle;
static retro_u8 menu_requested(void)
{
    return campaign.mode == RETRO_CAMPAIGN_TITLE || campaign.mode == RETRO_MENU_EXIT ||
           ((campaign.mode == RETRO_MENU_OPTIONS || campaign.mode == RETRO_MENU_CONTROLS) &&
            adventure.options_return == RETRO_CAMPAIGN_TITLE);
}
static void menu_prepare(void)
{
    retro_u8 requested;
    requested = menu_requested();
    if (requested && !menu_active) {
        REG_DISPLAY = 1 << 6;
        dma_copy(gba_menu_texture, (void *)0x06000000, 16384);
        dma_copy(gba_menu_map, (void *)0x0600e000, 256);
        dma_copy(gba_menu_palette, (void *)0x05000000, 512);
        dma_copy(gba_font, (void *)0x0600c000, GBA_FONT_BYTES);
        REG16(0x050001e2) = 0x7fff;
        REG16(0x04000008) = (3 << 2) | (26 << 8);
        REG16(0x0400000c) = 3 | (28 << 8) | (1 << 13);
        hud_reset();
        menu_active = 1;
    } else if (!requested && menu_active) {
        video_init();
        background_reset();
        hud_reset();
        menu_active = 0;
    }
    if (menu_active) {
        stream_changed = 0;
        menu_angle = (menu_angle + 3) & 4095;
    }
}
static void menu_commit(void)
{
    retro_i16 sine, cosine;
    const retro_i16 *table;
    table = (const retro_i16 *)gba_menu_sine;
    sine = table[menu_angle];
    cosine = table[(menu_angle + 1024) & 4095];
    REG16(0x04000020) = cosine;
    REG16(0x04000022) = -sine;
    REG16(0x04000024) = sine;
    REG16(0x04000026) = cosine;
    REG32(0x04000028) = (retro_u32)(16384 - (retro_i32)cosine * 120 + (retro_i32)sine * 80);
    REG32(0x0400002c) = (retro_u32)(16384 - (retro_i32)sine * 120 - (retro_i32)cosine * 80);
    REG16(0x04000010) = hud_scroll_x;
    REG16(0x04000012) = hud_scroll_y;
    REG16(0x04000050) = (3 << 6) | (1 << 2);
    REG16(0x04000054) = 7;
    REG_DISPLAY = 1 | (1 << 6) | (1 << 8) | (1 << 10);
}
#endif

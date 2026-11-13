#if GBA_CAMPAIGN
static deceptus_u16 hud_map[1024] __attribute__((aligned(4)));

static void hud_text(deceptus_u16 row, deceptus_u16 column, const char *text)
{
    deceptus_u8 character;
    while (*text && column < 30 && row < 20) {
        character = (deceptus_u8)*text++;
        if (character >= 'a' && character <= 'z') {
            character -= 'a' - 'A';
        }
        if (character < 32 || character > 127) {
            character = 32;
        }
        hud_map[row * 32 + column++] = (deceptus_u16)(character - 31) | (15 << 12);
    }
}
static void hud_number(deceptus_u16 row, deceptus_u16 column, deceptus_u16 value, deceptus_u8 digits)
{
    char text[6];
    deceptus_u16 quotient;
    deceptus_u8 index;
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
#if GBA_TUTORIAL
static deceptus_u16 hud_ui_selected = 65535, hud_ui_loaded = 65535;
static deceptus_u16 hud_scroll_x, hud_scroll_y, hud_prompt_start;
static const deceptus_interaction *hud_prompt_action;
static deceptus_u8 hud_cue_pixels[64 * 32] __attribute__((aligned(4)));
static deceptus_u16 hud_cue_resource = 65535, hud_cue_x, hud_cue_y, hud_cue_bytes;
static deceptus_u8 hud_cue_dirty;
static deceptus_u8 hud_title_pixels[32 * 32] __attribute__((aligned(4)));
static deceptus_u16 hud_title_bytes;
static void hud_reset(void)
{
    hud_ui_loaded = 65535;
    hud_ui_selected = 65535;
    hud_prompt_action = 0;
}
static void hud_image(deceptus_u16 resource, deceptus_i16 x, deceptus_i16 y)
{
    const gba_ui_resource *image;
    const deceptus_u16 *cells;
    deceptus_u16 row, column;
    image = &gba_ui_resources[resource];
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if (x + image->columns * 8 > 240) {
        x = 240 - image->columns * 8;
    }
    if (y + image->rows * 8 > 160) {
        y = 160 - image->rows * 8;
    }
    hud_scroll_x = (256 - x) & 255;
    hud_scroll_y = (256 - y) & 255;
    hud_ui_selected = resource;
    cells = (const deceptus_u16 *)(gba_ui_blob + image->map);
    for (row = 0; row < image->rows; ++row) {
        for (column = 0; column < image->columns; ++column) {
            hud_map[row * 32 + column] = cells[row * image->columns + column] | (15 << 12);
        }
    }
}
static void hud_cue(deceptus_u16 resource, deceptus_i16 x, deceptus_i16 y)
{
    const gba_ui_resource *image;
    const deceptus_u16 *cells;
    deceptus_u16 row, column, px, py, shift_x, shift_y, columns, rows, source_tile, destination;
    deceptus_i16 label_x, label_y;
    deceptus_u8 packed, value;
    image = &gba_ui_resources[resource];
    cells = (const deceptus_u16 *)(gba_ui_blob + image->map);
    if (x < 0) {
        x = 0;
    }
    if (x > 192) {
        x = 192;
    }
    if (y < 0) {
        y = 0;
    }
    if (y > 112) {
        y = 112;
    }
    hud_ui_selected = resource;
    hud_scroll_x = (256 - (x & 7)) & 255;
    hud_scroll_y = (256 - (y & 7)) & 255;
    for (row = 0; row < 6; ++row) {
        for (column = 0; column < 6; ++column) {
            hud_map[((y >> 3) + row) * 32 + (x >> 3) + column] = cells[row * image->columns + column] | (15 << 12);
        }
    }
    label_x = 240 - image->columns * 8;
    label_y = 136;
    shift_x = (label_x - (x & 7)) & 7;
    shift_y = (label_y - (y & 7)) & 7;
    columns = image->columns + (shift_x ? 1 : 0);
    rows = 3 + (shift_y ? 1 : 0);
    hud_cue_bytes = columns * rows * 32;
    if (resource != hud_cue_resource || shift_x != hud_cue_x || shift_y != hud_cue_y) {
        for (destination = 0; destination < sizeof(hud_cue_pixels); ++destination) {
            hud_cue_pixels[destination] = 0;
        }
        for (row = 0; row < 24; ++row) {
            for (column = 0; column < image->columns * 8; ++column) {
                source_tile = cells[((48 + row) >> 3) * image->columns + (column >> 3)];
                if (source_tile >= 192) {
                    source_tile -= 64;
                }
                packed = gba_ui_blob[image->graphics + source_tile * 32 + ((48 + row) & 7) * 4 + ((column & 7) >> 1)];
                value = (packed >> ((column & 1) * 4)) & 15;
                px = column + shift_x;
                py = row + shift_y;
                destination = ((py >> 3) * columns + (px >> 3)) * 32 + (py & 7) * 4 + ((px & 7) >> 1);
                hud_cue_pixels[destination] |= value << ((px & 1) * 4);
            }
        }
        hud_cue_resource = resource;
        hud_cue_x = shift_x;
        hud_cue_y = shift_y;
        hud_cue_dirty = 1;
    }
    label_x = (label_x - (x & 7)) >> 3;
    label_y = (label_y - (y & 7)) >> 3;
    for (row = 0; row < rows; ++row) {
        for (column = 0; column < columns; ++column) {
            hud_map[(label_y + row) * 32 + label_x + column] = (192 + row * columns + column) | (15 << 12);
        }
    }
}
static void hud_room_title(void)
{
    const gba_ui_resource *image;
    const deceptus_u16 *palette;
    const char *text;
    deceptus_u16 index, pixel, colour, brightest, score, best;
    deceptus_u8 character, packed, low, high;
    if (!adventure.room_label_ticks) {
        return;
    }
    if (hud_ui_selected == 65535) {
        hud_text(2, 1, active_stage->title);
        return;
    }
    image = &gba_ui_resources[hud_ui_selected];
    if (image->patterns > 96) {
        return;
    }
    palette = (const deceptus_u16 *)(gba_ui_blob + image->palette);
    brightest = 1;
    best = 0;
    for (index = 1; index < 16; ++index) {
        colour = palette[index];
        score = (colour & 31) + ((colour >> 5) & 31) + ((colour >> 10) & 31);
        if (score > best) {
            best = score;
            brightest = index;
        }
    }
    text = active_stage->title;
    for (index = 0; index < 28 && text[index]; ++index) {
        character = (deceptus_u8)text[index];
        if (character >= 'a' && character <= 'z') {
            character -= 'a' - 'A';
        }
        if (character < 32 || character > 127) {
            character = 32;
        }
        for (pixel = 0; pixel < 32; ++pixel) {
            packed = gba_font[(character - 31) * 32 + pixel];
            low = (packed & 15) ? brightest : 0;
            high = (packed & 240) ? brightest : 0;
            hud_title_pixels[index * 32 + pixel] = low | (high << 4);
        }
        hud_map[2 * 32 + 1 + index] = (96 + index) | (15 << 12);
    }
    hud_title_bytes = index * 32;
}
static void hud_lines(deceptus_u16 row, const char *text)
{
    char line[29];
    deceptus_u8 length;
    while (*text && row < 19) {
        length = 0;
        while (*text && *text != '\n' && length < 28) {
            line[length++] = *text++;
        }
        line[length] = 0;
        hud_text(row++, 1, line);
        if (*text == '\n') {
            ++text;
        }
    }
}
static void hud_prepare(void)
{
    deceptus_u16 index;
    deceptus_i16 x, y, column;
    deceptus_u16 resource, elapsed;
    const deceptus_interaction *action;
    const deceptus_adventure_room *room;
    const gba_ui_presentation *presentation;
    for (index = 0; index < 1024; ++index) {
        hud_map[index] = 0;
    }
    hud_ui_selected = 65535;
    hud_scroll_x = 0;
    hud_scroll_y = 0;
    hud_cue_dirty = 0;
    hud_title_bytes = 0;
    if (campaign.mode != DECEPTUS_CAMPAIGN_PLAYING) {
        hud_cue_resource = 65535;
    }
    if (campaign.mode != DECEPTUS_CAMPAIGN_PLAYING) {
        hud_prompt_action = 0;
    }
    if (campaign.mode == DECEPTUS_CAMPAIGN_TITLE) {
        hud_image(gba_menu_main[adventure.selection], 0, 0);
        return;
    }
    if (campaign.mode == DECEPTUS_DIALOGUE && adventure.dialogue) {
        for (index = 0; index < GBA_UI_PRESENTATIONS; ++index) {
            presentation = &gba_ui_presentations[index];
            if (presentation->action == adventure.dialogue) {
                hud_image(presentation->page_first + adventure.page, 8, 8);
                return;
            }
        }
        hud_lines(14, adventure.dialogue->pages[adventure.page]);
        return;
    }
    if (campaign.mode == DECEPTUS_MENU_PAUSE) {
        hud_image(gba_menu_pause[adventure.selection], 0, 0);
        return;
    }
    if (campaign.mode == DECEPTUS_MENU_INVENTORY) {
        index = (adventure.inventory.counts[1] ? 1 : 0) | (adventure.inventory.counts[2] ? 2 : 0);
        hud_image(gba_menu_inventory[index], 0, 0);
        return;
    }
    if (campaign.mode == DECEPTUS_MENU_CONTROLS) {
        hud_image(gba_menu_controls[0], 0, 0);
        return;
    }
    if (campaign.mode == DECEPTUS_MENU_OPTIONS) {
        hud_image(gba_menu_options[adventure.selection], 0, 0);
        return;
    }
    if (campaign.mode == DECEPTUS_MENU_CONFIRM) {
        hud_image(gba_menu_confirm[adventure.selection], 0, 0);
        return;
    }
    if (campaign.mode == DECEPTUS_MENU_EXIT) {
        hud_image(gba_menu_exit[0], 0, 0);
        return;
    }
    if (engine.content_error) {
        hud_text(18, 2, "CONTENT LIMIT");
        hud_number(18, 18, engine.content_error, 2);
        return;
    }
    room = &adventure.definition->rooms[campaign.room_index];

    if (adventure.prompt < room->interaction_count) {
        action = &room->interactions[adventure.prompt];
        for (index = 0; index < GBA_UI_PRESENTATIONS; ++index) {
            presentation = &gba_ui_presentations[index];
            if (presentation->action != action) {
                continue;
            }
            if (hud_prompt_action != action) {
                hud_prompt_action = action;
                hud_prompt_start = engine.ticks;
            }
            elapsed = (deceptus_u16)(engine.ticks - hud_prompt_start);
            resource = elapsed >= 28 ? presentation->cue_count - 1 : elapsed * 5 / 3;
            if (resource >= presentation->cue_count) {
                resource = presentation->cue_count - 1;
            }
            resource += presentation->cue_first;
            hud_cue(resource, presentation->x - frame.camera_x - 24, presentation->y - frame.camera_y - 24);
            hud_room_title();
            return;
        }
        column = (deceptus_floor(engine.entities[0].x) - frame.camera_x) / 8 - 3;
        if (column < 1) {
            column = 1;
        }
        if (column > 20) {
            column = 20;
        }
        hud_text(7, column, action->grant_item ? "UP TAKE" : action->cost_item ? "UP USE" : "UP");
    } else {
        hud_prompt_action = 0;
        x = deceptus_floor(engine.entities[0].x);
        y = deceptus_floor(engine.entities[0].y);
        for (index = 0; index < engine.mechanisms.count; ++index) {
            if (engine.mechanisms.definitions[index].kind == DECEPTUS_MECH_LEVER &&
                x + 8 > engine.mechanisms.definitions[index].bounds.x &&
                x - 8 <
                    engine.mechanisms.definitions[index].bounds.x + engine.mechanisms.definitions[index].bounds.width &&
                y > engine.mechanisms.definitions[index].bounds.y &&
                y - 30 < engine.mechanisms.definitions[index].bounds.y +
                             engine.mechanisms.definitions[index].bounds.height) {
                hud_text(7, 13, "UP");
                break;
            }
        }
    }
    hud_room_title();
}
#else
static void hud_prepare(void)
{
    deceptus_u16 index, minutes;
    for (index = 0; index < 1024; ++index) {
        hud_map[index] = 0;
    }
    if (campaign.mode == DECEPTUS_CAMPAIGN_TITLE) {
        hud_text(5, 10, "DECEPTUS");
        hud_text(7, 5, "GBA PLAYABLE PREVIEW");
        hud_text(9, 6, "FOUR CATACOMB AREAS");
        hud_text(11, 4, "ARROWS MOVE. A/B JUMP");
        hud_text(12, 3, "UP INTERACT. START PAUSE");
        hud_text(13, 3, "SELECT RESTARTS THE ROOM");
        hud_text(16, 9, "PRESS START");
        return;
    }
    if (campaign.mode == DECEPTUS_CAMPAIGN_READY) {
        hud_text(5, 10, "AREA");
        hud_number(5, 15, campaign.room_index + 1, 1);
        hud_text(7, 3, active_stage->title);
        hud_text(10, 1, active_stage->hint1);
        hud_text(12, 1, active_stage->hint2);
        hud_text(16, 9, "PRESS START");
        return;
    }
    minutes = campaign.seconds / 60;
    if (campaign.mode == DECEPTUS_CAMPAIGN_FINISHED) {
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
    if (engine.paused) {
        hud_text(9, 4, "PAUSED - START TO RESUME");
    } else if (deceptus_campaign_at_goal(&campaign)) {
        hud_text(18, 7, "UP TO CONTINUE");
    }
    if (engine.content_error) {
        hud_text(18, 0, "CONTENT LIMIT ");
        hud_number(18, 15, engine.content_error, 2);
    }
}
#endif
static void hud_commit(void)
{
#if GBA_TUTORIAL
    const gba_ui_resource *image;
    deceptus_u16 count;
    if (hud_ui_selected != hud_ui_loaded) {
        if (hud_ui_selected == 65535) {
            dma_copy(gba_font, (void *)0x0600c000, GBA_FONT_BYTES);
            REG16(0x050001e2) = 0x7fff;
            REG16(0x050001e6) = 0x0821;
        } else {
            image = &gba_ui_resources[hud_ui_selected];
            count = image->patterns < 128 ? image->patterns : 128;
            dma_copy(gba_ui_blob + image->graphics, (void *)0x0600c000, count * 32);
            if (image->patterns > 128) {
                dma_copy(gba_ui_blob + image->graphics + 4096, (void *)0x0600d800, (image->patterns - 128) * 32);
            }
            dma_copy(gba_ui_blob + image->palette, (void *)0x050001e0, 32);
        }
        hud_ui_loaded = hud_ui_selected;
    }
    REG16(0x04000018) = hud_scroll_x;
    REG16(0x0400001a) = hud_scroll_y;
    if (hud_cue_dirty) {
        dma_copy(hud_cue_pixels, (void *)0x0600d800, hud_cue_bytes);
    }
    if (hud_title_bytes) {
        dma_copy(hud_title_pixels, (void *)0x0600cc00, hud_title_bytes);
    }
#endif
    dma_copy(hud_map, (void *)0x0600d000, sizeof(hud_map));
}
#else
static void hud_prepare(void) {}
static void hud_commit(void) {}
#endif

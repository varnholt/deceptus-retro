#include "deceptus/adventure.h"
#include "deceptus/enemies.h"
#include "deceptus/engine.h"
#include "gba_assets.h"

#define REG16(address) (*(volatile deceptus_u16 *)(address))
#define REG32(address) (*(volatile deceptus_u32 *)(address))
#define REG_DISPLAY REG16(0x04000000)
#define REG_SCANLINE REG16(0x04000006)
#define REG_KEYS REG16(0x04000130)
#define REG_DMA_SOURCE REG32(0x040000d4)
#define REG_DMA_DESTINATION REG32(0x040000d8)
#define REG_DMA_CONTROL REG32(0x040000dc)
#define GBA_OBJECT_PRIORITY (GBA_LAYERED_BACKGROUND ? 2 : (GBA_CAMPAIGN ? 1 : 0))

extern const deceptus_room deceptus_scene;

typedef struct gba_object {
    deceptus_u16 attribute0;
    deceptus_u16 attribute1;
    deceptus_u16 attribute2;
    deceptus_u16 affine;
} gba_object;

#if GBA_CAMPAIGN
#if GBA_TUTORIAL
static deceptus_adventure adventure;
#define campaign adventure.campaign
#define GBA_SCENERY_FLAGS adventure.flags
#else
static deceptus_campaign campaign;
#endif
#define engine campaign.engine
static const gba_stage *active_stage;
#else
static deceptus_engine engine;
#endif
#ifndef GBA_SCENERY_FLAGS
#define GBA_SCENERY_FLAGS 0
#endif
static deceptus_frame frame;
static deceptus_video_caps video = {240, 160, DECEPTUS_DRAW_CAPACITY};
static gba_object objects[128] __attribute__((aligned(4)));
static deceptus_u16 uploaded_frame[3];
static const deceptus_draw *actor_draws[3];
static deceptus_u16 actor_count;
static deceptus_u16 swim_palette[32] __attribute__((aligned(4)));
typedef struct gba_upload {
    deceptus_u16 resource;
    deceptus_u16 offset;
} gba_upload;
static gba_upload uploads[32];
static deceptus_u16 upload_count, upload_bytes;
volatile deceptus_u16 deceptus_debug[46];
#if !GBA_STREAMED_BACKGROUND
static deceptus_u16 map_chunk = 65535;
#endif

static void dma_copy(const void *source, void *destination, deceptus_u32 bytes)
{
    REG_DMA_CONTROL = 0;
    REG_DMA_SOURCE = (deceptus_u32)source;
    REG_DMA_DESTINATION = (deceptus_u32)destination;
    REG_DMA_CONTROL = (1UL << 31) | (1UL << 26) | (bytes >> 2);
    /* allow immediate DMA to start before a following transfer reconfigures it. */
    __asm__ volatile("nop\n nop\n nop\n nop" ::: "memory");
}

static void wait_vblank(void)
{
    while (REG_SCANLINE >= 160) {
    }
    while (REG_SCANLINE < 160) {
    }
}

static deceptus_u16 input_read(void)
{
    deceptus_u16 keys, buttons;
    keys = (deceptus_u16)~REG_KEYS;
    buttons = 0;
    if (keys & (1 << 5)) {
        buttons |= DECEPTUS_LEFT;
    }
    if (keys & (1 << 4)) {
        buttons |= DECEPTUS_RIGHT;
    }
    if (keys & 3) {
        buttons |= DECEPTUS_JUMP;
    }
    if (keys & 1) {
        buttons |= DECEPTUS_ACCEPT;
    }
    if (keys & 2) {
        buttons |= DECEPTUS_BACK;
    }
    if (keys & (1 << 7)) {
        buttons |= DECEPTUS_DOWN;
    }
    if (keys & (1 << 6)) {
        buttons |= DECEPTUS_UP;
    }
    if (keys & (1 << 3)) {
        buttons |= DECEPTUS_PAUSE;
    }
    if (keys & (1 << 2)) {
        buttons |= DECEPTUS_RESTART;
    }
    return buttons;
}

static void __attribute__((section(".romtext"))) video_init(void)
{
    deceptus_u16 index;
    {
        const gba_mechanism_resource *resource;
        const deceptus_u16 *palette;
        deceptus_u16 colour;
        resource = &((const gba_mechanism_resource *)gba_mechanism_table)[GBA_SWIM_FIRST];
        palette = (const deceptus_u16 *)(gba_mechanism_palettes + resource->palette_offset);
        for (index = 0; index < 32; ++index) {
            colour = palette[index];
            swim_palette[index] = ((colour & 31) * 3 / 4) | (((((colour >> 5) & 31) * 3 + 14) / 4) << 5) |
                                  (((((colour >> 10) & 31) * 3 + 18) / 4) << 10);
        }
    }
    REG_DISPLAY = 1 << 6;
    dma_copy(gba_background0, (void *)0x06000000, GBA_BACKGROUND0_BYTES);
    dma_copy(gba_background1, (void *)0x06008000, GBA_BACKGROUND1_BYTES);

#if GBA_CAMPAIGN
    dma_copy(active_stage->palette, (void *)0x05000000, 512);
    dma_copy(gba_font, (void *)0x0600c000, GBA_FONT_BYTES);
    REG16(0x050001e2) = 0x7fff;
    REG16(0x050001e6) = 0x0821;
    REG16(0x0400000c) = (3 << 2) | (26 << 8);
#else
    dma_copy(gba_background_palette, (void *)0x05000000, GBA_BACKGROUND_PALETTE_BYTES);
#endif
    dma_copy(gba_actor_palette, (void *)0x05000200, GBA_ACTOR_PALETTE_BYTES);
    /* two 512x256 4bpp planes, with independent character blocks. */
    REG16(0x04000008) = (GBA_LAYERED_BACKGROUND ? 3 : 1) | (28 << 8) | (1 << 14);
    REG16(0x0400000a) = (GBA_LAYERED_BACKGROUND ? 1 : 2) | (2 << 2) | (30 << 8) | (1 << 14);
    for (index = 0; index < 128; ++index) {
        objects[index].attribute0 = 1 << 9;
        objects[index].attribute1 = 0;
        objects[index].attribute2 = 0;
        objects[index].affine = 0;
    }
    for (index = 0; index < 3; ++index) {
        uploaded_frame[index] = 65535;
    }
    dma_copy(objects, (void *)0x07000000, sizeof(objects));
    REG16(0x04000010) = 0;
    REG16(0x04000012) = 0;
    REG16(0x04000014) = 0;
    REG16(0x04000016) = 0;
}

#include "background.h"
#include "hud.h"
#include "menu.h"

static deceptus_u16 mechanism_resource(deceptus_u16 resource)
{
    deceptus_u16 index, colour;
    const gba_mechanism_resource *table, *entry, *other;
    const deceptus_u16 *first, *second;
    table = (const gba_mechanism_resource *)gba_mechanism_table;
    if (resource >= GBA_MECHANISM_FRAME_COUNT) {
        engine.content_error = 1;
        return 65535;
    }
    entry = &table[resource];
    for (index = 0; index < upload_count; ++index) {
        if (uploads[index].resource == resource) {
            return uploads[index].offset >> 5;
        }
        other = &table[uploads[index].resource];
        if (other->palette_first < entry->palette_first + entry->palette_count &&
            entry->palette_first < other->palette_first + other->palette_count) {
            first = (const deceptus_u16 *)(gba_mechanism_palettes + entry->palette_offset);
            second = (const deceptus_u16 *)(gba_mechanism_palettes + other->palette_offset);
            for (colour = entry->palette_first; colour < entry->palette_first + entry->palette_count; ++colour) {
                if (colour >= other->palette_first && colour < other->palette_first + other->palette_count &&
                    first[colour - entry->palette_first] != second[colour - other->palette_first]) {
                    engine.content_error = 2;
                    return 65535;
                }
            }
        }
    }
    if (upload_count >= 32 || upload_bytes + entry->bytes > 32768) {
        engine.content_error = 3;
        return 65535;
    }
    uploads[upload_count].resource = resource;
    uploads[upload_count].offset = upload_bytes;
    ++upload_count;
    index = upload_bytes >> 5;
    upload_bytes += entry->bytes;
    return index;
}

static void mechanism_objects(deceptus_u16 *count)
{
    deceptus_u16 index, segment, segments, resource, piece, tile, piece_x, piece_y;
    deceptus_i16 x, y, origin_x, origin_y;
    const deceptus_mechanism_def *definition;
    const deceptus_mechanism_state *state;
    const gba_mechanism_resource *entry;
    gba_object *object;
    for (index = 0; index < engine.mechanisms.count; ++index) {
        definition = &engine.mechanisms.definitions[index];
        state = &engine.mechanisms.states[index];
        if (!state->visible) {
            continue;
        }
        segments = 1;
        if (definition->kind == DECEPTUS_MECH_LEVER) {
            resource = GBA_LEVER_FIRST + (!state->handle_available ? 32 : state->frame);
        } else if (definition->kind == DECEPTUS_MECH_BLOCK && engine.mechanisms.timings &&
                   (engine.mechanisms.timings[index].flags & DECEPTUS_TIMED_ANIMATION)) {
            resource = GBA_BLOCK_FIRST + state->frame;
        } else if (definition->kind == DECEPTUS_MECH_SPIKES) {
            resource = GBA_SPIKES_FIRST + state->frame;
        } else if (definition->kind == DECEPTUS_MECH_COLLAPSING) {
            if (state->frame >= 38 || state->phase == 2) {
                continue;
            }
            resource = GBA_COLLAPSE_FIRST + state->frame;
            segments = definition->bounds.width / 24;
        } else {
            continue;
        }
        for (segment = 0; segment < segments; ++segment) {
            entry = &((const gba_mechanism_resource *)
                          gba_mechanism_table)[resource +
                                               (definition->kind == DECEPTUS_MECH_COLLAPSING ? (segment % 4) * 38 : 0)];
            origin_x = definition->bounds.x - frame.camera_x + segment * 24;
            origin_y = definition->bounds.y - frame.camera_y;
            if (definition->kind == DECEPTUS_MECH_COLLAPSING) {
                origin_y -= 24;
                if (state->phase == 1) {
                    origin_y += state->timer / 10;
                } else if (state->timer) {
                    origin_y += (deceptus_i16)((state->timer / 2 + segment) & 3) - 1;
                }
            }
            tile = 65535;
            piece_x = 0;
            piece_y = 0;
            for (piece = 0; piece < entry->columns * entry->rows; ++piece) {
                x = origin_x + piece_x * 16;
                y = origin_y + piece_y * 16;
                if (++piece_x >= entry->columns) {
                    piece_x = 0;
                    ++piece_y;
                }
                if (x <= -16 || x >= 240 || y <= -16 || y >= 160) {
                    continue;
                }
                if (tile == 65535) {
                    tile = mechanism_resource(resource +
                                              (definition->kind == DECEPTUS_MECH_COLLAPSING ? (segment % 4) * 38 : 0));
                }
                if (tile == 65535 || *count >= 128) {
                    engine.content_error = engine.content_error ? engine.content_error : 4;
                    return;
                }
                object = &objects[(*count)++];
                object->attribute0 = ((deceptus_u16)y & 255) | (1 << 13);
                object->attribute1 = ((deceptus_u16)x & 511) | (1 << 14);
                object->attribute2 = tile + piece * 8 + (GBA_OBJECT_PRIORITY << 10);
            }
        }
    }
}

static void enemy_objects(deceptus_u16 *count, const deceptus_draw *draw)
{
    deceptus_u16 resource, tile, row, column, piece;
    deceptus_i16 x, y;
    const gba_mechanism_resource *entry;
    gba_object *object;
    if (draw->kind == DECEPTUS_WATER_SPLASH) {
        resource = GBA_SPLASH_FIRST + draw->frame;
    } else if (draw->kind == DECEPTUS_BLOB_2) {
        resource = GBA_BLOB_FIRST + draw->frame;
    } else if (draw->kind == DECEPTUS_PLAYER_SWIM) {
        resource = GBA_SWIM_FIRST + draw->frame;
    } else if (draw->kind == DECEPTUS_RAT) {
        resource = GBA_RAT_FIRST + draw->frame;
    } else if (draw->kind == DECEPTUS_ARROW_TRAP) {
        resource = GBA_TRAP_FIRST + draw->frame;
    } else if (draw->kind == DECEPTUS_ARROW_MUZZLE) {
        resource = GBA_MUZZLE_FIRST + draw->frame;
    } else if (draw->kind == DECEPTUS_ARROW) {
        resource = GBA_ARROW_FIRST + draw->frame;
    } else {
        engine.content_error = 6;
        return;
    }
    tile = mechanism_resource(resource);
    if (tile == 65535) {
        return;
    }
    entry = &((const gba_mechanism_resource *)gba_mechanism_table)[resource];
    piece = 0;
    for (row = 0; row < entry->rows; ++row) {
        for (column = 0; column < entry->columns; ++column, ++piece) {
            x = draw->x + (draw->flip_x ? entry->columns - 1 - column : column) * 16;
            y = draw->y + row * 16;
            if (x <= -16 || x >= 240 || y <= -16 || y >= 160) {
                continue;
            }
            if (*count >= 128) {
                engine.content_error = 4;
                return;
            }
            object = &objects[(*count)++];
            object->attribute0 = ((deceptus_u16)y & 255) | (1 << 13);
            object->attribute1 = ((deceptus_u16)x & 511) | (1 << 14) | (draw->flip_x ? 1 << 12 : 0);
            object->attribute2 = tile + piece * 8 + (GBA_OBJECT_PRIORITY << 10);
        }
    }
}

static void video_prepare(void)
{
    deceptus_u16 index, piece, count;
    deceptus_i16 x, y;
    const deceptus_draw *draw;
    gba_object *object;
    for (index = 0; index < 128; ++index) {
        objects[index].attribute0 = 1 << 9;
    }
    count = 0;
    upload_count = 0;
    upload_bytes = 3 * GBA_ACTOR_FRAME_BYTES;
    actor_count = 0;
#if GBA_CAMPAIGN
#if GBA_TUTORIAL
    if (menu_active) {
        return;
    }
#endif
    if (campaign.mode == DECEPTUS_CAMPAIGN_TITLE || campaign.mode == DECEPTUS_CAMPAIGN_FINISHED ||
        campaign.mode == DECEPTUS_CAMPAIGN_READY) {
        return;
    }
#endif
    for (index = 0; index < frame.count; ++index) {
        draw = &frame.draws[index];
        if (draw->kind >= DECEPTUS_RAT) {
            enemy_objects(&count, draw);
            continue;
        }
        if (actor_count >= 3) {
            engine.content_error = 4;
            break;
        }
        actor_draws[actor_count] = draw;
        if (draw->frame >= GBA_ACTOR_FRAME_COUNT || draw->palette > 1) {
            engine.content_error = 1;
            continue;
        }
        for (piece = 0; piece < 6; ++piece) {
            x = draw->x + (draw->flip_x ? 1 - piece % 2 : piece % 2) * 16;
            y = draw->y + (piece / 2) * 16;
            if (x <= -16 || x >= 240 || y <= -16 || y >= 160) {
                continue;
            }
            object = &objects[count++];
            object->attribute0 = ((deceptus_u16)y & 255) | (1 << 13);
            object->attribute1 = ((deceptus_u16)x & 511) | (1 << 14) | (draw->flip_x ? 1 << 12 : 0);
            object->attribute2 = actor_count * 48 + piece * 8 + (GBA_OBJECT_PRIORITY << 10);
        }
        ++actor_count;
    }
    mechanism_objects(&count);
}

static void maps_commit(void)
{
#if GBA_STREAMED_BACKGROUND
    background_commit();
#else
    deceptus_u16 chunk, next, chunks;
    chunk = frame.camera_x >> 8;
    if (chunk == map_chunk) {
        return;
    }
    chunks = GBA_MAP0_BYTES / 2048;
    next = chunk + 1;
    if (next >= chunks) {
        next = 0;
    }
    /* screen blocks alternate as the camera crosses each 256px boundary. */
    dma_copy(gba_map0 + chunk * 2048, (void *)(0x0600e000 + (chunk & 1) * 2048), 2048);
    dma_copy(gba_map1 + chunk * 2048, (void *)(0x0600f000 + (chunk & 1) * 2048), 2048);
    dma_copy(gba_map0 + next * 2048, (void *)(0x0600e000 + ((chunk + 1) & 1) * 2048), 2048);
    dma_copy(gba_map1 + next * 2048, (void *)(0x0600f000 + ((chunk + 1) & 1) * 2048), 2048);
    map_chunk = chunk;
#endif
}

static void video_commit(void)
{
    deceptus_u16 index, resource;
    const deceptus_draw *draw;
    for (index = 0; index < actor_count; ++index) {
        draw = actor_draws[index];
        if (draw->frame >= GBA_ACTOR_FRAME_COUNT || draw->palette > 1) {
            continue;
        }
        resource = draw->frame + (draw->palette ? GBA_ACTOR_FRAME_COUNT : 0);
        if (resource != uploaded_frame[index]) {
            dma_copy(gba_actor_frames + resource * GBA_ACTOR_FRAME_BYTES,
                     (void *)(0x06010000 + index * GBA_ACTOR_FRAME_BYTES), GBA_ACTOR_FRAME_BYTES);
            uploaded_frame[index] = resource;
        }
    }
    for (index = frame.count; index < 3; ++index) {
        uploaded_frame[index] = 65535;
    }
    for (index = 0; index < upload_count; ++index) {
        const gba_mechanism_resource *entry;
        entry = &((const gba_mechanism_resource *)gba_mechanism_table)[uploads[index].resource];
        dma_copy(gba_mechanism_frames + entry->offset, (void *)(0x06010000 + uploads[index].offset), entry->bytes);
        dma_copy(uploads[index].resource >= GBA_SWIM_FIRST && uploads[index].resource < GBA_SWIM_FIRST + 26
                     ? (const void *)swim_palette
                     : (const void *)(gba_mechanism_palettes + entry->palette_offset),
                 (void *)(0x05000200 + entry->palette_first * 2), entry->palette_count * 2);
    }
    maps_commit();
    hud_commit();
    dma_copy(objects, (void *)0x07000000, sizeof(objects));
    REG16(0x04000010) = frame.camera_x & 511;
    REG16(0x04000012) = frame.camera_y;
    REG16(0x04000014) = frame.camera_x & 511;
    REG16(0x04000016) = frame.camera_y;
#if GBA_CAMPAIGN
    REG16(0x04000050) =
        (engine.paused || (campaign.mode != DECEPTUS_CAMPAIGN_PLAYING && campaign.mode != DECEPTUS_NOTIFICATION))
            ? (3 << 6) | 0x13
            : 0;
    REG16(0x04000054) =
        (engine.paused || (campaign.mode != DECEPTUS_CAMPAIGN_PLAYING && campaign.mode != DECEPTUS_NOTIFICATION)) ? 8
                                                                                                                  : 0;
#else
    REG16(0x04000050) = engine.paused ? (3 << 6) | 0x13 : 0;
    REG16(0x04000054) = engine.paused ? 8 : 0;
#endif
    REG_DISPLAY = (1 << 6) | (1 << 8) | (1 << 9) | (1 << 12) | (GBA_CAMPAIGN ? 1 << 10 : 0);
#if GBA_TUTORIAL
    if (menu_active) {
        menu_commit();
    }
#endif
}

static void debug_write(void)
{
#if GBA_TUTORIAL
    deceptus_debug[36] = adventure.flags;
    deceptus_debug[37] = adventure.inventory.counts[1];
    deceptus_debug[38] = adventure.inventory.counts[2];
    deceptus_debug[39] = adventure.page;
    deceptus_debug[40] = hud_ui_selected;
    deceptus_debug[41] = stream_jobs;
    deceptus_debug[42] = engine.room->player_gravity;
    deceptus_debug[43] = engine.room->player_speed;
#endif
#if GBA_BACKGROUND_ANIMATION_CAPACITY
    deceptus_debug[44] = GBA_BACKGROUND_ANIMATION_COUNT > 1 ? background_clocks[1].frame : 65535;
    deceptus_debug[45] = GBA_BACKGROUND_ANIMATION_COUNT > 1 ? background_clocks[1].elapsed : 65535;
    deceptus_debug[34] = background_clocks[0].frame;
    deceptus_debug[35] = background_clocks[0].elapsed;
#endif
    deceptus_debug[0] = 0xd3ce;
    deceptus_debug[1] = deceptus_floor(engine.entities[0].x);
    deceptus_debug[2] = deceptus_floor(engine.entities[0].y);
    deceptus_debug[3] = engine.camera_x;
    deceptus_debug[4] = engine.ticks;
    deceptus_debug[5] = engine.paused;
    deceptus_debug[6] = engine.health;
    deceptus_debug[7] = engine.entities[0].grounded;
    deceptus_debug[8] = engine.door_entries;
    deceptus_debug[9] = engine.event_count;
    deceptus_debug[10] = frame.count;
    deceptus_debug[11] = engine.entities[1].active;
    deceptus_debug[12] = engine.checkpoint_id;
    deceptus_debug[13] = engine.checkpoint_x;
    deceptus_debug[14] = engine.checkpoint_y;
    deceptus_debug[15] = engine.content_error;
    deceptus_debug[16] = engine.camera_y;
    deceptus_debug[17] = REG_SCANLINE;
    deceptus_debug[18] = engine.mechanisms.count;
    {
        deceptus_u16 index, mask;
        mask = 0;
        deceptus_debug[23] = 65535;
        deceptus_debug[24] = 65535;
        deceptus_debug[25] = 65535;
        deceptus_debug[26] = 65535;
        deceptus_debug[27] = 65535;
        deceptus_debug[19] = 65535;
        for (index = 0; index < engine.mechanisms.count; ++index) {
            if (engine.mechanisms.definitions[index].kind == DECEPTUS_MECH_LEVER) {
                deceptus_debug[19] = engine.mechanisms.states[index].enabled;
                deceptus_debug[23] = engine.mechanisms.states[index].frame;
            }
            if (engine.mechanisms.definitions[index].kind == DECEPTUS_MECH_SPIKES) {
                deceptus_debug[24] = engine.mechanisms.states[index].frame;
            }
            if (engine.mechanisms.definitions[index].kind == DECEPTUS_MECH_COLLAPSING && deceptus_debug[25] == 65535) {
                deceptus_debug[25] = engine.mechanisms.states[index].phase;
                deceptus_debug[26] = engine.mechanisms.states[index].active;
                deceptus_debug[27] = engine.mechanisms.states[index].timer;
            }
            if (index < 16 && engine.mechanisms.states[index].enabled) {
                mask |= (deceptus_u16)(1U << index);
            }
        }
        deceptus_debug[20] = mask;
        deceptus_debug[21] = engine.mechanisms.overflow;
        deceptus_debug[22] = engine.hurt_ticks;
    }
    deceptus_debug[30] = engine.deaths;
    deceptus_debug[32] = engine.projectiles_fired;
#if GBA_CAMPAIGN
    deceptus_debug[28] = campaign.room_index;
    deceptus_debug[29] = campaign.mode;
    deceptus_debug[30] = campaign.deaths;
    deceptus_debug[31] = campaign.seconds;
    deceptus_debug[33] = campaign.restarts;
#endif
}

int __attribute__((section(".iwram"))) main(void)
{
#if GBA_CAMPAIGN
#if GBA_TUTORIAL
    deceptus_adventure_init(&adventure, &gba_tutorial_definition, &video);
#else
    deceptus_campaign_init(&campaign, &gba_campaign_definition, &video);
#endif
    active_stage = &gba_stages[0];
#else
    deceptus_init(&engine, &deceptus_scene, &video);
#endif
    video_init();
#if GBA_TUTORIAL
    hud_reset();
#endif
    background_reset();
    while (1) {
#if GBA_CAMPAIGN
#if GBA_TUTORIAL
        if (deceptus_adventure_tick(&adventure, input_read())) {
#else
        if (deceptus_campaign_tick(&campaign, input_read())) {
#endif
            active_stage = &gba_stages[campaign.room_index];
            video_init();
#if GBA_TUTORIAL
            hud_reset();
#endif
            background_reset();
        }
#else
        deceptus_tick(&engine, input_read());
#endif
        deceptus_render(&engine, &frame);
#if GBA_TUTORIAL
        menu_prepare();
        if (!menu_active) {
            background_prepare();
        }
#else
        background_prepare();
#endif
        video_prepare();
        hud_prepare();

        debug_write();
        wait_vblank();
        video_commit();
    }
    return 0;
}

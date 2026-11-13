#include "gba_assets.h"
#include "retro/engine.h"

#define REG16(address) (*(volatile retro_u16 *)(address))
#define REG32(address) (*(volatile retro_u32 *)(address))
#define REG_DISPLAY REG16(0x04000000)
#define REG_SCANLINE REG16(0x04000006)
#define REG_KEYS REG16(0x04000130)
#define REG_DMA_SOURCE REG32(0x040000d4)
#define REG_DMA_DESTINATION REG32(0x040000d8)
#define REG_DMA_CONTROL REG32(0x040000dc)

extern const retro_room deceptus_room;

typedef struct gba_object {
    retro_u16 attribute0;
    retro_u16 attribute1;
    retro_u16 attribute2;
    retro_u16 affine;
} gba_object;

static retro_engine engine;
static retro_frame frame;
static retro_video_caps video = {240, 160, RETRO_DRAW_CAPACITY};
static gba_object objects[128] __attribute__((aligned(4)));
static retro_u16 uploaded_frame[RETRO_DRAW_CAPACITY];
typedef struct gba_upload {
    retro_u16 resource;
    retro_u16 offset;
} gba_upload;
static gba_upload uploads[32];
static retro_u16 upload_count, upload_bytes;
volatile retro_u16 retro_debug[28];
static retro_u16 map_chunk = 65535;

static void dma_copy(const void *source, void *destination, retro_u32 bytes)
{
    REG_DMA_CONTROL = 0;
    REG_DMA_SOURCE = (retro_u32)source;
    REG_DMA_DESTINATION = (retro_u32)destination;
    REG_DMA_CONTROL = (1UL << 31) | (1UL << 26) | (bytes >> 2);
}

static void wait_vblank(void)
{
    while (REG_SCANLINE >= 160) {
    }
    while (REG_SCANLINE < 160) {
    }
}

static retro_u16 input_read(void)
{
    retro_u16 keys, buttons;
    keys = (retro_u16)~REG_KEYS;
    buttons = 0;
    if (keys & (1 << 5)) {
        buttons |= RETRO_LEFT;
    }
    if (keys & (1 << 4)) {
        buttons |= RETRO_RIGHT;
    }
    if (keys & 3) {
        buttons |= RETRO_JUMP;
    }
    if (keys & (1 << 6)) {
        buttons |= RETRO_UP;
    }
    if (keys & (1 << 3)) {
        buttons |= RETRO_PAUSE;
    }
    if (keys & (1 << 2)) {
        buttons |= RETRO_RESTART;
    }
    return buttons;
}

static void video_init(void)
{
    retro_u16 index;
    REG_DISPLAY = 1 << 7;
    dma_copy(gba_background0, (void *)0x06000000, GBA_BACKGROUND0_BYTES);
    dma_copy(gba_background1, (void *)0x06008000, GBA_BACKGROUND1_BYTES);

    dma_copy(gba_background_palette, (void *)0x05000000, GBA_BACKGROUND_PALETTE_BYTES);
    dma_copy(gba_actor_palette, (void *)0x05000200, GBA_ACTOR_PALETTE_BYTES);
    /* two 512x256 4bpp planes, with independent character blocks. */
    REG16(0x04000008) = 1 | (28 << 8) | (1 << 14);
    REG16(0x0400000a) = 2 | (2 << 2) | (30 << 8) | (1 << 14);
    for (index = 0; index < 128; ++index) {
        objects[index].attribute0 = 1 << 9;
        objects[index].attribute1 = 0;
        objects[index].attribute2 = 0;
        objects[index].affine = 0;
    }
    for (index = 0; index < RETRO_DRAW_CAPACITY; ++index) {
        uploaded_frame[index] = 65535;
    }
    dma_copy(objects, (void *)0x07000000, sizeof(objects));
    REG16(0x04000010) = 0;
    REG16(0x04000012) = 0;
    REG16(0x04000014) = 0;
    REG16(0x04000016) = 0;
    REG_DISPLAY = (1 << 6) | (1 << 8) | (1 << 9) | (1 << 12);
}

static retro_u16 mechanism_resource(retro_u16 resource)
{
    retro_u16 index, colour;
    const gba_mechanism_resource *table, *entry, *other;
    const retro_u16 *first, *second;
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
        if (other->palette_first == entry->palette_first) {
            first = (const retro_u16 *)(gba_mechanism_palettes + entry->palette_offset);
            second = (const retro_u16 *)(gba_mechanism_palettes + other->palette_offset);
            for (colour = 0; colour < entry->palette_count; ++colour) {
                if (first[colour] != second[colour]) {
                    /* distinct animated palettes need another hardware palette allocation. */
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

static void mechanism_objects(retro_u16 *count)
{
    retro_u16 index, segment, segments, resource, piece, tile, piece_x, piece_y;
    retro_i16 x, y, origin_x, origin_y;
    const retro_mechanism_def *definition;
    const retro_mechanism_state *state;
    const gba_mechanism_resource *entry;
    gba_object *object;
    for (index = 0; index < engine.mechanisms.count; ++index) {
        definition = &engine.mechanisms.definitions[index];
        state = &engine.mechanisms.states[index];
        if (!state->visible) {
            continue;
        }
        segments = 1;
        if (definition->kind == RETRO_MECH_LEVER) {
            resource = GBA_LEVER_FIRST + ((definition->flags & RETRO_MECH_MISSING_HANDLE) ? 32 : state->frame);
        } else if (definition->kind == RETRO_MECH_BLOCK && engine.mechanisms.timings &&
                   (engine.mechanisms.timings[index].flags & RETRO_TIMED_ANIMATION)) {
            resource = GBA_BLOCK_FIRST + state->frame;
        } else if (definition->kind == RETRO_MECH_SPIKES) {
            resource = GBA_SPIKES_FIRST + state->frame;
        } else if (definition->kind == RETRO_MECH_COLLAPSING) {
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
                                               (definition->kind == RETRO_MECH_COLLAPSING ? (segment % 4) * 38 : 0)];
            origin_x = definition->bounds.x - frame.camera_x + segment * 24;
            origin_y = definition->bounds.y - frame.camera_y;
            if (definition->kind == RETRO_MECH_COLLAPSING) {
                origin_y -= 24;
                if (state->phase == 1) {
                    origin_y += state->timer / 10;
                } else if (state->timer) {
                    origin_y += (retro_i16)((state->timer / 2 + segment) & 3) - 1;
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
                                              (definition->kind == RETRO_MECH_COLLAPSING ? (segment % 4) * 38 : 0));
                }
                if (tile == 65535 || *count >= 128) {
                    engine.content_error = engine.content_error ? engine.content_error : 4;
                    return;
                }
                object = &objects[(*count)++];
                object->attribute0 = ((retro_u16)y & 255) | (1 << 13);
                object->attribute1 = ((retro_u16)x & 511) | (1 << 14);
                object->attribute2 = tile + piece * 8;
            }
        }
    }
}

static void video_prepare(void)
{
    retro_u16 index, piece, count;
    retro_i16 x, y;
    const retro_draw *draw;
    gba_object *object;
    for (index = 0; index < 128; ++index) {
        objects[index].attribute0 = 1 << 9;
    }
    count = 0;
    upload_count = 0;
    upload_bytes = frame.count * GBA_ACTOR_FRAME_BYTES;
    for (index = 0; index < frame.count; ++index) {
        draw = &frame.draws[index];
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
            object->attribute0 = ((retro_u16)y & 255) | (1 << 13);
            object->attribute1 = ((retro_u16)x & 511) | (1 << 14) | (draw->flip_x ? 1 << 12 : 0);
            object->attribute2 = index * 48 + piece * 8;
        }
    }
    mechanism_objects(&count);
}

static void maps_commit(void)
{
    retro_u16 chunk, next, chunks;
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
}

static void video_commit(void)
{
    retro_u16 index, resource;
    const retro_draw *draw;
    for (index = 0; index < frame.count; ++index) {
        draw = &frame.draws[index];
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
    for (index = frame.count; index < RETRO_DRAW_CAPACITY; ++index) {
        uploaded_frame[index] = 65535;
    }
    for (index = 0; index < upload_count; ++index) {
        const gba_mechanism_resource *entry;
        entry = &((const gba_mechanism_resource *)gba_mechanism_table)[uploads[index].resource];
        dma_copy(gba_mechanism_frames + entry->offset, (void *)(0x06010000 + uploads[index].offset), entry->bytes);
        dma_copy(gba_mechanism_palettes + entry->palette_offset, (void *)(0x05000200 + entry->palette_first * 2),
                 entry->palette_count * 2);
    }
    maps_commit();
    dma_copy(objects, (void *)0x07000000, sizeof(objects));
    REG16(0x04000010) = frame.camera_x & 511;
    REG16(0x04000012) = frame.camera_y;
    REG16(0x04000014) = frame.camera_x & 511;
    REG16(0x04000016) = frame.camera_y;
    REG16(0x04000050) = engine.paused ? (3 << 6) | 0x13 : 0;
    REG16(0x04000054) = engine.paused ? 8 : 0;
}

static void debug_write(void)
{
    retro_debug[0] = 0xd3ce;
    retro_debug[1] = retro_floor(engine.entities[0].x);
    retro_debug[2] = retro_floor(engine.entities[0].y);
    retro_debug[3] = engine.camera_x;
    retro_debug[4] = engine.ticks;
    retro_debug[5] = engine.paused;
    retro_debug[6] = engine.health;
    retro_debug[7] = engine.entities[0].grounded;
    retro_debug[8] = engine.door_entries;
    retro_debug[9] = engine.event_count;
    retro_debug[10] = frame.count;
    retro_debug[11] = engine.entities[1].active;
    retro_debug[12] = engine.checkpoint_id;
    retro_debug[13] = engine.checkpoint_x;
    retro_debug[14] = engine.checkpoint_y;
    retro_debug[15] = engine.content_error;
    retro_debug[16] = engine.camera_y;
    retro_debug[17] = REG_SCANLINE;
    retro_debug[18] = engine.mechanisms.count;
    {
        retro_u16 index, mask;
        mask = 0;
        retro_debug[23] = 65535;
        retro_debug[24] = 65535;
        retro_debug[25] = 65535;
        retro_debug[26] = 65535;
        retro_debug[27] = 65535;
        retro_debug[19] = 65535;
        for (index = 0; index < engine.mechanisms.count; ++index) {
            if (engine.mechanisms.definitions[index].kind == RETRO_MECH_LEVER) {
                retro_debug[19] = engine.mechanisms.states[index].enabled;
                retro_debug[23] = engine.mechanisms.states[index].frame;
            }
            if (engine.mechanisms.definitions[index].kind == RETRO_MECH_SPIKES) {
                retro_debug[24] = engine.mechanisms.states[index].frame;
            }
            if (engine.mechanisms.definitions[index].kind == RETRO_MECH_COLLAPSING && retro_debug[25] == 65535) {
                retro_debug[25] = engine.mechanisms.states[index].phase;
                retro_debug[26] = engine.mechanisms.states[index].active;
                retro_debug[27] = engine.mechanisms.states[index].timer;
            }
            if (index < 16 && engine.mechanisms.states[index].enabled) {
                mask |= (retro_u16)(1U << index);
            }
        }
        retro_debug[20] = mask;
        retro_debug[21] = engine.mechanisms.overflow;
        retro_debug[22] = engine.hurt_ticks;
    }
}

int main(void)
{
    retro_init(&engine, &deceptus_room, &video);
    video_init();
    while (1) {
        retro_tick(&engine, input_read());
        retro_render(&engine, &frame);
        video_prepare();

        debug_write();
        wait_vblank();
        video_commit();
    }
    return 0;
}

/* visible-tile cache for room-local backgrounds larger than VRAM. */
#if GBA_STREAMED_BACKGROUND
#define GBA_CACHE_SLOTS 1535
#define GBA_STREAM_JOBS 704
static retro_u16 pattern_slots[GBA_PATTERN_COUNT];
static retro_u16 slot_patterns[GBA_CACHE_SLOTS];
static retro_u8 used_slots[GBA_CACHE_SLOTS];
static retro_u16 stream_maps[2][2048] __attribute__((aligned(4)));
static retro_u16 stream_patterns[GBA_STREAM_JOBS], stream_slots[GBA_STREAM_JOBS];
static retro_u16 stream_jobs, stream_cursor = 1;
static retro_i16 stream_x = -1, stream_y = -1;
static retro_u8 stream_changed;
#if GBA_BACKGROUND_ANIMATION_COUNT
#include "retro/animation.h"
static retro_animation background_clocks[GBA_BACKGROUND_ANIMATION_COUNT];
static retro_u16 background_previous_tick;
#endif

static void background_reset(void)
{
    retro_u16 index;
    for (index = 0; index < GBA_PATTERN_COUNT; ++index) {
        pattern_slots[index] = 0;
    }
    for (index = 0; index < GBA_CACHE_SLOTS; ++index) {
        slot_patterns[index] = 0;
        used_slots[index] = 0;
    }
    for (index = 0; index < 2048; ++index) {
        stream_maps[0][index] = 0;
        stream_maps[1][index] = 0;
    }
    stream_cursor = 1;
    stream_x = -1;
    stream_y = -1;
#if GBA_BACKGROUND_ANIMATION_COUNT
    for (index = 0; index < GBA_BACKGROUND_ANIMATION_COUNT; ++index) {
        retro_animation_reset(&background_clocks[index]);
    }
    background_previous_tick = engine.ticks;
#endif
}

static const retro_u16 *background_cell(retro_u16 column, retro_u16 row)
{
#if GBA_BACKGROUND_ANIMATION_COUNT
    retro_u16 index;
    retro_u32 address;
    const gba_background_animation *clip;
    for (index = 0; index < GBA_BACKGROUND_ANIMATION_COUNT; ++index) {
        clip = &gba_background_animations[index];
        if (column >= clip->left && column < clip->left + clip->columns && row >= clip->top &&
            row < clip->top + clip->rows) {
            address = ((retro_u32)background_clocks[index].frame * clip->rows + row - clip->top) * clip->columns +
                      column - clip->left;
            return (const retro_u16 *)clip->cells + address * 2;
        }
    }
#endif
    if (column >= GBA_ROOM_COLUMNS || row >= GBA_ROOM_ROWS) {
        return 0;
    }
    return (const retro_u16 *)gba_stream_cells + ((retro_u32)row * GBA_ROOM_COLUMNS + column) * 2;
}

static void __attribute__((section(".iwram"))) background_prepare(void)
{
    retro_u16 index, column, row, pattern, slot, old, address, attributes;
    retro_u16 left, top;
    const retro_u16 *cell;
    left = frame.camera_x >> 3;
    top = frame.camera_y >> 3;
    stream_changed = 0;
#if GBA_BACKGROUND_ANIMATION_COUNT
    {
        retro_u16 ticks;
        const gba_background_animation *clip;
        ticks = (retro_u16)(engine.ticks - background_previous_tick);
        /* restart resets the engine clock, rather than advancing it by 65535 ticks. */
        if (ticks > 1) {
            for (index = 0; index < GBA_BACKGROUND_ANIMATION_COUNT; ++index) {
                retro_animation_reset(&background_clocks[index]);
            }
            stream_x = -1;
            ticks = 0;
        }
        background_previous_tick = engine.ticks;
        for (index = 0; index < GBA_BACKGROUND_ANIMATION_COUNT; ++index) {
            clip = &gba_background_animations[index];
            if (retro_animation_advance(&background_clocks[index], clip->frames, clip->frame_ticks, ticks, 1) &&
                left < clip->left + clip->columns && left + 32 > clip->left && top < clip->top + clip->rows &&
                top + 22 > clip->top) {
                stream_x = -1;
            }
        }
    }
#endif
    if (stream_x == left && stream_y == top) {
        return;
    }
    stream_x = left;
    stream_y = top;
    stream_jobs = 0;
    for (index = 0; index < GBA_CACHE_SLOTS; ++index) {
        used_slots[index] = 0;
    }
    for (row = top; row < top + 22; ++row) {
        for (column = left; column < left + 32; ++column) {
            cell = background_cell(column, row);
            if (cell && pattern_slots[cell[0]]) {
                used_slots[pattern_slots[cell[0]]] = 1;
            }
        }
    }
    for (row = top; row < top + 22; ++row) {
        for (column = left; column < left + 32; ++column) {
            cell = background_cell(column, row);
            pattern = cell ? cell[0] : 0;
            attributes = cell ? cell[1] : 0;
            slot = pattern_slots[pattern];
            if (pattern && !slot) {
                while (used_slots[stream_cursor]) {
                    if (++stream_cursor >= GBA_CACHE_SLOTS) {
                        stream_cursor = 1;
                    }
                }
                slot = stream_cursor;
                old = slot_patterns[slot];
                if (old) {
                    pattern_slots[old] = 0;
                }
                slot_patterns[slot] = pattern;
                pattern_slots[pattern] = slot;
                used_slots[slot] = 1;
                if (stream_jobs >= GBA_STREAM_JOBS) {
                    engine.content_error = 5;
                    return;
                }
                stream_patterns[stream_jobs] = pattern;
                stream_slots[stream_jobs++] = slot;
                if (++stream_cursor >= GBA_CACHE_SLOTS) {
                    stream_cursor = 1;
                }
            }
            address = ((column & 63) >> 5) * 1024 + (row & 31) * 32 + (column & 31);
            stream_maps[0][address] = slot < 1024 ? attributes | slot : 0;
            stream_maps[1][address] = slot >= 1024 ? attributes | (slot - 1023) : 0;
        }
    }
    stream_changed = 1;
}

static void background_commit(void)
{
    retro_u16 index, slot;
    retro_u32 destination;
    if (!stream_changed) {
        return;
    }
    for (index = 0; index < stream_jobs; ++index) {
        slot = stream_slots[index];
        destination = slot < 1024 ? 0x06000000UL + slot * 32UL : 0x06008000UL + (slot - 1023) * 32UL;
        dma_copy(gba_stream_patterns + (retro_u32)stream_patterns[index] * 32, (void *)destination, 32);
    }
    dma_copy(stream_maps[0], (void *)0x0600e000, sizeof(stream_maps[0]));
    dma_copy(stream_maps[1], (void *)0x0600f000, sizeof(stream_maps[1]));
}
#else
static void background_reset(void) {}
static void __attribute__((section(".iwram"))) background_prepare(void) {}
#endif

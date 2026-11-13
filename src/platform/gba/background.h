/* visible-tile cache for room-local backgrounds larger than VRAM. */
#if GBA_STREAMED_BACKGROUND
#define GBA_CACHE_SLOTS 1535
#define GBA_VISIBLE_CELLS 704
#define GBA_STREAM_JOBS (GBA_LAYERED_BACKGROUND ? 1408 : 704)
static retro_u16 pattern_slots[GBA_PATTERN_COUNT];
static retro_u16 slot_patterns[GBA_CACHE_SLOTS];
static retro_u16 used_slots[GBA_CACHE_SLOTS];
static retro_u16 stream_maps[2][2048] __attribute__((aligned(4)));
static retro_u16 stream_patterns[GBA_STREAM_JOBS], stream_slots[GBA_STREAM_JOBS];
#if !GBA_LAYERED_BACKGROUND
static const retro_u16 *stream_cells[GBA_VISIBLE_CELLS];
#endif
#if GBA_LAYERED_BACKGROUND
static retro_u16 foreground_cursor = 1024;
static retro_u8 background_dirty[4];
#endif
static retro_u16 stream_jobs, stream_cursor = 1;
static retro_i16 stream_x = -1, stream_y = -1;
static retro_u8 stream_changed;
#if GBA_BACKGROUND_ANIMATION_CAPACITY
#include "retro/animation.h"
static retro_animation background_clocks[GBA_BACKGROUND_ANIMATION_CAPACITY];
static retro_u16 background_previous_tick, background_previous_flags;
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
#if GBA_LAYERED_BACKGROUND
    foreground_cursor = 1024;
#endif
    stream_x = -1;
    stream_y = -1;
#if GBA_BACKGROUND_ANIMATION_CAPACITY
    for (index = 0; index < GBA_BACKGROUND_ANIMATION_COUNT; ++index) {
        retro_animation_reset(&background_clocks[index]);
    }
    background_previous_tick = engine.ticks;
    background_previous_flags = GBA_SCENERY_FLAGS;
#endif
}

static const retro_u16 *background_cell(retro_u16 column, retro_u16 row)
{
#if GBA_BACKGROUND_ANIMATION_CAPACITY
    retro_u16 index;
    retro_u32 address;
    const gba_background_animation *clip;
    for (index = 0; index < GBA_BACKGROUND_ANIMATION_COUNT; ++index) {
        clip = &gba_background_animations[index];
        if ((GBA_SCENERY_FLAGS & clip->required_flags) != clip->required_flags ||
            (GBA_SCENERY_FLAGS & clip->forbidden_flags)) {
            continue;
        }
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

static retro_u16 background_resident(retro_u16 pattern, retro_u8 plane)
{
    retro_u16 slot, old, first, limit, *cursor, attempts;
    if (!pattern) {
        return 0;
    }
    slot = pattern_slots[pattern];
    if (slot) {
        return slot;
    }
    first = 1;
    limit = GBA_LAYERED_BACKGROUND ? 1024 : GBA_CACHE_SLOTS;
    cursor = &stream_cursor;
#if GBA_LAYERED_BACKGROUND
    if (plane) {
        first = 1024;
        limit = GBA_CACHE_SLOTS;
        cursor = &foreground_cursor;
    }
#else
    (void)plane;
#endif
    attempts = 0;
    while (used_slots[*cursor]) {
        if (++*cursor >= limit) {
            *cursor = first;
        }
        if (++attempts >= limit - first) {
            engine.content_error = 5;
            return 0;
        }
    }
    slot = *cursor;
    old = slot_patterns[slot];
    if (old) {
        pattern_slots[old] = 0;
    }
    slot_patterns[slot] = pattern;
    pattern_slots[pattern] = slot;
    used_slots[slot] = 1;
    if (stream_jobs >= GBA_STREAM_JOBS) {
        engine.content_error = 5;
        return 0;
    }
    stream_patterns[stream_jobs] = pattern;
    stream_slots[stream_jobs++] = slot;
    if (++*cursor >= limit) {
        *cursor = first;
    }
    return slot;
}

#if GBA_LAYERED_BACKGROUND
static void background_remove(retro_u16 column, retro_u16 row, retro_u8 plane)
{
    retro_u16 address, slot;
    address = ((column & 63) >> 5) * 1024 + (row & 31) * 32 + (column & 31);
    slot = stream_maps[plane][address] & 1023;
    if (!slot) {
        return;
    }
    if (plane) {
        slot += 1023;
    }
    if (used_slots[slot]) {
        --used_slots[slot];
    }
}
static void background_add(retro_u16 column, retro_u16 row, retro_u8 plane)
{
    const retro_u16 *cell;
    retro_u16 pattern, slot, attributes, address;
    if (plane) {
        cell = column < GBA_ROOM_COLUMNS && row < GBA_ROOM_ROWS
                   ? (const retro_u16 *)gba_foreground_cells + ((retro_u32)row * GBA_ROOM_COLUMNS + column) * 2
                   : 0;
    } else {
        cell = background_cell(column, row);
    }
    pattern = cell ? cell[0] : 0;
    attributes = cell ? cell[1] : 0;
    slot = pattern ? pattern_slots[pattern] : 0;
    if (slot) {
        ++used_slots[slot];
    } else {
        slot = background_resident(pattern, plane);
    }
    address = ((column & 63) >> 5) * 1024 + (row & 31) * 32 + (column & 31);
    stream_maps[plane][address] = attributes | (slot && plane ? slot - 1023 : slot);
}
static void background_edge(retro_u16 column, retro_u16 row, retro_u8 add)
{
    if (add) {
        background_add(column, row, 0);
        background_add(column, row, 1);
    } else {
        background_remove(column, row, 0);
        background_remove(column, row, 1);
    }
}
static void background_scroll(retro_u16 left, retro_u16 top)
{
    retro_u16 index, column, row, first, last, edge;
    retro_i16 dx, dy;
    const gba_background_animation *clip;
    dx = (retro_i16)left - stream_x;
    dy = (retro_i16)top - stream_y;
    stream_jobs = 0;
    if (stream_x < 0 || dx < -1 || dx > 1 || dy < -1 || dy > 1) {
        for (index = 0; index < GBA_CACHE_SLOTS; ++index) {
            used_slots[index] = 0;
        }
        for (row = top; row < top + 22; ++row) {
            for (column = left; column < left + 32; ++column) {
                background_edge(column, row, 1);
            }
        }
    } else {
        if (dx) {
            edge = dx > 0 ? stream_x : stream_x + 31;
            for (row = stream_y; row < stream_y + 22; ++row) {
                background_edge(edge, row, 0);
            }
        }
        first = left > stream_x ? left : stream_x;
        last = left < stream_x ? left + 32 : stream_x + 32;
        if (dy) {
            edge = dy > 0 ? stream_y : stream_y + 21;
            for (column = first; column < last; ++column) {
                background_edge(column, edge, 0);
            }
        }
        if (dx) {
            edge = dx > 0 ? left + 31 : left;
            for (row = top; row < top + 22; ++row) {
                background_edge(edge, row, 1);
            }
        }
        if (dy) {
            edge = dy > 0 ? top + 21 : top;
            for (column = first; column < last; ++column) {
                background_edge(column, edge, 1);
            }
        }
    }
    for (index = 0; index < GBA_BACKGROUND_ANIMATION_COUNT; ++index) {
        if (!background_dirty[index]) {
            continue;
        }
        background_dirty[index] = 0;
        clip = &gba_background_animations[index];
        for (row = top; row < top + 22; ++row) {
            if (row < clip->top || row >= clip->top + clip->rows) {
                continue;
            }
            for (column = left; column < left + 32; ++column) {
                if (column >= clip->left && column < clip->left + clip->columns) {
                    background_remove(column, row, 0);
                }
            }
        }
        for (row = top; row < top + 22; ++row) {
            if (row < clip->top || row >= clip->top + clip->rows) {
                continue;
            }
            for (column = left; column < left + 32; ++column) {
                if (column >= clip->left && column < clip->left + clip->columns) {
                    background_add(column, row, 0);
                }
            }
        }
    }
    stream_x = left;
    stream_y = top;
    stream_changed = 1;
}
#endif

static void __attribute__((section(".iwram"))) background_prepare(void)
{
    retro_u16 index;
    retro_u16 left, top;
#if !GBA_LAYERED_BACKGROUND
    retro_u16 column, row, pattern, slot, address, attributes;
    retro_u8 plane;
    const retro_u16 *cell;
#endif
    left = frame.camera_x >> 3;
    top = frame.camera_y >> 3;
    stream_changed = 0;
#if GBA_BACKGROUND_ANIMATION_CAPACITY
    if (background_previous_flags != GBA_SCENERY_FLAGS) {
#if GBA_LAYERED_BACKGROUND
        for (index = 0; index < GBA_BACKGROUND_ANIMATION_COUNT; ++index) {
            if ((background_previous_flags ^ GBA_SCENERY_FLAGS) &
                (gba_background_animations[index].required_flags | gba_background_animations[index].forbidden_flags)) {
                background_dirty[index] = 1;
            }
        }
#else
        stream_x = -1;
#endif
        background_previous_flags = GBA_SCENERY_FLAGS;
    }
#endif
#if GBA_BACKGROUND_ANIMATION_CAPACITY
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
            if ((clip->durations
                     ? retro_animation_advance_timed(&background_clocks[index], clip->frames, clip->durations, ticks, 1)
                     : retro_animation_advance(&background_clocks[index], clip->frames, clip->frame_ticks, ticks, 1)) &&
                left < clip->left + clip->columns && left + 32 > clip->left && top < clip->top + clip->rows &&
                top + 22 > clip->top) {
#if GBA_LAYERED_BACKGROUND
                background_dirty[index] = 1;
#else
                stream_x = -1;
#endif
            }
        }
    }
#endif
    if (stream_x == left && stream_y == top) {
#if GBA_LAYERED_BACKGROUND
        for (index = 0; index < GBA_BACKGROUND_ANIMATION_COUNT; ++index) {
            if (background_dirty[index]) {
                break;
            }
        }
        if (index == GBA_BACKGROUND_ANIMATION_COUNT) {
            return;
        }
#else
        return;
#endif
    }
#if GBA_LAYERED_BACKGROUND
    background_scroll(left, top);
#else
    stream_x = left;
    stream_y = top;
    stream_jobs = 0;
    for (index = 0; index < GBA_CACHE_SLOTS; ++index) {
        used_slots[index] = 0;
    }
    index = 0;
    for (row = top; row < top + 22; ++row) {
        for (column = left; column < left + 32; ++column) {
            cell = background_cell(column, row);
            stream_cells[index++] = cell;
            if (cell && pattern_slots[cell[0]]) {
                used_slots[pattern_slots[cell[0]]] = 1;
            }
#if GBA_LAYERED_BACKGROUND
            cell = column < GBA_ROOM_COLUMNS && row < GBA_ROOM_ROWS
                       ? (const retro_u16 *)gba_foreground_cells + ((retro_u32)row * GBA_ROOM_COLUMNS + column) * 2
                       : 0;
            foreground_cells[index - 1] = cell;
            if (cell && pattern_slots[cell[0]]) {
                used_slots[pattern_slots[cell[0]]] = 1;
            }
#endif
        }
    }
    for (plane = 0; plane < (GBA_LAYERED_BACKGROUND ? 2 : 1); ++plane) {
        index = 0;
        for (row = top; row < top + 22; ++row) {
            for (column = left; column < left + 32; ++column) {
                cell = stream_cells[index++];
#if GBA_LAYERED_BACKGROUND
                if (plane) {
                    cell = foreground_cells[index - 1];
                }
#endif
                pattern = cell ? cell[0] : 0;
                attributes = cell ? cell[1] : 0;
                slot = background_resident(pattern, plane);
                if (engine.content_error) {
                    return;
                }
                address = ((column & 63) >> 5) * 1024 + (row & 31) * 32 + (column & 31);
#if GBA_LAYERED_BACKGROUND
                stream_maps[plane][address] = attributes | (slot && plane ? slot - 1023 : slot);
#else
                stream_maps[0][address] = slot < 1024 ? attributes | slot : 0;
                stream_maps[1][address] = slot >= 1024 ? attributes | (slot - 1023) : 0;
#endif
            }
        }
    }
    stream_changed = 1;
#endif
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

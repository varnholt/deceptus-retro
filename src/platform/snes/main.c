#include "asset_sizes.h"
#include "retro/engine.h"
#include <snes.h>

extern char room_patterns, room_patterns_end;
extern char overflow_patterns, overflow_patterns_end;
extern char room_palette, room_palette_end;
extern char room_map, room_map_end;
extern char overflow_map, overflow_map_end;
extern char actor_patterns, actor_patterns_end;
extern char actor_palette, actor_palette_end;
extern const retro_room deceptus_room;

static retro_engine engine;
static retro_frame frame;
static retro_video_caps video = {256, 224, RETRO_DRAW_CAPACITY};
static retro_i16 accepted_top[RETRO_DRAW_CAPACITY];
static retro_i16 accepted_bottom[RETRO_DRAW_CAPACITY];

/* a stable little-endian debug mailbox for emulator integration tests. */
volatile retro_u16 retro_debug[16];

static retro_u16 input_read(void)
{
    retro_u16 pad, buttons;
    pad = padsCurrent(0);
    buttons = 0;
    if (pad & KEY_LEFT) {
        buttons |= RETRO_LEFT;
    }
    if (pad & KEY_RIGHT) {
        buttons |= RETRO_RIGHT;
    }
    if (pad & (KEY_B | KEY_A)) {
        buttons |= RETRO_JUMP;
    }
    if (pad & KEY_UP) {
        buttons |= RETRO_UP;
    }
    if (pad & KEY_START) {
        buttons |= RETRO_PAUSE;
    }
    if (pad & KEY_SELECT) {
        buttons |= RETRO_RESTART;
    }
    return buttons;
}

static void video_init(void)
{
    setScreenOff();
    bgInitTileSet(0, (u8 *)&room_patterns, (u8 *)&room_palette, 0, ROOM_PATTERN_BYTES, ROOM_PALETTE_BYTES, BG_16COLORS,
                  0x1000);
    bgInitMapSet(0, (u8 *)&room_map, 4096, SC_64x32, 0x0000);
    bgInitTileSet(1, (u8 *)&overflow_patterns, (u8 *)&room_palette, 0, OVERFLOW_PATTERN_BYTES, ROOM_PALETTE_BYTES,
                  BG_16COLORS, 0x5000);
    bgInitMapSet(1, (u8 *)&overflow_map, 4096, SC_64x32, 0x0800);
    oamInitGfxSet((u8 *)&actor_patterns, ACTOR_PATTERN_BYTES, (u8 *)&actor_palette, 64, 0, 0x6000, OBJ_SIZE16_L32);
    setMode(BG_MODE1, 0);
    bgSetDisable(2);
    bgSetScroll(0, 0, 0);
    bgSetScroll(1, 0, 0);
    oamClear(0, 0);
    setScreenOn();
}

static void video_prepare(void)
{
    retro_u16 index, other, accepted, overlaps, object_id, tile_base, piece;
    retro_i16 x, y;
    const retro_draw *draw;
    oamClear(0, 0);
    object_id = 0;
    accepted = 0;
    for (index = 0; index < frame.count; ++index) {
        draw = &frame.draws[index];
        overlaps = 0;
        for (other = 0; other < accepted; ++other) {
            if (draw->y < accepted_bottom[other] && draw->y + 48 > accepted_top[other]) {
                ++overlaps;
            }
        }
        /* conservative interval budget: at most 8 actors overlap, hence <=16
         * objects and <=32 eight-pixel slivers on any scanline. */
        if (overlaps >= 8 || object_id > 128 * 4 - 6 * 4) {
            continue;
        }
        accepted_top[accepted] = draw->y;
        accepted_bottom[accepted] = draw->y + 48;
        ++accepted;
        tile_base = (draw->frame % 4) * 4 + (draw->frame / 4) * 96;
        for (piece = 0; piece < 6; ++piece) {
            x = draw->x + (draw->flip_x ? 1 - piece % 2 : piece % 2) * 16;
            y = draw->y + (piece / 2) * 16;
            /* skip vertically clipped pieces to prevent 8-bit OAM y wrapping. */
            if (y <= -16 || y >= 224 || x <= -16 || x >= 256) {
                continue;
            }
            oamSet(object_id, x, y, 3, draw->flip_x, 0, tile_base + (piece / 2) * 32 + (piece % 2) * 2, draw->palette);
            oamSetEx(object_id, OBJ_SMALL, OBJ_SHOW);
            object_id += 4;
        }
    }
}

static void debug_write(void)
{
    retro_debug[0] = 0xD3CE;
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
}

int main(void)
{
    retro_u16 previous_vblank, current_vblank, elapsed, buttons;
    retro_init(&engine, &deceptus_room, &video);
    video_init();
    previous_vblank = snes_vblank_count - 1;
    while (1) {
        current_vblank = snes_vblank_count;
        elapsed = current_vblank - previous_vblank;
        previous_vblank = current_vblank;
        /* keep simulation at display time when a render misses its deadline.
         * cap catch-up so overload cannot create an unbounded update loop. */
        if (elapsed > 4) {
            elapsed = 4;
        }
        buttons = input_read();
        while (elapsed) {
            retro_tick(&engine, buttons);
            --elapsed;
        }
        retro_render(&engine, &frame);
        video_prepare();
        debug_write();
        WaitForVBlank();
        /* only register writes occur here; all OAM work is buffered above. */
        bgSetScroll(0, frame.camera_x, 0);
        bgSetScroll(1, frame.camera_x, 0);
        setBrightness(engine.paused ? 7 : 15);
    }
    return 0;
}

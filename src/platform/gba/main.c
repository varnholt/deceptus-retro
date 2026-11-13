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
volatile retro_u16 retro_debug[18];

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
    dma_copy(gba_map0, (void *)0x0600e000, GBA_MAP0_BYTES);
    dma_copy(gba_map1, (void *)0x0600f000, GBA_MAP1_BYTES);
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
    dma_copy(objects, (void *)0x07000000, sizeof(objects));
    REG16(0x04000010) = frame.camera_x;
    REG16(0x04000012) = frame.camera_y;
    REG16(0x04000014) = frame.camera_x;
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

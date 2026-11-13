#include "retro/enemies.h"
#include "retro/physics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef RETRO_HAVE_LUA_REFERENCE
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
static unsigned reference_tick, reference_count;
static unsigned reference_shots[64];
static int noop(lua_State *state)
{
    (void)state;
    return 0;
}
static int record_shot(lua_State *state)
{
    (void)state;
    if (reference_count < 64) {
        reference_shots[reference_count++] = reference_tick;
    }
    return 0;
}
#endif
#define CHECK(value)                                                                                                   \
    do {                                                                                                               \
        if (!(value)) {                                                                                                \
            fprintf(stderr, "line %d: %s\n", __LINE__, #value);                                                        \
            exit(1);                                                                                                   \
        }                                                                                                              \
    } while (0)
static retro_u8 cells[32 * 28];
static retro_engine engine;
static retro_room room;
static const retro_video_caps video = {240, 160, RETRO_DRAW_CAPACITY};
static const retro_enemy_def trap = {
    1059, 96, 0, 0, 0, 0, 96, 0, 60, 180, {60, 75, 90}, RETRO_ARROW_TRAP, 2, {0, 0, 0, 0, 0}, 20, 0};
static const retro_enemy_def rat = {111, 200, 178, 0, 2, 0, 10, 1, 0, 0, {0, 0, 0}, RETRO_RAT, 1, {7, 7, 5, 7, 3},
                                    20,  60};
static const retro_path_point path[] = {{200, 178}, {240, 178}};
static void reset(const retro_enemy_def *enemy)
{
    retro_u16 column;
    memset(&room, 0, sizeof(room));
    memset(cells, 0, sizeof(cells));
    for (column = 0; column < 32; ++column) {
        cells[24 * 32 + column] = RETRO_SOLID;
    }
    room.collision = cells;
    room.columns = 32;
    room.rows = 28;
    room.spawn_x = 40;
    room.spawn_y = 192;
    room.patrol_x = -1;
    room.door_x = -1;
    room.max_health = 100;
    room.enemy_definitions = enemy;
    room.enemy_count = 1;
    room.enemy_paths = path;
    room.enemy_path_count = 2;
    retro_init(&engine, &room, &video);
    CHECK(!engine.content_error);
}
int main(void)
{
    retro_u16 tick, previous, count, states, index;
    retro_i16 minimum, maximum;
    unsigned shots[64];
    retro_frame frame;
#ifdef RETRO_HAVE_LUA_REFERENCE
    lua_State *reference;
    const char *stubs[] = {"addSprite", "addShapeRect",    "addHitbox",        "addAudioRange",    "addSample",
                           "addWeapon", "setSpriteOffset", "updateSpriteRect", "updateProperties", "playSample"};
    reference = luaL_newstate();
    CHECK(reference);
    luaL_openlibs(reference);
    lua_getglobal(reference, "package");
    lua_pushstring(reference, RETRO_SOURCE_ROOT "/?.lua");
    lua_setfield(reference, -2, "path");
    lua_pop(reference, 1);
    for (index = 0; index < sizeof(stubs) / sizeof(stubs[0]); ++index) {
        lua_register(reference, stubs[index], noop);
    }
    lua_register(reference, "useWeapon", record_shot);
    if (luaL_dofile(reference, RETRO_SOURCE_ROOT "/data/scripts/enemies/arrowtrap_2.lua") != LUA_OK) {
        fprintf(stderr, "%s\n", lua_tostring(reference, -1));
        return 1;
    }
    lua_getglobal(reference, "initialize");
    CHECK(lua_pcall(reference, 0, 0, 0) == LUA_OK);
#endif
    reset(&trap);
    count = 0;
    for (tick = 1; tick <= 720; ++tick) {
        previous = engine.projectiles_fired;
        retro_tick(&engine, 0);
        while (previous++ < engine.projectiles_fired) {
            CHECK(count < 64);
            shots[count++] = tick;
        }
#ifdef RETRO_HAVE_LUA_REFERENCE
        reference_tick = tick;
        lua_getglobal(reference, "update");
        lua_pushnumber(reference, 1.0 / 60.0);
        CHECK(lua_pcall(reference, 1, 0, 0) == LUA_OK);
#endif
        CHECK(!engine.content_error);
    }
    CHECK(count == 12 && shots[0] == 72 && shots[1] == 87 && shots[2] == 102);
#ifdef RETRO_HAVE_LUA_REFERENCE
    CHECK(reference_count == count);
    for (index = 0; index < count; ++index) {
        CHECK(shots[index] + 1 >= reference_shots[index] && shots[index] <= reference_shots[index] + 1);
    }
    lua_close(reference);
    puts("arrow firing schedule agrees with execution of original Lua");
#else
    (void)index;
#endif
    reset(&trap);
    engine.entities[0].x = RETRO_FIXED(100);
    engine.checkpoint_x = 100;
    for (tick = 0; tick < 150; ++tick) {
        retro_tick(&engine, 0);
    }
    CHECK(engine.health == 40 && engine.deaths == 0);
    for (tick = 0; tick < 160; ++tick) {
        retro_tick(&engine, 0);
    }
    CHECK(engine.deaths == 1 && engine.health == 100 && retro_floor(engine.entities[0].x) == 100);
    reset(&rat);
    minimum = 240;
    maximum = 0;
    states = 0;
    for (tick = 0; tick < 1800; ++tick) {
        retro_tick(&engine, 0);
        index = (retro_u16)retro_floor(engine.entities[1].x);
        if (index < minimum) {
            minimum = index;
        }
        if (index > maximum) {
            maximum = index;
        }
        states |= (retro_u16)(1U << engine.entities[1].state);
        CHECK(engine.entities[1].frame < 70 && engine.health == 100 && !engine.content_error);
    }
    CHECK(minimum <= 201 && maximum >= 239 && (states & 1) && (states & (1 << 4)));
    retro_render(&engine, &frame);
    CHECK(frame.count >= 1);
    room.enemy_count = 9;
    retro_init(&engine, &room, &video);
    CHECK(engine.content_error == 6);
    reset(&rat);
    room.skills = RETRO_SKILL_WALL_JUMP;
    room.enemy_count = 0;
    room.spawn_x = 112;
    for (tick = 0; tick < 24; ++tick) {
        cells[tick * 32 + 15] = RETRO_SOLID;
    }
    retro_init(&engine, &room, &video);
    retro_tick(&engine, 0);
    for (tick = 0; tick < 20; ++tick) {
        retro_tick(&engine, RETRO_RIGHT | RETRO_JUMP);
    }
    retro_tick(&engine, RETRO_RIGHT);
    previous = (retro_u16)retro_floor(engine.entities[0].y);
    retro_tick(&engine, RETRO_RIGHT | RETRO_JUMP);
    CHECK(engine.entities[0].velocity_x < 0 && engine.entities[0].velocity_y < 0 &&
          retro_floor(engine.entities[0].y) < previous);
    puts("native rat patrol, trap projectiles, damage, respawn and wall jump checks passed");
    return 0;
}

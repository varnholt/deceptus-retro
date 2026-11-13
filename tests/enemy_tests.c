#include "deceptus/enemies.h"
#include "deceptus/physics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef DECEPTUS_HAVE_LUA_REFERENCE
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
static unsigned reference_blob_frame;
static int record_blob_sprite(lua_State *state)
{
    static const unsigned first[6] = {0, 15, 31, 0, 0, 47};
    unsigned row, column;
    column = (unsigned)lua_tointeger(state, 2) / 72;
    row = (unsigned)lua_tointeger(state, 3) / 72;
    if (row < 6) {
        reference_blob_frame = first[row] + column;
    }
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
static deceptus_u8 cells[32 * 28];
static deceptus_engine engine;
static deceptus_room room;
static const deceptus_video_caps video = {240, 160, DECEPTUS_DRAW_CAPACITY};
static const deceptus_enemy_def trap = {
    1059, 96, 0, 0, 0, 0, 96, 0, 60, 180, {60, 75, 90}, DECEPTUS_ARROW_TRAP, 2, {0, 0, 0, 0, 0}, 20, 0};
static const deceptus_enemy_def rat = {111, 200, 178, 0, 2, 0, 10, 1, 0, 0, {0, 0, 0}, DECEPTUS_RAT, 1, {7, 7, 5, 7, 3},
                                       20,  60};
static const deceptus_enemy_def blob = {
    1048, 200, 180, 0, 2, 0, 5, 1, 4, 0, {0, 0, 0}, DECEPTUS_BLOB_2, 1, {15, 16, 16, 14, 3}, 20, 0};
static const deceptus_path_point path[] = {{200, 178}, {240, 178}};
static void reset(const deceptus_enemy_def *enemy)
{
    deceptus_u16 column;
    memset(&room, 0, sizeof(room));
    memset(cells, 0, sizeof(cells));
    for (column = 0; column < 32; ++column) {
        cells[24 * 32 + column] = DECEPTUS_SOLID;
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
    deceptus_init(&engine, &room, &video);
    CHECK(!engine.content_error);
}
int main(void)
{
    deceptus_u16 tick, previous, count, states, index;
    deceptus_i16 minimum, maximum;
    unsigned shots[64];
    deceptus_frame frame;
#ifdef DECEPTUS_HAVE_LUA_REFERENCE
    lua_State *reference;
    const char *stubs[] = {"addSprite", "addShapeRect",    "addHitbox",        "addAudioRange",    "addSample",
                           "addWeapon", "setSpriteOffset", "updateSpriteRect", "updateProperties", "playSample"};
    reference = luaL_newstate();
    CHECK(reference);
    luaL_openlibs(reference);
    lua_getglobal(reference, "package");
    lua_pushstring(reference, DECEPTUS_SOURCE_ROOT "/?.lua");
    lua_setfield(reference, -2, "path");
    lua_pop(reference, 1);
    for (index = 0; index < sizeof(stubs) / sizeof(stubs[0]); ++index) {
        lua_register(reference, stubs[index], noop);
    }
    lua_register(reference, "useWeapon", record_shot);
    if (luaL_dofile(reference, DECEPTUS_SOURCE_ROOT "/data/scripts/enemies/arrowtrap_2.lua") != LUA_OK) {
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
        deceptus_tick(&engine, 0);
        while (previous++ < engine.projectiles_fired) {
            CHECK(count < 64);
            shots[count++] = tick;
        }
#ifdef DECEPTUS_HAVE_LUA_REFERENCE
        reference_tick = tick;
        lua_getglobal(reference, "update");
        lua_pushnumber(reference, 1.0 / 60.0);
        CHECK(lua_pcall(reference, 1, 0, 0) == LUA_OK);
#endif
        CHECK(!engine.content_error);
    }
    CHECK(count == 12 && shots[0] == 72 && shots[1] == 87 && shots[2] == 102);
#ifdef DECEPTUS_HAVE_LUA_REFERENCE
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
    engine.entities[0].x = DECEPTUS_FIXED(100);
    engine.checkpoint_x = 100;
    for (tick = 0; tick < 150; ++tick) {
        deceptus_tick(&engine, 0);
    }
    CHECK(engine.health == 40 && engine.deaths == 0);
    for (tick = 0; tick < 160; ++tick) {
        deceptus_tick(&engine, 0);
    }
    CHECK(engine.deaths == 1 && engine.health == 100 && deceptus_floor(engine.entities[0].x) == 100);
    reset(&rat);
    minimum = 240;
    maximum = 0;
    states = 0;
    for (tick = 0; tick < 1800; ++tick) {
        deceptus_tick(&engine, 0);
        index = (deceptus_u16)deceptus_floor(engine.entities[1].x);
        if (index < minimum) {
            minimum = index;
        }
        if (index > maximum) {
            maximum = index;
        }
        states |= (deceptus_u16)(1U << engine.entities[1].state);
        CHECK(engine.entities[1].frame < 70 && engine.health == 100 && !engine.content_error);
    }
    CHECK(minimum <= 201 && maximum >= 239 && (states & 1) && (states & (1 << 4)));
    deceptus_render(&engine, &frame);
    CHECK(frame.count >= 1);
    room.enemy_count = 9;
    deceptus_init(&engine, &room, &video);
    CHECK(engine.content_error == 6);
    reset(&blob);
#ifdef DECEPTUS_HAVE_LUA_REFERENCE
    reference = luaL_newstate();
    CHECK(reference);
    luaL_openlibs(reference);
    lua_getglobal(reference, "package");
    lua_pushstring(reference, DECEPTUS_SOURCE_ROOT "/?.lua");
    lua_setfield(reference, -2, "path");
    lua_pop(reference, 1);
    for (index = 0; index < sizeof(stubs) / sizeof(stubs[0]); ++index) {
        lua_register(reference, stubs[index], noop);
    }
    lua_register(reference, "addShapeCircle", noop);
    lua_register(reference, "setReferenceVolume", noop);
    lua_register(reference, "setAudioUpdateBehavior", noop);
    lua_register(reference, "updateKeysPressed", noop);
    lua_register(reference, "setDamage", noop);
    lua_register(reference, "die", noop);
    lua_register(reference, "updateSpriteRect", record_blob_sprite);
    CHECK(luaL_dofile(reference, DECEPTUS_SOURCE_ROOT "/data/scripts/enemies/blob_2.lua") == LUA_OK);
    lua_getglobal(reference, "initialize");
    CHECK(lua_pcall(reference, 0, 0, 0) == LUA_OK);
    CHECK(luaL_dostring(reference, "setPath('path',{200,180,240,180})") == LUA_OK);
#endif
    minimum = 32767;
    maximum = 0;
    states = 0;
    for (tick = 0; tick < 1800; ++tick) {
#ifdef DECEPTUS_HAVE_LUA_REFERENCE
        lua_getglobal(reference, "movedTo");
        lua_pushnumber(reference, deceptus_floor(engine.entities[1].x));
        lua_pushnumber(reference, deceptus_floor(engine.entities[1].y) - 12);
        CHECK(lua_pcall(reference, 2, 0, 0) == LUA_OK);
        lua_getglobal(reference, "update");
        lua_pushnumber(reference, 1.0 / 60.0 + 1e-10);
        CHECK(lua_pcall(reference, 1, 0, 0) == LUA_OK);
#endif
        deceptus_tick(&engine, 0);
#ifdef DECEPTUS_HAVE_LUA_REFERENCE
        if (engine.entities[1].frame != reference_blob_frame) {
            fprintf(stderr, "blob frame mismatch tick %u: C %u Lua %u\n", tick, engine.entities[1].frame,
                    reference_blob_frame);
            exit(1);
        }
#endif
        index = (deceptus_u16)deceptus_floor(engine.entities[1].x);
        if (index < minimum) {
            minimum = (deceptus_i16)index;
        }
        if (index > maximum) {
            maximum = (deceptus_i16)index;
        }
        states |= (deceptus_u16)(1U << engine.entities[1].state);
        CHECK(engine.entities[1].frame < 61 && !engine.content_error);
    }
    CHECK(minimum <= 201 && maximum >= 239 && (states & 1) && (states & 2));
#ifdef DECEPTUS_HAVE_LUA_REFERENCE
    lua_close(reference);
    puts("blob ground-patrol sprite transitions agree with original Lua under identical position inputs");
#endif
    deceptus_place_player(&engine, deceptus_floor(engine.entities[1].x), deceptus_floor(engine.entities[1].y) - 15);
    engine.entities[0].velocity_y = 16;
    deceptus_tick(&engine, 0);
    CHECK(engine.entities[1].state == 3 && engine.entities[0].velocity_y < 0);
    for (tick = 0; tick < 43; ++tick) {
        deceptus_tick(&engine, 0);
    }
    CHECK(!engine.entities[1].active);
    reset(&rat);
    room.skills = DECEPTUS_SKILL_WALL_JUMP;
    room.enemy_count = 0;
    room.spawn_x = 112;
    for (tick = 0; tick < 24; ++tick) {
        cells[tick * 32 + 15] = DECEPTUS_SOLID;
    }
    deceptus_init(&engine, &room, &video);
    deceptus_tick(&engine, 0);
    for (tick = 0; tick < 20; ++tick) {
        deceptus_tick(&engine, DECEPTUS_RIGHT | DECEPTUS_JUMP);
    }
    deceptus_tick(&engine, DECEPTUS_RIGHT);
    previous = (deceptus_u16)deceptus_floor(engine.entities[0].y);
    deceptus_tick(&engine, DECEPTUS_RIGHT | DECEPTUS_JUMP);
    CHECK(engine.entities[0].velocity_x < 0 && engine.entities[0].velocity_y < 0 &&
          deceptus_floor(engine.entities[0].y) < previous);
    puts("native rat patrol, trap projectiles, damage, respawn and wall jump checks passed");
    return 0;
}

#include "retro/logic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef RETRO_HAVE_LUA_REFERENCE
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
extern const retro_u16 source_logic_definition_count;
extern const char *const source_logic_names[];
extern const char *const source_logic_groups[];
static retro_u8 reference_enabled[RETRO_MECHANISM_CAPACITY];
static retro_u8 reference_seen[RETRO_MECHANISM_CAPACITY];
static retro_u16 reference_writes;

static int reference_set_enabled(lua_State *state)
{
    const char *name;
    const char *group;
    retro_u16 index;
    name = luaL_checkstring(state, 1);
    group = luaL_checkstring(state, 3);
    for (index = 0; index < source_logic_definition_count; ++index) {
        if (strcmp(name, source_logic_names[index]) == 0 && strcmp(group, source_logic_groups[index]) == 0) {
            reference_enabled[index] = lua_toboolean(state, 2) != 0;
            reference_seen[index] = 1;
            ++reference_writes;
            return 0;
        }
    }
    return luaL_error(state, "unexpected source mechanism selector: %s / %s", name, group);
}
#endif

extern const retro_mechanism_def source_logic_definitions[];
extern const retro_u16 source_logic_definition_count;
extern const retro_u8 source_logic_program[];
extern const retro_u16 source_logic_program_length;

#define CHECK(condition)                                                                                               \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            fprintf(stderr, "failure at line %d\n", __LINE__);                                                         \
            exit(1);                                                                                                   \
        }                                                                                                              \
    } while (0)

int main(void)
{
    retro_mechanism_set mechanisms;
    retro_u16 index;
#ifdef RETRO_HAVE_LUA_REFERENCE
    lua_State *reference;
    int released;
    reference = luaL_newstate();
    CHECK(reference != 0);
    luaL_openlibs(reference);
    lua_getglobal(reference, "package");
    lua_pushstring(reference, RETRO_SOURCE_ROOT "/?.lua");
    lua_setfield(reference, -2, "path");
    lua_pop(reference, 1);
    lua_register(reference, "setMechanismEnabled", reference_set_enabled);
    if (luaL_dofile(reference, RETRO_SOURCE_ROOT "/data/level-catacombs/level.lua") != LUA_OK) {
        fprintf(stderr, "source Lua error: %s\n", lua_tostring(reference, -1));
        return 1;
    }
#endif
    CHECK(retro_mechanisms_init(&mechanisms, source_logic_definitions, source_logic_definition_count, 0, 0));
    CHECK(source_logic_definition_count > 0 && source_logic_definition_count <= RETRO_MECHANISM_CAPACITY);
    CHECK(retro_logic_execute(&mechanisms, source_logic_program, source_logic_program_length, 1) == RETRO_LOGIC_OK);
    for (index = 0; index < mechanisms.count; ++index) {
        CHECK(!mechanisms.states[index].enabled);
    }
    CHECK(retro_logic_execute(&mechanisms, source_logic_program, source_logic_program_length, 0) == RETRO_LOGIC_OK);
    for (index = 0; index < mechanisms.count; ++index) {
        CHECK(mechanisms.states[index].enabled);
    }
#ifdef RETRO_HAVE_LUA_REFERENCE
    for (released = 0; released < 2; ++released) {
        reference_writes = 0;
        lua_getglobal(reference, "releaseSwordRing");
        lua_pushboolean(reference, released);
        if (lua_pcall(reference, 1, 0, 0) != LUA_OK) {
            fprintf(stderr, "source callback error: %s\n", lua_tostring(reference, -1));
            return 1;
        }
        CHECK(reference_writes >= source_logic_definition_count);
        CHECK(retro_logic_execute(&mechanisms, source_logic_program, source_logic_program_length, (retro_u8)released) ==
              RETRO_LOGIC_OK);
        for (index = 0; index < mechanisms.count; ++index) {
            CHECK(reference_seen[index]);
            CHECK(mechanisms.states[index].enabled == reference_enabled[index]);
        }
    }
    lua_close(reference);
    puts("compiled callback matches execution of the original full Lua source for both arguments");
#endif
    puts("original releaseSwordRing Lua callback controls its original Tiled targets");
    return 0;
}

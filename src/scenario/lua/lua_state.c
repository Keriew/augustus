#include "scenario/lua/lua_state.h"

#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"

#include "core/buffer.h"
#include "core/dir.h"
#include "core/file.h"
#include "core/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations for API registration
void lua_api_game_register(lua_State *L);
void lua_api_city_register(lua_State *L);
void lua_api_finance_register(lua_State *L);
void lua_api_scenario_register(lua_State *L);
void lua_api_map_register(lua_State *L);
void lua_api_ui_register(lua_State *L);
void lua_api_building_register(lua_State *L);
void lua_api_resource_register(lua_State *L);
void lua_api_population_register(lua_State *L);
void lua_api_sound_register(lua_State *L);
void lua_api_empire_register(lua_State *L);
// Forward declaration for clearing custom handlers
void lua_api_scenario_clear_handlers(void);

static lua_State *global_lua_state = 0;

// Stored script source for savegame embedding
#define MAX_LUA_SCRIPT_SIZE 65536
static char lua_script_source[MAX_LUA_SCRIPT_SIZE];
static int lua_script_source_len = 0;

static void register_all_apis(lua_State *L)
{
    lua_api_game_register(L);
    lua_api_city_register(L);
    lua_api_finance_register(L);
    lua_api_scenario_register(L);
    lua_api_map_register(L);
    lua_api_ui_register(L);
    lua_api_building_register(L);
    lua_api_resource_register(L);
    lua_api_population_register(L);
    lua_api_sound_register(L);
    lua_api_empire_register(L);
}

static int create_lua_state(void)
{
    global_lua_state = luaL_newstate();
    if (!global_lua_state) {
        log_error("Failed to create Lua state", 0, 0);
        return 0;
    }

    // Open safe standard libraries (no io, os, debug for security)
    luaL_requiref(global_lua_state, "_G", luaopen_base, 1);
    lua_pop(global_lua_state, 1);
    luaL_requiref(global_lua_state, "table", luaopen_table, 1);
    lua_pop(global_lua_state, 1);
    luaL_requiref(global_lua_state, "string", luaopen_string, 1);
    lua_pop(global_lua_state, 1);
    luaL_requiref(global_lua_state, "math", luaopen_math, 1);
    lua_pop(global_lua_state, 1);
    luaL_requiref(global_lua_state, "utf8", luaopen_utf8, 1);
    lua_pop(global_lua_state, 1);
    luaL_requiref(global_lua_state, "coroutine", luaopen_coroutine, 1);
    lua_pop(global_lua_state, 1);

    // Remove dangerous base functions
    lua_pushnil(global_lua_state);
    lua_setglobal(global_lua_state, "dofile");
    lua_pushnil(global_lua_state);
    lua_setglobal(global_lua_state, "loadfile");

    register_all_apis(global_lua_state);
    return 1;
}

static int execute_lua_source(const char *source, int length, const char *name)
{
    if (luaL_loadbuffer(global_lua_state, source, length, name) != LUA_OK) {
        log_error("Lua load error:", lua_tostring(global_lua_state, -1), 0);
        lua_pop(global_lua_state, 1);
        scenario_lua_shutdown();
        return 0;
    }

    if (lua_pcall(global_lua_state, 0, 0, 0) != LUA_OK) {
        log_error("Lua exec error:", lua_tostring(global_lua_state, -1), 0);
        lua_pop(global_lua_state, 1);
        scenario_lua_shutdown();
        return 0;
    }
    return 1;
}

int scenario_lua_load_script(const char *scenario_file)
{
    scenario_lua_shutdown();
    lua_script_source_len = 0;

    if (!scenario_file || !*scenario_file) {
        return 0;
    }

    // Derive Lua script name from scenario filename: "map_name.map" -> "map_name.lua"
    char lua_filename[FILE_NAME_MAX];
    strncpy(lua_filename, scenario_file, FILE_NAME_MAX - 1);
    lua_filename[FILE_NAME_MAX - 1] = '\0';
    file_remove_extension(lua_filename);
    file_append_extension(lua_filename, "lua", FILE_NAME_MAX);

    const char *full_path = dir_get_file_at_location(lua_filename, PATH_LOCATION_SCENARIO);
    if (!full_path) {
        // No .lua file for this scenario - that's normal, not an error
        log_info("No Lua script found for scenario:", lua_filename, 0);
        return 0;
    }
    log_info("Lua script found:", full_path, 0);

    // Read script source into memory for savegame embedding
    FILE *fp = fopen(full_path, "rb");
    if (!fp) {
        log_error("Failed to open Lua script:", full_path, 0);
        return 0;
    }
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (file_size <= 0 || file_size >= MAX_LUA_SCRIPT_SIZE) {
        log_error("Lua script too large or empty:", full_path, 0);
        fclose(fp);
        return 0;
    }
    size_t read = fread(lua_script_source, 1, file_size, fp);
    fclose(fp);
    if ((long) read != file_size) {
        log_error("Failed to read Lua script:", full_path, 0);
        return 0;
    }
    lua_script_source_len = (int) file_size;

    if (!create_lua_state()) {
        return 0;
    }

    if (!execute_lua_source(lua_script_source, lua_script_source_len, full_path)) {
        lua_script_source_len = 0;
        return 0;
    }

    log_info("Lua script loaded:", full_path, 0);
    return 1;
}

int scenario_lua_load_script_from_source(const char *source, int length)
{
    scenario_lua_shutdown();
    lua_script_source_len = 0;

    if (!source || length <= 0) {
        return 0;
    }

    // Store source for future saves
    if (length >= MAX_LUA_SCRIPT_SIZE) {
        log_error("Lua script source too large", 0, 0);
        return 0;
    }
    memcpy(lua_script_source, source, length);
    lua_script_source_len = length;

    if (!create_lua_state()) {
        lua_script_source_len = 0;
        return 0;
    }

    if (!execute_lua_source(source, length, "savegame")) {
        lua_script_source_len = 0;
        return 0;
    }

    log_info("Lua script loaded from savegame source", 0, 0);
    return 1;
}

void scenario_lua_shutdown(void)
{
    if (global_lua_state) {
        lua_close(global_lua_state);
        global_lua_state = 0;
    }
    lua_api_scenario_clear_handlers();
}

int scenario_lua_is_active(void)
{
    return global_lua_state != 0;
}

lua_State *scenario_lua_get_state(void)
{
    return global_lua_state;
}

int scenario_lua_ensure_state(void)
{
    if (global_lua_state) {
        return 1;
    }
    if (!create_lua_state()) {
        return 0;
    }
    log_info("Lua state created for terminal (no script)", 0, 0);
    return 1;
}

// Lua state serialization for savegames
// Save format:
//   [script_len:u32][script_source:raw]
//   [globals: sequence of [name_len:u16][name:raw][value] entries, terminated by name_len=0]
// Value format:
//   [type:u8] followed by type-specific data
#define LUA_SAVE_TYPE_NIL     0
#define LUA_SAVE_TYPE_BOOLEAN 1
#define LUA_SAVE_TYPE_NUMBER  2
#define LUA_SAVE_TYPE_STRING  3
#define LUA_SAVE_TYPE_TABLE   4
#define LUA_SAVE_TYPE_END     0xFF

#define MAX_TABLE_DEPTH 10

static int is_builtin_global(const char *name)
{
    static const char *builtins[] = {
        // API modules
        "game", "city", "finance", "scenario", "map", "ui",
        "building", "resource", "population", "sound", "empire",
        // Standard library
        "_G", "_VERSION", "assert", "collectgarbage", "error", "getmetatable",
        "ipairs", "load", "next", "pairs", "pcall", "print", "rawequal",
        "rawget", "rawlen", "rawset", "require", "select", "setmetatable",
        "tonumber", "tostring", "type", "unpack", "xpcall",
        "table", "string", "math", "utf8", "coroutine",
        "dofile", "loadfile",
        NULL
    };
    for (int i = 0; builtins[i]; i++) {
        if (strcmp(name, builtins[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static void serialize_value(lua_State *L, int index, buffer *buf, int depth);

static void serialize_table(lua_State *L, int index, buffer *buf, int depth)
{
    if (depth > MAX_TABLE_DEPTH) {
        return;
    }
    if (index < 0) {
        index = lua_gettop(L) + index + 1;
    }
    lua_pushnil(L);
    while (lua_next(L, index) != 0) {
        int key_type = lua_type(L, -2);
        if (key_type == LUA_TSTRING) {
            const char *key = lua_tostring(L, -2);
            size_t key_len = strlen(key);
            if (key_len > 0xFFFF) {
                lua_pop(L, 1);
                continue;
            }
            buffer_write_u8(buf, LUA_SAVE_TYPE_STRING);
            buffer_write_u16(buf, (uint16_t) key_len);
            buffer_write_raw(buf, key, key_len);
        } else if (key_type == LUA_TNUMBER) {
            buffer_write_u8(buf, LUA_SAVE_TYPE_NUMBER);
            double num = lua_tonumber(L, -2);
            buffer_write_raw(buf, &num, sizeof(double));
        } else {
            lua_pop(L, 1);
            continue;
        }
        serialize_value(L, -1, buf, depth);
        lua_pop(L, 1);
    }
    buffer_write_u8(buf, LUA_SAVE_TYPE_END);
}

static void serialize_value(lua_State *L, int index, buffer *buf, int depth)
{
    int type = lua_type(L, index);
    switch (type) {
        case LUA_TNIL:
            buffer_write_u8(buf, LUA_SAVE_TYPE_NIL);
            break;
        case LUA_TBOOLEAN:
            buffer_write_u8(buf, LUA_SAVE_TYPE_BOOLEAN);
            buffer_write_u8(buf, lua_toboolean(L, index) ? 1 : 0);
            break;
        case LUA_TNUMBER:
            buffer_write_u8(buf, LUA_SAVE_TYPE_NUMBER);
            {
                double num = lua_tonumber(L, index);
                buffer_write_raw(buf, &num, sizeof(double));
            }
            break;
        case LUA_TSTRING:
            buffer_write_u8(buf, LUA_SAVE_TYPE_STRING);
            {
                size_t len;
                const char *str = lua_tolstring(L, index, &len);
                if (len > 0xFFFF) {
                    len = 0xFFFF;
                }
                buffer_write_u16(buf, (uint16_t) len);
                buffer_write_raw(buf, str, len);
            }
            break;
        case LUA_TTABLE:
            buffer_write_u8(buf, LUA_SAVE_TYPE_TABLE);
            serialize_table(L, index, buf, depth + 1);
            break;
        default:
            buffer_write_u8(buf, LUA_SAVE_TYPE_NIL);
            break;
    }
}

void scenario_lua_save_state(buffer *buf)
{
    // Use a temporary buffer to serialize, since we don't know size upfront
    #define LUA_SAVE_TEMP_SIZE (MAX_LUA_SCRIPT_SIZE + 65536)
    uint8_t *temp_data = malloc(LUA_SAVE_TEMP_SIZE);
    if (!temp_data) {
        log_error("Failed to allocate Lua save buffer", 0, 0);
        return;
    }
    buffer temp;
    buffer_init(&temp, temp_data, LUA_SAVE_TEMP_SIZE);

    // Write script source
    buffer_write_u32(&temp, (uint32_t) lua_script_source_len);
    if (lua_script_source_len > 0) {
        buffer_write_raw(&temp, lua_script_source, lua_script_source_len);
    }

    // Write globals
    if (!global_lua_state) {
        buffer_write_u16(&temp, 0); // end marker
    } else {
        lua_State *L = global_lua_state;

        lua_pushglobaltable(L);
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            if (lua_type(L, -2) == LUA_TSTRING) {
                const char *name = lua_tostring(L, -2);
                if (!is_builtin_global(name)) {
                    int val_type = lua_type(L, -1);
                    if (val_type == LUA_TBOOLEAN || val_type == LUA_TNUMBER ||
                        val_type == LUA_TSTRING || val_type == LUA_TTABLE) {
                        size_t name_len = strlen(name);
                        if (name_len <= 0xFFFF) {
                            buffer_write_u16(&temp, (uint16_t) name_len);
                            buffer_write_raw(&temp, name, name_len);
                            serialize_value(L, -1, &temp, 0);
                        }
                    }
                }
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1); // pop global table

        buffer_write_u16(&temp, 0); // end marker
    }

    // Now allocate the real buffer with exact size and copy data
    size_t data_size = temp.index;
    uint8_t *buf_data = malloc(data_size);
    memcpy(buf_data, temp_data, data_size);
    free(temp_data);

    buffer_init(buf, buf_data, data_size);
    buf->index = data_size; // mark buffer as fully written
}

static int deserialize_value(lua_State *L, buffer *buf, int depth);

static int deserialize_table(lua_State *L, buffer *buf, int depth)
{
    if (depth > MAX_TABLE_DEPTH) {
        return 0;
    }
    lua_newtable(L);
    while (1) {
        uint8_t key_type = buffer_read_u8(buf);
        if (key_type == LUA_SAVE_TYPE_END) {
            break;
        }
        if (key_type == LUA_SAVE_TYPE_STRING) {
            uint16_t key_len = buffer_read_u16(buf);
            char key_buf[0x10000];
            buffer_read_raw(buf, key_buf, key_len);
            key_buf[key_len] = '\0';
            lua_pushstring(L, key_buf);
        } else if (key_type == LUA_SAVE_TYPE_NUMBER) {
            double num;
            buffer_read_raw(buf, &num, sizeof(double));
            lua_pushnumber(L, num);
        } else {
            return 0;
        }
        if (!deserialize_value(L, buf, depth)) {
            lua_pop(L, 1);
            return 0;
        }
        lua_settable(L, -3);
    }
    return 1;
}

static int deserialize_value(lua_State *L, buffer *buf, int depth)
{
    uint8_t type = buffer_read_u8(buf);
    switch (type) {
        case LUA_SAVE_TYPE_NIL:
            lua_pushnil(L);
            break;
        case LUA_SAVE_TYPE_BOOLEAN:
            lua_pushboolean(L, buffer_read_u8(buf));
            break;
        case LUA_SAVE_TYPE_NUMBER:
            {
                double num;
                buffer_read_raw(buf, &num, sizeof(double));
                lua_pushnumber(L, num);
            }
            break;
        case LUA_SAVE_TYPE_STRING:
            {
                uint16_t len = buffer_read_u16(buf);
                char str_buf[0x10000];
                buffer_read_raw(buf, str_buf, len);
                str_buf[len] = '\0';
                lua_pushstring(L, str_buf);
            }
            break;
        case LUA_SAVE_TYPE_TABLE:
            if (!deserialize_table(L, buf, depth + 1)) {
                return 0;
            }
            break;
        default:
            return 0;
    }
    return 1;
}

void scenario_lua_load_state(buffer *buf)
{
    if (!buf || buf->size == 0) {
        return;
    }

    // Read script source
    uint32_t script_len = buffer_read_u32(buf);
    if (script_len > 0) {
        if (script_len >= MAX_LUA_SCRIPT_SIZE) {
            log_error("Lua script in savegame too large", 0, 0);
            return;
        }
        char source[MAX_LUA_SCRIPT_SIZE];
        buffer_read_raw(buf, source, script_len);
        scenario_lua_load_script_from_source(source, (int) script_len);
    } else {
        // No script - just ensure a bare state for globals
        scenario_lua_ensure_state();
    }

    // Read and apply globals
    if (!global_lua_state) {
        return;
    }
    lua_State *L = global_lua_state;

    while (1) {
        uint16_t name_len = buffer_read_u16(buf);
        if (name_len == 0) {
            break;
        }
        char name[0x10000];
        buffer_read_raw(buf, name, name_len);
        name[name_len] = '\0';

        if (!deserialize_value(L, buf, 0)) {
            log_error("Lua state load: corrupt data at global:", name, 0);
            break;
        }
        lua_setglobal(L, name);
    }
    log_info("Lua state restored from savegame", 0, 0);
}

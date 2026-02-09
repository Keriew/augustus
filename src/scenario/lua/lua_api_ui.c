#include "lua/lua.h"
#include "lua/lauxlib.h"

#include "city/message.h"
#include "city/warning.h"
#include "core/log.h"

static int l_ui_log(lua_State *L)
{
    const char *msg = luaL_checkstring(L, 1);
    log_info("[Lua]", msg, 0);
    return 0;
}

static int l_ui_show_warning(lua_State *L)
{
    const char *text = luaL_checkstring(L, 1);
    city_warning_show_custom((const uint8_t *) text, NEW_WARNING_SLOT);
    return 0;
}

static int l_ui_post_message(lua_State *L)
{
    int message_type = (int) luaL_checkinteger(L, 1);
    int param1 = (int) luaL_optinteger(L, 2, 0);
    int param2 = (int) luaL_optinteger(L, 3, 0);
    city_message_post(1, message_type, param1, param2);
    return 0;
}

static const luaL_Reg ui_funcs[] = {
    {"log", l_ui_log},
    {"show_warning", l_ui_show_warning},
    {"post_message", l_ui_post_message},
    {NULL, NULL}
};

void lua_api_ui_register(lua_State *L)
{
    luaL_newlib(L, ui_funcs);
    lua_setglobal(L, "ui");
}

#include "lua/lua.h"
#include "lua/lauxlib.h"

#include "city/message.h"
#include "city/warning.h"
#include "core/encoding.h"
#include "core/log.h"
#include "core/string.h"
#include "scenario/custom_messages.h"
#include "scenario/custom_media.h"
#include "scenario/lua/lua_state.h"
#include "scenario/message_media_text_blob.h"
#include "window/lua_input_dialog.h"

#include <string.h>

// Button callback refs stored in Lua registry
static int button_callback_refs[LUA_INPUT_DIALOG_MAX_BUTTONS];
static int button_callback_count;

static void clear_button_callbacks(void)
{
    lua_State *L = scenario_lua_get_state();
    if (L) {
        for (int i = 0; i < button_callback_count; i++) {
            if (button_callback_refs[i] != LUA_NOREF) {
                luaL_unref(L, LUA_REGISTRYINDEX, button_callback_refs[i]);
            }
        }
    }
    memset(button_callback_refs, 0, sizeof(button_callback_refs));
    button_callback_count = 0;
}

static void on_input_dialog_button_click(int button_index)
{
    if (button_index < 0 || button_index >= button_callback_count) {
        return;
    }
    int ref = button_callback_refs[button_index];
    if (ref == LUA_NOREF || ref == LUA_REFNIL) {
        clear_button_callbacks();
        return;
    }

    lua_State *L = scenario_lua_get_state();
    if (!L) {
        clear_button_callbacks();
        return;
    }

    int top = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        log_error("[Lua] input_dialog button callback error:", lua_tostring(L, -1), 0);
    }
    lua_settop(L, top);
    clear_button_callbacks();
}

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

static int l_ui_show_custom_message(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    custom_message_t *message = custom_messages_create_blank();
    if (!message) {
        log_error("[Lua] Failed to create custom message", 0, 0);
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_getfield(L, 1, "title");
    if (lua_isstring(L, -1)) {
        message->title = message_media_text_blob_add_encoded(lua_tostring(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "subtitle");
    if (lua_isstring(L, -1)) {
        message->subtitle = message_media_text_blob_add_encoded(lua_tostring(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "text");
    if (lua_isstring(L, -1)) {
        message->display_text = message_media_text_blob_add_encoded(lua_tostring(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "background_image");
    if (lua_isstring(L, -1)) {
        message->linked_media[CUSTOM_MEDIA_BACKGROUND_IMAGE] = custom_media_create(
            CUSTOM_MEDIA_BACKGROUND_IMAGE, string_from_ascii(lua_tostring(L, -1)),
            CUSTOM_MEDIA_LINK_TYPE_CUSTOM_MESSAGE_AS_MAIN, message->id);
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "sound");
    if (lua_isstring(L, -1)) {
        message->linked_media[CUSTOM_MEDIA_SOUND] = custom_media_create(
            CUSTOM_MEDIA_SOUND, string_from_ascii(lua_tostring(L, -1)),
            CUSTOM_MEDIA_LINK_TYPE_CUSTOM_MESSAGE_AS_MAIN, message->id);
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "speech");
    if (lua_isstring(L, -1)) {
        message->linked_media[CUSTOM_MEDIA_SPEECH] = custom_media_create(
            CUSTOM_MEDIA_SPEECH, string_from_ascii(lua_tostring(L, -1)),
            CUSTOM_MEDIA_LINK_TYPE_CUSTOM_MESSAGE_AS_MAIN, message->id);
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "background_music");
    if (lua_isstring(L, -1)) {
        message->linked_background_music = custom_media_create(
            CUSTOM_MEDIA_SOUND, string_from_ascii(lua_tostring(L, -1)),
            CUSTOM_MEDIA_LINK_TYPE_CUSTOM_MESSAGE_AS_BACKGROUND_MUSIC, message->id);
    }
    lua_pop(L, 1);

    city_message_post(1, MESSAGE_CUSTOM_MESSAGE, message->id, 0);

    lua_pushboolean(L, 1);
    return 1;
}

static int l_ui_input_dialog(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    clear_button_callbacks();

    lua_input_dialog_data_t *dialog = window_lua_input_dialog_data();
    memset(dialog, 0, sizeof(lua_input_dialog_data_t));
    dialog->on_button_click = on_input_dialog_button_click;

    // Parse title
    lua_getfield(L, 1, "title");
    if (lua_isstring(L, -1)) {
        encoding_from_utf8(lua_tostring(L, -1), dialog->title, LUA_INPUT_DIALOG_MAX_TEXT_LENGTH);
    }
    lua_pop(L, 1);

    // Parse subtitle
    lua_getfield(L, 1, "subtitle");
    if (lua_isstring(L, -1)) {
        encoding_from_utf8(lua_tostring(L, -1), dialog->subtitle, LUA_INPUT_DIALOG_MAX_TEXT_LENGTH);
    }
    lua_pop(L, 1);

    // Parse text
    lua_getfield(L, 1, "text");
    if (lua_isstring(L, -1)) {
        encoding_from_utf8(lua_tostring(L, -1), dialog->text, LUA_INPUT_DIALOG_MAX_TEXT_LENGTH);
    }
    lua_pop(L, 1);

    // Parse image (filepath string, resolved via rich_text_parse_image_id)
    lua_getfield(L, 1, "image");
    if (lua_isstring(L, -1)) {
        encoding_from_utf8(lua_tostring(L, -1), dialog->image, LUA_INPUT_DIALOG_MAX_TEXT_LENGTH);
    }
    lua_pop(L, 1);

    // Parse buttons array
    lua_getfield(L, 1, "buttons");
    if (lua_istable(L, -1)) {
        int buttons_table = lua_gettop(L);
        int len = (int) lua_rawlen(L, buttons_table);
        if (len > LUA_INPUT_DIALOG_MAX_BUTTONS) {
            len = LUA_INPUT_DIALOG_MAX_BUTTONS;
        }
        dialog->num_buttons = len;
        button_callback_count = len;

        for (int i = 0; i < len; i++) {
            button_callback_refs[i] = LUA_NOREF;

            lua_rawgeti(L, buttons_table, i + 1); // push buttons[i+1]
            if (lua_istable(L, -1)) {
                int btn_table = lua_gettop(L);

                // Get label
                lua_getfield(L, btn_table, "label");
                if (lua_isstring(L, -1)) {
                    encoding_from_utf8(lua_tostring(L, -1),
                        dialog->buttons[i].label, LUA_INPUT_DIALOG_MAX_TEXT_LENGTH);
                }
                lua_pop(L, 1);

                // Get on_click callback
                lua_getfield(L, btn_table, "on_click");
                if (lua_isfunction(L, -1)) {
                    button_callback_refs[i] = luaL_ref(L, LUA_REGISTRYINDEX);
                    dialog->buttons[i].has_callback = 1;
                } else {
                    lua_pop(L, 1);
                    button_callback_refs[i] = LUA_NOREF;
                }
            }
            lua_pop(L, 1); // pop buttons[i+1]
        }
    }
    lua_pop(L, 1); // pop buttons table

    if (dialog->num_buttons == 0) {
        log_error("[Lua] input_dialog requires at least one button", 0, 0);
        clear_button_callbacks();
        lua_pushboolean(L, 0);
        return 1;
    }

    window_lua_input_dialog_show();

    lua_pushboolean(L, 1);
    return 1;
}

static const luaL_Reg ui_funcs[] = {
    {"log", l_ui_log},
    {"show_warning", l_ui_show_warning},
    {"post_message", l_ui_post_message},
    {"show_custom_message", l_ui_show_custom_message},
    {"input_dialog", l_ui_input_dialog},
    {NULL, NULL}
};

void lua_api_ui_register(lua_State *L)
{
    luaL_newlib(L, ui_funcs);
    lua_setglobal(L, "ui");
}

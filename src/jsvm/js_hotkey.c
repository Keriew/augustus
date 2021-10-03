#include "js_hotkey.h"

#include "js_defines.h"

#include "input/hotkey.h"
#include "mujs/mujs.h"

void js_hotkey_escape_pressed(js_State *J)
{
    js_pushboolean(J, hotkey_state()->escape_pressed);
}

void js_hotkey_load_file(js_State *J)
{
    js_pushboolean(J, hotkey_state()->load_file);
}

void js_register_hotkey_functions(js_State *J)
{
    DEF_GLOBAL_OBJECT(J, hotkey)
        REGISTER_FUNCTION(J, js_hotkey_escape_pressed, "escape_pressed", 0);
        REGISTER_FUNCTION(J, js_hotkey_load_file, "load_file", 0);
    REGISTER_GLOBAL_OBJECT(J, hotkey)
}
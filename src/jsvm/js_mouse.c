#include "js_mouse.h"

#include "input/mouse.h"
#include "jsvm/js_defines.h"
#include "mujs/mujs.h"

void js_mouse_left_went_up(js_State *J)
{
    js_pushboolean(J, mouse_get()->left.went_up);
}

void js_mouse_right_went_up(js_State *J)
{
    js_pushboolean(J, mouse_get()->right.went_up);
}

void js_register_mouse_functions(js_State *J)
{
    DEF_GLOBAL_OBJECT(J, mouse)
        REGISTER_FUNCTION(J, js_mouse_left_went_up, "left_went_up", 0);
        REGISTER_FUNCTION(J, js_mouse_right_went_up, "right_went_up", 0);
    REGISTER_GLOBAL_OBJECT(J, mouse)
}
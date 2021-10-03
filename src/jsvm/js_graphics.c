#include "js_graphics.h"

#include "graphics/font.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "jsvm/js_defines.h"
#include "mujs/mujs.h"

void js_draw_button(js_State *J)
{
    int left = js_tointeger(J, 1);
    int top = js_tointeger(J, 2);
    int width = js_tointeger(J, 3);
    int type = js_tointeger(J, 4);
    int text_group = js_tointeger(J, 5);
    int text_id = js_tointeger(J, 6);

    large_label_draw(left, top, width / BLOCK_SIZE, type);
    lang_text_draw_centered(text_group, text_id, left, top + 6, width, FONT_NORMAL_GREEN);

    js_pushundefined(J);
}

void js_register_graphics_functions(js_State *J)
{
    REGISTER_GLOBAL_FUNCTION(J, js_draw_button, "draw_button", 6);
}
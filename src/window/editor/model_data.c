#include "model_data.h"

#include "graphics/font.h"
#include "graphics/graphics.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/window.h"
#include "translation/translation.h"
#include "window/editor/map.h"

static void init(void)
{
    
}

static void draw_background(void)
{
    window_editor_map_draw_all();

    graphics_in_dialog();

    outer_panel_draw(16, 32, 38, 26);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_ACTION_TYPE_CHANGE_MODEL_DATA, 26, 42, 608, FONT_LARGE_BLACK);
    lang_text_draw_centered(13, 3, 16, 424, 608, FONT_NORMAL_BLACK);

    graphics_reset_dialog();
}

static void draw_foreground(void)
{
    
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    
}

void window_model_data_show(void)
{
    init();
    window_type window = {
        WINDOW_EDITOR_MODEL_DATA,
        draw_background,
        draw_foreground,
        handle_input
    };
    window_show(&window);
}
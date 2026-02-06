#include "empire_properties.h"

#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/panel.h"
#include "graphics/window.h"
#include "input/hotkey.h"
#include "input/input.h"
#include "input/mouse.h"



static generic_button generic_buttons[] = {
    {}
};

static void init(void)
{
    
}

static void draw_background(void)
{
    window_draw_underlying_window();
    graphics_in_dialog();

    outer_panel_draw(64, 32, 32, 18);


    graphics_reset_dialog();
}

static void draw_foreground(void)
{
    graphics_in_dialog();
    
    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);
    
    if (input_go_back_requested(m, h)) {
        window_go_back();
    }
}


void window_empire_properties_show(void)
{
    init();
    window_type window = {
        WINDOW_EDITOR_EMPIRE_PROPERTIES,
        draw_background,
        draw_foreground,
        handle_input,
    };
    window_show(&window);
}

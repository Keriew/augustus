#include "empire_properties.h"

#include "graphics/button.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/panel.h"
#include "graphics/window.h"
#include "input/hotkey.h"
#include "input/input.h"
#include "input/mouse.h"

static struct {
    unsigned int focus_button_id;
} data;

static void button_select_image(const generic_button *button);

static generic_button generic_buttons[] = {
    {16, 16, 200, 30, button_select_image}
};
#define NUM_GENERIC_BUTTONS sizeof(generic_buttons) / sizeof(generic_button)

static void init(void)
{
    
}

static void draw_background(void)
{
    window_draw_underlying_window();
    graphics_in_dialog();

    outer_panel_draw(0, 0, 40, 30);


    graphics_reset_dialog();
}

static void draw_foreground(void)
{
    graphics_in_dialog();
    
    for (int i = 0; i < NUM_GENERIC_BUTTONS; i++) {
        button_border_draw(generic_buttons[i].x, generic_buttons[i].y, generic_buttons[i].width,
            generic_buttons[i].height, data.focus_button_id == 1);
    }
    
    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);
    
    if (generic_buttons_handle_mouse(m_dialog, 0, 0, generic_buttons, NUM_GENERIC_BUTTONS, &data.focus_button_id)) {
        return;
    }
    
    if (input_go_back_requested(m, h)) {
        window_go_back();
    }
}

static void button_select_image(const generic_button *button)
{
    
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

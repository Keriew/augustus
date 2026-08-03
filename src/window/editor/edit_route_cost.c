#include "edit_route_cost.h"

#include "empire/city.h"
#include "empire/object.h"
#include "input/input.h"
#include "graphics/font.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "translation/translation.h"
#include "window/editor/empire.h"
#include "window/numeric_input.h"

#define WINDOW_WIDTH 30
#define WINDOW_HEIGHT 20

static struct {
    int object_id;
    unsigned int focus_button_id;
} data;

static void button_dn_cost(const generic_button *button);

static generic_button cost_button[] = {
    {24, 24, 27 * BLOCK_SIZE, 29, button_dn_cost}
};

static void init(int object_id)
{
    data.object_id = object_id;
}

static void draw_background(void)
{
    window_draw_underlying_window();
    graphics_in_dialog_with_size(WINDOW_WIDTH * BLOCK_SIZE, WINDOW_HEIGHT * BLOCK_SIZE);

    outer_panel_draw(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    graphics_reset_dialog();
}

static void draw_foreground(void)
{
    graphics_in_dialog_with_size(WINDOW_WIDTH * BLOCK_SIZE, WINDOW_HEIGHT * BLOCK_SIZE);

    full_empire_object *full = empire_object_get_full(data.object_id);
    large_label_draw(cost_button[0].x, cost_button[0].y, cost_button[0].width / BLOCK_SIZE, data.focus_button_id == 1);
    text_draw_centered(translation_for(TR_EMPIRE_ROUTE_COST_DN), cost_button[0].x, cost_button[0].y + 8,
            cost_button[0].width / 2, FONT_NORMAL_GREEN, COLOR_MASK_NONE);
    text_draw_number_centered(full->trade_route_cost, cost_button[0].x + cost_button[0].width / 2, cost_button[0].y + 8,
        cost_button[0].width / 2, FONT_NORMAL_GREEN);

    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);

    if (generic_buttons_handle_mouse(m_dialog, 0, 0, cost_button, 1, &data.focus_button_id)) {
        return;
    }

    if (input_go_back_requested(m, h)) {
        window_editor_empire_show();
    }
}

static void set_opening_cost(int value)
{
    empire_city_set_trade_route_cost(empire_object_get(data.object_id)->trade_route_id, value);
}

static void button_dn_cost(const generic_button *button)
{
    window_numeric_input_bound_show(0, 0, button, 6, 1, 999999, set_opening_cost);
}

void window_editor_edit_route_cost_show(unsigned int object_id)
{
    init(object_id);
    window_type window = {
        WINDOW_EDITOR_EDIT_ROUTE_COST,
        draw_background,
        draw_foreground,
        handle_input,
    };
    window_show(&window);
}

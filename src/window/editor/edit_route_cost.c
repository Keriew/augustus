#include "edit_route_cost.h"

#include "empire/city.h"
#include "empire/object.h"
#include "game/resource.h"
#include "graphics/font.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/image_button.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "translation/translation.h"
#include "window/editor/empire.h"
#include "window/numeric_input.h"
#include "window/select_list.h"

#include <math.h>
#include <stdio.h>

#define WINDOW_WIDTH 30
#define WINDOW_HEIGHT 20

#define RESOURCE_ICON_SIDE 26

#define DEFAULT_RESOURCE_COST 16

typedef struct {
    int x;
    int y;
    int enabled;
    int highlighted;
} resource_button;

static struct {
    int object_id;
    unsigned int focus_button_id;
    int resource_pulse_start;
    resource_type selected_resource;
    resource_type available_resources[RESOURCE_MAX];
} data;

static void button_dn_cost(const generic_button *button);
static void button_add_resource(int param1, int param2);

static generic_button cost_button[] = {
    {24, 24, 27 * BLOCK_SIZE, 29, button_dn_cost}
};

static resource_button resource_buttons[RESOURCE_MAX] = { 0 };

static image_button add_resource_button[] = {
    {0, 0, 39, 24, IB_NORMAL, 0, 0, button_add_resource, button_none, 0, 0, 1, "UI", "Plus_Button_Idle"}
};

static void init(int object_id)
{
    data.object_id = object_id;
    data.resource_pulse_start = time_get_millis();
}

static void draw_background(void)
{
    window_draw_underlying_window();
    graphics_in_dialog_with_size(WINDOW_WIDTH * BLOCK_SIZE, WINDOW_HEIGHT * BLOCK_SIZE);

    outer_panel_draw(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    inner_panel_draw(BLOCK_SIZE, 4 * BLOCK_SIZE, WINDOW_WIDTH - 2, WINDOW_HEIGHT - 5);

    graphics_reset_dialog();
}

static int draw_resource(resource_type resource, int resource_amount, int x_offset, int y_offset, int is_highlighted)
{
    graphics_draw_inset_rect(x_offset, y_offset, RESOURCE_ICON_SIDE, RESOURCE_ICON_SIDE, COLOR_INSET_DARK, COLOR_INSET_LIGHT);
    image_draw(resource_get_data(resource)->image.editor.empire, x_offset + 1, y_offset + 1,
        COLOR_MASK_NONE, SCALE_NONE);
    if (is_highlighted) {
        time_millis elapsed = time_get_millis() - data.resource_pulse_start;
        float time_seconds = elapsed / 1000.0f; // Convert to seconds
        float pulse = sinf(time_seconds * 1.0f * 3.14f); // 1 full cycle per second
        int alpha = 96 + (int) (pulse * 64); // Range: 32–160
        graphics_tint_rect(x_offset, y_offset, RESOURCE_ICON_SIDE - 1, RESOURCE_ICON_SIDE - 1,
            COLOR_MASK_DARK_PINK, alpha);
    }
    return text_draw_number(resource_amount, '\0', "", x_offset + 28, y_offset + 9, FONT_NORMAL_GREEN, 0);
}

static void draw_foreground(void)
{
    graphics_in_dialog_with_size(WINDOW_WIDTH * BLOCK_SIZE, WINDOW_HEIGHT * BLOCK_SIZE);

    full_empire_object *full = empire_object_get_full(data.object_id);

    // Denarii cost
    large_label_draw(cost_button[0].x, cost_button[0].y, cost_button[0].width / BLOCK_SIZE, data.focus_button_id == 1);
    text_draw_centered(translation_for(TR_EMPIRE_ROUTE_COST), cost_button[0].x, cost_button[0].y + 8,
            cost_button[0].width / 2, FONT_NORMAL_GREEN, COLOR_MASK_NONE);
    const uint8_t cost_text[32] = "";
    snprintf((char *) cost_text, 32, "%i %s", full->trade_route_cost, lang_get_string(6, 0));
    text_draw_centered(cost_text, cost_button[0].x + cost_button[0].width / 2, cost_button[0].y + 8,
        cost_button[0].width / 2, FONT_NORMAL_GREEN, COLOR_MASK_NONE);

    // Resource cost
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EMPIRE_RESOURCE_COST, BLOCK_SIZE, 4 * BLOCK_SIZE + 8,
        (WINDOW_WIDTH - 2) * BLOCK_SIZE, FONT_NORMAL_GREEN);

    // Resources
    int y_offset = 6 * BLOCK_SIZE;
    int resource_x_offset = 24;
    for (resource_type r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        int resource_amount = full->route_resource_cost[r];
        if (resource_amount) {
            int estimated_width = 32 + text_get_number_width(resource_amount, '\0', "", FONT_NORMAL_GREEN);
            if (resource_x_offset + estimated_width >= (WINDOW_WIDTH - 1) * BLOCK_SIZE) {
                // if the resource doesn't fit in this row put it in the next one
                y_offset += 32;
                resource_x_offset = 24;
            }
            int real_width = draw_resource(r, resource_amount, resource_x_offset, y_offset - 9, resource_buttons[r].highlighted);
            resource_buttons[r] = (resource_button) { resource_x_offset, y_offset - 9, 1, 0 };
            resource_x_offset += 32 + real_width;
        } else {
            resource_buttons[r] = (resource_button) { 0, 0, 0, 0 };
        }
    }

    // Add resource button
    int estimated_width = 8 + add_resource_button[0].width;
    if (resource_x_offset + estimated_width >= (WINDOW_WIDTH - 1) * BLOCK_SIZE) {
        // if the button doesn't fit in this row put it in the next one
        y_offset += 32;
        resource_x_offset = 24;
    }
    add_resource_button[0].x_offset = resource_x_offset;
    add_resource_button[0].y_offset = y_offset - 9 + (y_offset + RESOURCE_ICON_SIDE - y_offset) / 2 - add_resource_button[0].height / 2;
    image_buttons_draw(0, 0, add_resource_button, 1);

    graphics_reset_dialog();
}

static void set_resource_cost(int value)
{
    full_empire_object *city = empire_object_get_full(data.object_id);
    city->route_resource_cost[data.selected_resource] = value;
}

static int resource_buttons_handle_input(const mouse *m)
{
    for (resource_type r = RESOURCE_NONE; r < RESOURCE_MAX; r++) {
        resource_button *btn = &resource_buttons[r];
        if (m->x >= btn->x && m->x < btn->x + RESOURCE_ICON_SIDE &&
            m->y >= btn->y && m->y < btn->y + RESOURCE_ICON_SIDE && btn->enabled) {
            btn->highlighted = 1;
            if (m->left.went_up) {
                data.selected_resource = r;
                generic_button converted_button = (generic_button) { btn->x, btn->y, RESOURCE_ICON_SIDE, RESOURCE_ICON_SIDE };
                window_numeric_input_bound_show(0, 0, &converted_button, 5, 0, 99999, set_resource_cost);
                window_request_refresh();
                return 1;
            }
            if (m->right.went_up) {
                full_empire_object *city = empire_object_get_full(data.object_id);
                city->route_resource_cost[r] = 0;

                window_request_refresh();
                return 1;
            }
        } else {
            btn->highlighted = 0;
        }
    }

    return 0;
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);

    if (generic_buttons_handle_mouse(m_dialog, 0, 0, cost_button, 1, &data.focus_button_id)) {
        return;
    }

    if (resource_buttons_handle_input(m_dialog)) {
        return;
    }

    if (image_buttons_handle_mouse(m_dialog, 0, 0, add_resource_button, 1, NULL)) {
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

static void add_resource(int value)
{
    resource_type resource = data.available_resources[value];
    full_empire_object *city = empire_object_get_full(data.object_id);
    if (!city->route_resource_cost[resource]) {
        city->route_resource_cost[resource] = DEFAULT_RESOURCE_COST;
    }
}

static void button_add_resource(int param1, int param2)
{
    static const uint8_t *resource_texts[RESOURCE_MAX];
    int total_resources = 0;
    for (resource_type resource = RESOURCE_NONE; resource < RESOURCE_MAX; resource++) {
        if (!resource_is_storable(resource)) {
            continue;
        }
        resource_texts[total_resources] = resource_get_data(resource)->text;
        data.available_resources[total_resources] = resource;
        total_resources++;
    }

    generic_button converted_button = (generic_button) { add_resource_button[0].x_offset, add_resource_button[0].y_offset,
        add_resource_button[0].width, add_resource_button[0].height };
    window_select_list_show_text(0, 0, &converted_button, resource_texts, total_resources, add_resource);
}

static void get_tooltip(tooltip_context *c)
{
    image_button *btn = &add_resource_button[0];
    const mouse *m = mouse_in_dialog(mouse_get());
    if (m->x >= btn->x_offset && m->x < btn->x_offset + btn->width &&
        m->y >= btn->y_offset && m->y < btn->y_offset + btn->height && btn->enabled) {
        c->text_group = CUSTOM_TRANSLATION;
        c->text_id = TR_EMPIRE_TOOLTIP_ADD_RESOURCE;
        c->type = TOOLTIP_BUTTON;
        return;
    }
}

void window_editor_edit_route_cost_show(unsigned int object_id)
{
    init(object_id);
    window_type window = {
        WINDOW_EDITOR_EDIT_ROUTE_COST,
        draw_background,
        draw_foreground,
        handle_input,
        get_tooltip
    };
    window_show(&window);
}

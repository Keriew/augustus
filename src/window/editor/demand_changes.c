#include "demand_changes.h"

#include "core/image_group_editor.h"
#include "core/string.h"
#include "empire/city.h"
#include "empire/trade_route.h"
#include "game/resource.h"
#include "graphics/button.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/grid_box.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "scenario/demand_change.h"
#include "scenario/editor.h"
#include "scenario/property.h"
#include "window/editor/attributes.h"
#include "window/editor/edit_demand_change.h"
#include "window/editor/map.h"

#include <stdlib.h>

static void button_demand_change(const grid_box_item *item);
static void button_new_demand_change(const generic_button *button);
static void draw_demand_change_button(const grid_box_item *item);

typedef struct {
    int value;
    int difference;
} demand_change_amount_t;

static struct {
    const demand_change_t **demand_changes;
    unsigned int total_demand_changes;
    unsigned int demand_changes_in_use;
    unsigned int new_demand_change_button_focused;
} data;

static generic_button new_demand_change_button = {
    195, 340, 250, 25, button_new_demand_change
};

static grid_box_type demand_change_buttons = {
    .x = 10,
    .y = 40,
    .width = 38 * BLOCK_SIZE,
    .height = 19 * BLOCK_SIZE,
    .num_columns = 1,
    .item_height = 28,
    .item_margin.horizontal = 10,
    .item_margin.vertical = 4,
    .extend_to_hidden_scrollbar = 1,
    .on_click = button_demand_change,
    .draw_item = draw_demand_change_button
};

static void limit_and_sort_list(void)
{
    data.demand_changes_in_use = 0;
    for (unsigned int i = 0; i < data.total_demand_changes; i++) {
        const demand_change_t *demand_change = scenario_demand_change_get(i);
        if (!demand_change->year) {
            continue;
        }
        data.demand_changes[data.demand_changes_in_use] = demand_change;
        data.demand_changes_in_use++;
    }
    for (unsigned int i = 0; i < data.demand_changes_in_use; i++) {
        for (unsigned int j = data.demand_changes_in_use - 1; j > 0; j--) {
            const demand_change_t *current = data.demand_changes[j];
            const demand_change_t *prev = data.demand_changes[j - 1];
            if (current->year && (!prev->year || prev->year > current->year)) {
                const demand_change_t *tmp = data.demand_changes[j];
                data.demand_changes[j] = data.demand_changes[j - 1];
                data.demand_changes[j - 1] = tmp;
            }
        }
    }
}

static void update_demand_changes_list(void)
{
    unsigned int current_demand_changes = scenario_demand_change_count_total();
    if (current_demand_changes != data.total_demand_changes) {
        free(data.demand_changes);
        data.demand_changes = 0;
        if (current_demand_changes) {
            data.demand_changes = malloc(current_demand_changes * sizeof(demand_change_t *));
            if (!data.demand_changes) {
                grid_box_update_total_items(&demand_change_buttons, 0);
                data.total_demand_changes = 0;
                data.demand_changes_in_use = 0;
                return;
            }
        }
        data.total_demand_changes = current_demand_changes;
    }
    limit_and_sort_list();
    if (grid_box_get_total_items(&demand_change_buttons) != data.demand_changes_in_use) {
        grid_box_update_total_items(&demand_change_buttons, data.demand_changes_in_use);
    }
}

static void draw_background(void)
{
    update_demand_changes_list();

    window_editor_map_draw_all();

    graphics_in_dialog();

    outer_panel_draw(0, 0, 40, 25);
    lang_text_draw(44, 94, 20, 12, FONT_LARGE_BLACK);

    if (!data.demand_changes_in_use) {
        lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_NO_DEMAND_CHANGES, 0, 165, 640, FONT_LARGE_BLACK);
    }

    lang_text_draw_centered(13, 3, 0, 374, 640, FONT_NORMAL_BLACK);

    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_NEW_DEMAND_CHANGE, new_demand_change_button.x + 8,
        new_demand_change_button.y + 8, new_demand_change_button.width - 16, FONT_NORMAL_BLACK);

    graphics_reset_dialog();

    grid_box_request_refresh(&demand_change_buttons);
}

static void get_change_amount(unsigned int index, demand_change_amount_t *amount)
{
    const demand_change_t *new_demand_change = data.demand_changes[index];
    int previous_value = 0;
    amount->value = trade_route_limit(new_demand_change->route_id, new_demand_change->resource, new_demand_change->buys);
    for (unsigned int i = 0; i <= index; i++) {
        const demand_change_t *current_demand_change = data.demand_changes[i];
        if (current_demand_change->resource != new_demand_change->resource ||
            current_demand_change->route_id != new_demand_change->route_id) {
            continue;
        }
        previous_value = amount->value;
        if (current_demand_change->amount == DEMAND_CHANGE_LEGACY_IS_RISE) {
            if (previous_value == 0) {
                amount->value = 15;
            } else if (previous_value == 15) {
                amount->value = 25;
            } else if (previous_value == 25) {
                amount->value = 40;
            }
        } else if (current_demand_change->amount == DEMAND_CHANGE_LEGACY_IS_FALL) {
            if (previous_value == 40) {
                amount->value = 25;
            } else if (previous_value == 25) {
                amount->value = 15;
            } else if (previous_value == 15) {
                amount->value = 0;
            }
        } else {
            amount->value = current_demand_change->amount;
        }
    }
    amount->difference = amount->value - previous_value;
}

static const uint8_t *get_route_name(int route_id)
{
    int city_id = empire_city_get_for_trade_route(route_id);
    if (city_id < 0) {
        return lang_get_string(CUSTOM_TRANSLATION, TR_EDITOR_UNKNOWN_ROUTE);
    }
    empire_city *city = empire_city_get(city_id);
    if (city->type == EMPIRE_CITY_TRADE || city->type == EMPIRE_CITY_FUTURE_TRADE) {
        return empire_city_get_name(city);
    }
    return lang_get_string(CUSTOM_TRANSLATION, TR_EDITOR_UNKNOWN_ROUTE);
}

static void draw_demand_change_button(const grid_box_item *item)
{
    button_border_draw(item->x, item->y, item->width, item->height, item->is_focused);
    const demand_change_t *demand_change = data.demand_changes[item->index];

    int COL_YEAR = 5;
    int COL_SCENARIO = 45;
    int COL_RESOURCE = 140;
    int COL_AMOUNT = 165;
    int COL_BUY_SELL = 280;
    int COL_CITY = 380;

    // 1 year
    text_draw_number(demand_change->year, '+', " ", item->x + COL_YEAR, item->y + 7, FONT_NORMAL_BLACK, 0);

    // 2 scenario start year
    lang_text_draw_year(scenario_property_start_year() + demand_change->year, item->x + COL_SCENARIO, item->y + 7,
        FONT_NORMAL_BLACK);

    // 3 resource icon
    int image_id = resource_get_data(demand_change->resource)->image.editor.icon;
    const image *img = image_get(image_id);
    int base_width = (24 - img->original.width) / 2;
    int base_height = (item->height - img->original.height) / 2;
    image_draw(image_id, item->x + COL_RESOURCE + base_width, item->y + base_height, COLOR_MASK_NONE, SCALE_NONE);

    // 4 amount
    demand_change_amount_t amount;
    get_change_amount(item->index, &amount);
    int width = 0;
    int x_amount = item->x + COL_AMOUNT;
    width += text_draw_number(amount.value, '@', " ", x_amount + width, item->y + 7,
        FONT_NORMAL_BLACK, 0);
    if (amount.difference > 0) {
        width += text_draw(string_from_ascii("("), x_amount + width - 8, item->y + 7,
            FONT_NORMAL_PLAIN, COLOR_FONT_DARK_GREEN);
        width += text_draw_number(amount.difference, '+', ")", x_amount + width - 13, item->y + 7,
            FONT_NORMAL_PLAIN, COLOR_FONT_DARK_GREEN);
    } else {
        width += text_draw_number(amount.difference, '(', ")", x_amount + width - 8, item->y + 7,
            FONT_NORMAL_PLAIN, COLOR_FONT_RED);
    }

    // 5 buys-sells
    translation_key key = demand_change->buys ? TR_EDITOR_DEMAND_CHANGE_BUYS : TR_EDITOR_DEMAND_CHANGE_SELLS;
    int color = demand_change->buys ? COLOR_MASK_DARK_GREEN : COLOR_MASK_PURPLE;
    lang_text_draw_colored(CUSTOM_TRANSLATION, key, item->x + COL_BUY_SELL, item->y + 7,
        FONT_NORMAL_PLAIN, color);

    // 6 city name
    text_draw(get_route_name(demand_change->route_id), item->x + COL_CITY, item->y + 7,
        FONT_NORMAL_BLACK, COLOR_MASK_NONE);
}

static void draw_foreground(void)
{
    graphics_in_dialog();

    if (data.demand_changes_in_use) {
        grid_box_draw(&demand_change_buttons);
    }
    button_border_draw(new_demand_change_button.x, new_demand_change_button.y, new_demand_change_button.width,
        new_demand_change_button.height, data.new_demand_change_button_focused);

    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);
    if (grid_box_handle_input(&demand_change_buttons, m_dialog, 1) ||
        generic_buttons_handle_mouse(m_dialog, 0, 0, &new_demand_change_button, 1,
            &data.new_demand_change_button_focused)) {
        return;
    }
    if (input_go_back_requested(m, h)) {
        window_editor_attributes_show();
    }
}

static void button_demand_change(const grid_box_item *item)
{
    window_editor_edit_demand_change_show(data.demand_changes[item->index]->id);
}

static void button_new_demand_change(const generic_button *button)
{
    int new_demand_change_id = scenario_demand_change_new();
    if (new_demand_change_id >= 0) {
        window_editor_edit_demand_change_show(new_demand_change_id);
    }
}

void window_editor_demand_changes_show(void)
{
    window_type window = {
        WINDOW_EDITOR_DEMAND_CHANGES,
        draw_background,
        draw_foreground,
        handle_input
    };
    grid_box_init(&demand_change_buttons, scenario_demand_change_count_total());
    window_show(&window);
}

#include "select_city_trade_route.h"

#include "core/lang.h"
#include "core/string.h"
#include "empire/city.h"
#include "empire/trade_route.h"
#include "graphics/button.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/screen.h"
#include "graphics/scrollbar.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "window/editor/map.h"
#include "window/numeric_input.h"

#include <string.h>
#include <stdio.h>

#define MAX_BUTTONS 14
#define BUTTON_LEFT_PADDING 32
#define BUTTON_WIDTH 608
#define DETAILS_Y_OFFSET 32
#define DETAILS_ROW_HEIGHT 32
#define MAX_VISIBLE_ROWS 14

#define RESOURCE_ALL_BUYS RESOURCE_MAX + 1 // max +1 indicates all resources that this trade route buys
#define RESOURCE_ALL_SELLS RESOURCE_MAX + 2 // max +2 indicates all resources that this trade route sells

typedef enum {
    WINDOW_TYPE_TRADE_ROUTES,
    WINDOW_TYPE_RESOURCES
} active_window_type;

static active_window_type current_window_type;

static void on_scroll(void);
static void button_click(const generic_button *button);
static void draw_foreground_resources(void);
static void handle_input_resources(const mouse *m, const hotkeys *h);

static const uint8_t UNKNOWN[4] = { '?', '?', '?', 0 };

static scrollbar_type scrollbar = {
    640, DETAILS_Y_OFFSET, DETAILS_ROW_HEIGHT * MAX_VISIBLE_ROWS, BUTTON_WIDTH, MAX_VISIBLE_ROWS, on_scroll, 0, 4
};

static generic_button buttons[] = {
    {BUTTON_LEFT_PADDING, DETAILS_Y_OFFSET + (0 * DETAILS_ROW_HEIGHT), BUTTON_WIDTH, DETAILS_ROW_HEIGHT - 2, button_click, 0, 0},
    {BUTTON_LEFT_PADDING, DETAILS_Y_OFFSET + (1 * DETAILS_ROW_HEIGHT), BUTTON_WIDTH, DETAILS_ROW_HEIGHT - 2, button_click, 0, 1},
    {BUTTON_LEFT_PADDING, DETAILS_Y_OFFSET + (2 * DETAILS_ROW_HEIGHT), BUTTON_WIDTH, DETAILS_ROW_HEIGHT - 2, button_click, 0, 2},
    {BUTTON_LEFT_PADDING, DETAILS_Y_OFFSET + (3 * DETAILS_ROW_HEIGHT), BUTTON_WIDTH, DETAILS_ROW_HEIGHT - 2, button_click, 0, 3},
    {BUTTON_LEFT_PADDING, DETAILS_Y_OFFSET + (4 * DETAILS_ROW_HEIGHT), BUTTON_WIDTH, DETAILS_ROW_HEIGHT - 2, button_click, 0, 4},
    {BUTTON_LEFT_PADDING, DETAILS_Y_OFFSET + (5 * DETAILS_ROW_HEIGHT), BUTTON_WIDTH, DETAILS_ROW_HEIGHT - 2, button_click, 0, 5},
    {BUTTON_LEFT_PADDING, DETAILS_Y_OFFSET + (6 * DETAILS_ROW_HEIGHT), BUTTON_WIDTH, DETAILS_ROW_HEIGHT - 2, button_click, 0, 6},
    {BUTTON_LEFT_PADDING, DETAILS_Y_OFFSET + (7 * DETAILS_ROW_HEIGHT), BUTTON_WIDTH, DETAILS_ROW_HEIGHT - 2, button_click, 0, 7},
    {BUTTON_LEFT_PADDING, DETAILS_Y_OFFSET + (8 * DETAILS_ROW_HEIGHT), BUTTON_WIDTH, DETAILS_ROW_HEIGHT - 2, button_click, 0, 8},
    {BUTTON_LEFT_PADDING, DETAILS_Y_OFFSET + (9 * DETAILS_ROW_HEIGHT), BUTTON_WIDTH, DETAILS_ROW_HEIGHT - 2, button_click, 0, 9},
    {BUTTON_LEFT_PADDING, DETAILS_Y_OFFSET + (10 * DETAILS_ROW_HEIGHT), BUTTON_WIDTH, DETAILS_ROW_HEIGHT - 2, button_click, 0, 10},
    {BUTTON_LEFT_PADDING, DETAILS_Y_OFFSET + (11 * DETAILS_ROW_HEIGHT), BUTTON_WIDTH, DETAILS_ROW_HEIGHT - 2, button_click, 0, 11},
    {BUTTON_LEFT_PADDING, DETAILS_Y_OFFSET + (12 * DETAILS_ROW_HEIGHT), BUTTON_WIDTH, DETAILS_ROW_HEIGHT - 2, button_click, 0, 12},
    {BUTTON_LEFT_PADDING, DETAILS_Y_OFFSET + (13 * DETAILS_ROW_HEIGHT), BUTTON_WIDTH, DETAILS_ROW_HEIGHT - 2, button_click, 0, 13}
};

typedef struct {
    int id; //route for the city, resource for the resource list
    const uint8_t *name;
} list_item_entry_t;

static struct {
    unsigned int focus_button_id;
    unsigned int list_size;
    void (*callback)(int);

    list_item_entry_t list[MAX_VISIBLE_ROWS];
} data;

static struct {
    city_resource_state resource[RESOURCE_MAX];
    int trade_route_id;
    list_item_entry_t list[MAX_VISIBLE_ROWS];
    resource_type traded_resources[RESOURCE_MAX]; // Maps list index to actual resource_type
    unsigned int resource_list_size;
    void (*callback)(int);
    unsigned int focus_button_id;
    const uint8_t *display_name_pointer; //pointer to the name to update after selection is made
} route_resource_data;

static void populate_list(int offset)
{
    if (data.list_size - offset < MAX_VISIBLE_ROWS) {
        offset = data.list_size - MAX_VISIBLE_ROWS;
    }
    if (offset < 0) {
        offset = 0;
    }
    for (int i = 0; i < MAX_VISIBLE_ROWS; i++) {
        unsigned int target_id = i + offset + 1;
        if (target_id < data.list_size) {
            data.list[i].id = target_id;
            int city_id = empire_city_get_for_trade_route(target_id);
            if (city_id != -1) {
                empire_city *city = empire_city_get(city_id);
                data.list[i].name = empire_city_get_name(city);
            } else {
                data.list[i].name = UNKNOWN;
            }
        } else {
            data.list[i].id = 0;
            data.list[i].name = UNKNOWN;
        }
    }
}

static void populate_resource_list_for_route(int offset)
{
    if (route_resource_data.resource_list_size - offset < MAX_VISIBLE_ROWS) {
        offset = route_resource_data.resource_list_size - MAX_VISIBLE_ROWS;
    }
    if (offset < 0) {
        offset = 0;
    }

    for (int i = 0; i < MAX_VISIBLE_ROWS; i++) {
        unsigned int list_index = i + offset;

        if (list_index < route_resource_data.resource_list_size) {
            // Handle special 'All' entries (first two positions)
            if (list_index == 0) {
                route_resource_data.list[i].id = RESOURCE_ALL_BUYS;
                route_resource_data.list[i].name = translation_for(TR_EDITOR_ALL_BUYS);
                continue;
            } else if (list_index == 1) {
                route_resource_data.list[i].id = RESOURCE_ALL_SELLS;
                route_resource_data.list[i].name = translation_for(TR_EDITOR_ALL_SELLS);
                continue;
            }

            // Regular resource entry - get the actual resource_type from the mapping
            resource_type r = route_resource_data.traded_resources[list_index];
            route_resource_data.list[i].id = r;

            const resource_data *r_data = resource_get_data(r);

            if (r_data) {
                const uint8_t *resource_name = r_data->text;
                const uint8_t *suffix =
                    route_resource_data.resource[r] == RESOURCE_BUYS ? translation_for(TR_EDITOR_BUYS) :
                    route_resource_data.resource[r] == RESOURCE_SELLS ? translation_for(TR_EDITOR_SELLS) :
                    NULL;

                static uint8_t name_buffer[MAX_VISIBLE_ROWS][64];
                if (suffix) {
                    snprintf((char *) name_buffer[i], sizeof(name_buffer[i]), "%s %s", resource_name, suffix);
                    route_resource_data.list[i].name = name_buffer[i];
                } else {
                    route_resource_data.list[i].name = resource_name;
                }
            } else {
                route_resource_data.list[i].name = (const uint8_t *) "UNKNOWN";
            }
        } else {
            route_resource_data.list[i].id = 0;
            route_resource_data.list[i].name = (const uint8_t *) "";
        }
    }
}

static void create_resource_list_for_route(int route_id)
{
    route_resource_data.trade_route_id = route_id;

    // Start with 2 to reserve space for the two 'All' entries at the beginning
    route_resource_data.resource_list_size = 2;

    // Build a sequential list of traded resources (starting after the 'All' entries)
    for (resource_type r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        if (!resource_is_storable(r)) {
            continue;
        }
        int buys = 0;
        int sells = 0;
        int city_id = empire_city_get_for_trade_route(route_id);
        if (city_id != -1) {
            buys = empire_city_buys_resource(city_id, r);
            sells = empire_city_sells_resource(city_id, r);
        }
        if (buys) {
            route_resource_data.resource[r] = RESOURCE_BUYS;
            route_resource_data.traded_resources[route_resource_data.resource_list_size] = r;
            route_resource_data.resource_list_size++;
        } else if (sells) {
            route_resource_data.resource[r] = RESOURCE_SELLS;
            route_resource_data.traded_resources[route_resource_data.resource_list_size] = r;
            route_resource_data.resource_list_size++;
        } else {
            route_resource_data.resource[r] = RESOURCE_NOT_TRADED;
        }
    }
}

static void init(void (*callback)(int))
{
    current_window_type = WINDOW_TYPE_TRADE_ROUTES;
    data.callback = callback;
    data.list_size = trade_route_count();

    memset(data.list, 0, sizeof(data.list));

    scrollbar_init(&scrollbar, 0, data.list_size);
    populate_list(0);
}

static void draw_background(void)
{
    window_editor_map_draw_all();
}

static void draw_foreground(void)
{
    graphics_in_dialog();

    outer_panel_draw(16, 16, 42, 33);

    int y_offset = DETAILS_Y_OFFSET;
    for (unsigned int i = 0; i < MAX_VISIBLE_ROWS; i++) {
        if (i < data.list_size - 1) {
            large_label_draw(buttons[i].x, buttons[i].y, buttons[i].width / 16, data.focus_button_id == i + 1 ? 1 : 0);

            text_draw(data.list[i].name, 48, y_offset + 8, FONT_NORMAL_PLAIN, COLOR_BLACK);
        }

        y_offset += DETAILS_ROW_HEIGHT;
    }

    lang_text_draw_centered(13, 3, 48, 32 + 16 * 30, BUTTON_WIDTH, FONT_NORMAL_BLACK);

    scrollbar_draw(&scrollbar);
    graphics_reset_dialog();
}

static void on_scroll(void)
{
    window_request_refresh();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);
    if (scrollbar_handle_mouse(&scrollbar, m_dialog, 1) ||
        generic_buttons_handle_mouse(m_dialog, 0, 0, buttons, MAX_BUTTONS, &data.focus_button_id)) {
        return;
    }
    if (input_go_back_requested(m, h)) {
        window_go_back();
    }
    populate_list(scrollbar.scroll_position);
}

static void button_click(const generic_button *button)
{
    int index = button->parameter1;

    if (current_window_type == WINDOW_TYPE_TRADE_ROUTES) {
        if (index >= (int) data.list_size) {
            return;
        }
        data.callback(data.list[index].id);
        window_go_back();
    } else if (current_window_type == WINDOW_TYPE_RESOURCES) {
        if (index >= (int) route_resource_data.resource_list_size) {
            return;
        }

        // Encode both route_id and resource_id into a single value
        int resource_id = route_resource_data.list[index].id;
        int encoded_value = window_editor_select_city_trade_route_encode_route_resource(
            route_resource_data.trade_route_id, resource_id);
        route_resource_data.callback(encoded_value);
        window_go_back();
    }
}

static void draw_foreground_resources(void)
{
    graphics_in_dialog();
    outer_panel_draw(16, 16, 42, 33);

    int y_offset = DETAILS_Y_OFFSET;
    for (unsigned int i = 0; i < MAX_VISIBLE_ROWS; i++) {
        if (i < route_resource_data.resource_list_size) {
            large_label_draw(buttons[i].x, buttons[i].y, buttons[i].width / 16, route_resource_data.focus_button_id == i + 1 ? 1 : 0);
            if (route_resource_data.focus_button_id == i + 1) {
                button_border_draw(BUTTON_LEFT_PADDING, y_offset, BUTTON_WIDTH, DETAILS_ROW_HEIGHT, 1);
            }
            text_draw(route_resource_data.list[i].name, 48, y_offset + 8, FONT_NORMAL_PLAIN, COLOR_BLACK);
        }
        y_offset += DETAILS_ROW_HEIGHT;
    }

    scrollbar_draw(&scrollbar);
    graphics_reset_dialog();
}

static void handle_input_resources(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);
    if (scrollbar_handle_mouse(&scrollbar, m_dialog, 1) ||
        generic_buttons_handle_mouse(m_dialog, 0, 0, buttons, MAX_BUTTONS, &route_resource_data.focus_button_id)) {
        return;
    }

    if (input_go_back_requested(m, h)) {
        window_go_back();
    }

    populate_resource_list_for_route(scrollbar.scroll_position);
}

void window_editor_select_city_trade_route_show(void (*callback)(int))
{
    window_type window = {
        WINDOW_EDITOR_SELECT_CITY_TRADE_ROUTE,
        draw_background,
        draw_foreground,
        handle_input
    };
    init(callback);
    window_show(&window);
}
static void init_resources(void (*callback)(int), int route_id)
{
    current_window_type = WINDOW_TYPE_RESOURCES;
    route_resource_data.callback = callback;
    route_resource_data.trade_route_id = route_id;
    create_resource_list_for_route(route_id);
    scrollbar_init(&scrollbar, 0, route_resource_data.resource_list_size);
    populate_resource_list_for_route(0);
}

void window_editor_select_city_resources_for_route_show(void (*callback)(int), int route_id)
{
    window_type window = {
        WINDOW_EDITOR_SELECT_CITY_RESOURCES_FOR_ROUTE,
        draw_background,
        draw_foreground_resources,
        handle_input_resources
    };
    init_resources(callback, route_id);
    window_show(&window);
}

// Encoding helper: combines route_id and resource_id into single value
// Upper 16 bits: route_id, Lower 16 bits: resource_id
int window_editor_select_city_trade_route_encode_route_resource(int trade_route_id, int resource_id)
{
    return (trade_route_id << 16) | (resource_id & 0xFFFF);
}

// Decoding helpers
int window_editor_select_city_trade_route_decode_route_id(int encoded_value)
{
    return (encoded_value >> 16) & 0xFFFF;
}

int window_editor_select_city_trade_route_decode_resource_id(int encoded_value)
{
    return encoded_value & 0xFFFF;
}

const uint8_t *window_editor_select_city_trade_route_show_get_selected_name(int encoded_value)
{
    static uint8_t result_buffer[64];

    // Decode the composite value
    int trade_route_id = window_editor_select_city_trade_route_decode_route_id(encoded_value);
    int resource_id = window_editor_select_city_trade_route_decode_resource_id(encoded_value);

    // Handle special 'All' cases
    if (resource_id == RESOURCE_ALL_BUYS) {
        return translation_for(TR_EDITOR_ALL_BUYS);
    } else if (resource_id == RESOURCE_ALL_SELLS) {
        return translation_for(TR_EDITOR_ALL_SELLS);
    }

    // Get the resource data
    resource_type r = (resource_type) resource_id;
    const resource_data *r_data = resource_get_data(r);

    if (!r_data) {
        return (const uint8_t *) "UNKNOWN";
    }

    // Determine if this resource is bought or sold by this trade route
    int city_id = empire_city_get_for_trade_route(trade_route_id);
    if (city_id == -1) {
        return r_data->text;
    }

    int buys = empire_city_buys_resource(city_id, r);
    int sells = empire_city_sells_resource(city_id, r);

    const uint8_t *suffix = NULL;
    if (buys) {
        suffix = translation_for(TR_EDITOR_BUYS);
    } else if (sells) {
        suffix = translation_for(TR_EDITOR_SELLS);
    }

    // Combine resource name with suffix
    if (suffix) {
        snprintf((char *) result_buffer, sizeof(result_buffer), "%s %s", r_data->text, suffix);
        return result_buffer;
    } else {
        return r_data->text;
    }
}
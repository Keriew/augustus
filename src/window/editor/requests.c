#include "requests.h"

#include "core/image_group_editor.h"
#include "core/string.h"
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
#include "scenario/data.h"
#include "scenario/editor.h"
#include "scenario/property.h"
#include "scenario/request.h"
#include "window/editor/attributes.h"
#include "window/editor/edit_request.h"
#include "window/editor/map.h"

static void button_edit_request(const grid_box_item *item);
static void button_new_request(const generic_button *button);
static void draw_request_button(const grid_box_item *item);

static struct {
    const scenario_request **requests;
    unsigned int total_requests;
    unsigned int requests_in_use;
    unsigned int new_request_button_focused;
    void (*on_select)(int);
} data;

static generic_button new_request_button = {
    195, 350, 250, 25, button_new_request
};

static grid_box_type request_buttons = {
    .x = 10,
    .y = 65,
    .width = 38 * BLOCK_SIZE,
    .height = 19 * BLOCK_SIZE,
    .num_columns = 1,
    .item_height = 28,
    .item_margin.horizontal = 10,
    .item_margin.vertical = 4,
    .extend_to_hidden_scrollbar = 1,
    .on_click = button_edit_request,
    .draw_item = draw_request_button
};

static void limit_and_sort_list(void)
{
    data.requests_in_use = 0;
    for (unsigned int i = 0; i < data.total_requests; i++) {
        const scenario_request *request = scenario_request_get(i);
        if (request->resource == RESOURCE_NONE) {
            continue;
        }
        data.requests[data.requests_in_use] = request;
        data.requests_in_use++;
    }
    for (unsigned int i = 0; i < data.requests_in_use; i++) {
        for (unsigned int j = data.requests_in_use - 1; j > 0; j--) {
            const scenario_request *current = data.requests[j];
            const scenario_request *prev = data.requests[j - 1];
            if (current->resource && (!prev->resource || prev->year > current->year)) {
                const scenario_request *tmp = data.requests[j];
                data.requests[j] = data.requests[j - 1];
                data.requests[j - 1] = tmp;
            }
        }
    }
}

static void update_request_list(void)
{
    int current_requests = scenario_request_count_total();
    if (current_requests != data.total_requests) {
        free(data.requests);
        data.requests = 0;
        if (current_requests) {
            data.requests = malloc(current_requests * sizeof(scenario_request *));
            if (!data.requests) {
                grid_box_update_total_items(&request_buttons, 0);
                data.total_requests = 0;
                data.requests_in_use = 0;
                return;
            }
        }
        data.total_requests = current_requests;
    }
    limit_and_sort_list();
    grid_box_update_total_items(&request_buttons, data.requests_in_use);
}

static void draw_background(void)
{
    update_request_list();

    window_editor_map_draw_all();

    graphics_in_dialog();

    outer_panel_draw(0, 0, 40, 30);
    lang_text_draw(44, 14, 20, 12, FONT_LARGE_BLACK);

    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_REQUEST_DATE, 15, 50, 120, FONT_SMALL_PLAIN); // Request date:
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_REQUEST_RESOURCE, 150, 50, 70, FONT_SMALL_PLAIN); // Resource:
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_REQUEST_AMOUNT, 235, 50, 80, FONT_SMALL_PLAIN); // Amount:
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_REQUEST_DEADLINE, 310, 50, 70, FONT_SMALL_PLAIN); // Deadline:
    lang_text_draw_centered(53, 4, 390, 50, 90, FONT_SMALL_PLAIN); // Favor
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_REPEAT_FREQUENCY2, 465, 30, 120, FONT_SMALL_PLAIN); // Repeat frequency
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_REPEAT_TIMES2, 480, 50, 45, FONT_SMALL_PLAIN); // Times:
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_REPEAT_FREQUENCY_YEARS2, 525, 50, 60, FONT_SMALL_PLAIN); // Years:

    if (!data.requests_in_use) {
        lang_text_draw_centered(44, 19, 0, 165, 640, FONT_LARGE_BLACK);
    }

    if (!data.on_select) {
        lang_text_draw_centered(13, 3, 0, 456, 640, FONT_NORMAL_BLACK);
        lang_text_draw_multiline(152, 1, 20, 380, 600, FONT_NORMAL_BLACK);
        lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_NEW_REQUEST, new_request_button.x + 8,
            new_request_button.y + 8, new_request_button.width - 16, FONT_NORMAL_BLACK);
    }

    graphics_reset_dialog();

    grid_box_request_refresh(&request_buttons);
}

static void draw_request_button(const grid_box_item *item)
{
    button_border_draw(item->x, item->y, item->width, item->height, item->is_focused);
    const scenario_request *request = data.requests[item->index];
    text_draw_number(request->year, '+', " ", item->x + 5, item->y + 7, FONT_NORMAL_BLACK, 0);
    lang_text_draw_year(scenario_property_start_year() + request->year, item->x + 40, item->y + 7, FONT_NORMAL_BLACK);

    int image_id = resource_get_data(request->resource)->image.editor.icon;
    const image *img = image_get(image_id);
    int base_width = (25 - img->original.width) / 2;   //centering resource icon
    int base_height = (item->height - img->original.height) / 2;
    image_draw(image_id, 150 + base_width, item->y + base_height, COLOR_MASK_NONE, SCALE_NONE);
    text_draw(resource_get_data(request->resource)->text, 180, item->y + 7, FONT_SMALL_PLAIN, 0);

    int width = text_draw_number(request->amount.min, '@', " ", 250, item->y + 7, FONT_NORMAL_BLACK, 0);
    if (request->amount.max > request->amount.min) {
        width += text_draw(string_from_ascii("-"), 242 + width, item->y + 7, FONT_NORMAL_BLACK, 0);
        width += text_draw_number(request->amount.max, '@', " ", 235 + width, item->y + 7, FONT_NORMAL_BLACK, 0);
    }

    text_draw_number(request->deadline_years, '@', " ", 335, item->y + 7, FONT_NORMAL_BLACK, 0);

    text_draw_number(request->extension_months_to_comply, '@', " ", 365, item->y + 7, FONT_NORMAL_RED, COLOR_MASK_NONE);
    text_draw_number(request->favor, '@', " ", 395, item->y + 7, FONT_NORMAL_BLACK, 0);
    text_draw_number(request->extension_disfavor, '-', " ", 425, item->y + 7, FONT_NORMAL_BLACK, 0);
    text_draw_number(request->ignored_disfavor, '-', " ", 455, item->y + 7, FONT_NORMAL_BLACK, 0);

    if (request->repeat.times == REQUESTS_REPEAT_INFINITE) {
        text_draw(string_from_ascii("INF"), 493, item->y + 7, FONT_SMALL_PLAIN, 0);
    } else if (request->repeat.times == 0) {
        text_draw(string_from_ascii("-"), 500, item->y + 7, FONT_NORMAL_BLACK, 0);
    } else {
        int width2 = text_get_number_width(request->repeat.times, '@', " ", FONT_NORMAL_BLACK);
        text_draw_number(request->repeat.times, '@', " ", 500 - (width2 / 2), item->y + 7, FONT_NORMAL_BLACK, 0);
    }

    int width3 = text_get_number_width(request->repeat.interval.min, '@', " ", FONT_NORMAL_BLACK);
    if (request->repeat.times == 0) {
        text_draw(string_from_ascii(" "), 525, item->y + 7, FONT_NORMAL_BLACK, 0);
    } else if (request->repeat.times > 0 || request->repeat.times == REQUESTS_REPEAT_INFINITE) {
        if (request->repeat.interval.max == request->repeat.interval.min) {
            text_draw_number(request->repeat.interval.min, '@', " ", 550 - (width3 / 2), item->y + 7, FONT_NORMAL_BLACK, 0);
        } else if (request->repeat.interval.max > request->repeat.interval.min) {
            text_draw_number(request->repeat.interval.min, '@', " ", 547 - width3, item->y + 7, FONT_NORMAL_BLACK, 0);
            text_draw(string_from_ascii("-"), 550, item->y + 7, FONT_NORMAL_BLACK, 0);
            text_draw_number(request->repeat.interval.max, '@', " ", 553, item->y + 7, FONT_NORMAL_BLACK, 0);
        }
    }

}

static void draw_foreground(void)
{
    graphics_in_dialog();

    if (data.requests_in_use) {
        grid_box_draw(&request_buttons);
    }

    if (!data.on_select) {
        button_border_draw(new_request_button.x, new_request_button.y, new_request_button.width, new_request_button.height,
            data.new_request_button_focused);
    }

    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);
    if (grid_box_handle_input(&request_buttons, m_dialog, 1)) {
        return;
    }
    if (!data.on_select &&
        generic_buttons_handle_mouse(m_dialog, 0, 0, &new_request_button, 1, &data.new_request_button_focused)) {
        return;
    }
    if (input_go_back_requested(m, h)) {
        window_go_back();
    }
}

static void button_edit_request(const grid_box_item *item)
{
    if (!data.on_select) {
        window_editor_edit_request_show(data.requests[item->index]->id);
        return;
    }
    if (data.requests[item->index]->resource == RESOURCE_NONE) {
        return;
    }
    data.on_select(data.requests[item->index]->id);
    window_go_back();
}

static void button_new_request(const generic_button *button)
{
    if (data.on_select) {
        return;
    }
    int new_request_id = scenario_request_new();
    if (new_request_id >= 0) {
        window_editor_edit_request_show(new_request_id);
    }
}

static void show_window(void (*on_select)(int))
{
    window_type window = {
        WINDOW_EDITOR_REQUESTS,
        draw_background,
        draw_foreground,
        handle_input
    };
    data.on_select = on_select;
    grid_box_init(&request_buttons, scenario_request_count_active());
    window_show(&window);
}

void window_editor_requests_show(void)
{
    show_window(0);
}

void window_editor_requests_show_with_callback(void (*on_select_callback)(int))
{
    show_window(on_select_callback);
}

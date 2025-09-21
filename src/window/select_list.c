/**
 * @file select_list.c
 * @brief Dropdown widget supporting up to 40 items with auto positioning
 *
 * USAGE:
 * - window_select_list_show() - translation groups
 * - window_select_list_show_text() - text arrays
 *
 * EXAMPLE:
 * static void callback(int selected_index) { handle_selection(selected_index); }
 * window_select_list_show_text(x, y, button, options, count, callback);
 *
 * FEATURES: Auto positioning, 2-column layout for >20 items, mouse/keyboard support
 */

#include "select_list.h"

#include "graphics/button.h"
#include "graphics/color.h"
#include "graphics/graphics.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"

#define MAX_ITEMS_PER_LIST 20
#define BASE_LIST_WIDTH 200
#define MAX_LIST_WIDTH 496

enum {
    MODE_TEXT,
    MODE_GROUP,
};

static void button_select_item(const generic_button *button);
static void draw_item(int item_id, int x, int y, int selected);

static generic_button buttons_list1[MAX_ITEMS_PER_LIST] = {
    {5, 8, 190, 18, button_select_item, 0, 0},
    {5, 28, 190, 18, button_select_item, 0, 1},
    {5, 48, 190, 18, button_select_item, 0, 2},
    {5, 68, 190, 18, button_select_item, 0, 3},
    {5, 88, 190, 18, button_select_item, 0, 4},
    {5, 108, 190, 18, button_select_item, 0, 5},
    {5, 128, 190, 18, button_select_item, 0, 6},
    {5, 148, 190, 18, button_select_item, 0, 7},
    {5, 168, 190, 18, button_select_item, 0, 8},
    {5, 188, 190, 18, button_select_item, 0, 9},
    {5, 208, 190, 18, button_select_item, 0, 10},
    {5, 228, 190, 18, button_select_item, 0, 11},
    {5, 248, 190, 18, button_select_item, 0, 12},
    {5, 268, 190, 18, button_select_item, 0, 13},
    {5, 288, 190, 18, button_select_item, 0, 14},
    {5, 308, 190, 18, button_select_item, 0, 15},
    {5, 328, 190, 18, button_select_item, 0, 16},
    {5, 348, 190, 18, button_select_item, 0, 17},
    {5, 368, 190, 18, button_select_item, 0, 18},
    {5, 388, 190, 18, button_select_item, 0, 19},
};

static generic_button buttons_list2[MAX_ITEMS_PER_LIST] = {
    {205, 8, 190, 18, button_select_item, 0, 0, 1},
    {205, 28, 190, 18, button_select_item, 0, 1, 1},
    {205, 48, 190, 18, button_select_item, 0, 2, 1},
    {205, 68, 190, 18, button_select_item, 0, 3, 1},
    {205, 88, 190, 18, button_select_item, 0, 4, 1},
    {205, 108, 190, 18, button_select_item, 0, 5, 1},
    {205, 128, 190, 18, button_select_item, 0, 6, 1},
    {205, 148, 190, 18, button_select_item, 0, 7, 1},
    {205, 168, 190, 18, button_select_item, 0, 8, 1},
    {205, 188, 190, 18, button_select_item, 0, 9, 1},
    {205, 208, 190, 18, button_select_item, 0, 10, 1},
    {205, 228, 190, 18, button_select_item, 0, 11, 1},
    {205, 248, 190, 18, button_select_item, 0, 12, 1},
    {205, 268, 190, 18, button_select_item, 0, 13, 1},
    {205, 288, 190, 18, button_select_item, 0, 14, 1},
    {205, 308, 190, 18, button_select_item, 0, 15, 1},
    {205, 328, 190, 18, button_select_item, 0, 16, 1},
    {205, 348, 190, 18, button_select_item, 0, 17, 1},
    {205, 368, 190, 18, button_select_item, 0, 18, 1},
    {205, 388, 190, 18, button_select_item, 0, 19, 1},
};

static struct {
    int x;
    int y;
    int mode;
    int group;
    const uint8_t **items;
    unsigned int num_items;
    int width;
    void (*callback)(int);
    unsigned int focus_button_id;
    select_list_style style;
    int item_height;
    int item_v_spacing;
} data;

static unsigned int items_in_first_list(void)
{
    return data.num_items / 2 + data.num_items % 2;
}

static void determine_offsets(int x, int y, const generic_button *button)
{
    if (!button) {
        data.x = x;
        data.y = y;
        return;
    }

    data.x = x + button->x;
    data.y = y + button->y;

    int width;
    int height;
    if (data.num_items > MAX_ITEMS_PER_LIST) {
        width = 26 * BLOCK_SIZE;
        height = data.item_height * items_in_first_list() + (data.style == SELECT_LIST_STYLE_BUTTONS ? data.item_v_spacing * 2 : 24);
    } else {
        width = data.width + (data.style == SELECT_LIST_STYLE_BUTTONS ? 2 : BLOCK_SIZE - 1);
        height = data.item_height * data.num_items + (data.style == SELECT_LIST_STYLE_BUTTONS ? data.item_v_spacing * 2 : 24);
    }

    if (data.x + width > screen_width()) {
        data.x -= width - button->width;
    }

    if (data.y + button->height + height > screen_height()) {
        data.y -= height;
        if (data.y < 0) {
            data.y = screen_height() - height;
        }
    } else {
        data.y += button->height;
    }
}

static void init_group(int x, int y, const generic_button *button, int group, int num_items, void (*callback)(int), select_list_style style)
{
    data.mode = MODE_GROUP;
    data.group = group;
    data.width = BASE_LIST_WIDTH;
    data.num_items = num_items;
    data.callback = callback;
    data.style = style;

    // Set style-dependent values
    if (style == SELECT_LIST_STYLE_BUTTONS) {
        data.item_height = 24;
        data.item_v_spacing = 2;
    } else {
        data.item_height = 20;
        data.item_v_spacing = 0;
    }

    // Update button positions and sizes based on style
    for (int i = 0; i < MAX_ITEMS_PER_LIST; i++) {
        buttons_list1[i].width = data.width + (style == SELECT_LIST_STYLE_BUTTONS ? 2 : -10);
        buttons_list1[i].height = data.item_height;
        buttons_list1[i].y = data.item_v_spacing + data.item_height * i + (style == SELECT_LIST_STYLE_BUTTONS ? 0 : 8);
        buttons_list1[i].x = style == SELECT_LIST_STYLE_BUTTONS ? 0 : 5;

        buttons_list2[i].width = data.width + (style == SELECT_LIST_STYLE_BUTTONS ? 2 : -10);
        buttons_list2[i].height = data.item_height;
        buttons_list2[i].y = data.item_v_spacing + data.item_height * i + (style == SELECT_LIST_STYLE_BUTTONS ? 0 : 8);
        buttons_list2[i].x = style == SELECT_LIST_STYLE_BUTTONS ? 200 : 205;
    }

    determine_offsets(x, y, button);
}

static void init_text(int x, int y, const generic_button *button, const uint8_t **items, int num_items,
    void (*callback)(int), select_list_style style)
{
    data.mode = MODE_TEXT;
    data.items = items;
    data.num_items = num_items;
    data.callback = callback;
    data.style = style;
    data.width = BASE_LIST_WIDTH;

    // Set style-dependent values
    if (style == SELECT_LIST_STYLE_BUTTONS) {
        data.item_height = 24;
        data.item_v_spacing = 2;
    } else {
        data.item_height = 20;
        data.item_v_spacing = 0;
    }

    if (data.num_items <= MAX_ITEMS_PER_LIST) {
        for (int i = 0; i < num_items; i++) {
            int width = text_get_width(data.items[i], FONT_NORMAL_PLAIN) + 10;
            if (width > data.width) {
                data.width = width;
                data.width += BLOCK_SIZE - (data.width % BLOCK_SIZE);
                if (width > MAX_LIST_WIDTH) {
                    data.width = MAX_LIST_WIDTH;
                }
            }
        }
        // Update button positions and sizes based on style
        for (int i = 0; i < num_items; i++) {
            buttons_list1[i].width = data.width + (style == SELECT_LIST_STYLE_BUTTONS ? 2 : 0);
            buttons_list1[i].height = data.item_height;
            buttons_list1[i].y = data.item_v_spacing + data.item_height * i + (style == SELECT_LIST_STYLE_BUTTONS ? 0 : 8);
            buttons_list1[i].x = style == SELECT_LIST_STYLE_BUTTONS ? 0 : 5;
        }
    } else {
        // Update button positions and sizes for multi-column layout
        for (int i = 0; i < MAX_ITEMS_PER_LIST; i++) {
            buttons_list1[i].width = data.width + (style == SELECT_LIST_STYLE_BUTTONS ? 2 : -10);
            buttons_list1[i].height = data.item_height;
            buttons_list1[i].y = data.item_v_spacing + data.item_height * i + (style == SELECT_LIST_STYLE_BUTTONS ? 0 : 8);
            buttons_list1[i].x = style == SELECT_LIST_STYLE_BUTTONS ? 0 : 5;

            buttons_list2[i].width = data.width + (style == SELECT_LIST_STYLE_BUTTONS ? 2 : -10);
            buttons_list2[i].height = data.item_height;
            buttons_list2[i].y = data.item_v_spacing + data.item_height * i + (style == SELECT_LIST_STYLE_BUTTONS ? 0 : 8);
            buttons_list2[i].x = style == SELECT_LIST_STYLE_BUTTONS ? 200 : 205;
        }
    }
    determine_offsets(x, y, button);
}

// Style-specific drawing functions
static void draw_item_default(int item_id, int x, int y, int selected)
{
    color_t color = selected ? COLOR_FONT_BLUE : COLOR_BLACK;
    if (data.mode == MODE_GROUP) {
        lang_text_draw_centered_colored(data.group, item_id, data.x + x, data.y + y, data.width,
            FONT_NORMAL_PLAIN, color);
    } else {
        if (data.width == BASE_LIST_WIDTH) {
            text_draw_centered(data.items[item_id], data.x + x, data.y + y, BASE_LIST_WIDTH, FONT_NORMAL_PLAIN, color);
        } else {
            text_draw_ellipsized(data.items[item_id], data.x + x + 5, data.y + y,
                data.width - 10, FONT_NORMAL_PLAIN, color);
        }
    }
}

static void draw_item_buttons(int item_id, int x, int y, int selected)
{
    // Draw individual button borders with gaps (spacing)
    int text_y_offset = 6; // Center text vertically within button
    int text_x_offset = 0; // No horizontal offset
    x += data.x;
    y += data.y;
    // Draw button border
    graphics_set_clip_rectangle(x, y - 2, data.width + 2, data.item_height + 4);
    int height_blocks = data.item_height / BLOCK_SIZE;
    unbordered_panel_draw(x, y, data.width / BLOCK_SIZE + 1, height_blocks + 1);
    graphics_reset_clip_rectangle();
    button_border_draw(x - 1, y, data.width + 2, data.item_height, selected);

    color_t color = COLOR_MASK_NONE;
    if (data.mode == MODE_GROUP) {
        lang_text_draw_centered_colored(data.group, item_id, x + text_x_offset, y + text_y_offset, data.width,
            FONT_NORMAL_BLACK, color);
    } else {
        if (data.width == BASE_LIST_WIDTH) {
            text_draw_centered(data.items[item_id], x + text_x_offset, y + text_y_offset, BASE_LIST_WIDTH, FONT_NORMAL_BLACK, color);
        } else {
            text_draw_ellipsized(data.items[item_id], x + text_x_offset, y + text_y_offset,
                data.width - 10, FONT_NORMAL_BLACK, color);
        }
    }
}

static void draw_foreground_default(void)
{
    if (data.num_items > MAX_ITEMS_PER_LIST) {
        unsigned int max_first = items_in_first_list();
        outer_panel_draw(data.x, data.y, 26, (data.item_height * max_first + 24) / BLOCK_SIZE);
        for (unsigned int i = 0; i < max_first; i++) {
            draw_item(i, 5, 11 + data.item_height * i, i + 1 == data.focus_button_id);
        }
        for (unsigned int i = 0; i < data.num_items - max_first; i++) {
            draw_item(i + max_first, 205, 11 + data.item_height * i, MAX_ITEMS_PER_LIST + i + 1 == data.focus_button_id);
        }
    } else {
        int width_blocks = (data.width + BLOCK_SIZE - 1) / BLOCK_SIZE;
        outer_panel_draw(data.x, data.y, width_blocks, (data.item_height * data.num_items + 24) / BLOCK_SIZE);
        for (unsigned int i = 0; i < data.num_items; i++) {
            draw_item(i, 5, 11 + data.item_height * i, i + 1 == data.focus_button_id);
        }
    }
}

static void draw_foreground_buttons(void)
{
    if (data.num_items > MAX_ITEMS_PER_LIST) {
        unsigned int max_first = items_in_first_list();
        for (unsigned int i = 0; i < max_first; i++) {
            draw_item(i, 0, data.item_v_spacing + data.item_height * i, i + 1 == data.focus_button_id);
        }
        for (unsigned int i = 0; i < data.num_items - max_first; i++) {
            int highlight = MAX_ITEMS_PER_LIST + i + 1 == data.focus_button_id;
            draw_item(i + max_first, 200, data.item_v_spacing + data.item_height * i, highlight);
        }
    } else {
        for (unsigned int i = 0; i < data.num_items; i++) {
            draw_item(i, 0, data.item_v_spacing + data.item_height * i, i + 1 == data.focus_button_id);
        }
    }
}

// Style dispatcher functions
static void draw_item(int item_id, int x, int y, int selected)
{
    switch (data.style) {
        case SELECT_LIST_STYLE_BUTTONS:
            draw_item_buttons(item_id, x, y, selected);
            break;
        case SELECT_LIST_STYLE_DEFAULT:
        default:
            draw_item_default(item_id, x, y, selected);
            break;
    }
}

static void draw_foreground(void)
{
    switch (data.style) {
        case SELECT_LIST_STYLE_BUTTONS:
            draw_foreground_buttons();
            break;
        case SELECT_LIST_STYLE_DEFAULT:
        default:
            draw_foreground_default();
            break;
    }
}

static int click_outside_window(const mouse *m)
{
    int width;
    int height;
    if (data.num_items > MAX_ITEMS_PER_LIST) {
        width = 26 * BLOCK_SIZE;
        height = data.item_height * items_in_first_list() + (data.style == SELECT_LIST_STYLE_BUTTONS ? data.item_v_spacing * 2 : 24);
    } else {
        width = data.width + (data.style == SELECT_LIST_STYLE_BUTTONS ? 2 : BLOCK_SIZE - 1);
        height = data.item_height * data.num_items + (data.style == SELECT_LIST_STYLE_BUTTONS ? data.item_v_spacing * 2 : 24);
    }
    return m->left.went_up && (m->x < data.x || m->x >= data.x + width || m->y < data.y || m->y >= data.y + height);
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    if (data.num_items > MAX_ITEMS_PER_LIST) {
        unsigned int items_first = items_in_first_list();
        if (generic_buttons_handle_mouse(m, data.x, data.y, buttons_list1, items_first, &data.focus_button_id)) {
            return;
        }
        unsigned int second_id = 0;
        generic_buttons_handle_mouse(m, data.x, data.y, buttons_list2, data.num_items - items_first, &second_id);
        if (second_id > 0) {
            data.focus_button_id = second_id + MAX_ITEMS_PER_LIST;
        }
    } else {
        if (generic_buttons_handle_mouse(m, data.x, data.y, buttons_list1, data.num_items, &data.focus_button_id)) {
            return;
        }
    }
    if (input_go_back_requested(m, h) || click_outside_window(m)) {
        window_go_back();
    }
}

void button_select_item(const generic_button *button)
{
    int id = button->parameter1;
    int list_id = button->parameter2;

    window_go_back();
    if (list_id == 0) {
        data.callback(id);
    } else {
        data.callback(id + items_in_first_list());
    }
}

void window_select_list_show(int x, int y, const generic_button *button, int group, int num_items,
    void (*callback)(int))
{
    window_select_list_show_styled(x, y, button, group, num_items, callback, SELECT_LIST_STYLE_DEFAULT);
}

void window_select_list_show_styled(int x, int y, const generic_button *button, int group, int num_items,
    void (*callback)(int), select_list_style style)
{
    window_type window = {
        WINDOW_SELECT_LIST,
        window_draw_underlying_window,
        draw_foreground,
        handle_input
    };
    init_group(x, y, button, group, num_items, callback, style);
    window_show(&window);
}

void window_select_list_show_text(int x, int y, const generic_button *button, const uint8_t **items, int num_items,
    void (*callback)(int))
{
    window_select_list_show_text_styled(x, y, button, items, num_items, callback, SELECT_LIST_STYLE_DEFAULT);
}

void window_select_list_show_text_styled(int x, int y, const generic_button *button, const uint8_t **items, int num_items,
    void (*callback)(int), select_list_style style)
{
    window_type window = {
        WINDOW_SELECT_LIST,
        window_draw_underlying_window,
        draw_foreground,
        handle_input
    };
    init_text(x, y, button, items, num_items, callback, style);
    window_show(&window);
}

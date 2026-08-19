#include "compose_figure_category.h"

#include "core/string.h"
#include "figure/properties.h"
#include "graphics/graphics.h"
#include "graphics/grid_box.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "translation/translation.h"
#include "window/editor/map.h"

#define WINDOW_WIDTH 30
#define WINDOW_HEIGHT 20

#define CHECK_BOX_HEIGHT 20

static struct {
    void (*callback)(int);
    figure_category category;
} data;

static void item_click(const grid_box_item *item);
static void draw_item(const grid_box_item *item);
static void explanation_tooltip(const grid_box_item *item, tooltip_context *c);

static grid_box_type checkbox_buttons = {
    .x = BLOCK_SIZE,
    .y = BLOCK_SIZE,
    .width = (WINDOW_WIDTH - 2) * BLOCK_SIZE ,
    .height = (WINDOW_HEIGHT - 2) * BLOCK_SIZE,
    .item_height = CHECK_BOX_HEIGHT + 8,
    .num_columns = 2,
    .item_margin.horizontal = 8,
    .item_margin.vertical = 2,
    .extend_to_hidden_scrollbar = 1,
    .on_click = item_click,
    .draw_item = draw_item,
    .handle_tooltip = explanation_tooltip
};

static void init(void (*callback)(int), int category)
{
    data.callback = callback;
    data.category = category;
    grid_box_init(&checkbox_buttons, FIGURE_MAX_CATEGORIES);
}

static void item_click(const grid_box_item *item)
{
    int category_value = 1 << item->index;
    data.category ^= category_value;

    window_request_refresh();
}

static void draw_item(const grid_box_item *item)
{
    button_border_draw(item->x, item->y + 4, CHECK_BOX_HEIGHT, CHECK_BOX_HEIGHT, item->is_focused);
    int is_checked = (data.category & 1 << item->index);
    if (is_checked) {
        text_draw(string_from_ascii("x"), item->x + 6, item->y + 8, FONT_NORMAL_BLACK, 0);
    }
    lang_text_draw(CUSTOM_TRANSLATION, TR_PARAMETER_FIGURE_CATEGORY_INACTIVE + item->index, item->x + 24, item->y + 8,
        FONT_NORMAL_BLACK);
}

static void draw_background(void)
{
    window_editor_map_draw_all();

    graphics_in_dialog_with_size(WINDOW_WIDTH * BLOCK_SIZE, WINDOW_HEIGHT * BLOCK_SIZE);

    outer_panel_draw(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    graphics_reset_dialog();

    grid_box_request_refresh(&checkbox_buttons);
}

static void draw_foreground(void)
{
    graphics_in_dialog_with_size(WINDOW_WIDTH * BLOCK_SIZE, WINDOW_HEIGHT * BLOCK_SIZE);

    grid_box_draw(&checkbox_buttons);

    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);
    if (grid_box_handle_input(&checkbox_buttons, m_dialog, 1)) {
        return;
    }

    if (input_go_back_requested(m, h)) {
        data.callback(data.category);
        window_go_back();
    }
}

static void explanation_tooltip(const grid_box_item *item, tooltip_context *c)
{
    c->translation_key = TR_PARAMETER_TOOLTIP_FIGURE_CATEGORY_INACTIVE + item->index;
    c->type = TOOLTIP_BUTTON;
}

static void get_tooltip(tooltip_context *c)
{
    grid_box_handle_tooltip(&checkbox_buttons, c);
}

void window_editor_compose_figure_category_show(void (*callback)(int), int category)
{
    init(callback, category);
    window_type window = {
        WINDOW_EDITOR_COMPOSE_FIGURE_CATEGORY,
        draw_background,
        draw_foreground,
        handle_input,
        get_tooltip
    };
    window_show(&window);
}

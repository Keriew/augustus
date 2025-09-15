#include "model_data.h"

#include "building/model.h"
#include "building/type.h"
#include "core/lang.h"
#include "core/string.h"
#include "graphics/font.h"
#include "graphics/button.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/grid_box.h"
#include "graphics/lang_text.h"
#include "graphics/text.h"
#include "graphics/panel.h"
#include "graphics/window.h"
#include "input/input.h"
#include "translation/translation.h"
#include "window/editor/map.h"

static void button_edit_cost(const generic_button *button);
static void button_edit_value(const generic_button *button);
static void button_edit_step(const generic_button *button);
static void button_edit_step_size(const generic_button *button);
static void button_edit_range(const generic_button *button);
static void button_edit_laborers(const generic_button *button);

static void draw_model_item(const grid_box_item *item);
static void model_item_click(const grid_box_item *item);


static struct {
    unsigned int total_items;
} data = {
    BUILDING_TYPE_MAX
};

static generic_button data_buttons[] = {
    {200, 2, 48, 20, button_edit_cost},
    {260, 2, 48, 20, button_edit_cost},
    {315, 2, 48, 20, button_edit_cost},
    {370, 2, 48, 20, button_edit_cost},
    {425, 2, 48, 20, button_edit_cost},
    {480, 2, 48, 20, button_edit_cost}
};

static grid_box_type model_buttons = {
    .x = 25,
    .y = 88,
    .width = 36 * BLOCK_SIZE,
    .height = 20 * BLOCK_SIZE,
    .item_height = 28,
    .item_margin.horizontal = 8,
    .item_margin.vertical = 2,
    .extend_to_hidden_scrollbar = 1,
    .on_click = model_item_click,
    .draw_item = draw_model_item
};

static void init(void)
{
    grid_box_init(&model_buttons, data.total_items);
}

static void button_edit_cost(const generic_button *button)
{
    
}

static void button_edit_value(const generic_button *button)
{
    
}

static void button_edit_step(const generic_button *button)
{
    
}

static void button_edit_step_size(const generic_button *button)
{
    
}

static void button_edit_range(const generic_button *button)
{
    
}

static void button_edit_laborers(const generic_button *button)
{
    
}

static void model_item_click(const grid_box_item *item)
{
    
}

static void draw_model_item(const grid_box_item *item)
{
    button_border_draw(item->x, item->y, item->width, item->height, 0);
    text_draw(lang_get_building_type_string(item->index), item->x + 8, item->y + 8, FONT_NORMAL_BLACK, 0);
    
    for (int i = 0; i < 6; i++) {
        button_border_draw(item->x + data_buttons[i].x, item->y + data_buttons[i].y,
            data_buttons[i].width, data_buttons[i].height, item->is_focused);

        uint8_t data_string[8];
        int value = 0;
        switch (i) {
            case 0:
                value = model_get_building(item->index)->cost;break;
            case 1:
                value = model_get_building(item->index)->desirability_value;break;
            case 2:
                value = model_get_building(item->index)->desirability_step;break;
            case 3:
                value = model_get_building(item->index)->desirability_step_size;break;
            case 4:
                value = model_get_building(item->index)->desirability_range;break;
            case 5:
                value = model_get_building(item->index)->laborers;break;
        }
        string_from_int(data_string, value, 0);
        text_draw(data_string, item->x + data_buttons[i].x + 8, item->y + data_buttons[i].y + 6, 
                  FONT_SMALL_PLAIN, 0);
    }
    
}

static void draw_background(void)
{
    window_editor_map_draw_all();

    graphics_in_dialog();

    outer_panel_draw(16, 32, 38, 26);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_ACTION_TYPE_CHANGE_MODEL_DATA, 26, 42, 608, FONT_LARGE_BLACK);
    lang_text_draw_centered(13, 3, 16, 424, 608, FONT_NORMAL_BLACK);
    
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_PARAMETER_MODEL, 80, 75, 30, FONT_SMALL_PLAIN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_PARAMETER_COST, 200, 75, 30, FONT_SMALL_PLAIN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_MODEL_DATA_DES_VALUE, 265, 75, 30, FONT_SMALL_PLAIN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_MODEL_DATA_DES_STEP, 330, 75, 30, FONT_SMALL_PLAIN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_MODEL_DATA_DES_STEP_SIZE, 395, 75, 30, FONT_SMALL_PLAIN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_MODEL_DATA_DES_RANGE, 460, 75, 30, FONT_SMALL_PLAIN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_PARAMETER_LABORERS, 525, 75, 30, FONT_SMALL_PLAIN);

    graphics_reset_dialog();
    
    grid_box_request_refresh(&model_buttons);
}

static void draw_foreground(void)
{
    graphics_in_dialog();
    
    grid_box_draw(&model_buttons);
    
    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    if (grid_box_handle_input(&model_buttons, mouse_in_dialog(m), 1)) {
        return;
    }
    if (input_go_back_requested(m, h)) {
        window_go_back();
    }
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
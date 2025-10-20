#include "model_data.h"

#include "building/properties.h"
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
#include "window/numeric_input.h"

#define NO_SELECTION (unsigned int) -1
#define NUM_DATA_BUTTONS (sizeof(data_buttons) / sizeof(generic_button))

static void button_edit_cost(const generic_button *button);
static void button_edit_value(const generic_button *button);
static void button_edit_step(const generic_button *button);
static void button_edit_step_size(const generic_button *button);
static void button_edit_range(const generic_button *button);
static void button_edit_laborers(const generic_button *button);

static void button_static_click(const generic_button *button);

static void populate_list(void);
static void draw_model_item(const grid_box_item *item);
static void model_item_click(const grid_box_item *item);


static struct {
    unsigned int total_items;
    building_type items[BUILDING_TYPE_MAX];
    
    unsigned int data_buttons_focus_id;
    unsigned int static_buttons_focus_id;
    unsigned int target_index;
} data;

static generic_button data_buttons[] = {
    {200, 2, 48, 20, button_edit_cost},
    {260, 2, 48, 20, button_edit_value},
    {315, 2, 48, 20, button_edit_step},
    {370, 2, 48, 20, button_edit_step_size},
    {425, 2, 48, 20, button_edit_range},
    {480, 2, 48, 20, button_edit_laborers}
};

static generic_button static_buttons[] = {
    {40, 25 * BLOCK_SIZE, 10 * BLOCK_SIZE, 24, button_static_click, 0, 0},
    {216, 25 * BLOCK_SIZE, 10 * BLOCK_SIZE, 24, button_static_click, 0, 1},
    {392, 25 * BLOCK_SIZE, 10 * BLOCK_SIZE, 24, button_static_click, 0, 2}
};
#define NUM_STATIC_BUTTONS (sizeof(static_buttons) / sizeof(generic_button))

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
    data.target_index = NO_SELECTION;
    populate_list();
    grid_box_init(&model_buttons, data.total_items);
}

static void populate_list(void)
{
    data.total_items = 0;
    for (int i = 0; i < BUILDING_TYPE_MAX; i++) {
        const building_properties *props = building_properties_for_type(i);
        if ((props->size && props->event_data.attr) &&
            i != BUILDING_GRAND_GARDEN && i != BUILDING_DOLPHIN_FOUNTAIN ||
            i == BUILDING_CLEAR_LAND || i == BUILDING_REPAIR_LAND) {
            data.items[data.total_items++] = i;
        }
    }
}

static void button_static_click(const generic_button *button)
{
    switch (button->parameter1) {
        case 0:
            
            break;
        case 1:
            model_reset();
            window_request_refresh();
            break;
        case 2:
            
            break;
        default:
            break;
    }
}

static void set_cost_value(int value)
{
    model_building *model = model_get_building(data.items[data.target_index]);
    model->cost = value;
    data.target_index = NO_SELECTION;
}

static void button_edit_cost(const generic_button *button)
{
    window_numeric_input_bound_show(model_buttons.focused_item.x, model_buttons.focused_item.y, button,
        9, -1000000000, 1000000000, set_cost_value);
}

static void set_desirability_value(int value)
{
    model_building *model = model_get_building(data.items[data.target_index]);
    model->desirability_value = value;
    data.target_index = NO_SELECTION;
}

static void button_edit_value(const generic_button *button)
{
    window_numeric_input_bound_show(model_buttons.focused_item.x, model_buttons.focused_item.y, button,
        9, -1000000000, 1000000000, set_desirability_value);
}

static void set_desirability_step(int value)
{
    model_building *model = model_get_building(data.items[data.target_index]);
    model->desirability_step = value;
    data.target_index = NO_SELECTION;
}

static void button_edit_step(const generic_button *button)
{
    window_numeric_input_bound_show(model_buttons.focused_item.x, model_buttons.focused_item.y, button,
        9, -1000000000, 1000000000, set_desirability_step);
}

static void set_desirability_step_size(int value)
{
    model_building *model = model_get_building(data.items[data.target_index]);
    model->desirability_step_size = value;
    data.target_index = NO_SELECTION;
}

static void button_edit_step_size(const generic_button *button)
{
    window_numeric_input_bound_show(model_buttons.focused_item.x, model_buttons.focused_item.y, button,
        9, -1000000000, 1000000000, set_desirability_step_size);
}

static void set_desirability_range(int value)
{
    model_building *model = model_get_building(data.items[data.target_index]);
    model->desirability_range = value;
    data.target_index = NO_SELECTION;
}

static void button_edit_range(const generic_button *button)
{
    window_numeric_input_bound_show(model_buttons.focused_item.x, model_buttons.focused_item.y, button,
        9, -1000000000, 1000000000, set_desirability_range);
}

static void set_laborers(int value)
{
    model_building *model = model_get_building(data.items[data.target_index]);
    model->laborers = value;
    data.target_index = NO_SELECTION;
}

static void button_edit_laborers(const generic_button *button)
{
    window_numeric_input_bound_show(model_buttons.focused_item.x, model_buttons.focused_item.y, button,
        9, -1000000000, 1000000000, set_laborers);
}

static void model_item_click(const grid_box_item *item)
{
    data.target_index = item->index;
}

static void draw_model_item(const grid_box_item *item)
{
    button_border_draw(item->x, item->y, item->width, item->height, 0);
    text_draw(lang_get_building_type_string(data.items[item->index]), item->x + 8, item->y + 8, FONT_NORMAL_BLACK, 0);
    
    for (int i = 0; i < 6; i++) {
        button_border_draw(item->x + data_buttons[i].x, item->y + data_buttons[i].y,
            data_buttons[i].width, data_buttons[i].height, item->is_focused && data.data_buttons_focus_id == i+1);

        uint8_t data_string[8];
        int value = 0;
        switch (i) {
            case 0:
                value = model_get_building(data.items[item->index])->cost;break;
            case 1:
                value = model_get_building(data.items[item->index])->desirability_value;break;
            case 2:
                value = model_get_building(data.items[item->index])->desirability_step;break;
            case 3:
                value = model_get_building(data.items[item->index])->desirability_step_size;break;
            case 4:
                value = model_get_building(data.items[item->index])->desirability_range;break;
            case 5:
                value = model_get_building(data.items[item->index])->laborers;break;
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

    outer_panel_draw(16, 32, 38, 27);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_ACTION_TYPE_CHANGE_MODEL_DATA, 26, 42, 38 * BLOCK_SIZE, FONT_LARGE_BLACK);
    lang_text_draw_centered(13, 3, 16, 27 * BLOCK_SIZE + 8, 38 * BLOCK_SIZE, FONT_NORMAL_BLACK);
    
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_PARAMETER_MODEL, 80, 75, 30, FONT_SMALL_PLAIN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_PARAMETER_COST, 235, 75, 30, FONT_SMALL_PLAIN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_MODEL_DATA_DES_VALUE, 295, 75, 30, FONT_SMALL_PLAIN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_MODEL_DATA_DES_STEP, 350, 75, 30, FONT_SMALL_PLAIN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_MODEL_DATA_DES_STEP_SIZE, 405, 75, 30, FONT_SMALL_PLAIN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_MODEL_DATA_DES_RANGE, 460, 75, 30, FONT_SMALL_PLAIN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_PARAMETER_LABORERS, 505, 75, 30, FONT_SMALL_PLAIN);

    graphics_reset_dialog();
    
    grid_box_request_refresh(&model_buttons);
}

static void draw_foreground(void)
{
    graphics_in_dialog();
    
    for (int i = 0; i < NUM_STATIC_BUTTONS; i++) {
        button_border_draw(static_buttons[i].x, static_buttons[i].y,
            static_buttons[i].width, static_buttons[i].height, data.static_buttons_focus_id == i+1);
        translation_key key;
        switch (i) {
            case 0:
                key = TR_EDITOR_SCENARIO_EVENTS_EXPORT;
                break;
            case 1:
                key = TR_BUTTON_RESET_DEFAULTS;
                break;
            case 2:
                key = TR_EDITOR_SCENARIO_EVENTS_IMPORT;
                break;
        }
        lang_text_draw_centered(CUSTOM_TRANSLATION, key, 
            static_buttons[i].x, static_buttons[i].y + 6, static_buttons[i].width, FONT_NORMAL_BLACK);
    }
    
    grid_box_draw(&model_buttons);
    
    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);
    if (generic_buttons_handle_mouse(m_dialog, 0, 0, static_buttons, NUM_STATIC_BUTTONS, &data.static_buttons_focus_id)) {
        return;
    }
    grid_box_handle_input(&model_buttons, m_dialog, 1);

    int x = 0, y = 0;
    if (model_buttons.focused_item.is_focused) {
        x = model_buttons.focused_item.x;
        y = model_buttons.focused_item.y;
    }
    if (generic_buttons_handle_mouse(m_dialog, x, y, data_buttons, NUM_DATA_BUTTONS, &data.data_buttons_focus_id)) {
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
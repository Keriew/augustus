#include "model_data.h"

#include "building/industry.h"
#include "building/properties.h"
#include "building/type.h"
#include "core/lang.h"
#include "core/string.h"
#include "game/resource.h"
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
#include "window/file_dialog.h"
#include "window/numeric_input.h"

#include <stdio.h>

#define NO_SELECTION (unsigned int) -1
#define NUM_DATA_BUTTONS (sizeof(data_buttons) / sizeof(generic_button))

static void button_edit_cost(const generic_button *button);
static void button_edit_value(const generic_button *button);
static void button_edit_step(const generic_button *button);
static void button_edit_step_size(const generic_button *button);
static void button_edit_range(const generic_button *button);
static void button_edit_laborers(const generic_button *button);
static void button_edit_production(const generic_button *button);

static void button_static_click(const generic_button *button);

static void populate_list(void);
static void draw_model_item(const grid_box_item *item);
static void model_item_click(const grid_box_item *item);

static void building_tooltip(const grid_box_item *item, tooltip_context *c);


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
    {480, 2, 48, 20, button_edit_laborers},
    {535, 2, 48, 20, button_edit_production}
};
#define MAX_DATA_BUTTONS (sizeof(data_buttons) / sizeof(generic_button))

static generic_button static_buttons[] = {
    {28, 25 * BLOCK_SIZE, 12 * BLOCK_SIZE, 24, button_static_click, 0, 0},
    {232, 25 * BLOCK_SIZE, 12 * BLOCK_SIZE, 24, button_static_click, 0, 1},
    {436, 25 * BLOCK_SIZE, 12 * BLOCK_SIZE, 24, button_static_click, 0, 2}
};
#define NUM_STATIC_BUTTONS (sizeof(static_buttons) / sizeof(generic_button))

static grid_box_type model_buttons = {
    .x = 25,
    .y = 88,
    .width = 40 * BLOCK_SIZE ,
    .height = 20 * BLOCK_SIZE,
    .item_height = 28,
    .item_margin.horizontal = 8,
    .item_margin.vertical = 2,
    .extend_to_hidden_scrollbar = 1,
    .on_click = model_item_click,
    .draw_item = draw_model_item,
    .handle_tooltip = building_tooltip
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
            window_file_dialog_show(FILE_TYPE_MODEL_DATA, FILE_DIALOG_SAVE);
            break;
        case 1:
            model_reset();
            resource_init();
            window_request_refresh();
            break;
        case 2:
            window_file_dialog_show(FILE_TYPE_MODEL_DATA, FILE_DIALOG_LOAD);
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

static void set_production(int value)
{
    resource_data *resource = resource_get_data(resource_get_from_industry(data.items[data.target_index]));
    resource->production_per_month = value;
    data.target_index = NO_SELECTION;
}

static void button_edit_production(const generic_button *button)
{
    building_type type = data.items[data.target_index];
    if (building_is_raw_resource_producer(type) || building_is_workshop(type) || type == BUILDING_WHARF || building_is_farm(type)) {
        window_numeric_input_bound_show(model_buttons.focused_item.x, model_buttons.focused_item.y, button,
            9, -1000000000, 1000000000, set_production);
    }
}

static void model_item_click(const grid_box_item *item)
{
    data.target_index = item->index;
}

static void draw_model_item(const grid_box_item *item)
{
    button_border_draw(item->x, item->y, item->width, item->height, 0);
    text_draw_ellipsized(lang_get_building_type_string(data.items[item->index]), item->x + 8, item->y + 8, 12 * BLOCK_SIZE, FONT_NORMAL_BLACK, 0);
    
    for (int i = 0; i < MAX_DATA_BUTTONS - (!(building_is_raw_resource_producer(data.items[item->index]) || 
        building_is_workshop(data.items[item->index]) || data.items[item->index] == BUILDING_WHARF || building_is_farm(data.items[item->index]))); i++) {
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
            case 6:
                value = resource_get_data(resource_get_from_industry(data.items[item->index]))->production_per_month;
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

    outer_panel_draw(16, 32, 42, 27);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_ACTION_TYPE_CHANGE_MODEL_DATA, 26, 42, 38 * BLOCK_SIZE, FONT_LARGE_BLACK);
    lang_text_draw_centered(13, 3, 16, 27 * BLOCK_SIZE + 8, 42 * BLOCK_SIZE, FONT_NORMAL_BLACK);
    
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_PARAMETER_MODEL, 80, 75, 30, FONT_SMALL_PLAIN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_PARAMETER_COST, 235, 75, 30, FONT_SMALL_PLAIN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_MODEL_DATA_DES_VALUE, 295, 75, 30, FONT_SMALL_PLAIN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_MODEL_DATA_DES_STEP, 350, 75, 30, FONT_SMALL_PLAIN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_MODEL_DATA_DES_STEP_SIZE, 405, 75, 30, FONT_SMALL_PLAIN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_MODEL_DATA_DES_RANGE, 460, 75, 30, FONT_SMALL_PLAIN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_PARAMETER_LABORERS, 505, 75, 30, FONT_SMALL_PLAIN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_MODEL_PRODUCTION, 570, 75, 30, FONT_SMALL_PLAIN);

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

static int desirability_tooltip(tooltip_context *c)
{
    const mouse *m_global = mouse_get();
    const mouse *m = mouse_in_dialog(m_global);

    for (int i = 0; i < 4; i++) {
        const uint8_t *text = translation_for(TR_EDITOR_MODEL_DATA_DES_VALUE + i);
        int width = text_get_width(text, FONT_SMALL_PLAIN);
        int x;

        switch (i) {
            case 0:
                x = 295;
                break;
            case 1:
                x = 350;
                break;
            case 2:
                x = 405;
                break;
            case 3:
                x = 460;
                break;
        }
        
        if (x <= m->x && x + width > m->x &&
            75 <= m->y && 75 + 10 > m->y) {
            c->text_group = CUSTOM_TRANSLATION;
            c->text_id = TR_EDITOR_DESIRABILITY_VALUE + i;
            c->type = TOOLTIP_BUTTON;
            return 1;
        }
    }
    return 0;
}

static void building_tooltip(const grid_box_item *item, tooltip_context *c)
{
    uint8_t *text;
    text = (uint8_t *)lang_get_building_type_string(data.items[item->index]);
    if (text_get_width(text, FONT_SMALL_PLAIN) > 12 * BLOCK_SIZE - 32 && !data.data_buttons_focus_id) {
        c->precomposed_text = text;
        c->type = TOOLTIP_BUTTON;
    }
}

static void get_tooltip(tooltip_context *c)
{
    if (!desirability_tooltip(c)) {
        grid_box_handle_tooltip(&model_buttons, c);
    }
}

void window_model_data_show(void)
{
    init();
    window_type window = {
        WINDOW_EDITOR_MODEL_DATA,
        draw_background,
        draw_foreground,
        handle_input,
        get_tooltip
    };
    window_show(&window);
}
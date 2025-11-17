#include "house_model_data.h"

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
#include "window/popup_dialog.h"

#include <stdio.h>

#define NO_SELECTION (unsigned int) -1
#define NUM_DATA_BUTTONS (sizeof(data_buttons) / sizeof(generic_button))

static void button_edit_model_value(const generic_button *button);

static void button_static_click(const generic_button *button);

static void populate_list(void);
static void draw_model_item(const grid_box_item *item);
static void model_item_click(const grid_box_item *item);


static struct {
    unsigned int total_items;
    house_level items[HOUSE_MAX + 1];

    unsigned int data_buttons_focus_id;
    unsigned int static_buttons_focus_id;
    unsigned int target_index;
    house_model_data_type data_type;
} data;

static generic_button data_buttons[] = {
    {141, 2, 48, 20, button_edit_model_value, 0, MODEL_DEVOLVE_DESIRABILITY},
    {196, 2, 48, 20, button_edit_model_value, 0, MODEL_EVOLVE_DESIRABILITY},
    {251, 2, 48, 20, button_edit_model_value, 0, MODEL_ENTERTAINMENT},
    {306, 2, 48, 20, button_edit_model_value, 0, MODEL_WATER},
    {361, 2, 48, 20, button_edit_model_value, 0, MODEL_RELIGION},
    {416, 2, 48, 20, button_edit_model_value, 0, MODEL_EDUCATION},
    {471, 2, 48, 20, button_edit_model_value, 0, MODEL_BARBER},
    {526, 2, 48, 20, button_edit_model_value, 0, MODEL_BATHHOUSE},
    {581, 2, 48, 20, button_edit_model_value, 0, MODEL_HEALTH},
    {141, 30, 48, 20, button_edit_model_value, 0, MODEL_FOOD_TYPES},
    {196, 30, 48, 20, button_edit_model_value, 0, MODEL_POTTERY},
    {251, 30, 48, 20, button_edit_model_value, 0, MODEL_OIL},
    {306, 30, 48, 20, button_edit_model_value, 0, MODEL_FURNITURE},
    {361, 30, 48, 20, button_edit_model_value, 0, MODEL_WINE},
    {416, 30, 48, 20, button_edit_model_value, 0, MODEL_PROSPERITY},
    {471, 30, 48, 20, button_edit_model_value, 0, MODEL_MAX_PEOPLE},
    {526, 30, 48, 20, button_edit_model_value, 0, MODEL_TAX_MULTIPLIER}
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
    .width = 42 * BLOCK_SIZE ,
    .height = 20 * BLOCK_SIZE,
    .item_height = 56,
    .item_margin.horizontal = 8,
    .item_margin.vertical = 2,
    .extend_to_hidden_scrollbar = 1,
    .on_click = model_item_click,
    .draw_item = draw_model_item,
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
    while (data.total_items < HOUSE_MAX + 1) {
        data.items[data.total_items] = data.total_items;
        data.total_items++;
    }
}

static void reset_confirmed(int accepted, int checked)
{
    if (accepted) {
        model_reset_houses();
        window_request_refresh();
    }
}

static void button_static_click(const generic_button *button)
{
    switch (button->parameter1) {
        case 0:
            window_file_dialog_show(FILE_TYPE_MODEL_DATA, FILE_DIALOG_SAVE);
            break;
        case 1:
            window_popup_dialog_show_confirmation(translation_for(TR_BUTTON_RESET_DEFAULTS),
                translation_for(TR_PARAMETER_HOUSE_MODEL_REST_CONFIRMATION), NULL, reset_confirmed);
            break;
        case 2:
            window_file_dialog_show(FILE_TYPE_MODEL_DATA, FILE_DIALOG_LOAD);
            break;
        default:
            break;
    }
}

static void set_model_value(int value)
{
    model_house *model = model_get_house(data.items[data.target_index]);
    *get_ptr_for_house_data_type(model, data.data_type) = value;
    data.target_index = NO_SELECTION;
}

static void button_edit_model_value(const generic_button *button)
{
    data.data_type = button->parameter1;
    window_numeric_input_bound_show(model_buttons.focused_item.x, model_buttons.focused_item.y, button,
        9, -1000000000, 1000000000, set_model_value);
}

static void model_item_click(const grid_box_item *item)
{
    data.target_index = item->index;
}

static void draw_model_item(const grid_box_item *item)
{
    button_border_draw(item->x, item->y, item->width, item->height, 0);
    int h_level = data.items[item->index];

    const uint8_t *h_string = lang_get_building_type_string(h_level + 10); // Conversion from house_level to building_type
    text_draw_multiline(h_string, item->x + 8, item->y + 8, 8 * BLOCK_SIZE, 0, FONT_NORMAL_BLACK, 0);

    for (unsigned int i = 0; i < MAX_DATA_BUTTONS; i++) {
        button_border_draw(item->x + data_buttons[i].x, item->y + data_buttons[i].y,
            data_buttons[i].width, data_buttons[i].height, item->is_focused && data.data_buttons_focus_id == i + 1);
        
        model_house *model = model_get_house(h_level);
        int value = *get_ptr_for_house_data_type(model, i);

        text_draw_number(value, 0, NULL, item->x + data_buttons[i].x + 8, item->y + data_buttons[i].y + 6,
                  FONT_SMALL_PLAIN, 0);
    }

}

static void draw_background(void)
{
    window_editor_map_draw_all();

    graphics_in_dialog();

    outer_panel_draw(16, 32, 46, 27);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_ACTION_TYPE_CHANGE_MODEL_DATA, 26, 42, 38 * BLOCK_SIZE, FONT_LARGE_BLACK);
    lang_text_draw_centered(13, 3, 16, 27 * BLOCK_SIZE + 8, 42 * BLOCK_SIZE, FONT_NORMAL_BLACK);

    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_PARAMETER_MODEL, 80, 75, 30, FONT_SMALL_PLAIN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_PARAMETER_COST, 240, 75, 30, FONT_SMALL_PLAIN);
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

    for (unsigned int i = 0; i < NUM_STATIC_BUTTONS; i++) {
        button_border_draw(static_buttons[i].x, static_buttons[i].y,
            static_buttons[i].width, static_buttons[i].height, data.static_buttons_focus_id == i + 1);
        translation_key key;
        switch (i) {
            default:
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
            default:
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

static void get_tooltip(tooltip_context *c)
{
    desirability_tooltip(c);
}

void window_house_model_data_show(void)
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
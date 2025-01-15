#include "custom_variables.h"

#include "assets/assets.h"
#include "core/log.h"
#include "core/string.h"
#include "editor/editor.h"
#include "graphics/button.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/grid_box.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/screen.h"
#include "graphics/scrollbar.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "scenario/custom_variable.h"
#include "scenario/message_media_text_blob.h"
#include "scenario/property.h"
#include "scenario/scenario.h"
#include "scenario/scenario_events_controller.h"
#include "window/editor/attributes.h"
#include "window/editor/map.h"
#include "window/numeric_input.h"
#include "window/plain_message_dialog.h"
#include "window/text_input.h"

#define CHECKBOX_ROW_WIDTH 25
#define ID_ROW_WIDTH 32
#define VALUE_ROW_WIDTH 120
#define BUTTONS_PADDING 4
#define NUM_ITEM_BUTTONS (sizeof(item_buttons) / sizeof(generic_button))
#define NUM_CONSTANT_BUTTONS (sizeof(constant_buttons) / sizeof(generic_button))

#define NO_SELECTION (unsigned int) -1

typedef enum {
    CHECKBOX_NO_SELECTION = 0,
    CHECKBOX_SOME_SELECTED = 1,
    CHECKBOX_ALL_SELECTED = 2
} checkbox_selection_type;

static void button_variable_checkbox(const generic_button *button);
static void button_edit_variable_name(const generic_button *button);
static void button_edit_variable_value(const generic_button *button);

static void button_select_all_none(const generic_button *button);
static void button_edit_variable(const grid_box_item *item);
static void button_new_variable(const generic_button *button);
static void draw_variable_button(const grid_box_item *item);

static struct {
    unsigned int constant_button_focus_id;
    unsigned int item_buttons_focus_id;

    unsigned int target_index;

    unsigned int *custom_variable_ids;
    unsigned int total_custom_variables;
    unsigned int custom_variables_in_use;

    uint8_t *selected;
    checkbox_selection_type selection_type;

    void (*callback)(unsigned int id);
} data;

static generic_button item_buttons[] = {
    { 0, 5, 20, 20, button_variable_checkbox },
    { CHECKBOX_ROW_WIDTH + ID_ROW_WIDTH, 0, 0, 30, button_edit_variable_name },
    { 0, 0, VALUE_ROW_WIDTH, 30, button_edit_variable_value }
};

static generic_button constant_buttons[] = {
    { 26, 96, 20, 20, button_select_all_none },
    { 195, 410, 250, 25, button_new_variable }
};

static grid_box_type variable_buttons = {
    .x = 26,
    .y = 108,
    .width = 38 * BLOCK_SIZE,
    .height = 19 * BLOCK_SIZE,
    .num_columns = 1,
    .item_height = 30,
    .item_margin.horizontal = 10,
    .item_margin.vertical = 5,
    .extend_to_hidden_scrollbar = 1,
    .on_click = button_edit_variable,
    .draw_item = draw_variable_button
};

static void select_all_to(uint8_t value)
{
    if (data.selected) {
        memset(data.selected, value, data.custom_variables_in_use * sizeof(uint8_t));
    }
    data.selection_type = value ? CHECKBOX_ALL_SELECTED : CHECKBOX_NO_SELECTION;
}

static void populate_list(void)
{
    data.target_index = NO_SELECTION;

    unsigned int total_custom_variables = scenario_custom_variable_count();
    if (total_custom_variables > data.total_custom_variables) {
        free(data.custom_variable_ids);
        free(data.selected);
        data.custom_variable_ids = calloc(total_custom_variables, sizeof(unsigned int));
        if (!data.custom_variable_ids) {
            data.total_custom_variables = 0;
            data.custom_variables_in_use = 0;
            data.selected = 0;
            log_error("Failed to allocate memory for custom variable list", 0, 0);
            return;
        }
        data.total_custom_variables = total_custom_variables;
        data.selected = calloc(total_custom_variables, sizeof(uint8_t));
    }
    data.custom_variables_in_use = 0;
    for (unsigned int i = 0; i < data.total_custom_variables; i++) {
        if (!scenario_custom_variable_exists(i)) {
            continue;
        }
        data.custom_variable_ids[data.custom_variables_in_use] = i;
        data.custom_variables_in_use++;
    }

    select_all_to(0);
}

static void init(void (*callback)(unsigned int id))
{
    data.callback = callback;
    populate_list();
    grid_box_init(&variable_buttons, data.custom_variables_in_use);
}

static void draw_background(void)
{
    window_editor_map_draw_all();

    graphics_in_dialog();

    outer_panel_draw(16, 16, 40, 30);

    text_draw_centered(translation_for(TR_EDITOR_CUSTOM_VARIABLES_TITLE), 0, 32, 640, FONT_LARGE_BLACK, 0);
    text_draw_label_and_number_centered(translation_for(TR_EDITOR_CUSTOM_VARIABLES_COUNT), data.custom_variables_in_use,
        "", 0, 70, 640, FONT_NORMAL_BLACK, 0);

    int base_x = variable_buttons.x;
    if (!data.callback) {
        base_x += CHECKBOX_ROW_WIDTH;
    }

    text_draw_centered(string_from_ascii("ID"), base_x, 96, 40, FONT_SMALL_PLAIN, 0);
    text_draw(string_from_ascii("Name"), base_x + 40, 96, FONT_SMALL_PLAIN, 0);

    lang_text_draw_centered(13, 3, 48, 450, 640, FONT_NORMAL_BLACK);

    grid_box_request_refresh(&variable_buttons);

    if (data.callback) {
        graphics_reset_dialog();
        return;
    }

    // Checkmarks for select all/none button
    int checkmark_id = assets_lookup_image_id(ASSET_UI_SELECTION_CHECKMARK);
    const image *img = image_get(checkmark_id);
    const generic_button *select_all_none_button = &constant_buttons[0];
    if (data.selection_type == CHECKBOX_SOME_SELECTED) {
        text_draw(string_from_ascii("-"), select_all_none_button->x + 8, select_all_none_button->y + 4,
            FONT_NORMAL_BLACK, 0);
    } else if (data.selection_type == CHECKBOX_ALL_SELECTED) {
        image_draw(checkmark_id, select_all_none_button->x + (20 - img->original.width) / 2,
             select_all_none_button->y + (20 - img->original.height) / 2, COLOR_MASK_NONE, SCALE_NONE);
    }

    int value_x_offset = variable_buttons.width - VALUE_ROW_WIDTH;
    if (grid_box_has_scrollbar(&variable_buttons)) {
        value_x_offset -= 2 * BLOCK_SIZE;
    }
    text_draw(string_from_ascii("Initial value"), base_x + value_x_offset, 96, FONT_SMALL_PLAIN, 0);

    const generic_button *new_variable_button = &constant_buttons[1];

    text_draw_centered(string_from_ascii("New custom variable"), new_variable_button->x, new_variable_button->y + 8,
        new_variable_button->width, FONT_NORMAL_BLACK, 0);

    graphics_reset_dialog();
}

static void draw_variable_button(const grid_box_item *item)
{
    unsigned int id = data.custom_variable_ids[item->index];
    const uint8_t *name = scenario_custom_variable_get_name(id);
    int value = scenario_custom_variable_get_value(id);
    int name_button_width = item->width - ID_ROW_WIDTH;

    if (!data.callback) {
        name_button_width -= VALUE_ROW_WIDTH + BUTTONS_PADDING;
    }

    text_draw_number_centered(id, item->x, item->y + 8, 32, FONT_NORMAL_BLACK);

    button_border_draw(item->x + 32, item->y, name_button_width, item->height,
        item->is_focused && item->mouse.x >= 32 && item->mouse.x < name_button_width + ID_ROW_WIDTH);

    if (name && *name) {
        text_draw(name, item->x + ID_ROW_WIDTH + 8, item->y + 8, FONT_NORMAL_BLACK, COLOR_MASK_NONE);
    }

    if (data.callback) {
        return;
    }

    button_border_draw(item->x + name_button_width + ID_ROW_WIDTH + BUTTONS_PADDING, item->y, VALUE_ROW_WIDTH,
        item->height, item->is_focused && item->mouse.x >= name_button_width + ID_ROW_WIDTH + BUTTONS_PADDING);

    text_draw_number(value, ' ', "", item->x + name_button_width + ID_ROW_WIDTH + BUTTONS_PADDING + 8, item->y + 8,
        FONT_NORMAL_BLACK, COLOR_MASK_NONE);
}

static void draw_foreground(void)
{
    graphics_in_dialog();

    grid_box_draw(&variable_buttons);

    for (unsigned int i = 0; i < NUM_CONSTANT_BUTTONS; i++) {
        int focus = data.constant_button_focus_id == i + 1;
        if (i == 0 && data.custom_variables_in_use == 0) {
            focus = 0;
        }
        button_border_draw(constant_buttons[i].x, constant_buttons[i].y, constant_buttons[i].width,
            constant_buttons[i].height, focus);
    }

    graphics_reset_dialog();
}

static void set_variable_value(int value)
{
    scenario_custom_variable_set_value(data.custom_variable_ids[data.target_index], value);
    data.target_index = NO_SELECTION;
}

static void set_variable_name(const uint8_t *value)
{
    scenario_custom_variable_rename(data.custom_variable_ids[data.target_index], value);
    data.target_index = NO_SELECTION;
}

static void button_select_all_none(const generic_button *button)
{
    if (data.selection_type == CHECKBOX_NO_SELECTION) {
        select_all_to(1);
    } else {
        select_all_to(0);
    }
    window_request_refresh();
}

static void button_variable_checkbox(const generic_button *button)
{

}

static void button_edit_variable_name(const generic_button *button)
{

}

static void button_edit_variable_value(const generic_button *button)
{

}

static void button_edit_variable(const grid_box_item *item)
{
    unsigned int id = data.custom_variable_ids[item->index];

    if (data.callback) {
        if (item->mouse.x >= ID_ROW_WIDTH) {
            data.callback(id);
        }
        return;
    }
    data.target_index = item->index;

    return;

    if (item->mouse.x > item->x + item->width - VALUE_ROW_WIDTH) {
        generic_button button = {
            item->x + item->width - VALUE_ROW_WIDTH, item->y, VALUE_ROW_WIDTH, item->height, 0, 0, id
        };
        window_numeric_input_bound_show(0, 0, &button, 9, -1000000000, 1000000000, set_variable_value);
        return;
    }
    static uint8_t text_input_title[100];
    uint8_t *cursor = string_copy(translation_for(TR_PARAMETER_TYPE_CUSTOM_VARIABLE), text_input_title, 100);
    cursor = string_copy(string_from_ascii(" "), cursor, 100 - (cursor - text_input_title));
    string_from_int(cursor, id, 0);

    window_text_input_show(text_input_title, 0, scenario_custom_variable_get_name(id),
        CUSTOM_VARIABLE_NAME_LENGTH, set_variable_name);
}

static void show_used_event_popup_dialog(const scenario_event_t *event)
{
    static uint8_t event_id_text[50];
    uint8_t *cursor = string_copy(translation_for(TR_EDITOR_CUSTOM_VARIABLE_UNABLE_TO_CHANGE_EVENT_ID),
        event_id_text, 50);
    string_from_int(cursor, event->id, 0);
    window_plain_message_dialog_show_with_extra(TR_EDITOR_CUSTOM_VARIABLE_UNABLE_TO_CHANGE_TITLE,
        TR_EDITOR_CUSTOM_VARIABLE_UNABLE_TO_CHANGE_TEXT, 0, event_id_text);
}

/***
static void button_delete_variable(const generic_button *button)
{
    int button_index = button->parameter1;

    if (data.callback) {
        return;
    }
    if (!data.list[button_index]) {
        return;
    };
    scenario_event_t *event = scenario_events_get_using_custom_variable(data.list[button_index]->id);
    if (event) {
        show_used_event_popup_dialog(event);
        return;
    }
    const uint8_t empty_name[2] = "";
    scenario_custom_variable_rename(data.list[button_index]->id, empty_name);
}***/


static void button_new_variable(const generic_button *button)
{
    if (data.callback || !scenario_custom_variable_create(0, 0)) {
        return;
    }
    populate_list();
    grid_box_update_total_items(&variable_buttons, data.custom_variables_in_use);
    window_request_refresh();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);
    if (grid_box_handle_input(&variable_buttons, m_dialog, 1)) {
        if (data.callback) {
            return;
        }
    }
    const grid_box_item *item = variable_buttons.focused_item;
    if (generic_buttons_handle_mouse(m_dialog, 0, 0, item_buttons, NUM_ITEM_BUTTONS, &data.item_buttons_focus_id) ||
        generic_buttons_handle_mouse(m_dialog, 0, 0, constant_buttons, NUM_CONSTANT_BUTTONS, &data.constant_button_focus_id)) {
        return;
    }

    if (input_go_back_requested(m, h)) {
        window_go_back();
    }
}

void window_editor_custom_variables_show(void (*callback)(unsigned int id))
{
    window_type window = {
        WINDOW_EDITOR_CUSTOM_VARIABLES,
        draw_background,
        draw_foreground,
        handle_input
    };
    init(callback);
    window_show(&window);
}

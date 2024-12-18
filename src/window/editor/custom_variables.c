#include "custom_variables.h"

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

#define WIDTH_VALUE_BUTTON 96

static void button_edit_variable(unsigned int index, unsigned int mouse_x, unsigned int mouse_y);
static void button_new_variable(const generic_button *button);
static void draw_variable_button(const grid_box_item *item);

static struct {
    unsigned int focus_button_id;

    unsigned int *custom_variable_ids;
    unsigned int total_custom_variables;
    unsigned int custom_variables_in_use;

    void (*callback)(unsigned int id);
} data;

static generic_button new_variable_button = {
    195, 340, 250, 25, button_new_variable
};

static grid_box_type variable_buttons = {
    .x = 10,
    .y = 146,
    .width = 35 * BLOCK_SIZE,
    .height = 19 * BLOCK_SIZE,
    .num_columns = 1,
    .item_height = 30,
    .item_margin.horizontal = 10,
    .item_margin.vertical = 5,
    .extend_to_hidden_scrollbar = 1,
    .on_click = button_edit_variable,
    .draw_item = draw_variable_button
};

static void populate_list(void)
{
    unsigned int total_custom_variables = scenario_custom_variable_count();
    if (total_custom_variables > data.total_custom_variables) {
        free(data.custom_variable_ids);
        data.custom_variable_ids = calloc(total_custom_variables, sizeof(unsigned int));
        if (!data.custom_variable_ids) {
            data.total_custom_variables = 0;
            data.custom_variables_in_use = 0;
            log_error("Failed to allocate memory for custom variable list", 0, 0);
            return;
        }
        data.total_custom_variables = total_custom_variables;
    }
    data.custom_variables_in_use = 0;
    for (unsigned int i = 0; i < data.total_custom_variables; i++) {
        if (!scenario_custom_variable_exists(i)) {
            continue;
        }
        data.custom_variable_ids[data.custom_variables_in_use] = i;
        data.custom_variables_in_use++;
    }
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

    text_draw_centered(translation_for(TR_EDITOR_CUSTOM_VARIABLES_TITLE), 48, 58, 640, FONT_LARGE_BLACK, 0);
    text_draw_label_and_number(translation_for(TR_EDITOR_CUSTOM_VARIABLES_COUNT), data.custom_variables_in_use, "",
        48, 106, FONT_NORMAL_PLAIN, COLOR_BLACK);

    lang_text_draw_centered(13, 3, 48, 450, 640, FONT_NORMAL_BLACK);

    text_draw_centered(string_from_ascii("New custom variable"), new_variable_button.x, new_variable_button.y + 4,
        new_variable_button.width, FONT_NORMAL_BLACK, 0);

    graphics_reset_dialog();

    grid_box_request_refresh(&variable_buttons);
}

static void draw_variable_button(const grid_box_item *item)
{
    unsigned int id = data.custom_variable_ids[item->index];
    const uint8_t *name = scenario_custom_variable_get_name(id);
    //int value = scenario_custom_variable_get_value(id);
    int value_button_x_offset = item->width - WIDTH_VALUE_BUTTON;

    button_border_draw(item->x, item->y, item->width, item->height,
        item->is_focused && item->mouse.x < value_button_x_offset);
    text_draw_label_and_number(0, id, "", item->x, item->y + 12, FONT_NORMAL_GREEN, COLOR_MASK_NONE);

    if (name && *name) {
        text_draw(name, item->x + 52, item->y + 12, FONT_NORMAL_GREEN, COLOR_MASK_NONE);
    }

    if (data.callback) {
        return;
    }
   // large_label_draw(buttons[j + 1].x, buttons[j + 1].y, buttons[j + 1].width / BLOCK_SIZE,
   //     data.focus_button_id == j + 2 ? 1 : 0);

   // text_draw_number(data.list[i]->value, ' ', "", buttons[j + 1].x, buttons[j + 1].y + 8,
   //     FONT_NORMAL_GREEN, COLOR_MASK_NONE);
}


static void draw_foreground(void)
{
    graphics_in_dialog();

    grid_box_draw(&variable_buttons);

    button_border_draw(new_variable_button.x, new_variable_button.y, new_variable_button.width,
        new_variable_button.height, data.focus_button_id == 1);

    graphics_reset_dialog();
}

static void set_variable_value(int value)
{
   // scenario_set_custom_variable_value(data.target_variable, value);
}

static void button_edit_variable(unsigned int index, unsigned int mouse_x, unsigned int mouse_y)
{
    if (data.callback) {
        return;
    }

  /**  if (!data.list[button_index] || !data.list[button_index]->in_use) {
        return;
    }
    data.target_variable = data.list[button_index]->id;
    window_numeric_input_bound_show(0, 0, button, 9, -1000000000, 1000000000, set_variable_value); ***/
}

/***static void set_variable_name(const uint8_t *value)
{
    scenario_custom_variable_rename(data.target_variable, value);
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

static void button_name_click(const generic_button *button)
{
    int button_index = button->parameter1;

    if (!data.list[button_index]) {
        return;
    };

    int has_name = data.list[button_index]->linked_uid && data.list[button_index]->linked_uid->in_use;
    if (data.callback) {
        if (!has_name) {
            return;
        }
        data.callback(data.list[button_index]);
        window_go_back();
    } else {
        scenario_event_t *event = scenario_events_get_using_custom_variable(data.list[button_index]->id);
        if (event) {
            show_used_event_popup_dialog(event);
            return;
        }
        data.target_variable = data.list[button_index]->id;
        static uint8_t text_input_title[100];
        uint8_t *cursor = string_copy(translation_for(TR_PARAMETER_TYPE_CUSTOM_VARIABLE), text_input_title, 100);
        cursor = string_copy(string_from_ascii(" "), cursor, 100 - (cursor - text_input_title));
        string_from_int(cursor, data.target_variable, 0);

        window_text_input_show(text_input_title, 0, scenario_get_custom_variable_name(data.target_variable),
            MAX_VARIABLE_NAME_SIZE, set_variable_name);
    }
}

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
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);
    if (grid_box_handle_input(&variable_buttons, m_dialog, 1) ||
        generic_buttons_handle_mouse(m_dialog, 0, 0, &new_variable_button, 1, &data.focus_button_id)) {
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

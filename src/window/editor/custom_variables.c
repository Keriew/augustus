#include "custom_variables.h"

#include "assets/assets.h"
#include "core/lang.h"
#include "core/log.h"
#include "core/string.h"
#include "editor/editor.h"
#include "graphics/button.h"
#include "graphics/complex_button.h"
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
#include "scenario/event/controller.h"
#include "scenario/message_media_text_blob.h"
#include "scenario/property.h"
#include "scenario/scenario.h"
#include "widget/dropdown_button.h"
#include "window/editor/attributes.h"
#include "window/editor/map.h"
#include "window/numeric_input.h"
#include "window/plain_message_dialog.h"
#include "window/popup_dialog.h"
#include "window/text_input.h"

#define CHECKBOX_ROW_WIDTH 25
#define ID_ROW_WIDTH 32
#define VALUE_ROW_WIDTH 60
#define NAME_ROW_WIDTH 130
#define NAME_ROW_WIDTH_CALLBACK 3 * NAME_ROW_WIDTH
#define BUTTONS_PADDING 4
#define COLOR_DROPDOWN_WIDTH 60
#define NUM_ITEM_BUTTONS (sizeof(item_buttons) / sizeof(generic_button))
#define NUM_CONSTANT_BUTTONS (sizeof(constant_buttons) / sizeof(generic_button))
#define COLOR_BUTTONS_COUNT 12 // 10 + "none" + anchor

//gridbox macros and constants
#define GRID_BOX_HEIGHT 23 * BLOCK_SIZE
#define GRID_BOX_ITEM_HEIGHT 28
#define MAX_VISIBLE_GRID_ITEMS (GRID_BOX_HEIGHT / GRID_BOX_ITEM_HEIGHT) // height of grid box / item height

#define NO_SELECTION (unsigned int) -1

typedef enum {
    CHECKBOX_NO_SELECTION = 0,
    CHECKBOX_SOME_SELECTED = 1,
    CHECKBOX_ALL_SELECTED = 2
} checkbox_selection_type;

static void button_variable_checkbox(const generic_button *button);
static void button_edit_variable_name(const generic_button *button);
static void button_edit_variable_value(const generic_button *button);
static void button_edit_color(dropdown_button *button);
static void button_edit_display_text(const generic_button *button);
static void button_variable_visible_checkbox(const generic_button *button);

static void button_select_all_none(const generic_button *button);
static void button_delete_selected(const generic_button *button);
static void button_new_variable(const generic_button *button);
static void button_ok(const generic_button *button);

static void variable_item_click(const grid_box_item *item);
static void draw_variable_item(const grid_box_item *item);

static struct {
    unsigned int constant_button_focus_id;
    unsigned int item_buttons_focus_id;
    unsigned int target_index;
    unsigned int *custom_variable_ids;
    unsigned int total_custom_variables;
    unsigned int custom_variables_in_use;
    unsigned int ellipsized_tooltip; // index of the dropdown button providing tooltip
    unsigned int expanded_dropdown; // index of the currently expanded dropdown button
    uint8_t *selected;
    checkbox_selection_type selection_type;
    int do_not_ask_again_for_delete;
    void (*callback)(unsigned int id);
} data;

static generic_button item_buttons[] = {
    { 0, 0, 20, 20, button_variable_checkbox },
    { 0, 0, NAME_ROW_WIDTH, 25, button_edit_variable_name },
    { 0, 0, VALUE_ROW_WIDTH, 25, button_edit_variable_value },
    { 0, 0, 0, 25, button_edit_display_text },
    { 0, 0, 20, 20, button_variable_visible_checkbox }
};

static generic_button constant_buttons[] = {
    { 31, 55, 20, 20, button_select_all_none },
    { 31, 454, 200, 30, button_delete_selected, 0 },
    { 237, 454, 200, 30, button_new_variable },
    { 442, 454, 150, 30, button_ok }
};

static lang_fragment color_fragments[COLOR_BUTTONS_COUNT];
static dropdown_button color_dropdowns[MAX_VISIBLE_GRID_ITEMS] = { 0 };
static complex_button color_dropdown_options[MAX_VISIBLE_GRID_ITEMS][COLOR_BUTTONS_COUNT] = { 0 };

// static array because adding a new complex structure to the gridbox exceeds my abilities
// as a dynamic structure more akin to an object, it needs to be initialized per item in the gridbox


static grid_box_type variable_buttons = {
    .x = 26,
    .y = 79,
    .width = 38 * BLOCK_SIZE,
    .height = GRID_BOX_HEIGHT, //if changing this 
    .num_columns = 1,
    .item_height = GRID_BOX_ITEM_HEIGHT, //or this, update the MAX_VISIBLE_GRID_ITEMS macro accordingly
    .item_margin.horizontal = 10,
    .item_margin.vertical = 5,
    .extend_to_hidden_scrollbar = 1,
    .on_click = variable_item_click,
    .draw_item = draw_variable_item
};

static void init_color_dropdown(void)
{
    for (int editor_colors = 0; editor_colors < COLOR_BUTTONS_COUNT; editor_colors++) {
        color_fragments[editor_colors].type = LANG_FRAG_LABEL;
        color_fragments[editor_colors].text_group = CUSTOM_TRANSLATION;
        color_fragments[editor_colors].text_id = TR_EDITOR_COLOR_LABEL + editor_colors;
    }

    for (int dd_anchors = 0; dd_anchors < MAX_VISIBLE_GRID_ITEMS; dd_anchors++) {
        color_dropdown_options[dd_anchors][0].is_hidden = 0;
        color_dropdown_options[dd_anchors][0].x = 568;
        color_dropdown_options[dd_anchors][0].y = GRID_BOX_ITEM_HEIGHT * dd_anchors + 79;
        color_dropdown_options[dd_anchors][0].height = CHECKBOX_ROW_WIDTH;
        color_dropdown_options[dd_anchors][0].width = COLOR_DROPDOWN_WIDTH + 20;
        for (int j = 0; j < COLOR_BUTTONS_COUNT; j++) { // dropdown option buttons - COLOR_BUTTONS_COUNT per dropdown
            color_dropdown_options[dd_anchors][j].sequence = &color_fragments[j];
            color_dropdown_options[dd_anchors][j].sequence_size = 1;
            color_dropdown_options[dd_anchors][j].left_click_handler = dropdown_button_default_option_click;
            color_dropdown_options[dd_anchors][j].user_data = &color_dropdowns[dd_anchors]; //backref to parent dropdown
            //color_dropdown_options[dd_anchors][j].style = COMPLEX_BUTTON_STYLE_DEFAULT;
            color_dropdown_options[dd_anchors][j].parameters[0] = dd_anchors;
            // draw_variable_item will reestablish the correct variable id with scroll offset
            color_dropdown_options[dd_anchors][j].color_mask = complex_button_basic_colors(j - 1);
            if (j > 8) {
                color_dropdown_options[dd_anchors][j].font = FONT_SMALL_PLAIN; //white font for dark colors
            }
        }
        dropdown_button_init(&color_dropdowns[dd_anchors], color_dropdown_options[dd_anchors], COLOR_BUTTONS_COUNT, COLOR_DROPDOWN_WIDTH, 2, 4);
        color_dropdowns[dd_anchors].selected_callback = button_edit_color;
    }
}

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
    init_color_dropdown();
    populate_list();
    grid_box_init(&variable_buttons, data.custom_variables_in_use);
    grid_box_request_refresh(&variable_buttons);
}

static void update_item_buttons_positions(void)
{
    if (data.callback) {
        item_buttons[1].x = ID_ROW_WIDTH;
        // Reset to base width and adjust for scrollbar
        item_buttons[1].width = NAME_ROW_WIDTH_CALLBACK;
        if (grid_box_has_scrollbar(&variable_buttons)) {
            item_buttons[1].width = NAME_ROW_WIDTH_CALLBACK - 2 * BLOCK_SIZE;
        }
        return;
    } else {
        item_buttons[1].x = CHECKBOX_ROW_WIDTH + ID_ROW_WIDTH;
        item_buttons[1].width = NAME_ROW_WIDTH;
    }

    // Position name button (fixed width)
    item_buttons[1].x = CHECKBOX_ROW_WIDTH + ID_ROW_WIDTH;
    item_buttons[1].width = NAME_ROW_WIDTH;

    // Position value button (fixed width)
    item_buttons[2].x = item_buttons[1].x + item_buttons[1].width + BUTTONS_PADDING;

    // Position visible checkbox (fixed width)
    item_buttons[4].x = variable_buttons.width - variable_buttons.item_margin.horizontal
        - CHECKBOX_ROW_WIDTH - COLOR_DROPDOWN_WIDTH;
    if (grid_box_has_scrollbar(&variable_buttons)) {
        item_buttons[4].x -= 2 * BLOCK_SIZE;
        for (int i = 0; i < MAX_VISIBLE_GRID_ITEMS; i++) {
            color_dropdown_options[i][0].x = 568 - 2 * BLOCK_SIZE;
        }
    } else {
        for (int i = 0; i < MAX_VISIBLE_GRID_ITEMS; i++) {
            color_dropdown_options[i][0].x = 568;
        }
    }

    item_buttons[3].x = item_buttons[2].x + item_buttons[2].width + BUTTONS_PADDING;
    item_buttons[3].width = item_buttons[4].x - item_buttons[3].x - BUTTONS_PADDING;
}

static void draw_background(void)
{
    window_editor_map_draw_all();

    update_item_buttons_positions();

    graphics_in_dialog();

    outer_panel_draw(16, 16, 40, data.callback ? 28 : 30);

    text_draw_centered(translation_for(TR_EDITOR_CUSTOM_VARIABLES_TITLE), 20, 27, 640, FONT_LARGE_BLACK, 0);
    text_draw_label_and_number(translation_for(TR_EDITOR_CUSTOM_VARIABLES_COUNT), data.custom_variables_in_use,
        "", 32, 30, FONT_SMALL_PLAIN, 0);

    int base_x_offset = variable_buttons.x + variable_buttons.item_margin.horizontal / 2;

    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_CUSTOM_VARIABLES_ID,
        variable_buttons.x + (data.callback ? 0 : CHECKBOX_ROW_WIDTH), 60, 40, FONT_SMALL_PLAIN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_CUSTOM_VARIABLES_NAME, base_x_offset + item_buttons[1].x, 60,
        data.callback ? NAME_ROW_WIDTH_CALLBACK : NAME_ROW_WIDTH, FONT_SMALL_PLAIN);

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

    lang_text_draw(CUSTOM_TRANSLATION, TR_EDITOR_CUSTOM_VARIABLES_VALUE, base_x_offset + item_buttons[2].x, 60,
        FONT_SMALL_PLAIN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_CUSTOM_VARIABLES_TEXT_DISPLAY,
        base_x_offset + item_buttons[3].x, 60, item_buttons[3].width, FONT_SMALL_PLAIN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_CUSTOM_VARIABLES_IS_VISIBLE,
        base_x_offset + item_buttons[4].x - 15, 60, 20, FONT_SMALL_PLAIN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_COLOR_LABEL,
        color_dropdowns->buttons[0].x, 60, COLOR_DROPDOWN_WIDTH, FONT_SMALL_PLAIN);
    // Bottom buttons
    const generic_button *delete_selected_button = &constant_buttons[1];
    color_t color = data.selection_type == CHECKBOX_NO_SELECTION ? COLOR_FONT_LIGHT_GRAY : COLOR_RED;
    lang_text_draw_centered_colored(CUSTOM_TRANSLATION, TR_EDITOR_SCENARIO_EVENTS_DELETE_SELECTED,
        delete_selected_button->x, delete_selected_button->y + 9, delete_selected_button->width,
        FONT_NORMAL_PLAIN, color);
    const generic_button *new_variable_button = &constant_buttons[2];
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_CUSTOM_VARIABLES_NEW,
        new_variable_button->x, new_variable_button->y + 9, new_variable_button->width, FONT_NORMAL_BLACK);
    lang_text_draw_centered(18, 3, constant_buttons[3].x, constant_buttons[3].y + 9, constant_buttons[3].width,
        FONT_NORMAL_BLACK);

    graphics_reset_dialog();
}

static void update_dd_anchor(int variable_id, int grid_box_position)
{
    int id = variable_id;
    int pos = grid_box_position;
    int color_group = scenario_custom_variable_get_color_group(id);
    color_dropdowns[pos].selected_index = scenario_custom_variable_get_color_group(id); // selcted index color
    color_dropdown_options[pos][0].parameters[0] = id; //set the variable id as parameter for the color dropdown
    color_dropdown_options[pos][0].color_mask = scenario_custom_variable_get_color(id); //set the selected colour option
    color_dropdown_options[pos][0].is_hidden = 0; //unhide the associated color dropdown
    color_dropdown_options[pos][0].sequence = &color_fragments[color_group]; //select text
    color_dropdown_options[pos][0].font = (color_group > 8) ? FONT_SMALL_PLAIN : FONT_NORMAL_BLACK; //match font
}

static void draw_variable_item(const grid_box_item *item)
{
    unsigned int id = data.custom_variable_ids[item->index];
    const uint8_t *name = scenario_custom_variable_get_name(id);
    int value = scenario_custom_variable_get_value(id);
    update_dd_anchor(id, item->position);

    // Variable ID
    text_draw_number_centered(id, item->x + (data.callback ? 0 : CHECKBOX_ROW_WIDTH), item->y + 8,
        32, FONT_SMALL_PLAIN);

    // Variable Name
    button_border_draw(item->x + item_buttons[1].x, item->y + item_buttons[1].y,
    item_buttons[1].width, item_buttons[1].height, item->is_focused && data.item_buttons_focus_id == 2);

    if (name && *name) {
        int x = item->x + item_buttons[1].x + 8;
        int y = item->y + item_buttons[1].y + 8;
        int name_box_w = item_buttons[1].width - 16;
        text_draw_ellipsized(name, x, y, name_box_w, FONT_SMALL_PLAIN, 0);
    }

    if (data.callback) {
        return;
    }

    // Checkbox
    button_border_draw(item->x + item_buttons[0].x, item->y + item_buttons[0].y, item_buttons[0].width,
        item_buttons[0].height, item->is_focused && data.item_buttons_focus_id == 1);

    if (data.selected && data.selected[item->index]) {
        int checkmark_id = assets_lookup_image_id(ASSET_UI_SELECTION_CHECKMARK);
        const image *img = image_get(checkmark_id);
        image_draw(checkmark_id, item->x + item_buttons[0].x + (20 - img->original.width) / 2,
            item->y + item_buttons[0].y + (20 - img->original.height) / 2, COLOR_MASK_NONE, SCALE_NONE);
    }

    // Variable Value
    button_border_draw(item->x + item_buttons[2].x, item->y + item_buttons[2].y, item_buttons[2].width,
        item_buttons[2].height, item->is_focused && data.item_buttons_focus_id == 3);

    text_draw_number(value, ' ', "", item->x + item_buttons[2].x + 4, item->y + item_buttons[2].y + 8,
        FONT_SMALL_PLAIN, 0);

    // Display Text
    button_border_draw(item->x + item_buttons[3].x, item->y + item_buttons[3].y, item_buttons[3].width,
        item_buttons[3].height, item->is_focused && data.item_buttons_focus_id == 4);

    const uint8_t *display_text = scenario_custom_variable_get_text_display(id);
    if (display_text && *display_text) {
        int x = item->x + item_buttons[3].x + 8;
        int y = item->y + item_buttons[3].y + 8;
        int display_box_w = item_buttons[3].width - 16;
        text_draw_ellipsized(display_text, x, y, display_box_w, FONT_SMALL_PLAIN, 0);
    }

    // Visible Checkbox
    button_border_draw(item->x + item_buttons[4].x, item->y + item_buttons[4].y, item_buttons[4].width,
        item_buttons[4].height, item->is_focused && data.item_buttons_focus_id == 5);

    if (scenario_custom_variable_is_visible(id)) {
        int checkmark_id = assets_lookup_image_id(ASSET_UI_SELECTION_CHECKMARK);
        const image *img = image_get(checkmark_id);
        image_draw(checkmark_id, item->x + item_buttons[4].x + (20 - img->original.width) / 2,
            item->y + item_buttons[4].y + (20 - img->original.height) / 2, COLOR_MASK_NONE, SCALE_NONE);
    }
}

static void draw_foreground(void)
{
    graphics_in_dialog();

    grid_box_draw(&variable_buttons);

    if (data.callback) {
        graphics_reset_dialog();
        return;
    }

    for (unsigned int i = 0; i < NUM_CONSTANT_BUTTONS; i++) {
        int focus = data.constant_button_focus_id == i + 1;
        if ((i == 0 && data.custom_variables_in_use == 0) || (i == 1 && data.selection_type == CHECKBOX_NO_SELECTION)) {
            focus = 0;
        }
        button_border_draw(constant_buttons[i].x, constant_buttons[i].y, constant_buttons[i].width,
            constant_buttons[i].height, focus);
    }
    data.expanded_dropdown = (unsigned int) -1; //reset expanded tracker before drawing dropdowns
    for (unsigned int i = 0; i < MAX_VISIBLE_GRID_ITEMS && i < data.custom_variables_in_use; i++) {
        dropdown_button *dd = &color_dropdowns[i];
        dd->buttons[0].is_hidden = 0;
        if (dd->expanded == 1) {
            data.expanded_dropdown = i;
            continue;
        }
        dropdown_button_draw(dd);
    }
    for (unsigned int i = 0; i < MAX_VISIBLE_GRID_ITEMS && i < data.custom_variables_in_use; i++) {
        dropdown_button *dd = &color_dropdowns[i];
        if (!dd->expanded) {
            continue;
        }
        dropdown_button_draw(dd);
    } //second loop to draw the expanded dropdowns on top
    graphics_reset_dialog();
}

static void button_select_all_none(const generic_button *button)
{
    if (!data.custom_variables_in_use || data.callback) {
        return;
    }
    if (data.selection_type != CHECKBOX_ALL_SELECTED) {
        select_all_to(1);
    } else {
        select_all_to(0);
    }
    window_request_refresh();
}

static void update_selection_type(void)
{
    uint8_t some_selected = 0;
    uint8_t all_selected = 1;
    for (unsigned int i = 0; i < data.custom_variables_in_use; i++) {
        some_selected |= data.selected[i];
        all_selected &= data.selected[i];
        if (some_selected != all_selected) {
            data.selection_type = CHECKBOX_SOME_SELECTED;
            return;
        }
    }
    data.selection_type = some_selected ? CHECKBOX_ALL_SELECTED : CHECKBOX_NO_SELECTION;
}

static void button_variable_checkbox(const generic_button *button)
{
    if (!data.selected || data.callback) {
        return;
    }
    data.selected[data.target_index] ^= 1;
    update_selection_type();
    window_request_refresh();
}

static void create_new_variable(const uint8_t *name)
{
    unsigned int id = scenario_custom_variable_create(name, 0);
    if (!id) {
        log_error("There was an error creating the new variable - out of memory", 0, 0);
        return;
    }
    populate_list();
    grid_box_update_total_items(&variable_buttons, data.custom_variables_in_use);
    for (unsigned int i = 0; i < data.custom_variables_in_use; i++) {
        if (data.custom_variable_ids[i] == id) {
            grid_box_show_index(&variable_buttons, i);
            break;
        }
    }
    window_request_refresh();
}

static int check_valid_name(const uint8_t *name)
{
    if (!name || !*name) {
        return 0;
    }
    for (unsigned int i = 0; i < data.custom_variables_in_use; i++) {
        if (data.target_index == i) {
            continue;
        }
        if (string_equals(name, scenario_custom_variable_get_name(data.custom_variable_ids[i]))) {
            return 0;
        }
    }
    return 1;
}

static void set_variable_name(const uint8_t *name)
{
    if (!check_valid_name(name)) {
        window_plain_message_dialog_show(TR_EDITOR_CUSTOM_VARIABLE_UNABLE_TO_SET_NAME_TITLE,
            TR_EDITOR_CUSTOM_VARIABLE_UNABLE_TO_SET_NAME_TEXT, 1);
        return;
    }
    // New variable
    if (data.target_index == NO_SELECTION) {
        create_new_variable(name);
        return;
    }
    scenario_custom_variable_rename(data.custom_variable_ids[data.target_index], name);
    data.target_index = NO_SELECTION;
}

static void show_name_edit_popup(void)
{
    const uint8_t *title;
    const uint8_t *name = 0;
    if (data.target_index != NO_SELECTION) {
        static uint8_t text_input_title[100];
        uint8_t *cursor = string_copy(translation_for(TR_PARAMETER_TYPE_CUSTOM_VARIABLE), text_input_title, 100);
        cursor = string_copy(string_from_ascii(" "), cursor, 100 - (cursor - text_input_title));
        unsigned int id = data.custom_variable_ids[data.target_index];
        string_from_int(cursor, id, 0);
        title = text_input_title;
        name = scenario_custom_variable_get_name(id);
    } else {
        title = lang_get_string(CUSTOM_TRANSLATION, TR_EDITOR_CUSTOM_VARIABLES_NEW);
    }

    window_text_input_show(title, 0, name, CUSTOM_VARIABLE_NAME_LENGTH, set_variable_name);
}

static void button_edit_variable_name(const generic_button *button)
{
    if (data.callback) {
        return;
    }
    show_name_edit_popup();
}

static void set_variable_value(int value)
{
    scenario_custom_variable_set_value(data.custom_variable_ids[data.target_index], value);
    data.target_index = NO_SELECTION;
}

static void button_edit_variable_value(const generic_button *button)
{
    if (data.callback) {
        return;
    }
    window_numeric_input_bound_show(variable_buttons.focused_item.x, variable_buttons.focused_item.y, button,
        9, -1000000000, 1000000000, set_variable_value);
}

static void button_edit_color(dropdown_button *dd)
{
    unsigned int color_id = dd->selected_index; //1-based index for custom variables
    int variable_id = dd->buttons[0].parameters[0]; //get variable id from the selected button
    scenario_custom_variable_set_color_group(variable_id, color_id);
    window_request_refresh();
}

static void set_display_text(const uint8_t *text)
{
    scenario_custom_variable_set_text_display(data.custom_variable_ids[data.target_index], text);
    data.target_index = NO_SELECTION;
    window_request_refresh();
}

static void button_edit_display_text(const generic_button *button)
{
    if (data.callback) {
        return;
    }
    unsigned int id = data.custom_variable_ids[data.target_index];
    const uint8_t *current_text = scenario_custom_variable_get_text_display(id);
    const uint8_t *title = lang_get_string(CUSTOM_TRANSLATION, TR_EDITOR_CUSTOM_VARIABLES_TEXT_DISPLAY);
    window_text_input_show(title, 0, current_text,
        CUSTOM_VARIABLE_TEXT_DISPLAY_LENGTH, set_display_text);
}

static void button_variable_visible_checkbox(const generic_button *button)
{
    if (data.callback) {
        return;
    }
    unsigned int id = data.custom_variable_ids[data.target_index];
    int current_visible = scenario_custom_variable_is_visible(id);
    scenario_custom_variable_set_visibility(id, !current_visible);
    window_request_refresh();
}

static void variable_item_click(const grid_box_item *item)
{
    unsigned int id = data.custom_variable_ids[item->index];

    if (data.callback) {
        if (item->mouse.x >= ID_ROW_WIDTH) {
            data.callback(id);
            data.target_index = NO_SELECTION;
        }
        window_go_back();
        return;
    }
    data.target_index = item->index;
}

static void show_used_event_sigle_variable_popup_dialog(const scenario_event_t *event)
{
    static uint8_t event_id_text[50];
    uint8_t *cursor = string_copy(translation_for(TR_EDITOR_CUSTOM_VARIABLE_UNABLE_TO_CHANGE_EVENT_ID),
        event_id_text, 50);
    string_from_int(cursor, event->id, 0);
    window_plain_message_dialog_show_with_extra(TR_EDITOR_CUSTOM_VARIABLE_UNABLE_TO_CHANGE_TITLE,
        TR_EDITOR_CUSTOM_VARIABLE_UNABLE_TO_CHANGE_TEXT, 0, event_id_text);
}

static void show_multiple_variables_in_use_popup_dialog(unsigned int *variables_in_use, unsigned int total)
{
    static uint8_t event_id_text[200];
    uint8_t *cursor = string_copy(translation_for(TR_EDITOR_CUSTOM_VARIABLES_IN_USE),
        event_id_text, 200);
    cursor += string_from_int(cursor, variables_in_use[0], 0);
    for (unsigned int i = 1; i < total; i++) {
        cursor = string_copy(string_from_ascii(", "), cursor, 200 - (cursor - event_id_text));
        if (cursor - event_id_text > 188) {
            cursor = string_copy(string_from_ascii("..."), cursor, 200 - (cursor - event_id_text));
            break;
        }
        cursor += string_from_int(cursor, variables_in_use[i], 0);
    }
    window_plain_message_dialog_show_with_extra(TR_EDITOR_CUSTOM_VARIABLE_UNABLE_TO_CHANGE_TITLE,
        TR_EDITOR_CUSTOM_VARIABLE_UNABLE_TO_CHANGE_TEXT, 0, event_id_text);
}

static void delete_selected(int is_ok, int checked)
{
    if (!is_ok) {
        return;
    }
    if (checked) {
        data.do_not_ask_again_for_delete = 1;
    }

    for (unsigned int i = 0; i < data.custom_variables_in_use; i++) {
        if (!data.selected[i]) {
            continue;
        }
        unsigned int id = data.custom_variable_ids[i];
        if (scenario_custom_variable_exists(id)) {
            scenario_custom_variable_delete(id);
        }
    }
    populate_list();
    grid_box_update_total_items(&variable_buttons, data.custom_variables_in_use);
    window_request_refresh();
}

static void button_delete_selected(const generic_button *button)
{
    if (data.callback || !data.selected || data.selection_type == CHECKBOX_NO_SELECTION) {
        return;
    }

    unsigned int *variables_in_use = calloc(data.custom_variables_in_use, sizeof(unsigned int));
    if (!variables_in_use) {
        log_error("Failed to allocate memory for custom variable list", 0, 0);
        return;
    }
    unsigned int total_variables_in_use = 0;
    const scenario_event_t *event = 0;

    // Step 1: Check if any of the selected variables are used in events
    for (unsigned int i = 0; i < data.custom_variables_in_use; i++) {
        if (!data.selected[i]) {
            continue;
        }
        event = scenario_events_get_using_custom_variable(data.custom_variable_ids[i]);
        if (!event) {
            continue;
        }
        variables_in_use[total_variables_in_use] = data.custom_variable_ids[i];
        total_variables_in_use++;
    }

    // If any variables are in use, alert user and abort
    if (total_variables_in_use) {
        if (total_variables_in_use == 1) {
            show_used_event_sigle_variable_popup_dialog(event);
        } else {
            show_multiple_variables_in_use_popup_dialog(variables_in_use, total_variables_in_use);
        }
        free(variables_in_use);
        return;
    }

    free(variables_in_use);

    // Step 2: Request confirmation
    if (!data.do_not_ask_again_for_delete) {
        const uint8_t *title = lang_get_string(CUSTOM_TRANSLATION, TR_EDITOR_SCENARIO_EVENTS_DELETE_SELECTED_CONFIRM_TITLE);
        const uint8_t *text = lang_get_string(CUSTOM_TRANSLATION, TR_EDITOR_SCENARIO_EVENTS_DELETE_SELECTED_CONFIRM_TEXT);
        const uint8_t *check_text = lang_get_string(CUSTOM_TRANSLATION, TR_SAVE_DIALOG_OVERWRITE_FILE_DO_NOT_ASK_AGAIN);
        window_popup_dialog_show_confirmation(title, text, check_text, delete_selected);
    } else {
        delete_selected(1, 1);
    }
}

static void button_new_variable(const generic_button *button)
{
    if (data.callback) {
        return;
    }
    data.target_index = NO_SELECTION;
    show_name_edit_popup();
}

static void button_ok(const generic_button *button)
{
    if (data.callback) {
        return;
    }
    window_go_back();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);
    if (data.expanded_dropdown != -1 && data.custom_variables_in_use) { // handle dropdowns, starting with expanded
        if (dropdown_button_handle_mouse(m_dialog, &color_dropdowns[data.expanded_dropdown])) {
            window_invalidate();
            return;
        }

    } else {
        for (int i = 0; i < MAX_VISIBLE_GRID_ITEMS; ++i) {
            if (dropdown_button_handle_mouse(m_dialog, &color_dropdowns[i])) {
                window_invalidate();
                return;
            }
        }
    }

    if (grid_box_handle_input(&variable_buttons, m_dialog, 1)) {
        if (data.callback) {
            return;
        }
    }
    int x = 0, y = 0;
    if (variable_buttons.focused_item.is_focused) {
        x = variable_buttons.focused_item.x;
        y = variable_buttons.focused_item.y;
    }

    if (generic_buttons_handle_mouse(m_dialog, x, y, item_buttons, NUM_ITEM_BUTTONS, &data.item_buttons_focus_id) ||
        generic_buttons_handle_mouse(m_dialog, 0, 0, constant_buttons, NUM_CONSTANT_BUTTONS,
            &data.constant_button_focus_id)) {
        return;
    }

    if (input_go_back_requested(m, h)) {
        window_go_back();
    }
}

static void get_tooltip(tooltip_context *c)
{
    if (data.selected && data.custom_variables_in_use && data.constant_button_focus_id == 1) {
        c->precomposed_text = lang_get_string(CUSTOM_TRANSLATION,
            data.selection_type == CHECKBOX_ALL_SELECTED ? TR_SELECT_NONE : TR_SELECT_ALL);
        c->type = TOOLTIP_BUTTON;
        return;
    }

    if (variable_buttons.focused_item.is_focused) {
        unsigned int id = data.custom_variable_ids[variable_buttons.focused_item.position];
        if (dropdown_button_handle_tooltip(&color_dropdowns[id - 1], c)) {
            return;
        }
        // Name
        if (data.item_buttons_focus_id == 2) {
            const uint8_t *name = scenario_custom_variable_get_name(id);
            int name_w = item_buttons[1].width;
            if (name && *name && text_get_width(name, FONT_SMALL_PLAIN) > name_w - 16) {
                c->precomposed_text = name;
                c->type = TOOLTIP_BUTTON;
                return;
            }
        }

        // Display Text
        if (data.item_buttons_focus_id == 4) {
            const uint8_t *display_text = scenario_custom_variable_get_text_display(id);
            int display_w = item_buttons[3].width;
            if (display_text && *display_text && text_get_width(display_text, FONT_SMALL_PLAIN) > display_w - 16) {
                c->precomposed_text = display_text;
                c->type = TOOLTIP_BUTTON;
                return;
            }
        }
    }
}

void window_editor_custom_variables_show(void (*callback)(unsigned int id))
{
    window_type window = {
        WINDOW_EDITOR_CUSTOM_VARIABLES,
        draw_background,
        draw_foreground,
        handle_input,
        get_tooltip
    };
    init(callback);
    window_show(&window);
}

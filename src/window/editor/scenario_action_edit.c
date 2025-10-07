#include "scenario_action_edit.h"

#include "core/string.h"
#include "game/resource.h"
#include "graphics/button.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "scenario/event/controller.h"
#include "scenario/event/parameter_data.h"
#include "scenario/event/parameter_city.h"
#include "scenario/event/action_handler.h"
#include "widget/input_box.h"
#include "window/editor/allowed_buildings.h"
#include "window/editor/custom_variables.h"
#include "window/editor/map.h"
#include "window/editor/requests.h"
#include "window/editor/select_scenario_action_type.h"
#include "window/editor/select_city_by_type.h"
#include "window/editor/select_city_trade_route.h"
#include "window/editor/select_custom_message.h"
#include "window/editor/select_special_attribute_mapping.h"
#include "window/numeric_input.h"
#include "window/select_list.h"
#include "window/text_input.h"

#define BUTTON_LEFT_PADDING 32
#define BUTTON_WIDTH 608
#define DETAILS_Y_OFFSET 128
#define DETAILS_ROW_HEIGHT 32
#define MAX_TEXT_LENGTH 50

static void init(scenario_action_t *action);
static void button_amount(const generic_button *button);
static void button_delete(const generic_button *button);
static void button_change_type(const generic_button *button);
static void set_param_value(int value);
static void set_parameter_being_edited(int value);
static void set_resource_value(int value);
static void resource_selection(const generic_button *button);
static void custom_message_selection(void);
static void change_parameter(xml_data_attribute_t *parameter, const generic_button *button);

static generic_button buttons[] = {
    {BUTTON_LEFT_PADDING, DETAILS_Y_OFFSET + (0 * DETAILS_ROW_HEIGHT), BUTTON_WIDTH, DETAILS_ROW_HEIGHT - 2, button_amount, 0, 1},
    {BUTTON_LEFT_PADDING, DETAILS_Y_OFFSET + (1 * DETAILS_ROW_HEIGHT), BUTTON_WIDTH, DETAILS_ROW_HEIGHT - 2, button_amount, 0, 2},
    {BUTTON_LEFT_PADDING, DETAILS_Y_OFFSET + (2 * DETAILS_ROW_HEIGHT), BUTTON_WIDTH, DETAILS_ROW_HEIGHT - 2, button_amount, 0, 3},
    {BUTTON_LEFT_PADDING, DETAILS_Y_OFFSET + (3 * DETAILS_ROW_HEIGHT), BUTTON_WIDTH, DETAILS_ROW_HEIGHT - 2, button_amount, 0, 4},
    {BUTTON_LEFT_PADDING, DETAILS_Y_OFFSET + (4 * DETAILS_ROW_HEIGHT), BUTTON_WIDTH, DETAILS_ROW_HEIGHT - 2, button_amount, 0, 5},
    {288, 32, 80, 25, button_delete},
    {32, 64, BUTTON_WIDTH, 32, button_change_type}
};

#define MAX_BUTTONS (sizeof(buttons) / sizeof(generic_button))

static struct {
    unsigned int focus_button_id;
    int parameter_being_edited;
    int parameter_being_edited_current_value;

    uint8_t display_text[MAX_TEXT_LENGTH];
    uint8_t formula[MAX_FORMULA_LENGTH];
    unsigned int formula_index;
    scenario_action_t *action;
    scenario_action_data_t *xml_info;
} data;

static uint8_t *translation_for_param_value(parameter_type type, int value)
{
    memset(data.display_text, 0, MAX_TEXT_LENGTH);
    scenario_events_parameter_data_get_display_string_for_value(type, value, data.display_text, MAX_TEXT_LENGTH);
    return data.display_text;
}

static void init(scenario_action_t *action)
{
    data.action = action;
}

static parameter_type get_resolved_parameter_type(xml_data_attribute_t *param_attr, int param_number)
{
    // If the parameter is not flexible, return its type directly
    if (param_attr->type != PARAMETER_TYPE_FLEXIBLE) {
        return param_attr->type;
    }

    // Resolve the flexible type based on the action context
    return scenario_events_parameter_data_resolve_flexible_type(data.action, param_number);
}

static translation_key get_resolved_parameter_key(xml_data_attribute_t *param_attr, int param_number)
{
    if (param_attr->type != PARAMETER_TYPE_FLEXIBLE) {
        return param_attr->key;
    }
    // For flexible parameters in city property actions, get the key from city_property_info_t
    if (data.action->type == ACTION_TYPE_CUSTOM_VARIABLE_CITY_PROPERTY && param_number >= 3 && param_number <= 5) {
        city_property_t city_property = data.action->parameter2;
        city_property_info_t info = city_property_get_param_info(city_property);
        int param_index = param_number - 3;
        if (param_index < info.count) {// Return the translation key if this parameter is needed
            return info.param_keys[param_index];
        }
    }
    // Fallback to the original key if resolution fails
    return param_attr->key;
}

static void draw_background(void)
{
    data.xml_info = scenario_events_parameter_data_get_actions_xml_attributes(data.action->type);
    window_editor_map_draw_all();
}

static void draw_foreground(void)
{
    graphics_in_dialog();

    outer_panel_draw(0, 0, 42, 24);

    for (unsigned int i = 5; i <= 6; i++) {
        large_label_draw(buttons[i].x, buttons[i].y, buttons[i].width / 16, data.focus_button_id == i + 1 ? 1 : 0);
    }

    text_draw_centered(translation_for(TR_EDITOR_DELETE), 288, 40, 80, FONT_NORMAL_GREEN, COLOR_MASK_NONE);

    text_draw_centered(translation_for(data.xml_info->xml_attr.key), 32, 72, BUTTON_WIDTH, FONT_NORMAL_GREEN, COLOR_MASK_NONE);

    unsigned int button_id = 0;
    parameter_type resolved_type1 = get_resolved_parameter_type(&data.xml_info->xml_parm1, 1);
    if (resolved_type1 > PARAMETER_TYPE_UNDEFINED) {
        large_label_draw(buttons[button_id].x, buttons[button_id].y, buttons[button_id].width / 16,
            data.focus_button_id == button_id + 1 ? 1 : 0);
        text_draw_centered(translation_for(data.xml_info->xml_parm1.key),
            buttons[button_id].x, buttons[button_id].y + 8, buttons[button_id].width / 2,
            FONT_NORMAL_GREEN, COLOR_MASK_NONE);
        text_draw_centered(translation_for_param_value(resolved_type1, data.action->parameter1),
            buttons[button_id].x + BUTTON_WIDTH / 2, buttons[button_id].y + 8, buttons[button_id].width / 2,
            FONT_NORMAL_GREEN, COLOR_MASK_NONE);
    }
    button_id++;

    parameter_type resolved_type2 = get_resolved_parameter_type(&data.xml_info->xml_parm2, 2);
    if (resolved_type2 > PARAMETER_TYPE_UNDEFINED) {
        large_label_draw(buttons[button_id].x, buttons[button_id].y, buttons[button_id].width / 16,
            data.focus_button_id == button_id + 1 ? 1 : 0);
        text_draw_centered(translation_for(data.xml_info->xml_parm2.key),
            buttons[button_id].x, buttons[button_id].y + 8, buttons[button_id].width / 2,
            FONT_NORMAL_GREEN, COLOR_MASK_NONE);
        text_draw_centered(translation_for_param_value(resolved_type2, data.action->parameter2),
            buttons[button_id].x + BUTTON_WIDTH / 2, buttons[button_id].y + 8, buttons[button_id].width / 2,
            FONT_NORMAL_GREEN, COLOR_MASK_NONE);
    }
    button_id++;

    parameter_type resolved_type3 = get_resolved_parameter_type(&data.xml_info->xml_parm3, 3);
    if (resolved_type3 > PARAMETER_TYPE_UNDEFINED) {
        large_label_draw(buttons[button_id].x, buttons[button_id].y, buttons[button_id].width / 16,
            data.focus_button_id == button_id + 1 ? 1 : 0);
        text_draw_centered(translation_for(get_resolved_parameter_key(&data.xml_info->xml_parm3, 3)),
            buttons[button_id].x, buttons[button_id].y + 8, buttons[button_id].width / 2,
            FONT_NORMAL_GREEN, COLOR_MASK_NONE);
        text_draw_centered(translation_for_param_value(resolved_type3, data.action->parameter3),
            buttons[button_id].x + BUTTON_WIDTH / 2, buttons[button_id].y + 8, buttons[button_id].width / 2,
            FONT_NORMAL_GREEN, COLOR_MASK_NONE);
    }
    button_id++;

    parameter_type resolved_type4 = get_resolved_parameter_type(&data.xml_info->xml_parm4, 4);
    if (resolved_type4 > PARAMETER_TYPE_UNDEFINED) {
        large_label_draw(buttons[button_id].x, buttons[button_id].y, buttons[button_id].width / 16,
            data.focus_button_id == button_id + 1 ? 1 : 0);
        text_draw_centered(translation_for(get_resolved_parameter_key(&data.xml_info->xml_parm4, 4)),
            buttons[button_id].x, buttons[button_id].y + 8, buttons[button_id].width / 2,
            FONT_NORMAL_GREEN, COLOR_MASK_NONE);
        text_draw_centered(translation_for_param_value(resolved_type4, data.action->parameter4),
            buttons[button_id].x + BUTTON_WIDTH / 2, buttons[button_id].y + 8, buttons[button_id].width / 2,
            FONT_NORMAL_GREEN, COLOR_MASK_NONE);
    }
    button_id++;

    parameter_type resolved_type5 = get_resolved_parameter_type(&data.xml_info->xml_parm5, 5);
    if (resolved_type5 > PARAMETER_TYPE_UNDEFINED) {
        large_label_draw(buttons[button_id].x, buttons[button_id].y, buttons[button_id].width / 16,
            data.focus_button_id == button_id + 1 ? 1 : 0);
        text_draw_centered(translation_for(get_resolved_parameter_key(&data.xml_info->xml_parm5, 5)),
            buttons[button_id].x, buttons[button_id].y + 8, buttons[button_id].width / 2,
            FONT_NORMAL_GREEN, COLOR_MASK_NONE);
        text_draw_centered(translation_for_param_value(resolved_type5, data.action->parameter5),
            buttons[button_id].x + BUTTON_WIDTH / 2, buttons[button_id].y + 8, buttons[button_id].width / 2,
            FONT_NORMAL_GREEN, COLOR_MASK_NONE);
    }

    lang_text_draw_centered(13, 3, 32, 32 + 16 * 20, BUTTON_WIDTH, FONT_NORMAL_BLACK);

    graphics_reset_dialog();
}

static void close_window(void)
{
    scenario_action_type_init(data.action);
    window_go_back();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);
    if (generic_buttons_handle_mouse(m_dialog, 0, 0, buttons, MAX_BUTTONS, &data.focus_button_id)) {
        return;
    }
    if (input_go_back_requested(m, h)) {
        close_window();
    }
}

static void button_delete(const generic_button *button)
{
    scenario_action_type_delete(data.action);
    close_window();
}

static void button_change_type(const generic_button *button)
{
    window_editor_select_scenario_action_type_show(data.action);
}

static void button_amount(const generic_button *button)
{
    // For flexible parameters, we need to resolve their actual types
    xml_data_attribute_t resolved_param;
    xml_data_attribute_t *param_ptr;

    switch (button->parameter1) {
        case 1:
            param_ptr = &data.xml_info->xml_parm1;
            break;
        case 2:
            param_ptr = &data.xml_info->xml_parm2;
            break;
        case 3:
            param_ptr = &data.xml_info->xml_parm3;
            break;
        case 4:
            param_ptr = &data.xml_info->xml_parm4;
            break;
        case 5:
            param_ptr = &data.xml_info->xml_parm5;
            break;
        default:
            return;
    }

    // If it's a flexible type, create a resolved copy
    if (param_ptr->type == PARAMETER_TYPE_FLEXIBLE) {
        resolved_param = *param_ptr;
        resolved_param.type = scenario_events_parameter_data_resolve_flexible_type(data.action, button->parameter1);
        change_parameter(&resolved_param, button);
    } else {
        change_parameter(param_ptr, button);
    }
}

static void set_param_value(int value)
{
    switch (data.parameter_being_edited) {
        case 1:
            data.action->parameter1 = value;
            return;
        case 2:
            data.action->parameter2 = value;
            // For ACTION_TYPE_CUSTOM_VARIABLE_CITY_PROPERTY, when parameter2 (city_property) changes,
            // we need to reset parameters 3-5 to their default values based on the newly resolved flexible types
            if (data.action->type == ACTION_TYPE_CUSTOM_VARIABLE_CITY_PROPERTY) {
                // Reset parameters 3-5 using the resolved flexible types
                xml_data_attribute_t resolved_param;
                
                // Parameter 3
                resolved_param = data.xml_info->xml_parm3;
                resolved_param.type = scenario_events_parameter_data_resolve_flexible_type(data.action, 3);
                data.action->parameter3 = scenario_events_parameter_data_get_default_value_for_parameter(&resolved_param);
                
                // Parameter 4
                resolved_param = data.xml_info->xml_parm4;
                resolved_param.type = scenario_events_parameter_data_resolve_flexible_type(data.action, 4);
                data.action->parameter4 = scenario_events_parameter_data_get_default_value_for_parameter(&resolved_param);
                
                // Parameter 5
                resolved_param = data.xml_info->xml_parm5;
                resolved_param.type = scenario_events_parameter_data_resolve_flexible_type(data.action, 5);
                data.action->parameter5 = scenario_events_parameter_data_get_default_value_for_parameter(&resolved_param);
            }
            return;
        case 3:
            data.action->parameter3 = value;
            return;
        case 4:
            data.action->parameter4 = value;
            return;
        case 5:
            data.action->parameter5 = value;
            return;
        default:
            return;
    }
}

static void set_parameter_being_edited(int value)
{
    data.parameter_being_edited = value;
    switch (value) {
        case 1:
            data.parameter_being_edited_current_value = data.action->parameter1;
            break;
        case 2:
            data.parameter_being_edited_current_value = data.action->parameter2;
            break;
        case 3:
            data.parameter_being_edited_current_value = data.action->parameter3;
            break;
        case 4:
            data.parameter_being_edited_current_value = data.action->parameter4;
            break;
        case 5:
            data.parameter_being_edited_current_value = data.action->parameter5;
            break;
        default:
            break;
    }
}

static void set_resource_value(int value)
{
    switch (data.parameter_being_edited) {
        case 1:
            data.action->parameter1 = value + 1;
            return;
        case 2:
            data.action->parameter2 = value + 1;
            return;
        case 3:
            data.action->parameter3 = value + 1;
            return;
        case 4:
            data.action->parameter4 = value + 1;
            return;
        case 5:
            data.action->parameter5 = value + 1;
            return;
        default:
            return;
    }
}

static void resource_selection(const generic_button *button)
{
    static const uint8_t *resource_texts[RESOURCE_MAX];
    for (resource_type resource = RESOURCE_MIN_FOOD; resource < RESOURCE_MAX; resource++) {
        resource_texts[resource - 1] = resource_get_data(resource)->text;
    }
    window_select_list_show_text(screen_dialog_offset_x(), screen_dialog_offset_y(), button,
        resource_texts, RESOURCE_MAX - 1, set_resource_value);
}

static void custom_message_selection(void)
{
    window_editor_select_custom_message_show(set_param_value);
}

static void set_param_custom_variable(unsigned int id)
{
    switch (data.parameter_being_edited) {
        case 1:
            data.action->parameter1 = id;
            return;
        case 2:
            data.action->parameter2 = id;
            return;
        case 3:
            data.action->parameter3 = id;
            return;
        case 4:
            data.action->parameter4 = id;
            return;
        case 5:
            data.action->parameter5 = id;
            return;
        default:
            return;
    }
}

static int get_param_value(void)
{
    switch (data.parameter_being_edited) {
        case 1:
            return data.action->parameter1;
        case 2:
            return data.action->parameter2;
        case 3:
            return data.action->parameter3;
        case 4:
            return data.action->parameter4;
        case 5:
            return data.action->parameter5;
        default:
            return -1;
    }
}

static void set_formula_value(const uint8_t *formula)
{
    strncpy((char *) data.formula, (const char *) formula, MAX_FORMULA_LENGTH - 1);
    data.formula[MAX_FORMULA_LENGTH - 1] = 0;
    // Add formula to list and get its index
    if (!data.formula_index) {
        data.formula_index = scenario_formula_add(data.formula);
        set_param_value(data.formula_index);
    } else {
        // Update existing formula
        scenario_formula_change(data.formula_index, data.formula);
        set_param_value(data.formula_index);
    }
    window_invalidate();
}

static void create_evaluation_formula(void)
{
    int current_index = get_param_value();
    if (current_index > 0) {
        const uint8_t *src = scenario_formula_get_string(current_index);
        if (src) {
            strncpy((char *) data.formula, (const char *) src, MAX_FORMULA_LENGTH - 1);
            data.formula[MAX_FORMULA_LENGTH - 1] = '\0';
            data.formula_index = current_index;
        } else {
            memset(data.formula, 0, MAX_FORMULA_LENGTH);
        }
    }
    window_text_input_expanded_show("FORMULA", "...", data.formula, MAX_FORMULA_LENGTH,
         set_formula_value, INPUT_BOX_CHARS_FORMULAS);
}

static void custom_variable_selection(void)
{
    window_editor_custom_variables_show(set_param_custom_variable);
}

static void set_param_allowed_building(int type)
{
    switch (data.parameter_being_edited) {
        case 1:
            data.action->parameter1 = type;
            return;
        case 2:
            data.action->parameter2 = type;
            return;
        case 3:
            data.action->parameter3 = type;
            return;
        case 4:
            data.action->parameter4 = type;
            return;
        case 5:
            data.action->parameter5 = type;
            return;
        default:
            return;
    }
}

static void change_parameter(xml_data_attribute_t *parameter, const generic_button *button)
{
    set_parameter_being_edited(button->parameter1);
    switch (parameter->type) {
        case PARAMETER_TYPE_NUMBER:
        case PARAMETER_TYPE_MIN_MAX_NUMBER:
            window_numeric_input_bound_show(BUTTON_WIDTH / 2, 0, button, 9, parameter->min_limit, parameter->max_limit,
                set_param_value);
            return;
        case PARAMETER_TYPE_BOOLEAN:
        case PARAMETER_TYPE_BUILDING:
        case PARAMETER_TYPE_BUILDING_COUNTING:
        case PARAMETER_TYPE_CHECK:
        case PARAMETER_TYPE_DIFFICULTY:
        case PARAMETER_TYPE_ENEMY_TYPE:
        case PARAMETER_TYPE_INVASION_TYPE:
        case PARAMETER_TYPE_POP_CLASS:
        case PARAMETER_TYPE_RATING_TYPE:
        case PARAMETER_TYPE_STANDARD_MESSAGE:
        case PARAMETER_TYPE_STORAGE_TYPE:
        case PARAMETER_TYPE_TARGET_TYPE:
        case PARAMETER_TYPE_GOD:
        case PARAMETER_TYPE_CLIMATE:
        case PARAMETER_TYPE_TERRAIN:
        case PARAMETER_TYPE_CITY_PROPERTY:
        case PARAMETER_TYPE_PERCENTAGE:
        case PARAMETER_TYPE_HOUSING_TYPE:
        case PARAMETER_TYPE_AGE_GROUP:
        case PARAMETER_TYPE_PLAYER_TROOPS:
        case PARAMETER_TYPE_ENEMY_CLASS:
        case PARAMETER_TYPE_COVERAGE_BUILDINGS:
            window_editor_select_special_attribute_mapping_show(parameter->type, set_param_value, data.parameter_being_edited_current_value);
            return;
        case PARAMETER_TYPE_ALLOWED_BUILDING:
            window_editor_allowed_buildings_select(set_param_allowed_building, data.parameter_being_edited_current_value);
            return;
        case PARAMETER_TYPE_REQUEST:
            window_editor_requests_show_with_callback(set_param_value);
            return;
        case PARAMETER_TYPE_ROUTE:
            window_editor_select_city_trade_route_show(set_param_value);
            return;
        case PARAMETER_TYPE_FUTURE_CITY:
            window_editor_select_city_by_type_show(set_param_value, EMPIRE_CITY_FUTURE_TRADE);
            return;
        case PARAMETER_TYPE_RESOURCE:
            resource_selection(button);
            return;
        case PARAMETER_TYPE_CUSTOM_MESSAGE:
            custom_message_selection();
            return;
        case PARAMETER_TYPE_CUSTOM_VARIABLE:
            custom_variable_selection();
            return;
        case PARAMETER_TYPE_FORMULA:
            create_evaluation_formula();
            return;
        default:
            return;
    }
}

void window_editor_scenario_action_edit_show(scenario_action_t *action)
{
    window_type window = {
        WINDOW_EDITOR_SCENARIO_ACTION_EDIT,
        draw_background,
        draw_foreground,
        handle_input
    };
    init(action);
    window_show(&window);
}

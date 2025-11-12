#include "export_xml.h"

#include "core/buffer.h"
#include "core/io.h"
#include "core/log.h"
#include "core/string.h"
#include "core/xml_exporter.h"
#include "empire/city.h"
#include "scenario/custom_messages.h"
#include "scenario/custom_variable.h"
#include "scenario/event/controller.h"
#include "scenario/event/parameter_city.h"
#include "scenario/event/parameter_data.h"
#include "window/plain_message_dialog.h"
#include "window/editor/select_city_trade_route.h"

#include <stdio.h>

#define XML_EXPORT_MAX_SIZE 5000000
#define ERROR_MESSAGE_LENGTH 200

static struct {
    int success;
    char error_message[ERROR_MESSAGE_LENGTH];
} data;

static void log_exporting_error(const char *msg)
{
    data.success = 0;
    snprintf(data.error_message, ERROR_MESSAGE_LENGTH, "%s", msg);
    log_error("Error while exporting scenario events to XML. ", data.error_message, 0);

    window_plain_message_dialog_show_with_extra(
        TR_EDITOR_UNABLE_TO_SAVE_EVENTS_TITLE, TR_EDITOR_CHECK_LOG_MESSAGE,
        string_from_ascii(data.error_message),
        0);
}

static int export_attribute_by_type(xml_data_attribute_t *attr, parameter_type type, int target)
{
    special_attribute_mapping_t *found = scenario_events_parameter_data_get_attribute_mapping_by_value(type, target);
    if (found != 0) {
        xml_exporter_add_attribute_text(attr->name, found->text);
        return 1;
    }
    return 0;
}

static int export_attribute_future_city(xml_data_attribute_t *attr, int target)
{
    empire_city *city = empire_city_get(target);
    if (city) {
        const uint8_t *city_name = empire_city_get_name(city);
        xml_exporter_add_attribute_encoded_text(attr->name, city_name);
        return 1;
    }
    return 0;
}

static int export_attribute_number(xml_data_attribute_t *attr, int target)
{
    xml_exporter_add_attribute_int(attr->name, target);
    return 1;
}

static int export_attribute_route(xml_data_attribute_t *attr, int target)
{
    int city_id = empire_city_get_for_trade_route(target);
    if (city_id) {
        empire_city *city = empire_city_get(city_id);
        const uint8_t *city_name = empire_city_get_name(city);
        xml_exporter_add_attribute_encoded_text(attr->name, city_name);
        return 1;
    }
    return 0;
}

static int export_attribute_resource(xml_data_attribute_t *attr, int target)
{
    if (target < RESOURCE_MIN || target > RESOURCE_MAX) {
        log_exporting_error("Error while exporting resource.");
        return 0;
    }

    const char *resource_name = resource_get_data(target)->xml_attr_name;
    char resource_name_to_use[50];

    const char *next = strchr(resource_name, '|');
    if (next) {
        size_t length = ((size_t) (next - resource_name) + 1) * sizeof(char);
        if (length > sizeof(resource_name_to_use)) {
            length = sizeof(resource_name_to_use);
        }
        snprintf(resource_name_to_use, length, "%s", resource_name);
        resource_name = resource_name_to_use;
    }
    xml_exporter_add_attribute_text(attr->name, resource_name);
    return 1;
}

static int export_attribute_custom_message(xml_data_attribute_t *attr, int target)
{
    custom_message_t *message = custom_messages_get(target);
    if (message) {
        const uint8_t *message_uid = message->linked_uid->text;
        xml_exporter_add_attribute_encoded_text(attr->name, message_uid);
        return 1;
    }
    return 0;
}

static int export_attribute_custom_variable(xml_data_attribute_t *attr, int target)
{
    if (!scenario_custom_variable_exists(target)) {
        return 0;
    }
    const uint8_t *name = scenario_custom_variable_get_name(target);
    if (!name) {
        name = string_from_ascii("ERROR_NO_VARIABLE_NAME_GIVEN");
    }
    xml_exporter_add_attribute_encoded_text(attr->name, name);
    return 1;
}

static int export_parse_attribute_with_resolved_type(xml_data_attribute_t *attr, parameter_type resolved_type, int target)
{
    switch (resolved_type) {
        case PARAMETER_TYPE_INVASION_TYPE:
        case PARAMETER_TYPE_BOOLEAN:
        case PARAMETER_TYPE_BUILDING:
        case PARAMETER_TYPE_ALLOWED_BUILDING:
        case PARAMETER_TYPE_CHECK:
        case PARAMETER_TYPE_DIFFICULTY:
        case PARAMETER_TYPE_ENEMY_TYPE:
        case PARAMETER_TYPE_POP_CLASS:
        case PARAMETER_TYPE_RATING_TYPE:
        case PARAMETER_TYPE_STANDARD_MESSAGE:
        case PARAMETER_TYPE_STORAGE_TYPE:
        case PARAMETER_TYPE_TARGET_TYPE:
        case PARAMETER_TYPE_GOD:
        case PARAMETER_TYPE_CLIMATE:
        case PARAMETER_TYPE_TERRAIN:
        case PARAMETER_TYPE_DATA_TYPE:
        case PARAMETER_TYPE_HOUSE_DATA_TYPE:
        case PARAMETER_TYPE_MODEL:
        case PARAMETER_TYPE_PERCENTAGE:
        case PARAMETER_TYPE_HOUSING_TYPE:
        case PARAMETER_TYPE_AGE_GROUP:
        case PARAMETER_TYPE_ENEMY_CLASS:
        case PARAMETER_TYPE_PLAYER_TROOPS:
        case PARAMETER_TYPE_COVERAGE_BUILDINGS:
        case PARAMETER_TYPE_RANK:
        case PARAMETER_TYPE_CITY_PROPERTY:
        case PARAMETER_TYPE_MEDIA_TYPE:
            return export_attribute_by_type(attr, resolved_type, target);
        case PARAMETER_TYPE_BUILDING_COUNTING:
            return export_attribute_by_type(attr, PARAMETER_TYPE_BUILDING, target);
        case PARAMETER_TYPE_ROUTE_RESOURCE:
        {
            int value = window_editor_select_city_trade_route_decode_resource_id(target);
            return export_attribute_number(attr, value);
        }
        case PARAMETER_TYPE_REQUEST:
            return export_attribute_number(attr, target);
        case PARAMETER_TYPE_FUTURE_CITY:
            return export_attribute_future_city(attr, target);
        case PARAMETER_TYPE_MIN_MAX_NUMBER:
            return export_attribute_number(attr, target);
        case PARAMETER_TYPE_NUMBER:
            return export_attribute_number(attr, target);
        case PARAMETER_TYPE_RESOURCE:
            return export_attribute_resource(attr, target);
        case PARAMETER_TYPE_ROUTE:
            return export_attribute_route(attr, target);
        case PARAMETER_TYPE_CUSTOM_MESSAGE:
            return export_attribute_custom_message(attr, target);
        case PARAMETER_TYPE_CUSTOM_VARIABLE:
            return export_attribute_custom_variable(attr, target);
        case PARAMETER_TYPE_FORMULA:
        {
            if (!attr->name) {
                return 1;
            }
            const uint8_t *formula = scenario_formula_get_string(target);
            if (!formula) {
                // If formula ID is invalid, replace with 000
                const char *buffer = "000";
                xml_exporter_add_attribute_text(attr->name, buffer);
            } else {
                xml_exporter_add_attribute_encoded_text(attr->name, formula);
            }
            return 1;
        }
        case PARAMETER_TYPE_UNDEFINED:
            return 1;
        case PARAMETER_TYPE_FLEXIBLE:
            // FLEXIBLE should have been resolved before calling this function
            log_exporting_error("Unresolved FLEXIBLE parameter type encountered during export");
            return 0;
        default:
            log_exporting_error("Something is very wrong. Failed to find attribute type.");
            return 0;
    }
}

static int export_parse_attribute(xml_data_attribute_t *attr, int target)
{
    return export_parse_attribute_with_resolved_type(attr, attr->type, target);
}

static void export_event_condition(scenario_condition_t *condition)
{
    if (condition->type == CONDITION_TYPE_UNDEFINED) {
        return;
    }

    scenario_condition_data_t *condition_data = scenario_events_parameter_data_get_conditions_xml_attributes(condition->type);
    if (condition_data->type == CONDITION_TYPE_UNDEFINED) {
        return;
    }

    if (condition_data->xml_attr.name) {
        xml_exporter_new_element(condition_data->xml_attr.name);
    } else {
        log_exporting_error("Error while exporting condition.");
        return;
    }

    export_parse_attribute(&condition_data->xml_parm1, condition->parameter1);
    export_parse_attribute(&condition_data->xml_parm2, condition->parameter2);
    export_parse_attribute(&condition_data->xml_parm3, condition->parameter3);
    export_parse_attribute(&condition_data->xml_parm4, condition->parameter4);
    export_parse_attribute(&condition_data->xml_parm5, condition->parameter5);

    xml_exporter_close_element();
}

static void export_event_action(scenario_action_t *action)
{
    if (action->type == ACTION_TYPE_UNDEFINED) {
        return;
    }

    scenario_action_data_t *action_data = scenario_events_parameter_data_get_actions_xml_attributes(action->type);
    if (action_data->type == ACTION_TYPE_UNDEFINED) {
        return;
    }

    if (action_data->xml_attr.name) {
        xml_exporter_new_element(action_data->xml_attr.name);
    } else {
        log_exporting_error("Error while exporting action.");
        return;
    }

    export_parse_attribute(&action_data->xml_parm1, action->parameter1);
    export_parse_attribute(&action_data->xml_parm2, action->parameter2);

    // Handle flexible parameters - resolve them to their actual types and get proper attribute names
    if (action_data->xml_parm3.type == PARAMETER_TYPE_FLEXIBLE) {
        parameter_type type3 = scenario_events_parameter_data_resolve_flexible_type(action, 3);
        city_property_info_t info = city_property_get_param_info(action->parameter2);
        if (type3 != PARAMETER_TYPE_UNDEFINED && info.count >= 1) {
            xml_data_attribute_t resolved_attr = action_data->xml_parm3;
            resolved_attr.type = type3;
            resolved_attr.name = info.param_names[0];
            export_parse_attribute_with_resolved_type(&resolved_attr, type3, action->parameter3);
        }
    } else {
        export_parse_attribute(&action_data->xml_parm3, action->parameter3);
    }

    if (action_data->xml_parm4.type == PARAMETER_TYPE_FLEXIBLE) {
        parameter_type type4 = scenario_events_parameter_data_resolve_flexible_type(action, 4);
        city_property_info_t info = city_property_get_param_info(action->parameter2);
        if (type4 != PARAMETER_TYPE_UNDEFINED && info.count >= 2) {
            xml_data_attribute_t resolved_attr = action_data->xml_parm4;
            resolved_attr.type = type4;
            resolved_attr.name = info.param_names[1];
            export_parse_attribute_with_resolved_type(&resolved_attr, type4, action->parameter4);
        }
    } else {
        export_parse_attribute(&action_data->xml_parm4, action->parameter4);
    }

    if (action_data->xml_parm5.type == PARAMETER_TYPE_FLEXIBLE) {
        parameter_type type5 = scenario_events_parameter_data_resolve_flexible_type(action, 5);
        city_property_info_t info = city_property_get_param_info(action->parameter2);
        if (type5 != PARAMETER_TYPE_UNDEFINED && info.count >= 3) {
            xml_data_attribute_t resolved_attr = action_data->xml_parm5;
            resolved_attr.type = type5;
            resolved_attr.name = info.param_names[2];
            export_parse_attribute_with_resolved_type(&resolved_attr, type5, action->parameter5);
        }
    } else {
        export_parse_attribute(&action_data->xml_parm5, action->parameter5);
    }

    xml_exporter_close_element();
}

static int export_event(scenario_event_t *event)
{
    if (event->state == EVENT_STATE_UNDEFINED) {
        return 1;
    }

    xml_exporter_new_element("event");

    xml_exporter_add_attribute_int("id", event->id);

    if (*event->name) {
        xml_exporter_add_attribute_encoded_text("name", event->name);
    }
    if (event->repeat_days_min > 0) {
        xml_exporter_add_attribute_int("repeat_days_min", event->repeat_days_min);
    }
    if (event->repeat_days_max > 0) {
        xml_exporter_add_attribute_int("repeat_days_max", event->repeat_days_max);
    }
    if (event->max_number_of_repeats > 0) {
        xml_exporter_add_attribute_int("max_number_of_repeats", event->max_number_of_repeats);
    }

    xml_exporter_new_element("conditions");

    const scenario_condition_group_t *group;
    for (unsigned int i = 0; i < event->condition_groups.size; i++) {
        group = array_item(event->condition_groups, i);
        if (group->conditions.size > 0) {
            if (group->type != FULFILLMENT_TYPE_ALL) {
                xml_exporter_new_element("group");
            }
            for (unsigned int j = 0; j < group->conditions.size; j++) {
                scenario_condition_t *condition = array_item(group->conditions, j);
                export_event_condition(condition);
            }
            if (group->type != FULFILLMENT_TYPE_ALL) {
                xml_exporter_close_element();
            }
        }
    }
    xml_exporter_close_element();

    xml_exporter_new_element("actions");
    for (unsigned int i = 0; i < event->actions.size; i++) {
        scenario_action_t *action = array_item(event->actions, i);
        export_event_action(action);
    }
    xml_exporter_close_element();

    xml_exporter_close_element();

    return 1;
}

static void export_scenario_variables(buffer *buf)
{
    xml_exporter_new_element("variables");

    unsigned int total_variables = scenario_custom_variable_count();

    for (unsigned int i = 0; i < total_variables; i++) {
        if (!scenario_custom_variable_exists(i)) {
            continue;
        }
        xml_exporter_new_element("variable");

        xml_exporter_add_attribute_encoded_text("name", scenario_custom_variable_get_name(i));
        xml_exporter_add_attribute_int("value", scenario_custom_variable_get_value(i));
        xml_exporter_add_attribute_int("visible", scenario_custom_variable_is_visible(i));
        xml_exporter_add_attribute_encoded_text("text", scenario_custom_variable_get_text_display(i));
        xml_exporter_add_attribute_int("colour_group", scenario_custom_variable_get_color_group(i));

        xml_exporter_close_element();
    }
    xml_exporter_close_element();
    xml_exporter_newline();
}

static void export_scenario_events(buffer *buf)
{
    xml_exporter_new_element("events");
    xml_exporter_add_attribute_int("version", SCENARIO_EVENTS_XML_CURRENT_VERSION);

    export_scenario_variables(buf);

    int event_count = scenario_events_get_count();
    for (int i = 0; i < event_count; i++) {
        scenario_event_t *event = scenario_event_get(i);
        export_event(event);
    }
    xml_exporter_close_element();
    xml_exporter_newline();
}

int scenario_events_export_to_xml(const char *filename)
{
    buffer buf;
    int buf_size = XML_EXPORT_MAX_SIZE;
    uint8_t *buf_data = malloc(buf_size);
    if (!buf_data) {
        log_error("Unable to allocate buffer to export scenario events XML", 0, 0);
        free(buf_data);
        return 0;
    }
    buffer_init(&buf, buf_data, buf_size);
    xml_exporter_init(&buf, "events");
    export_scenario_events(&buf);
    io_write_buffer_to_file(filename, buf.data, buf.index);
    free(buf_data);
    return 1;
}

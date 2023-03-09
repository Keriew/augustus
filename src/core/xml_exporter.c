#include "xml_exporter.h"

#include "core/log.h"
#include "core/string.h"
#include "empire/city.h"
#include "game/settings.h"
#include "scenario/scenario_events_controller.h"
#include "scenario/scenario_events_xml.h"
#include "translation/translation.h"
#include "window/plain_message_dialog.h"

#include <stdio.h>
#include <string.h>

#define SCENARIO_EVENTS_XML_VERSION 1

#define WHITESPACES_PER_TAB 4
#define TAB_INDENTS_1 (WHITESPACES_PER_TAB * 1)
#define TAB_INDENTS_2 (WHITESPACES_PER_TAB * 2)


static struct {
    int success;
    char error_message[200];
} data;

static void xml_exporter_log_error(const char *msg);
static int xml_export_ascii_as_string(buffer *buf, char *text_out);
static int xml_export_ascii_as_const_string(buffer *buf, const char *text_out);

static int xml_exporter_event_condition(buffer *buf, scenario_condition_t *condition);
static int xml_exporter_event_action(buffer *buf, scenario_action_t *action);

static special_attribute_mapping_t *xml_exporter_get_attribute_mapping(
    int target, special_attribute_mapping_t array[], int array_size);

static int xml_exporter_parse_attribute(buffer *buf, xml_data_attribute_t *attr, int target);
static int xml_exporter_attribute_allowed_building(buffer *buf, xml_data_attribute_t *attr, int target);
static int xml_exporter_attribute_boolean(buffer *buf, xml_data_attribute_t *attr, int target);
static int xml_exporter_attribute_building(buffer *buf, xml_data_attribute_t *attr, int target);
static int xml_exporter_attribute_check(buffer *buf, xml_data_attribute_t *attr, int target);
static int xml_exporter_attribute_difficulty(buffer *buf, xml_data_attribute_t *attr, int target);
static int xml_exporter_attribute_future_city(buffer *buf, xml_data_attribute_t *attr, int target);
static int xml_exporter_attribute_number(buffer *buf, xml_data_attribute_t *attr, int target);
static int xml_exporter_attribute_pop_class(buffer *buf, xml_data_attribute_t *attr, int target);
static int xml_exporter_attribute_resource(buffer *buf, xml_data_attribute_t *attr, int target);
static int xml_exporter_attribute_route(buffer *buf, xml_data_attribute_t *attr, int target);
static int xml_exporter_attribute_standard_message(buffer *buf, xml_data_attribute_t *attr, int target);

static int xml_export_ascii_as_string(buffer *buf, char *text_out)
{
    const uint8_t *converted = string_from_ascii(text_out);
    buffer_write_raw(buf, converted, string_length(converted));
    return 1;
}

static int xml_export_ascii_as_const_string(buffer *buf, const char *text_out)
{
    const uint8_t *converted = string_from_ascii(text_out);
    buffer_write_raw(buf, converted, string_length(converted));
    return 1;
}

static void xml_exporter_log_error(const char *msg)
{
    data.success = 0;
    strcpy(data.error_message, msg);
    log_error("Error while exporting scenario events to XML. ", data.error_message, 0);

    window_plain_message_dialog_show_with_extra(
        TR_EDITOR_UNABLE_TO_SAVE_EVENTS_TITLE, TR_EDITOR_CHECK_LOG_MESSAGE,
        string_from_ascii(data.error_message));
}

int xml_exporter_whitespaces(buffer *buf, int count)
{
    uint8_t text_out[60];
    for (int i = 0; i < count; i++) {
        text_out[i] = ' ';
    }
    buffer_write_raw(buf, text_out, count);
    return 1;
}

int xml_exporter_newline(buffer *buf)
{
    return xml_export_ascii_as_string(buf, "\n");
}

int xml_exporter_doctype_header_events(buffer *buf)
{
    buffer_write_raw(buf, "<?xml version=\"1.0\"?>", 21);
    xml_exporter_newline(buf);

    const uint8_t text_out_2[] = "<!DOCTYPE events>";
    buffer_write_raw(buf, text_out_2, string_length(text_out_2));
    return xml_exporter_newline(buf);
}

int xml_exporter_open_tag_start(buffer *buf, const uint8_t *tag, int indent)
{
    xml_exporter_whitespaces(buf, indent);
    buffer_write_raw(buf, "<", 1);
    buffer_write_raw(buf, tag, string_length(tag));

    return 1;
}

int xml_exporter_open_tag_attribute_int(buffer *buf, const char *name, int value)
{
    uint8_t attr_value[200];
    int value_length = string_from_int(attr_value, value, 0);

    xml_exporter_whitespaces(buf, 1);
    xml_export_ascii_as_const_string(buf, name);
    const uint8_t text_out[] = "=\"";
    buffer_write_raw(buf, text_out, 2);
    buffer_write_raw(buf, attr_value, value_length);
    const uint8_t text_out_2[] = "\"";
    buffer_write_raw(buf, text_out_2, 1);

    return 1;
}

int xml_exporter_open_tag_attribute(buffer *buf, const char *name, const uint8_t *value)
{
    xml_exporter_whitespaces(buf, 1);
    xml_export_ascii_as_const_string(buf, name);
    char text_out[] = "=\"";
    xml_export_ascii_as_string(buf, text_out);
    buffer_write_raw(buf, value, string_length(value));
    char text_out_2[] = "\"";
    xml_export_ascii_as_string(buf, text_out_2);

    return 1;
}

int xml_exporter_open_tag_end(buffer *buf, int finally)
{
    if (finally) {
        buffer_write_raw(buf, "/>", 2);
    } else {
        buffer_write_raw(buf, ">", 1);
    }
    return xml_exporter_newline(buf);
}

int xml_exporter_ending_tag(buffer *buf, const uint8_t *tag, int indent)
{
    xml_exporter_whitespaces(buf, indent);
    buffer_write_raw(buf, "</", 2);
    buffer_write_raw(buf, tag, string_length(tag));
    buffer_write_raw(buf, ">", 1);
    return xml_exporter_newline(buf);
}

static int xml_exporter_event_condition(buffer *buf, scenario_condition_t *condition)
{
    const uint8_t text_out[] = "condition";
    xml_exporter_open_tag_start(buf, text_out, TAB_INDENTS_2);
    scenario_condition_data_t *data = scenario_conditions_get_xml_attributes(condition->type);

    if (data->xml_attr.name) {
        xml_exporter_open_tag_attribute(buf, "type", string_from_ascii(data->xml_attr.name));
    } else {
        xml_exporter_log_error("Error while exporting condition.");
        return 0;
    }

    xml_exporter_parse_attribute(buf, &data->xml_parm1, condition->parameter1);
    xml_exporter_parse_attribute(buf, &data->xml_parm2, condition->parameter2);
    xml_exporter_parse_attribute(buf, &data->xml_parm3, condition->parameter3);
    xml_exporter_parse_attribute(buf, &data->xml_parm4, condition->parameter4);
    xml_exporter_parse_attribute(buf, &data->xml_parm5, condition->parameter5);

    xml_exporter_open_tag_end(buf, 1);
    return 1;
}

static int xml_exporter_event_action(buffer *buf, scenario_action_t *action)
{
    const uint8_t text_out[] = "action";
    xml_exporter_open_tag_start(buf, text_out, TAB_INDENTS_2);
    scenario_action_data_t *data = scenario_actions_get_xml_attributes(action->type);

    if (data->xml_attr.name) {
        xml_exporter_open_tag_attribute(buf, "type", string_from_ascii(data->xml_attr.name));
    } else {
        xml_exporter_log_error("Error while exporting action.");
        return 0;
    }

    xml_exporter_parse_attribute(buf, &data->xml_parm1, action->parameter1);
    xml_exporter_parse_attribute(buf, &data->xml_parm2, action->parameter2);
    xml_exporter_parse_attribute(buf, &data->xml_parm3, action->parameter3);
    xml_exporter_parse_attribute(buf, &data->xml_parm4, action->parameter4);
    xml_exporter_parse_attribute(buf, &data->xml_parm5, action->parameter5);

    xml_exporter_open_tag_end(buf, 1);
    return 1;
}


#pragma region ATTRIBUTE_PARSING


static int xml_exporter_parse_attribute(buffer *buf, xml_data_attribute_t *attr, int target)
{
    switch (attr->type) {
        case PARAMETER_TYPE_ALLOWED_BUILDING: return xml_exporter_attribute_allowed_building(buf, attr, target);
        case PARAMETER_TYPE_BOOLEAN: return xml_exporter_attribute_boolean(buf, attr, target);
        case PARAMETER_TYPE_BUILDING: return xml_exporter_attribute_building(buf, attr, target);
        case PARAMETER_TYPE_BUILDING_COUNTING: return xml_exporter_attribute_building(buf, attr, target);
        case PARAMETER_TYPE_CHECK: return xml_exporter_attribute_check(buf, attr, target);
        case PARAMETER_TYPE_DIFFICULTY: return xml_exporter_attribute_difficulty(buf, attr, target);
        case PARAMETER_TYPE_FUTURE_CITY: return xml_exporter_attribute_future_city(buf, attr, target);
        case PARAMETER_TYPE_MIN_MAX_NUMBER: return xml_exporter_attribute_number(buf, attr, target);
        case PARAMETER_TYPE_NUMBER: return xml_exporter_attribute_number(buf, attr, target);
        case PARAMETER_TYPE_POP_CLASS: return xml_exporter_attribute_pop_class(buf, attr, target);
        case PARAMETER_TYPE_RESOURCE: return xml_exporter_attribute_resource(buf, attr, target);
        case PARAMETER_TYPE_ROUTE: return xml_exporter_attribute_route(buf, attr, target);
        case PARAMETER_TYPE_STANDARD_MESSAGE: return xml_exporter_attribute_standard_message(buf, attr, target);
        case PARAMETER_TYPE_UNDEFINED:
            return 1;
        default:
            xml_exporter_log_error("Something is very wrong. Failed to find attribute type.");
            return 0;
    }
}

static int xml_exporter_attribute_boolean(buffer *buf, xml_data_attribute_t *attr, int target)
{
    for (int i = 0; i < SPECIAL_ATTRIBUTE_MAPPINGS_BOOLEAN_SIZE; i++) {
        special_attribute_mapping_t *current = &special_attribute_mappings_boolean[i];
        if (target == current->value) {
            xml_exporter_open_tag_attribute(buf, attr->name, string_from_ascii(current->text));
            return 1;
        }
    }
    return 0;
}

static special_attribute_mapping_t *xml_exporter_get_attribute_mapping(int target, special_attribute_mapping_t array[], int array_size)
{
    for (int i = 0; i < array_size; i++) {
        special_attribute_mapping_t *current = &array[i];
        if (target == current->value) {
            return current;
        }
    }
    return 0;
}

static int xml_exporter_attribute_allowed_building(buffer *buf, xml_data_attribute_t *attr, int target)
{
    for (int i = 0; i < SPECIAL_ATTRIBUTE_MAPPINGS_ALLOWED_BUILDINGS_SIZE; i++) {
        special_attribute_mapping_t *current = &special_attribute_mappings_allowed_buildings[i];
        if (target == current->value) {
            xml_exporter_open_tag_attribute(buf, attr->name, string_from_ascii(current->text));
            return 1;
        }
    }
    return 0;
}

static int xml_exporter_attribute_building(buffer *buf, xml_data_attribute_t *attr, int target)
{
    special_attribute_mapping_t *found = xml_exporter_get_attribute_mapping(
        target, special_attribute_mappings_buildings, BUILDING_TYPE_MAX);
    if (found != 0) {
        xml_exporter_open_tag_attribute(buf, attr->name, string_from_ascii(found->text));
        return 1;
    }
    return 0;
}

static int xml_exporter_attribute_check(buffer *buf, xml_data_attribute_t *attr, int target)
{
    for (int i = 0; i < SPECIAL_ATTRIBUTE_MAPPINGS_CHECK_SIZE; i++) {
        special_attribute_mapping_t *current = &special_attribute_mappings_check[i];
        if (target == current->value) {
            xml_exporter_open_tag_attribute(buf, attr->name, string_from_ascii(current->text));
            return 1;
        }
    }
    return 0;
}

static int xml_exporter_attribute_difficulty(buffer *buf, xml_data_attribute_t *attr, int target)
{
    for (int i = 0; i < SPECIAL_ATTRIBUTE_MAPPINGS_CHECK_DIFFICULTY; i++) {
        special_attribute_mapping_t *current = &special_attribute_mappings_difficulty[i];
        if (target == current->value) {
            xml_exporter_open_tag_attribute(buf, attr->name, string_from_ascii(current->text));
            return 1;
        }
    }
    return 0;
}

static int xml_exporter_attribute_future_city(buffer *buf, xml_data_attribute_t *attr, int target)
{
    empire_city *city = empire_city_get(target);
    if (city) {
        const uint8_t *city_name = empire_city_get_name(city);
        xml_exporter_open_tag_attribute(buf, attr->name, city_name);
        return 1;
    }
    return 0;
}

static int xml_exporter_attribute_number(buffer *buf, xml_data_attribute_t *attr, int target)
{
    xml_exporter_open_tag_attribute_int(buf, attr->name, target);
    return 1;
}

static int xml_exporter_attribute_pop_class(buffer *buf, xml_data_attribute_t *attr, int target)
{
    for (int i = 0; i < SPECIAL_ATTRIBUTE_MAPPINGS_POP_CLASS_SIZE; i++) {
        special_attribute_mapping_t *current = &special_attribute_mappings_pop_class[i];
        if (target == current->value) {
            xml_exporter_open_tag_attribute(buf, attr->name, string_from_ascii(current->text));
            return 1;
        }
    }
    return 0;
}

static int xml_exporter_attribute_route(buffer *buf, xml_data_attribute_t *attr, int target)
{
    int city_id = empire_city_get_for_trade_route(target);
    if (city_id) {
        empire_city *city = empire_city_get(city_id);
        const uint8_t *city_name = empire_city_get_name(city);
        xml_exporter_open_tag_attribute(buf, attr->name, city_name);
        return 1;
    }
    return 0;
}

static int xml_exporter_attribute_resource(buffer *buf, xml_data_attribute_t *attr, int target)
{
    const char *resource_name = resource_get_data(target)->xml_attr_name;
    char resource_name_to_use[50] = " ";

    const char *next = strchr(resource_name, '|');
    size_t length = next ? (next - resource_name) : strlen(resource_name);
    if (length > 48) {
        length = 48;
    }
    strncpy(resource_name_to_use, resource_name, length);

    xml_exporter_open_tag_attribute(buf, attr->name, string_from_ascii(resource_name_to_use));
    return 1;
}

static int xml_exporter_attribute_standard_message(buffer *buf, xml_data_attribute_t *attr, int target)
{
    for (int i = 0; i < SPECIAL_ATTRIBUTE_MAPPINGS_STANDARD_MESSAGE_SIZE; i++) {
        special_attribute_mapping_t *current = &special_attribute_mappings_standard_message[i];
        if (target == current->value) {
            xml_exporter_open_tag_attribute(buf, attr->name, string_from_ascii(current->text));
            return 1;
        }
    }
    return 0;
}

#pragma endregion ATTRIBUTE_PARSING

static int xml_exporter_event(buffer *buf, scenario_event_t *event)
{
    const uint8_t text_out[] = "event";
    xml_exporter_open_tag_start(buf, text_out, TAB_INDENTS_1);

    if (event->repeat_months_min > 0) {
        const char text_out_2[] = "repeat_months_min";
        xml_exporter_open_tag_attribute_int(buf, text_out_2, event->repeat_months_min);
    }
    if (event->repeat_months_max > 0) {
        const char text_out_3[] = "repeat_months_max";
        xml_exporter_open_tag_attribute_int(buf, text_out_3, event->repeat_months_max);
    }
    if (event->max_number_of_repeats > 0) {
        const char text_out_4[] = "max_number_of_repeats";
        xml_exporter_open_tag_attribute_int(buf, text_out_4, event->max_number_of_repeats);
    }
    xml_exporter_open_tag_end(buf, 0);

    for (int i = 0; i < event->conditions.size; i++) {
        scenario_condition_t *condition = array_item(event->conditions, i);
        xml_exporter_event_condition(buf, condition);
    }
    for (int i = 0; i < event->actions.size; i++) {
        scenario_action_t *action = array_item(event->actions, i);
        xml_exporter_event_action(buf, action);
    }

    
    xml_exporter_ending_tag(buf, text_out, TAB_INDENTS_1);
    
    return 1;
}

int xml_exporter_scenario_events(buffer *buf)
{
    xml_exporter_doctype_header_events(buf);

    const uint8_t text_out[] = "events";
    xml_exporter_open_tag_start(buf, text_out, 0);
    const char text_out_2[] = "version";
    xml_exporter_open_tag_attribute_int(buf, text_out_2, SCENARIO_EVENTS_XML_VERSION);
    xml_exporter_open_tag_end(buf, 0);

    int event_count = scenario_events_get_count();
    for (int i = 0; i < event_count; i++) {
        scenario_event_t *event = scenario_event_get(i);
        xml_exporter_event(buf, event);
    }
    xml_exporter_ending_tag(buf, text_out, 0);
    return 1;
}
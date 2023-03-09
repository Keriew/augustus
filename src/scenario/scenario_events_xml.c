#include "scenario_events_xml.h"

#include "assets/assets.h"
#include "core/array.h"
#include "core/buffer.h"
#include "core/calc.h"
#include "core/file.h"
#include "core/image_group.h"
#include "core/io.h"
#include "core/lang.h"
#include "core/log.h"
#include "core/string.h"
#include "core/xml_exporter.h"
#include "core/xml_parser.h"
#include "core/zlib_helper.h"
#include "empire/city.h"
#include "empire/type.h"
#include "scenario/scenario_event.h"
#include "scenario/scenario_event_data.h"
#include "scenario/scenario_events_controller.h"
#include "window/plain_message_dialog.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define XML_TOTAL_ELEMENTS 4
#define XML_EXPORT_MAX_SIZE 5000000

static struct {
    int success;
    char error_message[200];
    int error_line_number;
    uint8_t error_line_number_text[50];
    int version;
    scenario_event_t *current_event;
} data;

static int xml_import_start_scenario_events(void);
static int xml_import_start_event(void);
static int xml_import_start_condition(void);
static int xml_import_start_action(void);
static void xml_import_log_error(const char *msg);

static special_attribute_mapping_t *xml_exporter_get_attribute_mapping(
    const char *value, special_attribute_mapping_t array[], int array_size);

static int xml_import_special_parse_attribute(xml_data_attribute_t *attr, int *target);
static int xml_import_special_parse_allowed_buildings(xml_data_attribute_t *attr, int *target);
static int xml_import_special_parse_boolean(xml_data_attribute_t *attr, int *target);
static int xml_import_special_parse_building(xml_data_attribute_t *attr, int *target);
static int xml_import_special_parse_building_counting(xml_data_attribute_t *attr, int *target);
static int xml_import_special_parse_check(xml_data_attribute_t *attr, int *target);
static int xml_import_special_parse_difficulty(xml_data_attribute_t *attr, int *target);
static int xml_import_special_parse_future_city(xml_data_attribute_t *attr, int *target);
static int xml_import_special_parse_limited_number(xml_data_attribute_t *attr, int *target);
static int xml_import_special_parse_min_max_number(xml_data_attribute_t *attr, int *target);
static int xml_import_special_parse_pop_class(xml_data_attribute_t *attr, int *target);
static int xml_import_special_parse_resource(xml_data_attribute_t *attr, int *target);
static int xml_import_special_parse_route(xml_data_attribute_t *attr, int *target);
static int xml_import_special_parse_standard_message(xml_data_attribute_t *attr, int *target);

static int condition_populate_parameters(scenario_condition_t *condition);
static int action_populate_parameters(scenario_action_t *action);

static const xml_parser_element xml_elements[XML_TOTAL_ELEMENTS] = {
    { "events", xml_import_start_scenario_events },
    { "event", xml_import_start_event, 0, "events" },
    { "condition", xml_import_start_condition, 0, "event" },
    { "action", xml_import_start_action, 0, "event" },
};

static int xml_import_start_scenario_events(void)
{
    if (!data.success) {
        return 0;
    }

    data.version = xml_parser_get_attribute_int("version");
    if (!data.version) {
        data.success = 0;
        log_error("No version set", 0, 0);
        return 0;
    }
    return 1;
}

static int xml_import_start_event(void)
{
    if (!data.success) {
        return 0;
    }

    int min = xml_parser_get_attribute_int("repeat_months_min");
    if (!min) { min = 0; }

    int max = xml_parser_get_attribute_int("repeat_months_max");
    if (!max) { max = min; }

    int max_repeats = xml_parser_get_attribute_int("max_number_of_repeats");
    if (!max_repeats) { max_repeats = 0; }

    data.current_event = scenario_event_create(min, max, max_repeats);
    return 1;
}

scenario_condition_data_t *scenario_conditions_get_xml_attributes(condition_types type)
{
    return &scenario_condition_data[type];
}

static condition_types get_condition_type_from_attr(const char *key)
{
    const char *value = xml_parser_get_attribute_string(key);
    if (!value) {
        return CONDITION_TYPE_UNDEFINED;
    }
    for (condition_types i = CONDITION_TYPE_MIN; i < CONDITION_TYPE_MAX; i++) {
        const char *condition_name = scenario_conditions_get_xml_attributes(i)->xml_attr.name;
        if (xml_parser_compare_multiple(condition_name, value)) {
            return i;
        }
    }
    return CONDITION_TYPE_UNDEFINED;
}

static int condition_populate_parameters(scenario_condition_t *condition)
{
    scenario_condition_data_t *data = scenario_conditions_get_xml_attributes(condition->type);
    int success = 1;
    success &= xml_import_special_parse_attribute(&data->xml_parm1, &condition->parameter1);
    success &= xml_import_special_parse_attribute(&data->xml_parm2, &condition->parameter2);
    success &= xml_import_special_parse_attribute(&data->xml_parm3, &condition->parameter3);
    success &= xml_import_special_parse_attribute(&data->xml_parm4, &condition->parameter4);
    success &= xml_import_special_parse_attribute(&data->xml_parm5, &condition->parameter5);

    return success;
}

static int xml_import_start_condition(void)
{
    if (!data.success) {
        return 0;
    }
    
    if (!xml_parser_has_attribute("type")) {
        log_info("No condition type specified", 0, 0);
        return 0;
    }
    condition_types type = get_condition_type_from_attr("type");
    if (type == CONDITION_TYPE_UNDEFINED) {
        log_info("Invalid condition type specified", 0, 0);
        return 0;
    }

    scenario_condition_t *condition = scenario_event_condition_create(data.current_event, type);
    return condition_populate_parameters(condition);
}

scenario_action_data_t *scenario_actions_get_xml_attributes(action_types type)
{
    return &scenario_action_data[type];
}

static action_types get_action_type_from_attr(const char *key)
{
    const char *value = xml_parser_get_attribute_string(key);
    if (!value) {
        return ACTION_TYPE_UNDEFINED;
    }
    for (action_types i = ACTION_TYPE_MIN; i < ACTION_TYPE_MAX; i++) {
        const char *action_name = scenario_actions_get_xml_attributes(i)->xml_attr.name;
        if (xml_parser_compare_multiple(action_name, value)) {
            return i;
        }
    }
    return ACTION_TYPE_UNDEFINED;
}

static int action_populate_parameters(scenario_action_t *action)
{
    scenario_action_data_t *data = scenario_actions_get_xml_attributes(action->type);
    int success = 1;
    success &= xml_import_special_parse_attribute(&data->xml_parm1, &action->parameter1);
    success &= xml_import_special_parse_attribute(&data->xml_parm2, &action->parameter2);
    success &= xml_import_special_parse_attribute(&data->xml_parm3, &action->parameter3);
    success &= xml_import_special_parse_attribute(&data->xml_parm4, &action->parameter4);
    success &= xml_import_special_parse_attribute(&data->xml_parm5, &action->parameter5);

    return success;
}

static int xml_import_start_action(void)
{
    if (!data.success) {
        return 0;
    }
    
    if (!xml_parser_has_attribute("type")) {
        xml_import_log_error("No action type specified");
        log_info("No action type specified", 0, 0);
        return 0;
    }
    action_types type = get_action_type_from_attr("type");
    if (type == ACTION_TYPE_UNDEFINED) {
        xml_import_log_error("Invalid action type specified");
        log_info("Invalid action type specified", 0, 0);
        return 0;
    }

    scenario_action_t *action = scenario_event_action_create(data.current_event, type);
    return action_populate_parameters(action);
}

static void xml_import_log_error(const char *msg)
{
    data.success = 0;
    data.error_line_number = xml_parser_get_current_line_number();
    strcpy(data.error_message, msg);
    log_error("Error while import scenario events from XML. ", data.error_message, 0);
    log_error("Line:", 0, data.error_line_number);

    string_copy(translation_for(TR_EDITOR_IMPORT_LINE), data.error_line_number_text, 50);
    int length = string_length(data.error_line_number_text);

    uint8_t number_as_text[15];
    string_from_int(number_as_text, data.error_line_number, 0);
    string_copy(number_as_text, data.error_line_number_text + length, 50);

    window_plain_message_dialog_show_with_two_extra(
        TR_EDITOR_UNABLE_TO_LOAD_EVENTS_TITLE, TR_EDITOR_CHECK_LOG_MESSAGE,
        string_from_ascii(data.error_message),
        data.error_line_number_text);
}

#pragma region ATTRIBUTE_PARSING

static int xml_import_special_parse_attribute(xml_data_attribute_t *attr, int *target)
{
    switch (attr->type) {
        case PARAMETER_TYPE_ALLOWED_BUILDING: return xml_import_special_parse_allowed_buildings(attr, target);
        case PARAMETER_TYPE_BOOLEAN: return xml_import_special_parse_boolean(attr, target);
        case PARAMETER_TYPE_BUILDING: return xml_import_special_parse_building(attr, target);
        case PARAMETER_TYPE_BUILDING_COUNTING: return xml_import_special_parse_building_counting(attr, target);
        case PARAMETER_TYPE_CHECK: return xml_import_special_parse_check(attr, target);
        case PARAMETER_TYPE_DIFFICULTY: return xml_import_special_parse_difficulty(attr, target);
        case PARAMETER_TYPE_FUTURE_CITY: return xml_import_special_parse_future_city(attr, target);
        case PARAMETER_TYPE_NUMBER: return xml_import_special_parse_limited_number(attr, target);
        case PARAMETER_TYPE_MIN_MAX_NUMBER: return xml_import_special_parse_min_max_number(attr, target);
        case PARAMETER_TYPE_POP_CLASS: return xml_import_special_parse_pop_class(attr, target);
        case PARAMETER_TYPE_RESOURCE: return xml_import_special_parse_resource(attr, target);
        case PARAMETER_TYPE_ROUTE: return xml_import_special_parse_route(attr, target);
        case PARAMETER_TYPE_STANDARD_MESSAGE: return xml_import_special_parse_standard_message(attr, target);
        case PARAMETER_TYPE_UNDEFINED:
            return 1;
        default:
            xml_import_log_error("Something is very wrong. Failed to find attribute type.");
            return 0;
    }
}

static special_attribute_mapping_t *xml_exporter_get_attribute_mapping(const char *value, special_attribute_mapping_t array[], int array_size)
{
    for (int i = 0; i < array_size; i++) {
        special_attribute_mapping_t *current = &array[i];
        if (strcmp(value, current->text) == 0) {
            return current;
        }
    }
    return 0;
}

static int xml_import_special_parse_building(xml_data_attribute_t *attr, int *target)
{
    int has_attr = xml_parser_has_attribute(attr->name);
    if (!has_attr) {
        xml_import_log_error("Missing attribute.");
        return 0;
    }

    const char *value = xml_parser_get_attribute_string(attr->name);
    special_attribute_mapping_t *found = xml_exporter_get_attribute_mapping(
        value, special_attribute_mappings_buildings, BUILDING_TYPE_MAX);
    if (found != 0) {
        *target = found->value;
        return 1;
    }

    xml_import_log_error("Could not find the given building type.");
    return 0;
}

static int xml_import_special_parse_building_counting(xml_data_attribute_t *attr, int *target)
{
    int has_attr = xml_parser_has_attribute(attr->name);
    if (!has_attr) {
        xml_import_log_error("Missing attribute.");
        return 0;
    }

    const char *value = xml_parser_get_attribute_string(attr->name);
    special_attribute_mapping_t *found = xml_exporter_get_attribute_mapping(
        value, special_attribute_mappings_buildings, BUILDING_TYPE_MAX);
    if (found == 0) {
        xml_import_log_error("Could not find the given building type for counting.");
        return 0;
    }

    switch(found->value) {
        case BUILDING_CLEAR_LAND:
        case BUILDING_DISTRIBUTION_CENTER_UNUSED:
        case BUILDING_BURNING_RUIN:
            xml_import_log_error("I cannot count that.");
            return 0;
        default:
            break;
    }
    
    *target = found->value;
    return 1;
}

static int xml_import_special_parse_allowed_buildings(xml_data_attribute_t *attr, int *target)
{
    int has_attr = xml_parser_has_attribute(attr->name);
    if (!has_attr) {
        xml_import_log_error("Missing attribute.");
        return 0;
    }

    const char *value = xml_parser_get_attribute_string(attr->name);
    special_attribute_mapping_t *found = xml_exporter_get_attribute_mapping(
        value, special_attribute_mappings_allowed_buildings, SPECIAL_ATTRIBUTE_MAPPINGS_ALLOWED_BUILDINGS_SIZE);
    if (found == 0) {
        xml_import_log_error("Could not resolve the given value.");
        return 0;
    }

    *target = found->value;
    return 1;
}

static int xml_import_special_parse_boolean(xml_data_attribute_t *attr, int *target)
{
    int has_attr = xml_parser_has_attribute(attr->name);
    if (!has_attr) {
        xml_import_log_error("Missing attribute.");
        return 0;
    }

    const char *value = xml_parser_get_attribute_string(attr->name);
    special_attribute_mapping_t *found = xml_exporter_get_attribute_mapping(
        value, special_attribute_mappings_boolean, SPECIAL_ATTRIBUTE_MAPPINGS_BOOLEAN_SIZE);
    if (found == 0) {
        return xml_import_special_parse_limited_number(attr, target);
    }

    *target = found->value;
    return 1;
}

static int xml_import_special_parse_check(xml_data_attribute_t *attr, int *target)
{
    int has_attr = xml_parser_has_attribute(attr->name);
    if (!has_attr) {
        xml_import_log_error("Missing attribute.");
        return 0;
    }

    const char *value = xml_parser_get_attribute_string(attr->name);
    special_attribute_mapping_t *found = xml_exporter_get_attribute_mapping(
        value, special_attribute_mappings_check, SPECIAL_ATTRIBUTE_MAPPINGS_CHECK_SIZE);
    if (found == 0) {
        return xml_import_special_parse_limited_number(attr, target);
    }

    *target = found->value;
    return 1;
}

static int xml_import_special_parse_difficulty(xml_data_attribute_t *attr, int *target)
{
    int has_attr = xml_parser_has_attribute(attr->name);
    if (!has_attr) {
        xml_import_log_error("Missing attribute.");
        return 0;
    }

    const char *value = xml_parser_get_attribute_string(attr->name);
    special_attribute_mapping_t *found = xml_exporter_get_attribute_mapping(
        value, special_attribute_mappings_difficulty, SPECIAL_ATTRIBUTE_MAPPINGS_CHECK_DIFFICULTY);
    if (found == 0) {
        return xml_import_special_parse_limited_number(attr, target);
    }

    *target = found->value;
    return 1;
}

static int xml_import_special_parse_future_city(xml_data_attribute_t *attr, int *target)
{
    int has_attr = xml_parser_has_attribute(attr->name);
    if (!has_attr) {
        xml_import_log_error("Missing attribute.");
        return 0;
    }

    const char *value = xml_parser_get_attribute_string(attr->name);
    const uint8_t *converted_name = string_from_ascii(value);
    int city_id = empire_city_get_id_by_name(converted_name);
    empire_city *city = empire_city_get(city_id);
    if (city) {
        if (city->type == EMPIRE_CITY_FUTURE_TRADE) {
            *target = city_id;
            return 1;
        } else {
            xml_import_log_error("Can only target cities with the future_trade type");
            return 0;
        }
    } else {
        xml_import_log_error("Could not find city");
        return 0;
    }

    return 0;
}

static int xml_import_special_parse_pop_class(xml_data_attribute_t *attr, int *target)
{
    int has_attr = xml_parser_has_attribute(attr->name);
    if (!has_attr) {
        xml_import_log_error("Missing attribute.");
        return 0;
    }

    const char *value = xml_parser_get_attribute_string(attr->name);
    special_attribute_mapping_t *found = xml_exporter_get_attribute_mapping(
        value, special_attribute_mappings_pop_class, SPECIAL_ATTRIBUTE_MAPPINGS_POP_CLASS_SIZE);
    if (found == 0) {
        return xml_import_special_parse_limited_number(attr, target);
    }

    *target = found->value;
    return 1;
}

static int xml_import_special_parse_resource(xml_data_attribute_t *attr, int *target)
{
    int has_attr = xml_parser_has_attribute(attr->name);
    if (!has_attr) {
        xml_import_log_error("Missing attribute.");
        return 0;
    }
    
    const char *value = xml_parser_get_attribute_string(attr->name);
    for (resource_type i = RESOURCE_MIN; i < RESOURCE_MAX; i++) {
        const char *resource_name = resource_get_data(i)->xml_attr_name;
        if (xml_parser_compare_multiple(resource_name, value)) {
            *target = (int)i;
            return 1;
        }
    }

    return 0;
}

static int xml_import_special_parse_route(xml_data_attribute_t *attr, int *target)
{
    int has_attr = xml_parser_has_attribute(attr->name);
    if (!has_attr) {
        xml_import_log_error("Missing attribute.");
        return 0;
    }

    const char *value = xml_parser_get_attribute_string(attr->name);
    const uint8_t *converted_name = string_from_ascii(value);
    int city_id = empire_city_get_id_by_name(converted_name);
    empire_city *city = empire_city_get(city_id);
    if (city) {
        *target = city->route_id;
        return 1;
    } else {
        xml_import_log_error("Could not find city");
        return 0;
    }

    return 0;
}

static int xml_import_special_parse_limited_number(xml_data_attribute_t *attr, int *target)
{
    int has_attr = xml_parser_has_attribute(attr->name);
    if (!has_attr) {
        xml_import_log_error("Missing attribute.");
        return 0;
    }

    int param_value = xml_parser_get_attribute_int(attr->name);

    if (param_value < attr->min_limit) {
        xml_import_log_error("Number too small.");
        return 0;
    }

    if (param_value > attr->max_limit) {
        xml_import_log_error("Number too big.");
        return 0;
    }

    *target = param_value;
    return 1;
}

static int xml_import_special_parse_min_max_number(xml_data_attribute_t *attr, int *target)
{
    int has_attr = xml_parser_has_attribute("amount");
    if (has_attr) {
        int param_value = xml_parser_get_attribute_int("amount");
        if (param_value >= attr->min_limit
            && param_value <= attr->max_limit) {
            *target = param_value;
            return 1;
        }
    }

    return xml_import_special_parse_limited_number(attr, target);
}

static int xml_import_special_parse_standard_message(xml_data_attribute_t *attr, int *target)
{
    const char *value = xml_parser_get_attribute_string(attr->name);
    if (!value) {
        xml_import_log_error("Missing attribute.");
        return 0;
    }
    
    for (int i = 0; i < SPECIAL_ATTRIBUTE_MAPPINGS_STANDARD_MESSAGE_SIZE; i++) {
        special_attribute_mapping_t *current = &special_attribute_mappings_standard_message[i];
        if (strcmp(value, current->text) == 0) {
            *target = current->value;
            return 1;
        }
    }

    xml_import_log_error("Not a valid standard message.");
    return 0;
}

#pragma endregion ATTRIBUTE_PARSING

static void reset_data(void)
{
    data.success = 1;
    data.version = -1;
    data.error_line_number = -1;
}

static int parse_xml(char *buffer, int buffer_length)
{
    reset_data();
    scenario_events_clear();
    data.success = 1;
    if (!xml_parser_init(xml_elements, XML_TOTAL_ELEMENTS)) {
        data.success = 0;
    }
    if (data.success) {
        if (!xml_parser_parse(buffer, buffer_length, 1)) {
            data.success = 0;
            scenario_events_clear();
        }
    }
    xml_parser_free();

    return data.success;
}

static char *file_to_buffer(const char *filename, int *output_length)
{
    FILE *file = file_open(filename, "r");
    if (!file) {
        log_error("Error opening empire file", filename, 0);
        return 0;
    }
    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    rewind(file);

    char *buffer = malloc(size);
    if (!buffer) {
        log_error("Error opening empire file", filename, 0);
        return 0;
    }
    memset(buffer, 0, size);
    if (!buffer) {
        log_error("Unable to allocate buffer to read XML file", filename, 0);
        free(buffer);
        file_close(file);
        return 0;
    }
    *output_length = (int) fread(buffer, 1, size, file);
    if (*output_length > size) {
        log_error("Unable to read file into buffer", filename, 0);
        free(buffer);
        file_close(file);
        *output_length = 0;
        return 0;
    }
    file_close(file);
    return buffer;
}

int scenario_events_xml_parse_file(const char *filename)
{
    int output_length = 0;
    char *xml_contents = file_to_buffer(filename, &output_length);
    if (!xml_contents) {
        return 0;
    }
    int success = parse_xml(xml_contents, output_length);
    free(xml_contents);
    if (!success) {
        log_error("Error parsing file", filename, 0);
    }
    return success;
}

int scenario_events_export_to_xml(const char *filename)
{
    buffer buf;
    int buf_size = XML_EXPORT_MAX_SIZE;
    uint8_t *buf_data = malloc(buf_size);
    buffer_init(&buf, buf_data, buf_size);
    xml_exporter_scenario_events(&buf);
    io_write_buffer_to_file(filename, buf.data, buf.index);
    return 1;
}

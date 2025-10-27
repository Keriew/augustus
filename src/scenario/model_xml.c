#include "model_xml.h"

#include "building/industry.h"
#include "building/properties.h"
#include "building/type.h"
#include "core/buffer.h"
#include "core/io.h"
#include "core/log.h"
#include "core/string.h"
#include "core/xml_exporter.h"
#include "core/xml_parser.h"
#include "game/resource.h"
#include "scenario/event/parameter_data.h"
#include "window/plain_message_dialog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XML_EXPORT_MAX_SIZE 200000
#define ERROR_MESSAGE_LENGTH 200

static struct {
    int success;
    char error_message[ERROR_MESSAGE_LENGTH];
    int error_line_number;
} data;

// EXPORT

static void export_model_data(buffer *buf)
{
    xml_exporter_new_element("model_data");
    xml_exporter_add_attribute_int("version", MODEL_DATA_VERSION);
    
    int edited_models = 0;
    
    for (building_type type = BUILDING_ANY + 1; type < BUILDING_TYPE_MAX; ++type) {
        const building_properties *props = building_properties_for_type(type);
        if (!props) {
            continue;
        }
        
        if (((!props->size || !props->event_data.attr) && type != BUILDING_CLEAR_LAND && type != BUILDING_REPAIR_LAND )
             || type == BUILDING_GRAND_GARDEN
             || type == BUILDING_DOLPHIN_FOUNTAIN ) {
            continue;
        }

        model_building *model = model_get_building(type);
        model_building *prop_model = &props->building_model_data;
        if (!model) {
            continue;
        }
        
        resource_data *resource = resource_get_data(resource_get_from_industry(type));
        if (resource->production_per_month == resource_get_defaults(resource_get_from_industry(type))->production_per_month) {
            if (model == prop_model) {
                continue;
            }

            if (memcmp(model, prop_model, sizeof(*model)) == 0) {
                continue;
            }
        }
        
        xml_exporter_new_element("building_model");
        xml_exporter_add_attribute_text("building_type", props->event_data.attr);
        xml_exporter_add_attribute_int("cost", model->cost);
        xml_exporter_add_attribute_int("desirability_value", model->desirability_value);
        xml_exporter_add_attribute_int("desirability_step", model->desirability_step);
        xml_exporter_add_attribute_int("desirability_step_size", model->desirability_step_size);
        xml_exporter_add_attribute_int("desirability_range", model->desirability_range);
        xml_exporter_add_attribute_int("laborers", model->laborers);
        if ((building_is_raw_resource_producer(type) || building_is_workshop(type) || type == BUILDING_WHARF)) {
            if (resource->production_per_month != resource_get_defaults(resource_get_from_industry(type))->production_per_month) {
                xml_exporter_add_attribute_int("production_rate", resource->production_per_month);
            }
        }
        xml_exporter_close_element();
        
        edited_models++;
    }
    
    if (!edited_models) {
        xml_exporter_add_element_text("<!--Nothing here but xml parser doesn't like empty things-->");
        xml_exporter_close_element();
    }
    xml_exporter_close_element();
    
}

int scenario_model_export_to_xml(const char *filename)
{
    buffer buf;
    int buf_size = XML_EXPORT_MAX_SIZE;
    uint8_t *buf_data = malloc(buf_size);
    if (!buf_data) {
        log_error("Unable to allocate buffer to export model data XML", 0, 0);
        free(buf_data);
        return 0;
    }
    buffer_init(&buf, buf_data, buf_size);
    xml_exporter_init(&buf, "model_data");
    export_model_data(&buf);
    io_write_buffer_to_file(filename, buf.data, buf.index);
    free(buf_data);
    return 1;
}

// IMPORT

static int start_building_model(void);

static const xml_parser_element xml_elements[2] = {
    {"model_data"},
    {"building_model", start_building_model, 0, "model_data"}
};
#define MAX_XML_ELEMENTS sizeof(xml_elements)/sizeof(xml_parser_element)

static void xml_import_log_error(const char *msg)
{
    data.success = 0;
    data.error_line_number = xml_parser_get_current_line_number();
    snprintf(data.error_message, ERROR_MESSAGE_LENGTH, "%s", msg);
    log_error("Error while import scenario events from XML. ", data.error_message, 0);
    log_error("Line:", 0, data.error_line_number);

    window_plain_message_dialog_show_with_extra(
        TR_EDITOR_UNABLE_TO_LOAD_MODEL_DATA_TITLE, TR_EDITOR_CHECK_LOG_MESSAGE,
        string_from_ascii(data.error_message), 0);
}

static int start_building_model(void)
{
    const char *value = xml_parser_get_attribute_string("building_type");
    special_attribute_mapping_t *found = scenario_events_parameter_data_get_attribute_mapping_by_text(PARAMETER_TYPE_MODEL, value);
    if (found == 0) {
        xml_import_log_error("Could not resolve the given value. Invalid building_type");
        return 0;
    }
    building_type type = found->value;
    
    if (!xml_parser_has_attribute("cost")) {
        xml_import_log_error("Attribute missing. 'cost' not given");
        return 0;
    }
    if (!xml_parser_has_attribute("desirability_value")) {
        xml_import_log_error("Attribute missing. 'desirability_value' not given");
        return 0;
    }
    if (!xml_parser_has_attribute("desirability_step")) {
        xml_import_log_error("Attribute missing. 'desirability_step' not given");
        return 0;
    }
    if (!xml_parser_has_attribute("desirability_step_size")) {
        xml_import_log_error("Attribute missing. 'desirability_step_size' not given");
        return 0;
    }
    if (!xml_parser_has_attribute("desirability_range")) {
        xml_import_log_error("Attribute missing. 'desirability_range' not given");
        return 0;
    }
    if (!xml_parser_has_attribute("laborers")) {
        xml_import_log_error("Attribute missing. 'laborers' not given");
        return 0;
    }
    
    model_building *model_ptr = model_get_building(type);
    
    model_ptr->cost = xml_parser_get_attribute_int("cost");
    model_ptr->desirability_value = xml_parser_get_attribute_int("desirability_value");
    model_ptr->desirability_step = xml_parser_get_attribute_int("desirability_step");
    model_ptr->desirability_step_size = xml_parser_get_attribute_int("desirability_step_size");
    model_ptr->desirability_range = xml_parser_get_attribute_int("desirability_range");
    model_ptr->laborers = xml_parser_get_attribute_int("laborers");
    if (xml_parser_has_attribute("production_rate")) {
        resource_data *resource = resource_get_data(resource_get_from_industry(type));
        resource->production_per_month = xml_parser_get_attribute_int("production_rate");
    }
    
    return 1;
}

static int parse_xml(char *buf, int buffer_length)
{
    model_reset();
    resource_init();
    data.success = 1;
    if (!xml_parser_init(xml_elements, MAX_XML_ELEMENTS, 1)) {
        data.success = 0;
    }
    if (data.success) {
        if (!xml_parser_parse(buf, buffer_length, 1)) {
            data.success = 0;
            model_reset();
            resource_init();
        }
    }
    xml_parser_free();
    
    return data.success;
}

static char *file_to_buffer(const char *filename, int *output_length)
{
    FILE *file = file_open(filename, "r");
    if (!file) {
        log_error("Error opening model data file", filename, 0);
        return 0;
    }
    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    rewind(file);

    char *buf = malloc(size);
    if (!buf) {
        log_error("Error allocating memory to buffer", filename, 0);
        file_close(file);
        return 0;
    }
    memset(buf, 0, size);
    if (!buf) {
        log_error("Error initialising memory of buffer", filename, 0);
        free(buf);
        file_close(file);
        return 0;
    }
    *output_length = (int) fread(buf, 1, size, file);
    if (*output_length > size) {
        log_error("Unable to read file into buffer", filename, 0);
        free(buf);
        file_close(file);
        *output_length = 0;
        return 0;
    }
    file_close(file);
    return buf;
}

int scenario_model_xml_parse_file(const char *filename)
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
        model_reset();
    }
    return success;
}
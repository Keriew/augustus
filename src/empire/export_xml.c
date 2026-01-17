#include "export_xml.h"

#include "core/io.h"
#include "core/log.h"
#include "core/xml_exporter.h"

#include <stdlib.h>

#define XML_EXPORT_MAX_SIZE 5000000

static void export_empire(buffer *buf)
{
    xml_exporter_new_element("empire");
    xml_exporter_add_attribute_int("version", 2);
    
    xml_exporter_new_element("map");
    xml_exporter_add_attribute_int("show_ireland", 1);
}

int empire_export_xml(const char *filename)
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
    export_empire(&buf);
    io_write_buffer_to_file(filename, buf.data, buf.index);
    free(buf_data);
    return 1;
}

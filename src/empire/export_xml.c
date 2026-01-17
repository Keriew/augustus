#include "export_xml.h"

#include "core/image_group_editor.h"
#include "core/image_group.h"
#include "core/image.h"
#include "core/io.h"
#include "core/log.h"
#include "core/string.h"
#include "core/xml_exporter.h"
#include "editor/editor.h"
#include "empire/empire.h"
#include "empire/object.h"

#include <stdlib.h>

#define XML_EXPORT_MAX_SIZE 5000000

#define BASE_ORNAMENT_IMAGE_ID 3356
#define ORIGINAL_ORNAMENTS 20

static const char *ORNAMENTS[] = {
    "The Stonehenge",
    "Gallic Wheat",
    "The Pyrenees",
    "Iberian Aqueduct",
    "Triumphal Arch",
    "West Desert Wheat",
    "Lighthouse of Alexandria",
    "West Desert Palm Trees",
    "Trade Ship",
    "Waterside Palm Trees",
    "Colosseum|The Colosseum",
    "The Alps",
    "Roman Tree",
    "Greek Mountain Range",
    "The Parthenon",
    "The Pyramids",
    "The Hagia Sophia",
    "East Desert Palm Trees",
    "East Desert Wheat",
    "Trade Camel",
    "Mount Etna",
    "Colossus of Rhodes",
    "The Temple"
};

#define TOTAL_ORNAMENTS (sizeof(ORNAMENTS) / sizeof(const char *))

static void export_map(void)
{
    xml_exporter_new_element("map");
    int empire_image_id = empire_get_image_id();
    int empire_width, empire_height, empire_image_x, empire_image_y;
    empire_get_map_size(&empire_width, &empire_height);
    empire_get_coordinates(&empire_image_x, &empire_image_y);
    const char *image_path = empire_get_image_path();
    if (empire_image_id != image_group(editor_is_active() ? GROUP_EDITOR_EMPIRE_MAP : GROUP_EMPIRE_MAP)) {
        xml_exporter_add_attribute_text("image", image_path);
        xml_exporter_add_attribute_int("x_offset", empire_image_x);
        xml_exporter_add_attribute_int("y_offset", empire_image_y);
        xml_exporter_add_attribute_int("width", empire_width);
        xml_exporter_add_attribute_int("height", empire_height);
    } else {
        xml_exporter_add_attribute_int("show_ireland", empire_object_get_ireland());
    }
    if (empire_object_count_ornaments() == TOTAL_ORNAMENTS) {
        xml_exporter_new_element("ornament");
        xml_exporter_add_attribute_text("type", "all");
        xml_exporter_close_element();
    } else {
        for (int i = 0; i < TOTAL_ORNAMENTS; i++) {
            int ornament_image_id = i < ORIGINAL_ORNAMENTS ? BASE_ORNAMENT_IMAGE_ID + i : ORIGINAL_ORNAMENTS - i - 2;
            if (empire_object_get_ornament(ornament_image_id)) {
                xml_exporter_new_element("ornament");
                xml_exporter_add_attribute_text("type", ORNAMENTS[i]);
                xml_exporter_close_element();
            }
        }
    }
    xml_exporter_close_element();
}

static void export_border(void)
{
    const empire_object *border = empire_object_get_border();
    if (!border) {
        return;
    }
    xml_exporter_new_element("border");
    xml_exporter_add_attribute_int("density", border->width);
    for (int edge_index = 0; edge_index < empire_object_count(); ) {
        int edge_id = empire_object_get_next_in_order(border->id, &edge_index);
        if (!edge_id) {
            break;
        }
        empire_object *edge = empire_object_get(edge_id);
        if (edge->type != EMPIRE_OBJECT_BORDER_EDGE) {
            break;
        }
        xml_exporter_new_element("edge");
        xml_exporter_add_attribute_int("x", edge->x);
        xml_exporter_add_attribute_int("y", edge->y);
        if (!edge->image_id) {
            xml_exporter_add_attribute_int("hidden", 1);
        }
        xml_exporter_close_element();
    }
    xml_exporter_close_element();
}

static void export_city(const empire_object *obj)
{
    static const char *city_types[6] = { "roman", "ours", "trade", "future_trade", "distant", "vulnerable" };
    
    full_empire_object *city = empire_object_get_full(obj->id);
    if (city->city_type == EMPIRE_CITY_FUTURE_ROMAN) {
        return;
    }
    xml_exporter_new_element("city");
    uint8_t city_name[50];
    if (string_length(city->city_custom_name)) {
        string_copy(city->city_custom_name, city_name, 50);
    } else {
        string_copy(lang_get_string(21, city->city_name_id), city_name, 50);
    }
    xml_exporter_add_attribute_encoded_text("name", city_name);
    xml_exporter_add_attribute_int("x", city->obj.x);
    xml_exporter_add_attribute_int("y", city->obj.y);
    xml_exporter_add_attribute_text("type", city_types[city->city_type]);
    if (city->city_type == EMPIRE_CITY_TRADE || city->city_type == EMPIRE_CITY_FUTURE_TRADE) {
        xml_exporter_add_attribute_int("trade_route_cost", city->trade_route_cost);
        const char *route_type = empire_object_is_sea_trade_route(city->obj.trade_route_id) ? "sea" : "land";
        xml_exporter_add_attribute_text("trade_route_type", route_type);
    }
}

static void export_cities(void)
{
    xml_exporter_new_element("cities");
    empire_object_foreach_of_type(export_city, EMPIRE_OBJECT_CITY);
    xml_exporter_close_element();
}

static void export_empire(buffer *buf)
{
    xml_exporter_new_element("empire");
    xml_exporter_add_attribute_int("version", 2);
    export_map();
    export_border();
    
    xml_exporter_close_element();
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
    xml_exporter_init(&buf, "empire");
    export_empire(&buf);
    io_write_buffer_to_file(filename, buf.data, buf.index);
    free(buf_data);
    return 1;
}

#include "building/building_type_registry.h"

extern "C" {
#include "building/building.h"
#include "building/image.h"
#include "building/properties.h"
#include "core/dir.h"
#include "core/file.h"
#include "core/log.h"
#include "core/xml_parser.h"
#include "map/building_tiles.h"
#include "map/terrain.h"
#include "platform/file_manager.h"
}

#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

enum class GraphicComparison {
    None,
    GreaterThan,
    GreaterThanOrEqual
};

enum class WaterAccessMode {
    None,
    ReservoirRange
};

class BuildingType {
public:
    BuildingType(building_type type, std::string attr)
        : type_(type)
        , attr_(std::move(attr))
    {
    }

    void set_threshold(int threshold, GraphicComparison comparison)
    {
        threshold_ = threshold;
        comparison_ = comparison;
    }

    void set_water_access_mode(WaterAccessMode mode)
    {
        water_access_mode_ = mode;
    }

    building_type type() const
    {
        return type_;
    }

    const char *attr() const
    {
        return attr_.c_str();
    }

    WaterAccessMode water_access_mode() const
    {
        return water_access_mode_;
    }

    unsigned char upgrade_level_for(const ::building &building) const
    {
        switch (comparison_) {
            case GraphicComparison::GreaterThan:
                return building.desirability > threshold_;
            case GraphicComparison::GreaterThanOrEqual:
                return building.desirability >= threshold_;
            case GraphicComparison::None:
            default:
                return 0;
        }
    }

private:
    building_type type_;
    std::string attr_;
    int threshold_ = 0;
    GraphicComparison comparison_ = GraphicComparison::None;
    WaterAccessMode water_access_mode_ = WaterAccessMode::None;
};

class BuildingInstance {
public:
    BuildingInstance(::building *building, const BuildingType *definition)
        : building_(building)
        , definition_(definition)
    {
    }

    void set_building_graphic()
    {
        if (!building_ || !definition_ || building_->state != BUILDING_STATE_IN_USE) {
            return;
        }

        if (definition_->water_access_mode() == WaterAccessMode::ReservoirRange) {
            building_->has_water_access =
                map_terrain_exists_tile_in_area_with_type(building_->x, building_->y, building_->size, TERRAIN_RESERVOIR_RANGE);
        }

        building_->upgrade_level = definition_->upgrade_level_for(*building_);
        map_building_tiles_add(building_->id, building_->x, building_->y, building_->size, building_image_get(building_), TERRAIN_BUILDING);
    }

    const ::building *building() const
    {
        return building_;
    }

    const BuildingType *definition() const
    {
        return definition_;
    }

private:
    ::building *building_;
    const BuildingType *definition_;
};

struct ParseState {
    std::unique_ptr<BuildingType> definition;
    int saw_graphic = 0;
    int error = 0;
};

std::string g_mod_name = "Vespasian";
std::string g_mod_path = "Mods/Vespasian/";
std::string g_building_type_path = "Mods/Vespasian/BuildingType/";
std::array<std::unique_ptr<BuildingType>, BUILDING_TYPE_MAX> g_building_types;
std::vector<std::unique_ptr<BuildingInstance>> g_runtime_instances;
ParseState g_parse_state;

static int stop_on_first_entry(const char *name, long unused)
{
    return LIST_MATCH;
}

static void rebuild_mod_paths()
{
    g_mod_path = "Mods/" + g_mod_name + "/";
    g_building_type_path = g_mod_path + "BuildingType/";
}

static int directory_exists(const char *path)
{
    return platform_file_manager_list_directory_contents(path, TYPE_DIR | TYPE_FILE, 0, stop_on_first_entry) != LIST_ERROR;
}

static building_type find_building_type_by_attr(const char *type_attr)
{
    for (building_type type = BUILDING_NONE; type < BUILDING_TYPE_MAX; type = static_cast<building_type>(type + 1)) {
        const building_properties *properties = building_properties_for_type(type);
        if (!properties->event_data.attr) {
            continue;
        }
        if (xml_parser_compare_multiple(properties->event_data.attr, type_attr)) {
            return type;
        }
    }
    return BUILDING_NONE;
}

static int parse_building_root()
{
    if (!xml_parser_has_attribute("type")) {
        log_error("BuildingType xml is missing required attribute 'type'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *type_attr = xml_parser_get_attribute_string("type");
    building_type type = find_building_type_by_attr(type_attr);
    if (type == BUILDING_NONE) {
        log_error("Unknown BuildingType xml type", type_attr, 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition = std::make_unique<BuildingType>(type, type_attr);
    return 1;
}

static int parse_graphic()
{
    if (!g_parse_state.definition) {
        log_error("Encountered graphic definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    if (!xml_parser_has_attribute("threshold")) {
        log_error("BuildingType graphic is missing required attribute 'threshold'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("operator")) {
        log_error("BuildingType graphic is missing required attribute 'operator'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *comparison_text = xml_parser_get_attribute_string("operator");
    GraphicComparison comparison = GraphicComparison::None;
    if (comparison_text && strcmp(comparison_text, "gt") == 0) {
        comparison = GraphicComparison::GreaterThan;
    } else if (comparison_text && strcmp(comparison_text, "gte") == 0) {
        comparison = GraphicComparison::GreaterThanOrEqual;
    } else {
        log_error("Unsupported BuildingType graphic operator", comparison_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->set_threshold(xml_parser_get_attribute_int("threshold"), comparison);

    if (xml_parser_has_attribute("water_access")) {
        const char *water_access = xml_parser_get_attribute_string("water_access");
        if (water_access && strcmp(water_access, "reservoir_range") == 0) {
            g_parse_state.definition->set_water_access_mode(WaterAccessMode::ReservoirRange);
        } else {
            log_error("Unsupported BuildingType graphic water_access", water_access, 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    g_parse_state.saw_graphic = 1;
    return 1;
}

static const xml_parser_element XML_ELEMENTS[] = {
    { "building", parse_building_root, nullptr, nullptr, nullptr },
    { "graphic", parse_graphic, nullptr, "building", nullptr }
};

static int load_file_to_buffer(const char *filename, std::vector<char> &buffer)
{
    FILE *fp = file_open(filename, "rb");
    if (!fp) {
        log_error("Unable to open BuildingType xml", filename, 0);
        return 0;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        file_close(fp);
        log_error("Unable to seek BuildingType xml", filename, 0);
        return 0;
    }

    long size = ftell(fp);
    if (size < 0) {
        file_close(fp);
        log_error("Unable to size BuildingType xml", filename, 0);
        return 0;
    }
    rewind(fp);

    buffer.resize(static_cast<size_t>(size));
    size_t read = fread(buffer.data(), 1, buffer.size(), fp);
    file_close(fp);
    if (read != buffer.size()) {
        log_error("Unable to read BuildingType xml", filename, 0);
        return 0;
    }
    return 1;
}

static int parse_definition_file(const char *filename)
{
    std::vector<char> buffer;
    if (!load_file_to_buffer(filename, buffer)) {
        return 0;
    }

    g_parse_state = {};
    if (!xml_parser_init(XML_ELEMENTS, static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])), 1)) {
        log_error("Unable to initialize BuildingType xml parser", filename, 0);
        return 0;
    }

    int parsed = xml_parser_parse(buffer.data(), static_cast<unsigned int>(buffer.size()), 1);
    xml_parser_free();
    if (!parsed || g_parse_state.error || !g_parse_state.definition || !g_parse_state.saw_graphic) {
        if (!g_parse_state.saw_graphic) {
            log_error("BuildingType xml is missing a graphic node", filename, 0);
        }
        return 0;
    }

    g_building_types[g_parse_state.definition->type()] = std::move(g_parse_state.definition);
    return 1;
}

static BuildingInstance *get_or_create_instance(::building *building_data)
{
    if (!building_data || !building_data->id) {
        return nullptr;
    }

    if (g_runtime_instances.size() <= building_data->id) {
        g_runtime_instances.resize(building_data->id + 1);
    }

    const BuildingType *definition = g_building_types[building_data->type].get();
    if (!definition) {
        return nullptr;
    }

    std::unique_ptr<BuildingInstance> &slot = g_runtime_instances[building_data->id];
    if (!slot || slot->building() != building_data || slot->definition() != definition) {
        slot = std::make_unique<BuildingInstance>(building_data, definition);
    }
    return slot.get();
}

}

extern "C" void building_type_registry_set_mod_name(const char *mod_name)
{
    if (mod_name && *mod_name) {
        g_mod_name = mod_name;
    } else {
        g_mod_name = "Vespasian";
    }
    rebuild_mod_paths();
}

extern "C" const char *building_type_registry_get_mod_name(void)
{
    return g_mod_name.c_str();
}

extern "C" const char *building_type_registry_get_mod_path(void)
{
    return g_mod_path.c_str();
}

extern "C" const char *building_type_registry_get_building_type_path(void)
{
    return g_building_type_path.c_str();
}

extern "C" int building_type_registry_validate_mod(void)
{
    return directory_exists(g_mod_path.c_str()) && directory_exists(g_building_type_path.c_str());
}

extern "C" int building_type_registry_load(void)
{
    for (std::unique_ptr<BuildingType> &definition : g_building_types) {
        definition.reset();
    }

    const dir_listing *files = dir_find_files_with_extension(g_building_type_path.c_str(), "xml");
    if (!files || files->num_files <= 0) {
        log_error("No BuildingType xml files found in", g_building_type_path.c_str(), 0);
        return 0;
    }

    for (int i = 0; i < files->num_files; i++) {
        char full_path[FILE_NAME_MAX];
        snprintf(full_path, FILE_NAME_MAX, "%s%s", g_building_type_path.c_str(), files->files[i].name);
        if (!parse_definition_file(full_path)) {
            log_error("Unable to parse BuildingType xml", full_path, 0);
            return 0;
        }
    }

    building_type_registry_reset_runtime_instances();
    return 1;
}

extern "C" void building_type_registry_reset_runtime_instances(void)
{
    g_runtime_instances.clear();
}

extern "C" void building_type_registry_apply_graphic(building *b)
{
    if (BuildingInstance *instance = get_or_create_instance(b)) {
        instance->set_building_graphic();
    }
}

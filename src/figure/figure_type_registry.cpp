#include "figure/figure_type_registry_internal.h"

#include "core/crash_context.h"

extern "C" {
#include "building/properties.h"
#include "core/dir.h"
#include "core/file.h"
#include "core/image.h"
#include "core/log.h"
#include "core/xml_parser.h"
#include "game/mod_manager.h"
#include "platform/file_manager.h"
}

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace figure_type_registry_impl {

std::array<std::unique_ptr<FigureTypeDefinition>, FIGURE_TYPE_MAX> g_figure_types;
std::string g_failure_reason;

FigureTypeDefinition::FigureTypeDefinition(figure_type type, std::string attr)
    : type_(type)
    , attr_(std::move(attr))
{
}

figure_type FigureTypeDefinition::type() const
{
    return type_;
}

const char *FigureTypeDefinition::attr() const
{
    return attr_.c_str();
}

void FigureTypeDefinition::set_native_class(NativeClassId native_class_id)
{
    native_class_id_ = native_class_id;
}

NativeClassId FigureTypeDefinition::native_class() const
{
    return native_class_id_;
}

void FigureTypeDefinition::set_owner_binding(const OwnerBinding &owner_binding)
{
    owner_binding_ = owner_binding;
}

const OwnerBinding &FigureTypeDefinition::owner_binding() const
{
    return owner_binding_;
}

void FigureTypeDefinition::set_movement_profile(const MovementProfile &movement_profile)
{
    movement_profile_ = movement_profile;
}

const MovementProfile &FigureTypeDefinition::movement_profile() const
{
    return movement_profile_;
}

void FigureTypeDefinition::set_graphics_policy(const GraphicsPolicy &graphics_policy)
{
    graphics_policy_ = graphics_policy;
}

const GraphicsPolicy &FigureTypeDefinition::graphics_policy() const
{
    return graphics_policy_;
}

static int stop_on_first_entry(const char *name, long unused)
{
    (void) name;
    (void) unused;
    return LIST_MATCH;
}

int directory_exists(const char *path)
{
    return platform_file_manager_list_directory_contents(path, TYPE_DIR | TYPE_FILE, 0, stop_on_first_entry) != LIST_ERROR;
}

void set_failure_reason(const char *message, const char *detail)
{
    if (detail && *detail) {
        g_failure_reason = std::string(message ? message : "") + "\n\n" + detail;
    } else {
        g_failure_reason = message ? message : "";
    }
}

static std::string trim_copy(const std::string &value)
{
    size_t start = 0;
    while (start < value.size() &&
        (value[start] == ' ' || value[start] == '\t' || value[start] == '\r' || value[start] == '\n')) {
        start++;
    }

    size_t end = value.size();
    while (end > start &&
        (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r' || value[end - 1] == '\n')) {
        end--;
    }

    return value.substr(start, end - start);
}

static void append_unique_candidate_path(std::vector<std::string> &paths, const char *mod_path)
{
    if (!mod_path || !*mod_path) {
        return;
    }

    std::string candidate = std::string(mod_path) + "FigureType/";
    for (const std::string &existing : paths) {
        if (existing == candidate) {
            return;
        }
    }
    paths.push_back(std::move(candidate));
}

std::vector<std::string> build_candidate_definition_paths()
{
    std::vector<std::string> paths;
    append_unique_candidate_path(paths, mod_manager_get_mod_path());

    const int mod_count = mod_manager_get_mod_count();
    for (int i = 0; i < mod_count; i++) {
        const char *name = mod_manager_get_mod_name_at(i);
        if (!name) {
            continue;
        }
        if (strcmp(name, "Augustus") == 0 || strcmp(name, "Julius") == 0) {
            append_unique_candidate_path(paths, mod_manager_get_mod_path_at(i));
        }
    }
    return paths;
}

const FigureTypeDefinition *definition_for(figure_type type)
{
    if (type <= FIGURE_NONE || type >= FIGURE_TYPE_MAX) {
        return nullptr;
    }
    return g_figure_types[type].get();
}

static figure_type parse_figure_type_name(const char *name)
{
    struct NamedFigure {
        const char *name;
        figure_type type;
    };

    static const NamedFigure kFigureNames[] = {
        { "labor_seeker", FIGURE_LABOR_SEEKER },
        { "teacher", FIGURE_TEACHER },
        { "librarian", FIGURE_LIBRARIAN },
        { "barber", FIGURE_BARBER },
        { "bathhouse_worker", FIGURE_BATHHOUSE_WORKER },
        { "school_child", FIGURE_SCHOOL_CHILD }
    };

    for (size_t i = 0; i < sizeof(kFigureNames) / sizeof(kFigureNames[0]); i++) {
        if (name && strcmp(name, kFigureNames[i].name) == 0) {
            return kFigureNames[i].type;
        }
    }
    return FIGURE_NONE;
}

static building_type parse_building_type_name(const char *attr)
{
    if (!attr || strcmp(attr, "any") == 0) {
        return BUILDING_ANY;
    }

    for (building_type type = BUILDING_NONE; type < BUILDING_TYPE_MAX; type = static_cast<building_type>(type + 1)) {
        const building_properties *properties = building_properties_for_type(type);
        if (!properties || !properties->event_data.attr) {
            continue;
        }
        if (xml_parser_compare_multiple(properties->event_data.attr, attr)) {
            return type;
        }
    }
    return BUILDING_NONE;
}

static NativeClassId parse_native_class_name(const char *name)
{
    if (name && strcmp(name, "roaming_service") == 0) {
        return NativeClassId::RoamingService;
    }
    return NativeClassId::None;
}

static FigureSlot parse_figure_slot_name(const char *name)
{
    if (!name || strcmp(name, "none") == 0) {
        return FigureSlot::None;
    }
    if (strcmp(name, "primary") == 0) {
        return FigureSlot::Primary;
    }
    if (strcmp(name, "secondary") == 0) {
        return FigureSlot::Secondary;
    }
    if (strcmp(name, "quaternary") == 0) {
        return FigureSlot::Quaternary;
    }
    return FigureSlot::None;
}

static int is_known_figure_slot_name(const char *name)
{
    return !name ||
        strcmp(name, "none") == 0 ||
        strcmp(name, "primary") == 0 ||
        strcmp(name, "secondary") == 0 ||
        strcmp(name, "quaternary") == 0;
}

static OwnerStateRequirement parse_owner_state_name(const char *name)
{
    if (!name || strcmp(name, "in_use") == 0) {
        return OwnerStateRequirement::InUse;
    }
    if (strcmp(name, "in_use_or_mothballed") == 0) {
        return OwnerStateRequirement::InUseOrMothballed;
    }
    if (strcmp(name, "any") == 0) {
        return OwnerStateRequirement::Any;
    }
    return OwnerStateRequirement::InUse;
}

static int is_known_owner_state_name(const char *name)
{
    return !name ||
        strcmp(name, "any") == 0 ||
        strcmp(name, "in_use") == 0 ||
        strcmp(name, "in_use_or_mothballed") == 0;
}

static ReturnMode parse_return_mode_name(const char *name)
{
    if (name && strcmp(name, "die_at_limit") == 0) {
        return ReturnMode::DieAtLimit;
    }
    return ReturnMode::ReturnToOwnerRoad;
}

static int is_known_return_mode_name(const char *name)
{
    return !name ||
        strcmp(name, "return_to_owner_road") == 0 ||
        strcmp(name, "die_at_limit") == 0;
}

static int parse_terrain_usage_name(const char *name)
{
    if (!name || strcmp(name, "any") == 0) {
        return TERRAIN_USAGE_ANY;
    }
    if (strcmp(name, "roads") == 0) {
        return TERRAIN_USAGE_ROADS;
    }
    if (strcmp(name, "roads_highway") == 0) {
        return TERRAIN_USAGE_ROADS_HIGHWAY;
    }
    if (strcmp(name, "prefer_roads") == 0) {
        return TERRAIN_USAGE_PREFER_ROADS;
    }
    if (strcmp(name, "prefer_roads_highway") == 0) {
        return TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;
    }
    return -1;
}

static int parse_image_group_name(const char *name)
{
    struct NamedGroup {
        const char *name;
        int image_group_id;
    };

    static const NamedGroup kImageGroups[] = {
        { "labor_seeker", GROUP_FIGURE_LABOR_SEEKER },
        { "teacher_librarian", GROUP_FIGURE_TEACHER_LIBRARIAN },
        { "barber", GROUP_FIGURE_BARBER },
        { "bathhouse_worker", GROUP_FIGURE_BATHHOUSE_WORKER },
        { "school_child", GROUP_FIGURE_SCHOOL_CHILD }
    };

    for (size_t i = 0; i < sizeof(kImageGroups) / sizeof(kImageGroups[0]); i++) {
        if (name && strcmp(name, kImageGroups[i].name) == 0) {
            return kImageGroups[i].image_group_id;
        }
    }
    return 0;
}

struct ParseState {
    std::unique_ptr<FigureTypeDefinition> definition;
    int saw_root = 0;
    int saw_native = 0;
    int saw_owner = 0;
    int saw_movement = 0;
    int saw_graphics = 0;
    int error = 0;
};

static ParseState g_parse_state;

static int parse_definition_root()
{
    if (g_parse_state.saw_root) {
        g_parse_state.error = 1;
        log_error("Duplicate FigureType root node", 0, 0);
        return 0;
    }
    if (!xml_parser_has_attribute("type")) {
        g_parse_state.error = 1;
        log_error("FigureType root is missing required attribute 'type'", 0, 0);
        return 0;
    }

    const char *type_attr = xml_parser_get_attribute_string("type");
    figure_type type = parse_figure_type_name(type_attr);
    if (type == FIGURE_NONE) {
        g_parse_state.error = 1;
        log_error("FigureType root has an unknown figure type", type_attr, 0);
        return 0;
    }

    g_parse_state.definition = std::make_unique<FigureTypeDefinition>(type, type_attr ? type_attr : "");
    g_parse_state.saw_root = 1;
    return 1;
}

static int parse_native_node()
{
    if (!g_parse_state.definition) {
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_native) {
        g_parse_state.error = 1;
        log_error("FigureType xml contains duplicate native nodes", g_parse_state.definition->attr(), 0);
        return 0;
    }
    if (!xml_parser_has_attribute("class")) {
        g_parse_state.error = 1;
        log_error("FigureType native node is missing required attribute 'class'", 0, 0);
        return 0;
    }

    NativeClassId native_class_id = parse_native_class_name(xml_parser_get_attribute_string("class"));
    if (native_class_id == NativeClassId::None) {
        g_parse_state.error = 1;
        log_error("FigureType native node has an unknown class", xml_parser_get_attribute_string("class"), 0);
        return 0;
    }

    g_parse_state.definition->set_native_class(native_class_id);
    g_parse_state.saw_native = 1;
    return 1;
}

static int parse_owner_node()
{
    if (!g_parse_state.definition) {
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_owner) {
        g_parse_state.error = 1;
        log_error("FigureType xml contains duplicate owner nodes", g_parse_state.definition->attr(), 0);
        return 0;
    }

    OwnerBinding owner_binding;
    if (xml_parser_has_attribute("slot")) {
        if (!is_known_figure_slot_name(xml_parser_get_attribute_string("slot"))) {
            g_parse_state.error = 1;
            log_error("FigureType owner node has an unknown slot", xml_parser_get_attribute_string("slot"), 0);
            return 0;
        }
        owner_binding.slot = parse_figure_slot_name(xml_parser_get_attribute_string("slot"));
    }
    if (xml_parser_has_attribute("building")) {
        const char *building_attr = xml_parser_get_attribute_string("building");
        owner_binding.required_building_type = parse_building_type_name(building_attr);
        if (owner_binding.required_building_type == BUILDING_NONE &&
            (!building_attr || strcmp(building_attr, "any") != 0)) {
            g_parse_state.error = 1;
            log_error("FigureType owner node has an unknown building type", building_attr, 0);
            return 0;
        }
    }
    if (xml_parser_has_attribute("state")) {
        if (!is_known_owner_state_name(xml_parser_get_attribute_string("state"))) {
            g_parse_state.error = 1;
            log_error("FigureType owner node has an unknown state", xml_parser_get_attribute_string("state"), 0);
            return 0;
        }
        owner_binding.required_owner_state = parse_owner_state_name(xml_parser_get_attribute_string("state"));
    }

    g_parse_state.definition->set_owner_binding(owner_binding);
    g_parse_state.saw_owner = 1;
    return 1;
}

static int parse_movement_node()
{
    if (!g_parse_state.definition) {
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_movement) {
        g_parse_state.error = 1;
        log_error("FigureType xml contains duplicate movement nodes", g_parse_state.definition->attr(), 0);
        return 0;
    }
    if (!xml_parser_has_attribute("terrain_usage") ||
        !xml_parser_has_attribute("roam_ticks") ||
        !xml_parser_has_attribute("max_roam_length")) {
        g_parse_state.error = 1;
        log_error("FigureType movement node is missing required attributes", 0, 0);
        return 0;
    }

    MovementProfile movement_profile;
    movement_profile.terrain_usage = parse_terrain_usage_name(xml_parser_get_attribute_string("terrain_usage"));
    if (movement_profile.terrain_usage < 0) {
        g_parse_state.error = 1;
        log_error("FigureType movement node has an invalid terrain_usage", xml_parser_get_attribute_string("terrain_usage"), 0);
        return 0;
    }
    movement_profile.roam_ticks = xml_parser_get_attribute_int("roam_ticks");
    movement_profile.max_roam_length = xml_parser_get_attribute_int("max_roam_length");
    if (movement_profile.roam_ticks <= 0 || movement_profile.max_roam_length <= 0) {
        g_parse_state.error = 1;
        log_error("FigureType movement node requires positive roam_ticks and max_roam_length", 0, 0);
        return 0;
    }
    if (xml_parser_has_attribute("return_mode")) {
        if (!is_known_return_mode_name(xml_parser_get_attribute_string("return_mode"))) {
            g_parse_state.error = 1;
            log_error("FigureType movement node has an unknown return_mode", xml_parser_get_attribute_string("return_mode"), 0);
            return 0;
        }
        movement_profile.return_mode = parse_return_mode_name(xml_parser_get_attribute_string("return_mode"));
    }

    g_parse_state.definition->set_movement_profile(movement_profile);
    g_parse_state.saw_movement = 1;
    return 1;
}

static int parse_graphics_node()
{
    if (!g_parse_state.definition) {
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_graphics) {
        g_parse_state.error = 1;
        log_error("FigureType xml contains duplicate graphics nodes", g_parse_state.definition->attr(), 0);
        return 0;
    }
    if (!xml_parser_has_attribute("image_group")) {
        g_parse_state.error = 1;
        log_error("FigureType graphics node is missing required attribute 'image_group'", 0, 0);
        return 0;
    }

    GraphicsPolicy graphics_policy;
    graphics_policy.image_group = parse_image_group_name(xml_parser_get_attribute_string("image_group"));
    if (!graphics_policy.image_group) {
        g_parse_state.error = 1;
        log_error("FigureType graphics node has an unknown image_group", xml_parser_get_attribute_string("image_group"), 0);
        return 0;
    }
    if (xml_parser_has_attribute("max_image_offset")) {
        graphics_policy.max_image_offset = xml_parser_get_attribute_int("max_image_offset");
        if (graphics_policy.max_image_offset <= 0) {
            g_parse_state.error = 1;
            log_error("FigureType graphics node requires a positive max_image_offset", 0, 0);
            return 0;
        }
    }

    g_parse_state.definition->set_graphics_policy(graphics_policy);
    g_parse_state.saw_graphics = 1;
    return 1;
}

static const xml_parser_element XML_ELEMENTS[] = {
    { "figure", parse_definition_root, nullptr, nullptr, nullptr },
    { "native", parse_native_node, nullptr, "figure", nullptr },
    { "owner", parse_owner_node, nullptr, "figure", nullptr },
    { "movement", parse_movement_node, nullptr, "figure", nullptr },
    { "graphics", parse_graphics_node, nullptr, "figure", nullptr }
};

static int load_file_to_buffer(const char *filename, std::vector<char> &buffer)
{
    FILE *fp = file_open(filename, "rb");
    if (!fp) {
        set_failure_reason("Failed to load FigureType definition.", filename);
        return 0;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        file_close(fp);
        set_failure_reason("Failed to load FigureType definition.", filename);
        return 0;
    }

    long size = ftell(fp);
    if (size < 0) {
        file_close(fp);
        set_failure_reason("Failed to load FigureType definition.", filename);
        return 0;
    }

    rewind(fp);
    buffer.resize(static_cast<size_t>(size));
    const size_t read = buffer.empty() ? 0 : fread(buffer.data(), 1, buffer.size(), fp);
    file_close(fp);
    if (read != buffer.size()) {
        set_failure_reason("Failed to load FigureType definition.", filename);
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
        set_failure_reason("Failed to initialize FigureType XML parser.", filename);
        return 0;
    }

    const ErrorContextScope scope("FigureType XML", filename);
    const int parsed = xml_parser_parse(buffer.data(), static_cast<unsigned int>(buffer.size()), 1);
    xml_parser_free();

    if (!parsed ||
        g_parse_state.error ||
        !g_parse_state.saw_root ||
        !g_parse_state.saw_native ||
        !g_parse_state.saw_owner ||
        !g_parse_state.saw_movement ||
        !g_parse_state.saw_graphics ||
        !g_parse_state.definition) {
        error_context_report_error("Invalid FigureType XML", filename);
        set_failure_reason("Failed to parse FigureType definition.", filename);
        return 0;
    }

    figure_type type = g_parse_state.definition->type();
    if (type <= FIGURE_NONE || type >= FIGURE_TYPE_MAX) {
        set_failure_reason("FigureType definition has an invalid figure id.", filename);
        return 0;
    }

    if (!g_figure_types[type]) {
        g_figure_types[type] = std::move(g_parse_state.definition);
    }
    return 1;
}

} // namespace figure_type_registry_impl

extern "C" const char *figure_type_registry_get_failure_reason(void)
{
    return figure_type_registry_impl::g_failure_reason.c_str();
}

extern "C" void figure_type_registry_reset(void)
{
    using namespace figure_type_registry_impl;

    g_failure_reason.clear();
    for (std::unique_ptr<FigureTypeDefinition> &definition : g_figure_types) {
        definition.reset();
    }
}

extern "C" int figure_type_registry_load(void)
{
    using namespace figure_type_registry_impl;

    figure_type_registry_reset();

    const std::vector<std::string> candidate_paths = build_candidate_definition_paths();
    int found_any_definition_file = 0;

    for (const std::string &path : candidate_paths) {
        if (!directory_exists(path.c_str())) {
            continue;
        }

        const dir_listing *files = dir_find_files_with_extension(path.c_str(), "xml");
        if (!files || files->num_files <= 0) {
            continue;
        }

        found_any_definition_file = 1;
        for (int i = 0; i < files->num_files; i++) {
            char full_path[FILE_NAME_MAX];
            snprintf(full_path, FILE_NAME_MAX, "%s%s", path.c_str(), files->files[i].name);
            if (!parse_definition_file(full_path)) {
                log_error("Unable to parse FigureType xml", full_path, 0);
                return 0;
            }
        }
    }

    if (!found_any_definition_file) {
        std::string detail;
        for (size_t i = 0; i < candidate_paths.size(); i++) {
            if (i) {
                detail += "\n";
            }
            detail += candidate_paths[i];
        }
        set_failure_reason("No FigureType xml files were found in the configured mod precedence.", detail.c_str());
        log_error("No FigureType xml files found in configured mod precedence", 0, 0);
        return 0;
    }

    return 1;
}

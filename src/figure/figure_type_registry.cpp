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

#include <array>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string>
#include <string_view>
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

void FigureTypeDefinition::set_pathing_policy(const PathingPolicy &pathing_policy)
{
    pathing_policy_ = pathing_policy;
}

const PathingPolicy &FigureTypeDefinition::pathing_policy() const
{
    return pathing_policy_;
}

static int stop_on_first_entry([[maybe_unused]] const char *name, [[maybe_unused]] long unused)
{
    return LIST_MATCH;
}

bool text_equals(const char *value, std::string_view expected)
{
    return value && std::string_view(value) == expected;
}

class ScopedFile {
public:
    explicit ScopedFile(FILE *file)
        : file_(file)
    {
    }

    ~ScopedFile()
    {
        if (file_) {
            file_close(file_);
        }
    }

    FILE *get() const
    {
        return file_;
    }

private:
    FILE *file_ = nullptr;
};

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
        if (text_equals(name, "Augustus") || text_equals(name, "Julius")) {
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
        std::string_view name;
        figure_type type;
    };

    static constexpr std::array<NamedFigure, 8> kFigureNames = { {
        { "labor_seeker", FIGURE_LABOR_SEEKER },
        { "engineer", FIGURE_ENGINEER },
        { "prefect", FIGURE_PREFECT },
        { "teacher", FIGURE_TEACHER },
        { "librarian", FIGURE_LIBRARIAN },
        { "barber", FIGURE_BARBER },
        { "bathhouse_worker", FIGURE_BATHHOUSE_WORKER },
        { "school_child", FIGURE_SCHOOL_CHILD }
    } };

    for (const NamedFigure &entry : kFigureNames) {
        if (text_equals(name, entry.name)) {
            return entry.type;
        }
    }
    return FIGURE_NONE;
}

static building_type parse_building_type_name(const char *attr)
{
    if (!attr || text_equals(attr, "any")) {
        return BUILDING_ANY;
    }

    for (building_type type = BUILDING_NONE; type < BUILDING_TYPE_MAX;
        type = static_cast<building_type>(static_cast<int>(type) + 1)) {
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
    if (text_equals(name, "roaming_service")) {
        return NativeClassId::RoamingService;
    }
    if (text_equals(name, "engineer_service")) {
        return NativeClassId::EngineerService;
    }
    if (text_equals(name, "prefect_service")) {
        return NativeClassId::PrefectService;
    }
    return NativeClassId::None;
}

static FigureSlot parse_figure_slot_name(const char *name)
{
    if (!name || text_equals(name, "none")) {
        return FigureSlot::None;
    }
    if (text_equals(name, "primary")) {
        return FigureSlot::Primary;
    }
    if (text_equals(name, "secondary")) {
        return FigureSlot::Secondary;
    }
    if (text_equals(name, "quaternary")) {
        return FigureSlot::Quaternary;
    }
    return FigureSlot::None;
}

static bool is_known_figure_slot_name(const char *name)
{
    return !name ||
        text_equals(name, "none") ||
        text_equals(name, "primary") ||
        text_equals(name, "secondary") ||
        text_equals(name, "quaternary");
}

static OwnerStateRequirement parse_owner_state_name(const char *name)
{
    if (!name || text_equals(name, "in_use")) {
        return OwnerStateRequirement::InUse;
    }
    if (text_equals(name, "in_use_or_mothballed")) {
        return OwnerStateRequirement::InUseOrMothballed;
    }
    if (text_equals(name, "any")) {
        return OwnerStateRequirement::Any;
    }
    return OwnerStateRequirement::InUse;
}

static bool is_known_owner_state_name(const char *name)
{
    return !name ||
        text_equals(name, "any") ||
        text_equals(name, "in_use") ||
        text_equals(name, "in_use_or_mothballed");
}

static ReturnMode parse_return_mode_name(const char *name)
{
    if (text_equals(name, "die_at_limit")) {
        return ReturnMode::DieAtLimit;
    }
    return ReturnMode::ReturnToOwnerRoad;
}

static bool is_known_return_mode_name(const char *name)
{
    return !name ||
        text_equals(name, "return_to_owner_road") ||
        text_equals(name, "die_at_limit");
}

static int parse_terrain_usage_name(const char *name)
{
    if (!name || text_equals(name, "any")) {
        return TERRAIN_USAGE_ANY;
    }
    if (text_equals(name, "roads")) {
        return TERRAIN_USAGE_ROADS;
    }
    if (text_equals(name, "roads_highway")) {
        return TERRAIN_USAGE_ROADS_HIGHWAY;
    }
    if (text_equals(name, "prefer_roads")) {
        return TERRAIN_USAGE_PREFER_ROADS;
    }
    if (text_equals(name, "prefer_roads_highway")) {
        return TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;
    }
    return -1;
}

static int parse_image_group_name(const char *name)
{
    struct NamedGroup {
        std::string_view name;
        int image_group_id;
    };

    static constexpr std::array<NamedGroup, 7> kImageGroups = { {
        { "labor_seeker", GROUP_FIGURE_LABOR_SEEKER },
        { "engineer", GROUP_FIGURE_ENGINEER },
        { "prefect", GROUP_FIGURE_PREFECT },
        { "teacher_librarian", GROUP_FIGURE_TEACHER_LIBRARIAN },
        { "barber", GROUP_FIGURE_BARBER },
        { "bathhouse_worker", GROUP_FIGURE_BATHHOUSE_WORKER },
        { "school_child", GROUP_FIGURE_SCHOOL_CHILD }
    } };

    for (const NamedGroup &entry : kImageGroups) {
        if (text_equals(name, entry.name)) {
            return entry.image_group_id;
        }
    }
    return 0;
}

static PathingMode parse_pathing_mode_name(const char *name)
{
    if (text_equals(name, "smart_service")) {
        return PathingMode::SmartService;
    }
    return PathingMode::VanillaRoaming;
}

static bool is_known_pathing_mode_name(const char *name)
{
    return name &&
        (text_equals(name, "vanilla_roaming") ||
            text_equals(name, "smart_service"));
}

static road_service_effect parse_service_effect_name(const char *name)
{
    if (!name || text_equals(name, "none")) {
        return ROAD_SERVICE_EFFECT_NONE;
    }
    if (text_equals(name, "labor")) {
        return ROAD_SERVICE_EFFECT_LABOR;
    }
    if (text_equals(name, "academy")) {
        return ROAD_SERVICE_EFFECT_ACADEMY;
    }
    if (text_equals(name, "library")) {
        return ROAD_SERVICE_EFFECT_LIBRARY;
    }
    if (text_equals(name, "barber")) {
        return ROAD_SERVICE_EFFECT_BARBER;
    }
    if (text_equals(name, "bathhouse")) {
        return ROAD_SERVICE_EFFECT_BATHHOUSE;
    }
    if (text_equals(name, "school")) {
        return ROAD_SERVICE_EFFECT_SCHOOL;
    }
    if (text_equals(name, "damage_risk")) {
        return ROAD_SERVICE_EFFECT_DAMAGE_RISK;
    }
    if (text_equals(name, "fire_risk")) {
        return ROAD_SERVICE_EFFECT_FIRE_RISK;
    }
    return ROAD_SERVICE_EFFECT_NONE;
}

static bool is_known_service_effect_name(const char *name)
{
    return !name ||
        text_equals(name, "none") ||
        text_equals(name, "labor") ||
        text_equals(name, "academy") ||
        text_equals(name, "library") ||
        text_equals(name, "barber") ||
        text_equals(name, "bathhouse") ||
        text_equals(name, "school") ||
        text_equals(name, "damage_risk") ||
        text_equals(name, "fire_risk");
}

static bool is_road_only_terrain_usage(int terrain_usage)
{
    return terrain_usage == TERRAIN_USAGE_ROADS ||
        terrain_usage == TERRAIN_USAGE_ROADS_HIGHWAY;
}

struct ParseState {
    std::unique_ptr<FigureTypeDefinition> definition;
    bool saw_root = false;
    bool saw_native = false;
    bool saw_owner = false;
    bool saw_movement = false;
    bool saw_graphics = false;
    bool saw_pathing = false;
    bool error = false;
};

static ParseState g_parse_state;

static int parse_definition_root()
{
    if (g_parse_state.saw_root) {
        g_parse_state.error = true;
        log_error("Duplicate FigureType root node", 0, 0);
        return 0;
    }
    if (!xml_parser_has_attribute("type")) {
        g_parse_state.error = true;
        log_error("FigureType root is missing required attribute 'type'", 0, 0);
        return 0;
    }

    const char *type_attr = xml_parser_get_attribute_string("type");
    figure_type type = parse_figure_type_name(type_attr);
    if (type == FIGURE_NONE) {
        g_parse_state.error = true;
        log_error("FigureType root has an unknown figure type", type_attr, 0);
        return 0;
    }

    g_parse_state.definition = std::make_unique<FigureTypeDefinition>(type, type_attr ? type_attr : "");
    g_parse_state.saw_root = true;
    return 1;
}

static int parse_native_node()
{
    if (!g_parse_state.definition) {
        g_parse_state.error = true;
        return 0;
    }
    if (g_parse_state.saw_native) {
        g_parse_state.error = true;
        log_error("FigureType xml contains duplicate native nodes", g_parse_state.definition->attr(), 0);
        return 0;
    }
    if (!xml_parser_has_attribute("class")) {
        g_parse_state.error = true;
        log_error("FigureType native node is missing required attribute 'class'", 0, 0);
        return 0;
    }

    NativeClassId native_class_id = parse_native_class_name(xml_parser_get_attribute_string("class"));
    if (native_class_id == NativeClassId::None) {
        g_parse_state.error = true;
        log_error("FigureType native node has an unknown class", xml_parser_get_attribute_string("class"), 0);
        return 0;
    }

    g_parse_state.definition->set_native_class(native_class_id);
    g_parse_state.saw_native = true;
    return 1;
}

static int parse_owner_node()
{
    if (!g_parse_state.definition) {
        g_parse_state.error = true;
        return 0;
    }
    if (g_parse_state.saw_owner) {
        g_parse_state.error = true;
        log_error("FigureType xml contains duplicate owner nodes", g_parse_state.definition->attr(), 0);
        return 0;
    }

    OwnerBinding owner_binding;
    if (xml_parser_has_attribute("slot")) {
        if (!is_known_figure_slot_name(xml_parser_get_attribute_string("slot"))) {
            g_parse_state.error = true;
            log_error("FigureType owner node has an unknown slot", xml_parser_get_attribute_string("slot"), 0);
            return 0;
        }
        owner_binding.slot = parse_figure_slot_name(xml_parser_get_attribute_string("slot"));
    }
    if (xml_parser_has_attribute("building")) {
        const char *building_attr = xml_parser_get_attribute_string("building");
        owner_binding.required_building_type = parse_building_type_name(building_attr);
        if (owner_binding.required_building_type == BUILDING_NONE &&
            (!building_attr || !text_equals(building_attr, "any"))) {
            g_parse_state.error = true;
            log_error("FigureType owner node has an unknown building type", building_attr, 0);
            return 0;
        }
    }
    if (xml_parser_has_attribute("state")) {
        if (!is_known_owner_state_name(xml_parser_get_attribute_string("state"))) {
            g_parse_state.error = true;
            log_error("FigureType owner node has an unknown state", xml_parser_get_attribute_string("state"), 0);
            return 0;
        }
        owner_binding.required_owner_state = parse_owner_state_name(xml_parser_get_attribute_string("state"));
    }

    g_parse_state.definition->set_owner_binding(owner_binding);
    g_parse_state.saw_owner = true;
    return 1;
}

static int parse_movement_node()
{
    if (!g_parse_state.definition) {
        g_parse_state.error = true;
        return 0;
    }
    if (g_parse_state.saw_movement) {
        g_parse_state.error = true;
        log_error("FigureType xml contains duplicate movement nodes", g_parse_state.definition->attr(), 0);
        return 0;
    }
    if (!xml_parser_has_attribute("terrain_usage") ||
        !xml_parser_has_attribute("roam_ticks") ||
        !xml_parser_has_attribute("max_roam_length")) {
        g_parse_state.error = true;
        log_error("FigureType movement node is missing required attributes", 0, 0);
        return 0;
    }

    MovementProfile movement_profile;
    movement_profile.terrain_usage = parse_terrain_usage_name(xml_parser_get_attribute_string("terrain_usage"));
    if (movement_profile.terrain_usage < 0) {
        g_parse_state.error = true;
        log_error("FigureType movement node has an invalid terrain_usage", xml_parser_get_attribute_string("terrain_usage"), 0);
        return 0;
    }
    if (movement_profile.terrain_usage > std::numeric_limits<unsigned char>::max()) {
        g_parse_state.error = true;
        log_error("FigureType movement node terrain_usage exceeds figure storage range", 0, 0);
        return 0;
    }
    movement_profile.roam_ticks = xml_parser_get_attribute_int("roam_ticks");
    movement_profile.max_roam_length = xml_parser_get_attribute_int("max_roam_length");
    if (movement_profile.roam_ticks <= 0 || movement_profile.max_roam_length <= 0) {
        g_parse_state.error = true;
        log_error("FigureType movement node requires positive roam_ticks and max_roam_length", 0, 0);
        return 0;
    }
    if (movement_profile.max_roam_length > std::numeric_limits<short>::max()) {
        g_parse_state.error = true;
        log_error("FigureType movement node max_roam_length exceeds figure storage range", 0, 0);
        return 0;
    }
    if (xml_parser_has_attribute("return_mode")) {
        if (!is_known_return_mode_name(xml_parser_get_attribute_string("return_mode"))) {
            g_parse_state.error = true;
            log_error("FigureType movement node has an unknown return_mode", xml_parser_get_attribute_string("return_mode"), 0);
            return 0;
        }
        movement_profile.return_mode = parse_return_mode_name(xml_parser_get_attribute_string("return_mode"));
    }

    g_parse_state.definition->set_movement_profile(movement_profile);
    g_parse_state.saw_movement = true;
    return 1;
}

static int parse_graphics_node()
{
    if (!g_parse_state.definition) {
        g_parse_state.error = true;
        return 0;
    }
    if (g_parse_state.saw_graphics) {
        g_parse_state.error = true;
        log_error("FigureType xml contains duplicate graphics nodes", g_parse_state.definition->attr(), 0);
        return 0;
    }
    if (!xml_parser_has_attribute("image_group")) {
        g_parse_state.error = true;
        log_error("FigureType graphics node is missing required attribute 'image_group'", 0, 0);
        return 0;
    }

    GraphicsPolicy graphics_policy;
    graphics_policy.image_group = parse_image_group_name(xml_parser_get_attribute_string("image_group"));
    if (!graphics_policy.image_group) {
        g_parse_state.error = true;
        log_error("FigureType graphics node has an unknown image_group", xml_parser_get_attribute_string("image_group"), 0);
        return 0;
    }
    if (xml_parser_has_attribute("max_image_offset")) {
        graphics_policy.max_image_offset = xml_parser_get_attribute_int("max_image_offset");
        if (graphics_policy.max_image_offset <= 0) {
            g_parse_state.error = true;
            log_error("FigureType graphics node requires a positive max_image_offset", 0, 0);
            return 0;
        }
    }

    g_parse_state.definition->set_graphics_policy(graphics_policy);
    g_parse_state.saw_graphics = true;
    return 1;
}

static int parse_pathing_node()
{
    if (!g_parse_state.definition) {
        g_parse_state.error = true;
        return 0;
    }
    if (g_parse_state.saw_pathing) {
        g_parse_state.error = true;
        log_error("FigureType xml contains duplicate pathing nodes", g_parse_state.definition->attr(), 0);
        return 0;
    }
    if (!xml_parser_has_attribute("mode")) {
        g_parse_state.error = true;
        log_error("FigureType pathing node is missing required attribute 'mode'", 0, 0);
        return 0;
    }
    if (!is_known_pathing_mode_name(xml_parser_get_attribute_string("mode"))) {
        g_parse_state.error = true;
        log_error("FigureType pathing node has an unknown mode", xml_parser_get_attribute_string("mode"), 0);
        return 0;
    }
    if (xml_parser_has_attribute("effect") &&
        !is_known_service_effect_name(xml_parser_get_attribute_string("effect"))) {
        g_parse_state.error = true;
        log_error("FigureType pathing node has an unknown effect", xml_parser_get_attribute_string("effect"), 0);
        return 0;
    }

    PathingPolicy pathing_policy;
    pathing_policy.mode = parse_pathing_mode_name(xml_parser_get_attribute_string("mode"));
    pathing_policy.effect = parse_service_effect_name(xml_parser_get_attribute_string("effect"));

    if (pathing_policy.mode == PathingMode::SmartService) {
        // Smart service pathing compares road-tile history for one service effect,
        // so it is only valid for road walkers with an explicit effect id.
        if (pathing_policy.effect == ROAD_SERVICE_EFFECT_NONE) {
            g_parse_state.error = true;
            error_context_report_error("FigureType smart_service pathing requires a service effect",
                g_parse_state.definition->attr());
            return 0;
        }
        if (!g_parse_state.saw_movement ||
            !is_road_only_terrain_usage(g_parse_state.definition->movement_profile().terrain_usage)) {
            g_parse_state.error = true;
            error_context_report_error("FigureType smart_service pathing requires road-only movement",
                g_parse_state.definition->attr());
            return 0;
        }
    }

    g_parse_state.definition->set_pathing_policy(pathing_policy);
    g_parse_state.saw_pathing = true;
    return 1;
}

static const std::array<xml_parser_element, 6> XML_ELEMENTS = { {
    { "figure", parse_definition_root, nullptr, nullptr, nullptr },
    { "native", parse_native_node, nullptr, "figure", nullptr },
    { "owner", parse_owner_node, nullptr, "figure", nullptr },
    { "movement", parse_movement_node, nullptr, "figure", nullptr },
    { "graphics", parse_graphics_node, nullptr, "figure", nullptr },
    { "pathing", parse_pathing_node, nullptr, "figure", nullptr }
} };

static int load_file_to_buffer(const char *filename, std::vector<char> &buffer)
{
    ScopedFile fp(file_open(filename, "rb"));
    if (!fp.get()) {
        set_failure_reason("Failed to load FigureType definition.", filename);
        return 0;
    }

    if (fseek(fp.get(), 0, SEEK_END) != 0) {
        set_failure_reason("Failed to load FigureType definition.", filename);
        return 0;
    }

    long size = ftell(fp.get());
    if (size < 0) {
        set_failure_reason("Failed to load FigureType definition.", filename);
        return 0;
    }

    rewind(fp.get());
    buffer.resize(static_cast<size_t>(size));
    const size_t read = buffer.empty() ? 0 : fread(buffer.data(), 1, buffer.size(), fp.get());
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
    if (buffer.size() > std::numeric_limits<unsigned int>::max()) {
        set_failure_reason("FigureType definition is too large to parse.", filename);
        return 0;
    }

    g_parse_state = {};
    if (!xml_parser_init(XML_ELEMENTS.data(), static_cast<int>(XML_ELEMENTS.size()), 1)) {
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
        !g_parse_state.saw_pathing ||
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
    figure_type_registry_impl::g_failure_reason.clear();
    for (std::unique_ptr<figure_type_registry_impl::FigureTypeDefinition> &definition :
        figure_type_registry_impl::g_figure_types) {
        definition.reset();
    }
}

extern "C" int figure_type_registry_load(void)
{
    figure_type_registry_reset();

    // Definitions are resolved by precedence: selected mod first, then Augustus,
    // then Julius. The first loaded definition for a figure type wins.
    const std::vector<std::string> candidate_paths = figure_type_registry_impl::build_candidate_definition_paths();
    int found_any_definition_file = 0;

    for (const std::string &path : candidate_paths) {
        if (!figure_type_registry_impl::directory_exists(path.c_str())) {
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
            if (!figure_type_registry_impl::parse_definition_file(full_path)) {
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
        figure_type_registry_impl::set_failure_reason(
            "No FigureType xml files were found in the configured mod precedence.",
            detail.c_str());
        log_error("No FigureType xml files found in configured mod precedence", 0, 0);
        return 0;
    }

    return 1;
}

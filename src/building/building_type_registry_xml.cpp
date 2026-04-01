#include "building/building_type_registry_internal.h"

extern "C" {
#include "building/building_runtime.h"
#include "building/properties.h"
#include "core/dir.h"
#include "core/file.h"
#include "core/log.h"
#include "core/xml_parser.h"
#include "figure/action.h"
}

#include <cstdio>
#include <cstring>

namespace building_type_registry_impl {

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

static std::string trim_copy(const std::string &value)
{
    size_t start = 0;
    while (start < value.size() && (value[start] == ' ' || value[start] == '\t' || value[start] == '\r' || value[start] == '\n')) {
        start++;
    }

    size_t end = value.size();
    while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r' || value[end - 1] == '\n')) {
        end--;
    }
    return value.substr(start, end - start);
}

static int parse_bool_value(const char *value, int *out_value)
{
    if (!value || !out_value) {
        return 0;
    }

    if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0 || strcmp(value, "yes") == 0) {
        *out_value = 1;
        return 1;
    }
    if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0 || strcmp(value, "no") == 0) {
        *out_value = 0;
        return 1;
    }
    return 0;
}

static figure_type parse_figure_type_name(const char *name)
{
    struct named_figure {
        const char *name;
        figure_type type;
    };
    static const named_figure figure_names[] = {
        { "actor", FIGURE_ACTOR },
        { "barber", FIGURE_BARBER },
        { "bathhouse_worker", FIGURE_BATHHOUSE_WORKER },
        { "doctor", FIGURE_DOCTOR },
        { "gladiator", FIGURE_GLADIATOR },
        { "librarian", FIGURE_LIBRARIAN },
        { "school_child", FIGURE_SCHOOL_CHILD },
        { "surgeon", FIGURE_SURGEON },
        { "teacher", FIGURE_TEACHER },
        { "work_camp_architect", FIGURE_WORK_CAMP_ARCHITECT },
        { "work_camp_worker", FIGURE_WORK_CAMP_WORKER }
    };

    for (size_t i = 0; i < sizeof(figure_names) / sizeof(figure_names[0]); i++) {
        if (name && strcmp(name, figure_names[i].name) == 0) {
            return figure_names[i].type;
        }
    }
    return FIGURE_NONE;
}

static int parse_action_state_name(const char *name)
{
    struct named_action_state {
        const char *name;
        int action_state;
    };
    static const named_action_state action_names[] = {
        { "roaming", FIGURE_ACTION_125_ROAMING },
        { "entertainer_roaming", FIGURE_ACTION_94_ENTERTAINER_ROAMING },
        { "entertainer_school_created", FIGURE_ACTION_90_ENTERTAINER_AT_SCHOOL_CREATED },
        { "work_camp_worker_created", FIGURE_ACTION_203_WORK_CAMP_WORKER_CREATED },
        { "work_camp_architect_created", FIGURE_ACTION_206_WORK_CAMP_ARCHITECT_CREATED }
    };

    for (size_t i = 0; i < sizeof(action_names) / sizeof(action_names[0]); i++) {
        if (name && strcmp(name, action_names[i].name) == 0) {
            return action_names[i].action_state;
        }
    }
    return 0;
}

static int parse_figure_list_attribute(const char *value, std::vector<figure_type> &out_figures)
{
    if (!value || !*value) {
        return 1;
    }

    std::string list = value;
    size_t start = 0;
    while (start <= list.size()) {
        size_t end = list.find(',', start);
        std::string token = trim_copy(list.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (!token.empty()) {
            figure_type type = parse_figure_type_name(token.c_str());
            if (type == FIGURE_NONE) {
                return 0;
            }
            out_figures.push_back(type);
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return 1;
}

static RoadAccessMode parse_road_access_mode(const char *value)
{
    if (value && strcmp(value, "normal") == 0) {
        return RoadAccessMode::Normal;
    }
    return RoadAccessMode::None;
}

static LaborSeekerMode parse_labor_seeker_mode(const char *value)
{
    if (value && strcmp(value, "spawn_if_below") == 0) {
        return LaborSeekerMode::SpawnIfBelow;
    }
    if (value && strcmp(value, "generate_if_below") == 0) {
        return LaborSeekerMode::GenerateIfBelow;
    }
    return LaborSeekerMode::None;
}

static DelayProfile parse_delay_profile(const char *value)
{
    if (value && strcmp(value, "default") == 0) {
        return DelayProfile::Default;
    }
    return DelayProfile::None;
}

static GraphicTiming parse_graphic_timing(const char *value)
{
    if (value && strcmp(value, "before_delay_check") == 0) {
        return GraphicTiming::BeforeDelayCheck;
    }
    if (value && strcmp(value, "before_successful_spawn") == 0) {
        return GraphicTiming::BeforeSuccessfulSpawn;
    }
    return GraphicTiming::None;
}

static FigureSlot parse_figure_slot(const char *value)
{
    if (value && strcmp(value, "primary") == 0) {
        return FigureSlot::Primary;
    }
    if (value && strcmp(value, "secondary") == 0) {
        return FigureSlot::Secondary;
    }
    if (value && strcmp(value, "quaternary") == 0) {
        return FigureSlot::Quaternary;
    }
    if (value && strcmp(value, "none") == 0) {
        return FigureSlot::None;
    }
    return FigureSlot::Primary;
}

static GuardTiming parse_guard_timing(const char *value)
{
    if (value && strcmp(value, "after_labor_seeker") == 0) {
        return GuardTiming::AfterLaborSeeker;
    }
    return GuardTiming::BeforeRoadAccess;
}

static int parse_spawn_direction(const char *value)
{
    if (value && strcmp(value, "bottom") == 0) {
        return DIR_4_BOTTOM;
    }
    return DIR_0_TOP;
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

static int parse_spawn()
{
    if (!g_parse_state.definition) {
        log_error("Encountered spawn definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("mode")) {
        log_error("BuildingType spawn is missing required attribute 'mode'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *mode_text = xml_parser_get_attribute_string("mode");
    SpawnPolicy policy;
    if (mode_text && strcmp(mode_text, "service_roamer") == 0) {
        policy.mode = SpawnMode::ServiceRoamer;
    } else {
        log_error("Unsupported BuildingType spawn mode", mode_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *road_access_text = xml_parser_has_attribute("road_access") ?
        xml_parser_get_attribute_string("road_access") : "normal";
    policy.road_access_mode = parse_road_access_mode(road_access_text);
    if (policy.road_access_mode == RoadAccessMode::None) {
        log_error("Unsupported BuildingType spawn road_access", road_access_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *delay_profile_text = xml_parser_has_attribute("delay_profile") ?
        xml_parser_get_attribute_string("delay_profile") : "default";
    policy.delay_profile = parse_delay_profile(delay_profile_text);
    if (policy.delay_profile == DelayProfile::None) {
        log_error("Unsupported BuildingType spawn delay_profile", delay_profile_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *graphic_timing_text = xml_parser_has_attribute("graphic_timing") ?
        xml_parser_get_attribute_string("graphic_timing") : "none";
    policy.graphic_timing = parse_graphic_timing(graphic_timing_text);
    if (graphic_timing_text && strcmp(graphic_timing_text, "none") != 0 && policy.graphic_timing == GraphicTiming::None) {
        log_error("Unsupported BuildingType spawn graphic_timing", graphic_timing_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    if (xml_parser_has_attribute("labor_seeker_mode")) {
        const char *labor_mode_text = xml_parser_get_attribute_string("labor_seeker_mode");
        policy.labor_seeker_mode = parse_labor_seeker_mode(labor_mode_text);
        if (policy.labor_seeker_mode == LaborSeekerMode::None && strcmp(labor_mode_text, "none") != 0) {
            log_error("Unsupported BuildingType spawn labor_seeker_mode", labor_mode_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    if (xml_parser_has_attribute("labor_min_houses")) {
        policy.labor_min_houses = xml_parser_get_attribute_int("labor_min_houses");
    }

    if (xml_parser_has_attribute("existing_figure")) {
        const char *existing_figure_text = xml_parser_get_attribute_string("existing_figure");
        if (!parse_figure_list_attribute(existing_figure_text, policy.existing_figures)) {
            log_error("Unsupported BuildingType spawn existing_figure", existing_figure_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    if (!xml_parser_has_attribute("spawn_figure")) {
        log_error("BuildingType spawn is missing required attribute 'spawn_figure'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    policy.spawn_figure = parse_figure_type_name(xml_parser_get_attribute_string("spawn_figure"));
    if (policy.spawn_figure == FIGURE_NONE) {
        log_error("Unsupported BuildingType spawn spawn_figure", xml_parser_get_attribute_string("spawn_figure"), 0);
        g_parse_state.error = 1;
        return 0;
    }

    if (!xml_parser_has_attribute("action_state")) {
        log_error("BuildingType spawn is missing required attribute 'action_state'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    policy.action_state = parse_action_state_name(xml_parser_get_attribute_string("action_state"));
    if (!policy.action_state) {
        log_error("Unsupported BuildingType spawn action_state", xml_parser_get_attribute_string("action_state"), 0);
        g_parse_state.error = 1;
        return 0;
    }

    if (xml_parser_has_attribute("direction")) {
        const char *direction_text = xml_parser_get_attribute_string("direction");
        policy.spawn_direction = parse_spawn_direction(direction_text);
        if (strcmp(direction_text, "top") != 0 && strcmp(direction_text, "bottom") != 0) {
            log_error("Unsupported BuildingType spawn direction", direction_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    if (xml_parser_has_attribute("figure_slot")) {
        const char *figure_slot_text = xml_parser_get_attribute_string("figure_slot");
        policy.figure_slot = parse_figure_slot(figure_slot_text);
        if (strcmp(figure_slot_text, "primary") != 0 && strcmp(figure_slot_text, "secondary") != 0 &&
            strcmp(figure_slot_text, "quaternary") != 0 && strcmp(figure_slot_text, "none") != 0) {
            log_error("Unsupported BuildingType spawn figure_slot", figure_slot_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    if (xml_parser_has_attribute("guard_timing")) {
        const char *guard_timing_text = xml_parser_get_attribute_string("guard_timing");
        policy.guard_timing = parse_guard_timing(guard_timing_text);
        if (strcmp(guard_timing_text, "before_road_access") != 0 &&
            strcmp(guard_timing_text, "after_labor_seeker") != 0) {
            log_error("Unsupported BuildingType spawn guard_timing", guard_timing_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    if (xml_parser_has_attribute("init_roaming")) {
        if (!parse_bool_value(xml_parser_get_attribute_string("init_roaming"), &policy.init_roaming)) {
            log_error("Unsupported BuildingType spawn init_roaming", xml_parser_get_attribute_string("init_roaming"), 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    if (xml_parser_has_attribute("require_water_access")) {
        if (!parse_bool_value(xml_parser_get_attribute_string("require_water_access"), &policy.require_water_access)) {
            log_error("Unsupported BuildingType spawn require_water_access", xml_parser_get_attribute_string("require_water_access"), 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    if (xml_parser_has_attribute("mark_problem_if_no_water")) {
        if (!parse_bool_value(xml_parser_get_attribute_string("mark_problem_if_no_water"), &policy.mark_problem_if_no_water)) {
            log_error("Unsupported BuildingType spawn mark_problem_if_no_water", xml_parser_get_attribute_string("mark_problem_if_no_water"), 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    g_parse_state.definition->add_spawn_policy(std::move(policy));
    g_parse_state.saw_spawn = 1;
    return 1;
}

static const xml_parser_element XML_ELEMENTS[] = {
    { "building", parse_building_root, nullptr, nullptr, nullptr },
    { "graphic", parse_graphic, nullptr, "building", nullptr },
    { "spawn", parse_spawn, nullptr, "building", nullptr }
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
    if (!parsed || g_parse_state.error || !g_parse_state.definition || (!g_parse_state.saw_graphic && !g_parse_state.saw_spawn)) {
        if (!g_parse_state.saw_graphic && !g_parse_state.saw_spawn) {
            log_error("BuildingType xml is missing a supported node", filename, 0);
        }
        return 0;
    }

    g_building_types[g_parse_state.definition->type()] = std::move(g_parse_state.definition);
    return 1;
}

}

extern "C" int building_type_registry_load(void)
{
    using namespace building_type_registry_impl;

    refresh_building_type_path();

    for (std::unique_ptr<BuildingType> &definition : g_building_types) {
        definition.reset();
    }

    const dir_listing *files = dir_find_files_with_extension(g_building_type_path.c_str(), "xml");
    if (!files || files->num_files <= 0) {
        log_error("No BuildingType xml files found in", g_building_type_path.c_str(), 0);
        return 0;
    }

    // Load every live BuildingType definition from the selected mod folder.
    for (int i = 0; i < files->num_files; i++) {
        char full_path[FILE_NAME_MAX];
        snprintf(full_path, FILE_NAME_MAX, "%s%s", g_building_type_path.c_str(), files->files[i].name);
        if (!parse_definition_file(full_path)) {
            log_error("Unable to parse BuildingType xml", full_path, 0);
            return 0;
        }
    }

    building_runtime_reset();
    return 1;
}

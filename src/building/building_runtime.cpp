#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"

#include "assets/image_group_payload.h"
#include "core/crash_context.h"

extern "C" {
#include "building/animation.h"
#include "building/building_runtime_api.h"
#include "building/image.h"
#include "building/properties.h"
#include "city/population.h"
#include "core/calc.h"
#include "core/config.h"
#include "figure/action.h"
#include "figure/figure.h"
#include "figure/movement.h"
#include "map/building_tiles.h"
#include "map/road_access.h"
#include "map/terrain.h"
#include "core/log.h"
}

#include <cstdio>
#include <string>
#include <unordered_set>

namespace {

std::unordered_set<std::string> g_logged_building_graphics_fallbacks;

void make_building_context(char *buffer, size_t buffer_size, const building *b)
{
    if (b) {
        snprintf(
            buffer,
            buffer_size,
            "building_id=%u type=%d grid_offset=%d",
            b->id,
            static_cast<int>(b->type),
            b->grid_offset);
    } else {
        snprintf(buffer, buffer_size, "building=null");
    }
}

void log_building_scope_state(void *userdata)
{
    const building_runtime *runtime = static_cast<const building_runtime *>(userdata);
    const building *b = runtime ? runtime->building() : nullptr;
    const building_type_registry_impl::BuildingType *definition = runtime ? runtime->definition() : nullptr;
    if (!b) {
        return;
    }

    const building_type_registry_impl::GraphicsTarget *target =
        definition ? definition->resolve_graphics_target(*b) : nullptr;

    char details[256];
    snprintf(
        details,
        sizeof(details),
        "state=%d size=%d desirability=%d water=%d upgrade=%d graphics_path=%s graphics_image=%s target_path=%s target_image=%s",
        b->state,
        b->size,
        b->desirability,
        b->has_water_access,
        b->upgrade_level,
        definition && definition->graphics_path() ? definition->graphics_path() : "",
        definition && definition->graphics_image() ? definition->graphics_image() : "",
        target && target->has_path() ? target->path() : "",
        target && target->has_image() ? target->image() : "");
    log_info("Graphics building state", details, 0);
}

const image *resolve_group_image_candidates(const char *group_key, const char *const *image_ids, size_t image_id_count)
{
    if (!group_key || !*group_key || !image_ids) {
        return nullptr;
    }

    for (size_t i = 0; i < image_id_count; i++) {
        const char *image_id = image_ids[i];
        if (!image_id || !*image_id) {
            continue;
        }
        if (const image *img = image_group_payload_get_image(group_key, image_id)) {
            return img;
        }
    }
    return nullptr;
}

void log_building_graphics_fallback_once(
    const char *message,
    const building_runtime *runtime,
    const char *detail,
    const char *group_key = nullptr,
    const char *image_id = nullptr)
{
    const building *b = runtime ? runtime->building() : nullptr;
    const building_type_registry_impl::BuildingType *definition = runtime ? runtime->definition() : nullptr;

    char key_buffer[768];
    snprintf(
        key_buffer,
        sizeof(key_buffer),
        "%s|type=%d|path=%s|image=%s|detail=%s",
        message ? message : "",
        b ? static_cast<int>(b->type) : -1,
        group_key ? group_key : (definition && definition->graphics_path() ? definition->graphics_path() : ""),
        image_id ? image_id : (definition && definition->graphics_image() ? definition->graphics_image() : ""),
        detail ? detail : "");

    if (!g_logged_building_graphics_fallbacks.insert(key_buffer).second) {
        return;
    }

    crash_context_report_error(message, detail);
}

}

namespace building_runtime_impl {

std::vector<std::unique_ptr<building_runtime>> g_runtime_instances;

building_runtime *get_city_building(unsigned int id)
{
    if (!id) {
        return nullptr;
    }
    return get_or_create_instance(building_get(id));
}

building_runtime *get_city_building(::building *building_data)
{
    return get_or_create_instance(building_data);
}

building_runtime *get_or_create_instance(::building *building_data)
{
    if (!building_data || !building_data->id) {
        return nullptr;
    }

    // For now runtime wrappers are materialized lazily, but they are owned here rather than by the type registry.
    if (g_runtime_instances.size() <= building_data->id) {
        g_runtime_instances.resize(building_data->id + 1);
    }

    // Every live building gets a runtime object, even before that type has migrated to XML-driven behavior.
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::g_building_types[building_data->type].get();

    std::unique_ptr<building_runtime> &slot = g_runtime_instances[building_data->id];
    if (!slot || slot->building() != building_data || slot->definition() != definition) {
        slot = std::make_unique<building_runtime>(building_data, definition);
    }
    return slot.get();
}

}

void building_runtime::refresh_graphic_state()
{
    if (!building_ || !definition_ || !definition_->has_graphic()) {
        return;
    }

    if (definition_->water_access_mode() == building_type_registry_impl::WaterAccessMode::ReservoirRange) {
        building_->has_water_access =
            map_terrain_exists_tile_in_area_with_type(building_->x, building_->y, building_->size, TERRAIN_RESERVOIR_RANGE);
    }

    building_->upgrade_level = definition_->upgrade_level_for(*building_);
}

int building_runtime::uses_new_graphics() const
{
    return building_ && definition_ && definition_->has_graphic() && definition_->graphics_path() && *definition_->graphics_path();
}

const building_type_registry_impl::GraphicsTarget *building_runtime::resolve_graphic_target() const
{
    if (!uses_new_graphics()) {
        return nullptr;
    }
    if (definition_->uses_structured_graphics()) {
        return definition_->resolve_graphics_target(*building_);
    }
    return definition_->graphics().default_target();
}

const char *building_runtime::resolve_named_group_image_id(const char *group_key, const char *image_id) const
{
    if (!uses_new_graphics() || !group_key || !*group_key || !image_id || !*image_id) {
        return nullptr;
    }
    return image_group_payload_get_image(group_key, image_id) ? image_id : nullptr;
}

const char *building_runtime::resolve_candidate_graphic_image_id(
    const char *group_key,
    const char *const *image_ids,
    size_t image_id_count) const
{
    if (!uses_new_graphics() || !group_key || !*group_key || !image_ids) {
        return nullptr;
    }

    for (size_t i = 0; i < image_id_count; i++) {
        if (const char *image_id = resolve_named_group_image_id(group_key, image_ids[i])) {
            return image_id;
        }
    }
    return nullptr;
}

const image *building_runtime::resolve_named_group_image(const char *group_key, const char *image_id) const
{
    if (!uses_new_graphics() || !group_key || !*group_key || !image_id || !*image_id) {
        return nullptr;
    }
    return image_group_payload_get_image(group_key, image_id);
}

const image *building_runtime::resolve_candidate_graphic_image(
    const char *group_key,
    const char *const *image_ids,
    size_t image_id_count) const
{
    if (!uses_new_graphics() || !group_key || !*group_key) {
        return nullptr;
    }
    return resolve_group_image_candidates(group_key, image_ids, image_id_count);
}

const char *building_runtime::resolve_default_group_image_id(const char *group_key) const
{
    static const char *default_image_ids[] = {
        "main",
        "Main",
        "default",
        "Default",
        "Image_0000",
        "0"
    };

    if (const char *image_id = resolve_candidate_graphic_image_id(
            group_key,
            default_image_ids,
            sizeof(default_image_ids) / sizeof(default_image_ids[0]))) {
        return image_id;
    }
    return uses_new_graphics() ? image_group_payload_get_default_image_id(group_key) : nullptr;
}

const image *building_runtime::resolve_default_group_image(const char *group_key) const
{
    const char *image_id = resolve_default_group_image_id(group_key);
    return image_id ? resolve_named_group_image(group_key, image_id) : nullptr;
}

const char *building_runtime::resolve_type_specific_graphic_image_id(const char *group_key) const
{
    if (!uses_new_graphics() || !group_key || !*group_key) {
        return nullptr;
    }

    switch (building_->type) {
        case BUILDING_THEATER:
        {
            static const char *base_image_ids[] = { "Theatre ON", "Theater ON", "Theatre", "Theater", "ON" };
            static const char *upgrade_image_ids[] = { "Theatre Upgrade ON", "Theater Upgrade ON", "Upgrade ON" };
            return building_->upgrade_level ?
                resolve_candidate_graphic_image_id(group_key, upgrade_image_ids, sizeof(upgrade_image_ids) / sizeof(upgrade_image_ids[0])) :
                resolve_candidate_graphic_image_id(group_key, base_image_ids, sizeof(base_image_ids) / sizeof(base_image_ids[0]));
        }
        case BUILDING_AMPHITHEATER:
        {
            static const char *base_image_ids[] = { "Amphitheatre ON", "Amphitheater ON", "Amphitheatre", "Amphitheater", "ON" };
            static const char *upgrade_image_ids[] = { "Amphitheatre Upgrade ON", "Amphitheater Upgrade ON", "Upgrade ON" };
            return building_->upgrade_level ?
                resolve_candidate_graphic_image_id(group_key, upgrade_image_ids, sizeof(upgrade_image_ids) / sizeof(upgrade_image_ids[0])) :
                resolve_candidate_graphic_image_id(group_key, base_image_ids, sizeof(base_image_ids) / sizeof(base_image_ids[0]));
        }
        case BUILDING_ARENA:
        {
            static const char *base_image_ids[] = { "Arena ON", "Arena", "ON" };
            static const char *upgrade_image_ids[] = { "Arena Upgrade ON", "Upgrade ON" };
            return building_->upgrade_level ?
                resolve_candidate_graphic_image_id(group_key, upgrade_image_ids, sizeof(upgrade_image_ids) / sizeof(upgrade_image_ids[0])) :
                resolve_candidate_graphic_image_id(group_key, base_image_ids, sizeof(base_image_ids) / sizeof(base_image_ids[0]));
        }
        case BUILDING_SCHOOL:
        {
            static const char *upgrade_image_ids[] = { "Upgraded_School", "School Upgrade ON", "Upgrade ON" };
            if (building_->upgrade_level) {
                return resolve_candidate_graphic_image_id(group_key, upgrade_image_ids, sizeof(upgrade_image_ids) / sizeof(upgrade_image_ids[0]));
            }
            return resolve_default_group_image_id(group_key);
        }
        case BUILDING_ACADEMY:
        {
            static const char *base_image_ids[] = { "Academy_Fix", "Academy OFF", "Academy", "OFF" };
            static const char *upgrade_image_ids[] = { "Upgraded_Academy", "Academy Upgrade ON", "Upgrade ON" };
            const char *image_id = building_->upgrade_level ?
                resolve_candidate_graphic_image_id(group_key, upgrade_image_ids, sizeof(upgrade_image_ids) / sizeof(upgrade_image_ids[0])) :
                resolve_candidate_graphic_image_id(group_key, base_image_ids, sizeof(base_image_ids) / sizeof(base_image_ids[0]));
            return image_id ? image_id : resolve_default_group_image_id(group_key);
        }
        case BUILDING_LIBRARY:
        {
            static const char *base_image_ids[] = { "Downgraded_Library", "Library OFF", "Library", "OFF" };
            static const char *upgrade_image_ids[] = { "Library ON", "Upgraded_Library", "ON" };
            const char *image_id = building_->upgrade_level ?
                resolve_candidate_graphic_image_id(group_key, upgrade_image_ids, sizeof(upgrade_image_ids) / sizeof(upgrade_image_ids[0])) :
                resolve_candidate_graphic_image_id(group_key, base_image_ids, sizeof(base_image_ids) / sizeof(base_image_ids[0]));
            return image_id ? image_id : resolve_default_group_image_id(group_key);
        }
        case BUILDING_FORUM:
        {
            static const char *upgrade_image_ids[] = { "Upgraded_Forum", "Forum Upgrade ON", "Upgrade ON" };
            if (building_->upgrade_level) {
                if (const char *image_id = resolve_candidate_graphic_image_id(
                        group_key,
                        upgrade_image_ids,
                        sizeof(upgrade_image_ids) / sizeof(upgrade_image_ids[0]))) {
                    return image_id;
                }
            }
            return resolve_default_group_image_id(group_key);
        }
        case BUILDING_ACTOR_COLONY:
        case BUILDING_GLADIATOR_SCHOOL:
        case BUILDING_DOCTOR:
        case BUILDING_HOSPITAL:
        case BUILDING_BARBER:
        case BUILDING_LARGE_STATUE:
        case BUILDING_WELL:
        case BUILDING_WORKCAMP:
            return resolve_default_group_image_id(group_key);
        default:
            return nullptr;
    }
}

const image *building_runtime::resolve_type_specific_graphic_image(const char *group_key) const
{
    const char *image_id = resolve_type_specific_graphic_image_id(group_key);
    return image_id ? resolve_named_group_image(group_key, image_id) : nullptr;
}

const char *building_runtime::resolve_graphic_image_id(const building_type_registry_impl::GraphicsTarget *target) const
{
    if (!uses_new_graphics() || !target || !target->has_path()) {
        return nullptr;
    }

    const char *group_key = target->path();
    if (target->has_image()) {
        if (const char *image_id = resolve_named_group_image_id(group_key, target->image())) {
            return image_id;
        }
        log_building_graphics_fallback_once(
            "Building graphics image id could not be resolved. Falling back to legacy rendering.",
            this,
            target->image(),
            group_key,
            target->image());
        return nullptr;
    }

    if (!definition_->uses_structured_graphics()) {
        if (const char *image_id = resolve_type_specific_graphic_image_id(group_key)) {
            return image_id;
        }
    }

    return resolve_default_group_image_id(group_key);
}

const image *building_runtime::resolve_graphic_image()
{
    if (!uses_new_graphics()) {
        return nullptr;
    }

    char context[256];
    make_building_context(context, sizeof(context), building_);
    CrashContextScope crash_scope(
        "building_runtime.resolve_base_image",
        context,
        log_building_scope_state,
        this);
    refresh_graphic_state();

    const building_type_registry_impl::GraphicsTarget *target = resolve_graphic_target();
    if (!target || !target->has_path()) {
        log_building_graphics_fallback_once(
            "Building graphics target could not be resolved. Falling back to legacy rendering.",
            this,
            definition_ ? definition_->attr() : "");
        return nullptr;
    }

    const char *image_id = resolve_graphic_image_id(target);
    const image *img = image_id ? resolve_named_group_image(target->path(), image_id) : nullptr;
    if (!img) {
        log_building_graphics_fallback_once(
            "Building graphics image could not be resolved. Falling back to legacy rendering.",
            this,
            target->path(),
            target->path(),
            image_id);
    }
    return img;
}

const image *building_runtime::resolve_graphic_animation_frame()
{
    if (!uses_new_graphics()) {
        return nullptr;
    }

    char context[256];
    make_building_context(context, sizeof(context), building_);
    CrashContextScope crash_scope(
        "building_runtime.resolve_animation_image",
        context,
        log_building_scope_state,
        this);
    refresh_graphic_state();

    const building_type_registry_impl::GraphicsTarget *target = resolve_graphic_target();
    if (!target || !target->has_path()) {
        return nullptr;
    }

    const char *image_id = resolve_graphic_image_id(target);
    if (!image_id) {
        return nullptr;
    }

    const image *img = resolve_named_group_image(target->path(), image_id);
    if (!img || !img->animation) {
        return nullptr;
    }

    int animation_offset = building_animation_offset_for_image(building_, img, building_->grid_offset);
    if (animation_offset <= 0) {
        return nullptr;
    }

    const image *frame = image_group_payload_get_animation_frame(target->path(), image_id, animation_offset);
    if (!frame) {
        log_building_graphics_fallback_once(
            "Building animation frame could not be resolved. Falling back to legacy rendering.",
            this,
            image_id,
            target->path(),
            image_id);
    }
    return frame;
}

void building_runtime::set_building_graphic()
{
    if (!building_ || !definition_ || !definition_->has_graphic() || building_->state != BUILDING_STATE_IN_USE) {
        return;
    }

    refresh_graphic_state();
    map_building_tiles_add(building_->id, building_->x, building_->y, building_->size, building_image_get(building_), TERRAIN_BUILDING);
}

int building_runtime::worker_percentage() const
{
    return calc_percentage(building_->num_workers, model_get_building(building_->type)->laborers);
}

void building_runtime::check_labor_problem()
{
    if (building_->houses_covered <= 0) {
        building_->show_on_problem_overlay = 2;
    }
}

void building_runtime::generate_labor_seeker(int x, int y)
{
    if (city_population() <= 0) {
        return;
    }
    if (config_get(CONFIG_GP_CH_GLOBAL_LABOUR)) {
        if (building_->distance_from_entry) {
            building_->houses_covered = 100;
        } else {
            building_->houses_covered = 0;
        }
        return;
    }
    if (building_->figure_id2) {
        figure *existing = figure_get(building_->figure_id2);
        if (!existing->state || existing->type != FIGURE_LABOR_SEEKER || existing->building_id != building_->id) {
            building_->figure_id2 = 0;
        }
        return;
    }

    figure *labor_seeker = figure_create(FIGURE_LABOR_SEEKER, x, y, DIR_0_TOP);
    labor_seeker->action_state = FIGURE_ACTION_125_ROAMING;
    labor_seeker->building_id = building_->id;
    building_->figure_id2 = labor_seeker->id;
    figure_movement_init_roaming(labor_seeker);
}

void building_runtime::spawn_labor_seeker(int x, int y, int min_houses)
{
    if (config_get(CONFIG_GP_CH_GLOBAL_LABOUR)) {
        if (building_->distance_from_entry) {
            building_->houses_covered = 2 * min_houses;
        } else {
            building_->houses_covered = 0;
        }
    } else if (building_->houses_covered <= min_houses) {
        generate_labor_seeker(x, y);
    }
}

void building_runtime::run_labor_phase(const building_type_registry_impl::LaborPolicy &labor_policy, const map_point &road)
{
    switch (labor_policy.labor_seeker_mode) {
        case building_type_registry_impl::LaborSeekerMode::SpawnIfBelow:
            spawn_labor_seeker(road.x, road.y, labor_policy.labor_min_houses);
            break;
        case building_type_registry_impl::LaborSeekerMode::GenerateIfBelow:
            if (building_->houses_covered <= labor_policy.labor_min_houses) {
                generate_labor_seeker(road.x, road.y);
            }
            break;
        case building_type_registry_impl::LaborSeekerMode::None:
        default:
            break;
    }
}

int building_runtime::has_figure_of_type(figure_type type)
{
    if (building_->figure_id <= 0) {
        return 0;
    }
    figure *existing = figure_get(building_->figure_id);
    if (existing->state && existing->building_id == building_->id && existing->type == type) {
        return 1;
    }
    building_->figure_id = 0;
    return 0;
}

int building_runtime::has_figure_of_any(const std::vector<figure_type> &types)
{
    for (figure_type type : types) {
        if (has_figure_of_type(type)) {
            return 1;
        }
    }
    return 0;
}

int building_runtime::resolve_road_access(building_type_registry_impl::RoadAccessMode mode, map_point *road) const
{
    switch (mode) {
        case building_type_registry_impl::RoadAccessMode::Normal:
            return map_has_road_access(building_->x, building_->y, building_->size, road);
        case building_type_registry_impl::RoadAccessMode::None:
        default:
            return 0;
    }
}

int building_runtime::evaluate_delay(const std::vector<building_type_registry_impl::DelayBand> &delay_bands) const
{
    int pct_workers = worker_percentage();
    for (const building_type_registry_impl::DelayBand &delay_band : delay_bands) {
        if (pct_workers >= delay_band.min_worker_percentage) {
            return delay_band.delay;
        }
    }
    return 0;
}

int building_runtime::evaluate_condition(building_type_registry_impl::SpawnCondition condition) const
{
    switch (condition) {
        case building_type_registry_impl::SpawnCondition::Always:
            return 1;
        case building_type_registry_impl::SpawnCondition::Days1Positive:
            return building_->data.entertainment.days1 > 0;
        case building_type_registry_impl::SpawnCondition::Days1NotPositive:
            return building_->data.entertainment.days1 <= 0;
        case building_type_registry_impl::SpawnCondition::Days2Positive:
            return building_->data.entertainment.days2 > 0;
        case building_type_registry_impl::SpawnCondition::Days1OrDays2Positive:
            return building_->data.entertainment.days1 > 0 || building_->data.entertainment.days2 > 0;
        default:
            return 0;
    }
}

int building_runtime::should_apply_graphic_for_timing(
    const building_type_registry_impl::SpawnDelayGroup &group,
    building_type_registry_impl::GraphicTiming timing) const
{
    for (const building_type_registry_impl::SpawnPolicy &policy : group.policies) {
        if (policy.graphic_timing == timing && evaluate_condition(policy.condition)) {
            return 1;
        }
    }
    return 0;
}

unsigned char building_runtime::get_spawn_delay_counter(size_t policy_index) const
{
    if (policy_index == 0) {
        return building_->figure_spawn_delay;
    }
    if (spawn_delay_counters_.size() <= policy_index) {
        return 0;
    }
    return spawn_delay_counters_[policy_index];
}

void building_runtime::set_spawn_delay_counter(size_t policy_index, unsigned char value)
{
    if (policy_index == 0) {
        building_->figure_spawn_delay = value;
        return;
    }
    if (spawn_delay_counters_.size() <= policy_index) {
        spawn_delay_counters_.resize(policy_index + 1, 0);
    }
    spawn_delay_counters_[policy_index] = value;
}

void building_runtime::assign_figure_slot(building_type_registry_impl::FigureSlot slot, unsigned int figure_id)
{
    switch (slot) {
        case building_type_registry_impl::FigureSlot::Primary:
            building_->figure_id = figure_id;
            break;
        case building_type_registry_impl::FigureSlot::Secondary:
            building_->figure_id2 = figure_id;
            break;
        case building_type_registry_impl::FigureSlot::Quaternary:
            building_->figure_id4 = figure_id;
            break;
        case building_type_registry_impl::FigureSlot::None:
        default:
            break;
    }
}

int building_runtime::create_spawned_figure(const building_type_registry_impl::SpawnPolicy &policy, const map_point &road)
{
    int spawned_any = 0;
    int spawn_count = policy.spawn_count > 0 ? policy.spawn_count : 1;
    for (int i = 0; i < spawn_count; i++) {
        figure *spawned = figure_create(policy.spawn_figure, road.x, road.y, static_cast<direction_type>(policy.spawn_direction));
        if (!spawned) {
            continue;
        }
        spawned->action_state = policy.action_state;
        spawned->building_id = building_->id;
        // A multi-spawn policy still only owns one legacy tracked slot today; later spawns remain untracked for now.
        if (!spawned_any) {
            assign_figure_slot(policy.figure_slot, spawned->id);
        }
        if (policy.init_roaming) {
            figure_movement_init_roaming(spawned);
        }
        spawned_any = 1;
    }
    return spawned_any;
}

int building_runtime::try_spawn_policy(const building_type_registry_impl::SpawnPolicy &policy, const map_point &road)
{
    if (!evaluate_condition(policy.condition)) {
        return 0;
    }

    if (policy.mark_problem_if_no_water && !building_->has_water_access) {
        building_->show_on_problem_overlay = 2;
    }

    if (policy.require_water_access && !building_->has_water_access) {
        return 0;
    }

    if (policy.graphic_timing == building_type_registry_impl::GraphicTiming::BeforeSuccessfulSpawn) {
        set_building_graphic();
    }
    return create_spawned_figure(policy, road);
}

void building_runtime::spawn_service_roamer_group(const building_type_registry_impl::SpawnDelayGroup &group, size_t group_index)
{
    check_labor_problem();
    if (should_apply_graphic_for_timing(group, building_type_registry_impl::GraphicTiming::OnSpawnEntry)) {
        set_building_graphic();
    }

    if (group.guard_timing == building_type_registry_impl::GuardTiming::BeforeRoadAccess &&
        !group.existing_figures.empty() && has_figure_of_any(group.existing_figures)) {
        return;
    }

    map_point road;
    if (!resolve_road_access(group.road_access_mode, &road)) {
        return;
    }

    if (definition_ && definition_->has_labor_policy()) {
        run_labor_phase(definition_->labor_policy(), road);
    }

    if (group.guard_timing == building_type_registry_impl::GuardTiming::AfterLaborSeeker &&
        !group.existing_figures.empty() && has_figure_of_any(group.existing_figures)) {
        return;
    }

    if (should_apply_graphic_for_timing(group, building_type_registry_impl::GraphicTiming::BeforeDelayCheck)) {
        set_building_graphic();
    }

    int spawn_delay = evaluate_delay(group.delay_bands);
    if (!spawn_delay) {
        return;
    }

    unsigned char delay_counter = get_spawn_delay_counter(group_index);
    delay_counter++;
    if (delay_counter <= spawn_delay) {
        set_spawn_delay_counter(group_index, delay_counter);
        return;
    }

    set_spawn_delay_counter(group_index, 0);
    for (const building_type_registry_impl::SpawnPolicy &policy : group.policies) {
        if (policy.mode != building_type_registry_impl::SpawnMode::ServiceRoamer) {
            continue;
        }
        if (try_spawn_policy(policy, road) && policy.block_on_success) {
            break;
        }
    }
}

void building_runtime::spawn_figure()
{
    if (!building_ || !definition_ || building_->state != BUILDING_STATE_IN_USE) {
        return;
    }

    const std::vector<building_type_registry_impl::SpawnDelayGroup> &spawn_groups = definition_->spawn_groups();
    // Groups own the shared delay/guard phase, then policies inside them can either cooperate or block one another.
    for (size_t i = 0; i < spawn_groups.size(); i++) {
        const building_type_registry_impl::SpawnDelayGroup &group = spawn_groups[i];
        if (!group.policies.empty() && group.policies.front().mode == building_type_registry_impl::SpawnMode::ServiceRoamer) {
            spawn_service_roamer_group(group, i);
        }
    }
}

extern "C" void building_runtime_reset(void)
{
    building_runtime_impl::g_runtime_instances.clear();
    g_logged_building_graphics_fallbacks.clear();
}

extern "C" void building_runtime_apply_graphic(building *b)
{
    if (building_runtime *instance = building_runtime_impl::get_or_create_instance(b)) {
        instance->set_building_graphic();
    }
}

extern "C" void building_runtime_spawn_figure(building *b)
{
    if (building_runtime *instance = building_runtime_impl::get_or_create_instance(b)) {
        instance->spawn_figure();
    }
}

extern "C" const image *building_runtime_get_graphic_image(building *b)
{
    if (building_runtime *instance = building_runtime_impl::get_or_create_instance(b)) {
        return instance->resolve_graphic_image();
    }
    return nullptr;
}

extern "C" const image *building_runtime_get_graphic_animation_frame(building *b)
{
    if (building_runtime *instance = building_runtime_impl::get_or_create_instance(b)) {
        return instance->resolve_graphic_animation_frame();
    }
    return nullptr;
}

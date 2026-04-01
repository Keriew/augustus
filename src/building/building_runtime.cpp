#include "building/building_runtime_internal.h"

extern "C" {
#include "building/building_runtime.h"
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
}

namespace building_runtime_impl {

std::vector<std::unique_ptr<BuildingInstance>> g_runtime_instances;

Building *get_city_building(unsigned int id)
{
    if (!id) {
        return nullptr;
    }
    return get_or_create_instance(building_get(id));
}

Building *get_city_building(::building *building_data)
{
    return get_or_create_instance(building_data);
}

void BuildingInstance::set_building_graphic()
{
    if (!building_ || !definition_ || !definition_->has_graphic() || building_->state != BUILDING_STATE_IN_USE) {
        return;
    }

    if (definition_->water_access_mode() == building_type_registry_impl::WaterAccessMode::ReservoirRange) {
        building_->has_water_access =
            map_terrain_exists_tile_in_area_with_type(building_->x, building_->y, building_->size, TERRAIN_RESERVOIR_RANGE);
    }

    building_->upgrade_level = definition_->upgrade_level_for(*building_);
    map_building_tiles_add(building_->id, building_->x, building_->y, building_->size, building_image_get(building_), TERRAIN_BUILDING);
}

int BuildingInstance::worker_percentage() const
{
    return calc_percentage(building_->num_workers, model_get_building(building_->type)->laborers);
}

void BuildingInstance::check_labor_problem()
{
    if (building_->houses_covered <= 0) {
        building_->show_on_problem_overlay = 2;
    }
}

void BuildingInstance::generate_labor_seeker(int x, int y)
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

void BuildingInstance::spawn_labor_seeker(int x, int y, int min_houses)
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

int BuildingInstance::has_figure_of_type(figure_type type)
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

int BuildingInstance::has_figure_of_any(const std::vector<figure_type> &types)
{
    for (figure_type type : types) {
        if (has_figure_of_type(type)) {
            return 1;
        }
    }
    return 0;
}

int BuildingInstance::resolve_road_access(building_type_registry_impl::RoadAccessMode mode, map_point *road) const
{
    switch (mode) {
        case building_type_registry_impl::RoadAccessMode::Normal:
            return map_has_road_access(building_->x, building_->y, building_->size, road);
        case building_type_registry_impl::RoadAccessMode::None:
        default:
            return 0;
    }
}

int BuildingInstance::evaluate_delay(building_type_registry_impl::DelayProfile profile) const
{
    if (profile != building_type_registry_impl::DelayProfile::Default) {
        return 0;
    }

    int pct_workers = worker_percentage();
    if (pct_workers >= 100) {
        return 3;
    } else if (pct_workers >= 75) {
        return 7;
    } else if (pct_workers >= 50) {
        return 15;
    } else if (pct_workers >= 25) {
        return 29;
    } else if (pct_workers >= 1) {
        return 44;
    } else {
        return 0;
    }
}

unsigned char BuildingInstance::get_spawn_delay_counter(size_t policy_index) const
{
    if (policy_index == 0) {
        return building_->figure_spawn_delay;
    }
    if (spawn_delay_counters_.size() <= policy_index) {
        return 0;
    }
    return spawn_delay_counters_[policy_index];
}

void BuildingInstance::set_spawn_delay_counter(size_t policy_index, unsigned char value)
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

void BuildingInstance::assign_figure_slot(building_type_registry_impl::FigureSlot slot, unsigned int figure_id)
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

void BuildingInstance::create_spawned_figure(const building_type_registry_impl::SpawnPolicy &policy, const map_point &road)
{
    figure *spawned = figure_create(policy.spawn_figure, road.x, road.y, static_cast<direction_type>(policy.spawn_direction));
    spawned->action_state = policy.action_state;
    spawned->building_id = building_->id;
    assign_figure_slot(policy.figure_slot, spawned->id);
    if (policy.init_roaming) {
        figure_movement_init_roaming(spawned);
    }
}

void BuildingInstance::spawn_service_roamer(const building_type_registry_impl::SpawnPolicy &policy, size_t policy_index)
{
    check_labor_problem();
    if (policy.guard_timing == building_type_registry_impl::GuardTiming::BeforeRoadAccess &&
        !policy.existing_figures.empty() && has_figure_of_any(policy.existing_figures)) {
        return;
    }

    map_point road;
    if (!resolve_road_access(policy.road_access_mode, &road)) {
        return;
    }

    if (policy.graphic_timing == building_type_registry_impl::GraphicTiming::BeforeDelayCheck) {
        set_building_graphic();
    }

    if (policy.mark_problem_if_no_water && !building_->has_water_access) {
        building_->show_on_problem_overlay = 2;
    }

    if (policy.require_water_access && !building_->has_water_access) {
        return;
    }

    switch (policy.labor_seeker_mode) {
        case building_type_registry_impl::LaborSeekerMode::SpawnIfBelow:
            spawn_labor_seeker(road.x, road.y, policy.labor_min_houses);
            break;
        case building_type_registry_impl::LaborSeekerMode::GenerateIfBelow:
            if (building_->houses_covered <= policy.labor_min_houses) {
                generate_labor_seeker(road.x, road.y);
            }
            break;
        case building_type_registry_impl::LaborSeekerMode::None:
        default:
            break;
    }

    if (policy.guard_timing == building_type_registry_impl::GuardTiming::AfterLaborSeeker &&
        !policy.existing_figures.empty() && has_figure_of_any(policy.existing_figures)) {
        return;
    }

    int spawn_delay = evaluate_delay(policy.delay_profile);
    if (!spawn_delay) {
        return;
    }

    unsigned char delay_counter = get_spawn_delay_counter(policy_index);
    delay_counter++;
    if (delay_counter <= spawn_delay) {
        set_spawn_delay_counter(policy_index, delay_counter);
        return;
    }

    set_spawn_delay_counter(policy_index, 0);
    if (policy.graphic_timing == building_type_registry_impl::GraphicTiming::BeforeSuccessfulSpawn) {
        set_building_graphic();
    }
    create_spawned_figure(policy, road);
}

void BuildingInstance::spawn_figure()
{
    if (!building_ || !definition_ || building_->state != BUILDING_STATE_IN_USE) {
        return;
    }

    const std::vector<building_type_registry_impl::SpawnPolicy> &spawn_policies = definition_->spawn_policies();
    // Multiple policies are supported so the XML shape can grow into multi-spawn buildings later.
    for (size_t i = 0; i < spawn_policies.size(); i++) {
        const building_type_registry_impl::SpawnPolicy &policy = spawn_policies[i];
        if (policy.mode == building_type_registry_impl::SpawnMode::ServiceRoamer) {
            spawn_service_roamer(policy, i);
        }
    }
}

BuildingInstance *get_or_create_instance(::building *building_data)
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

    std::unique_ptr<BuildingInstance> &slot = g_runtime_instances[building_data->id];
    if (!slot || slot->building() != building_data || slot->definition() != definition) {
        slot = std::make_unique<BuildingInstance>(building_data, definition);
    }
    return slot.get();
}

}

extern "C" void building_runtime_reset(void)
{
    building_runtime_impl::g_runtime_instances.clear();
}

extern "C" void building_runtime_apply_graphic(building *b)
{
    if (building_runtime_impl::BuildingInstance *instance = building_runtime_impl::get_or_create_instance(b)) {
        instance->set_building_graphic();
    }
}

extern "C" void building_runtime_spawn_figure(building *b)
{
    if (building_runtime_impl::BuildingInstance *instance = building_runtime_impl::get_or_create_instance(b)) {
        instance->spawn_figure();
    }
}

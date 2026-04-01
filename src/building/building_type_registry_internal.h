#ifndef BUILDING_BUILDING_TYPE_REGISTRY_INTERNAL_H
#define BUILDING_BUILDING_TYPE_REGISTRY_INTERNAL_H

#include "building/building_type_registry.h"

extern "C" {
#include "building/building.h"
#include "core/direction.h"
#include "figure/type.h"
#include "game/mod_manager.h"
#include "map/point.h"
}

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace building_type_registry_impl {

enum class GraphicComparison {
    None,
    GreaterThan,
    GreaterThanOrEqual
};

enum class WaterAccessMode {
    None,
    ReservoirRange
};

enum class SpawnMode {
    None,
    ServiceRoamer
};

enum class RoadAccessMode {
    None,
    Normal
};

enum class LaborSeekerMode {
    None,
    SpawnIfBelow,
    GenerateIfBelow
};

enum class DelayProfile {
    None,
    Default
};

enum class GraphicTiming {
    None,
    BeforeDelayCheck,
    BeforeSuccessfulSpawn
};

enum class FigureSlot {
    None,
    Primary,
    Secondary,
    Quaternary
};

enum class GuardTiming {
    BeforeRoadAccess,
    AfterLaborSeeker
};

struct SpawnPolicy {
    SpawnMode mode = SpawnMode::None;
    RoadAccessMode road_access_mode = RoadAccessMode::None;
    LaborSeekerMode labor_seeker_mode = LaborSeekerMode::None;
    DelayProfile delay_profile = DelayProfile::None;
    GraphicTiming graphic_timing = GraphicTiming::None;
    FigureSlot figure_slot = FigureSlot::Primary;
    GuardTiming guard_timing = GuardTiming::BeforeRoadAccess;
    std::vector<figure_type> existing_figures;
    figure_type spawn_figure = FIGURE_NONE;
    int action_state = 0;
    int spawn_direction = 0;
    int labor_min_houses = 0;
    int init_roaming = 0;
    int require_water_access = 0;
    int mark_problem_if_no_water = 0;
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
        has_graphic_ = 1;
        threshold_ = threshold;
        comparison_ = comparison;
    }

    void set_water_access_mode(WaterAccessMode mode)
    {
        water_access_mode_ = mode;
    }

    void add_spawn_policy(SpawnPolicy policy)
    {
        spawn_policies_.push_back(std::move(policy));
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

    int has_graphic() const
    {
        return has_graphic_;
    }

    const std::vector<SpawnPolicy> &spawn_policies() const
    {
        return spawn_policies_;
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
    int has_graphic_ = 0;
    int threshold_ = 0;
    GraphicComparison comparison_ = GraphicComparison::None;
    WaterAccessMode water_access_mode_ = WaterAccessMode::None;
    std::vector<SpawnPolicy> spawn_policies_;
};

class BuildingInstance {
public:
    BuildingInstance(::building *building, const BuildingType *definition)
        : building_(building)
        , definition_(definition)
    {
    }

    void set_building_graphic();
    void spawn_figure();

    const ::building *building() const
    {
        return building_;
    }

    const BuildingType *definition() const
    {
        return definition_;
    }

private:
    int worker_percentage() const;
    void check_labor_problem();
    void generate_labor_seeker(int x, int y);
    void spawn_labor_seeker(int x, int y, int min_houses);
    int has_figure_of_type(figure_type type);
    int has_figure_of_any(const std::vector<figure_type> &types);
    int resolve_road_access(RoadAccessMode mode, map_point *road) const;
    int evaluate_delay(DelayProfile profile) const;
    unsigned char get_spawn_delay_counter(size_t policy_index) const;
    void set_spawn_delay_counter(size_t policy_index, unsigned char value);
    void assign_figure_slot(FigureSlot slot, unsigned int figure_id);
    void create_spawned_figure(const SpawnPolicy &policy, const map_point &road);
    void spawn_service_roamer(const SpawnPolicy &policy, size_t policy_index);

    ::building *building_;
    const BuildingType *definition_;
    std::vector<unsigned char> spawn_delay_counters_;
};

struct ParseState {
    std::unique_ptr<BuildingType> definition;
    int saw_graphic = 0;
    int saw_spawn = 0;
    int error = 0;
};

extern std::string g_building_type_path;
extern std::array<std::unique_ptr<BuildingType>, BUILDING_TYPE_MAX> g_building_types;
extern std::vector<std::unique_ptr<BuildingInstance>> g_runtime_instances;
extern ParseState g_parse_state;

int directory_exists(const char *path);
void refresh_building_type_path();
BuildingInstance *get_or_create_instance(::building *building_data);

}

#endif // BUILDING_BUILDING_TYPE_REGISTRY_INTERNAL_H

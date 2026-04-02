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

enum class GraphicTiming {
    None,
    OnSpawnEntry,
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

enum class SpawnCondition {
    Always,
    Days1Positive,
    Days1NotPositive,
    Days2Positive,
    Days1OrDays2Positive
};

struct LaborPolicy {
    LaborSeekerMode labor_seeker_mode = LaborSeekerMode::None;
    int labor_min_houses = 0;
};

struct DelayBand {
    int min_worker_percentage = 0;
    int delay = 0;
};

struct SpawnPolicy {
    SpawnMode mode = SpawnMode::None;
    GraphicTiming graphic_timing = GraphicTiming::None;
    FigureSlot figure_slot = FigureSlot::Primary;
    figure_type spawn_figure = FIGURE_NONE;
    int action_state = 0;
    int spawn_direction = 0;
    int spawn_count = 1;
    int init_roaming = 0;
    int require_water_access = 0;
    int mark_problem_if_no_water = 0;
    int block_on_success = 0;
    SpawnCondition condition = SpawnCondition::Always;
};

struct SpawnDelayGroup {
    RoadAccessMode road_access_mode = RoadAccessMode::None;
    std::vector<DelayBand> delay_bands;
    GuardTiming guard_timing = GuardTiming::BeforeRoadAccess;
    std::vector<figure_type> existing_figures;
    std::vector<SpawnPolicy> policies;
};

struct GraphicsDefinition {
    void set_path(std::string path)
    {
        path_ = std::move(path);
    }

    void set_upgrade_rule(int threshold, GraphicComparison comparison)
    {
        has_upgrade_rule_ = 1;
        upgrade_threshold_ = threshold;
        upgrade_comparison_ = comparison;
    }

    void set_water_access_mode(WaterAccessMode mode)
    {
        has_water_access_rule_ = 1;
        water_access_mode_ = mode;
    }

    int has_path() const
    {
        return !path_.empty();
    }

    const char *path() const
    {
        return path_.c_str();
    }

    WaterAccessMode water_access_mode() const
    {
        return has_water_access_rule_ ? water_access_mode_ : WaterAccessMode::None;
    }

    unsigned char upgrade_level_for(const ::building &building) const
    {
        if (!has_upgrade_rule_) {
            return 0;
        }

        switch (upgrade_comparison_) {
            case GraphicComparison::GreaterThan:
                return building.desirability > upgrade_threshold_;
            case GraphicComparison::GreaterThanOrEqual:
                return building.desirability >= upgrade_threshold_;
            case GraphicComparison::None:
            default:
                return 0;
        }
    }

private:
    std::string path_;
    int has_upgrade_rule_ = 0;
    int upgrade_threshold_ = 0;
    GraphicComparison upgrade_comparison_ = GraphicComparison::None;
    int has_water_access_rule_ = 0;
    WaterAccessMode water_access_mode_ = WaterAccessMode::None;
};

class BuildingType {
public:
    BuildingType(building_type type, std::string attr)
        : type_(type)
        , attr_(std::move(attr))
    {
    }

    void set_graphics_path(std::string path)
    {
        graphics_.set_path(std::move(path));
    }

    void set_upgrade_rule(int threshold, GraphicComparison comparison)
    {
        graphics_.set_upgrade_rule(threshold, comparison);
    }

    void set_graphics_water_access_mode(WaterAccessMode mode)
    {
        graphics_.set_water_access_mode(mode);
    }

    void add_spawn_policy(SpawnPolicy policy)
    {
        if (spawn_groups_.empty()) {
            spawn_groups_.emplace_back();
        }
        spawn_groups_.back().policies.push_back(std::move(policy));
    }

    void set_labor_policy(LaborPolicy policy)
    {
        labor_policy_ = policy;
        has_labor_policy_ = 1;
    }

    void add_spawn_group(SpawnDelayGroup group)
    {
        spawn_groups_.push_back(std::move(group));
    }

    SpawnDelayGroup *last_spawn_group()
    {
        if (spawn_groups_.empty()) {
            return nullptr;
        }
        return &spawn_groups_.back();
    }

    building_type type() const
    {
        return type_;
    }

    const char *attr() const
    {
        return attr_.c_str();
    }

    const GraphicsDefinition &graphics() const
    {
        return graphics_;
    }

    const char *graphics_path() const
    {
        return graphics_.path();
    }

    WaterAccessMode water_access_mode() const
    {
        return graphics_.water_access_mode();
    }

    int has_graphic() const
    {
        return graphics_.has_path();
    }

    int has_labor_policy() const
    {
        return has_labor_policy_;
    }

    const LaborPolicy &labor_policy() const
    {
        return labor_policy_;
    }

    const std::vector<SpawnDelayGroup> &spawn_groups() const
    {
        return spawn_groups_;
    }

    unsigned char upgrade_level_for(const ::building &building) const
    {
        return graphics_.upgrade_level_for(building);
    }

private:
    building_type type_;
    std::string attr_;
    GraphicsDefinition graphics_;
    int has_labor_policy_ = 0;
    LaborPolicy labor_policy_;
    std::vector<SpawnDelayGroup> spawn_groups_;
};

struct ParseState {
    std::unique_ptr<BuildingType> definition;
    size_t current_spawn_group_index = 0;
    int has_current_spawn_group = 0;
    int saw_graphic = 0;
    int parsing_graphics = 0;
    int current_graphics_has_path = 0;
    int saw_spawn = 0;
    int error = 0;
};

extern std::string g_building_type_path;
extern std::array<std::unique_ptr<BuildingType>, BUILDING_TYPE_MAX> g_building_types;
extern ParseState g_parse_state;

int directory_exists(const char *path);
void refresh_building_type_path();

}

#endif // BUILDING_BUILDING_TYPE_REGISTRY_INTERNAL_H

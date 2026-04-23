#ifndef BUILDING_BUILDING_TYPE_H
#define BUILDING_BUILDING_TYPE_H

extern "C" {
#include "building/building.h"
#include "core/direction.h"
#include "figure/type.h"
#include "game/resource.h"
#include "game/mod_manager.h"
#include "map/point.h"
}

#include <string>
#include <cstdint>
#include <vector>

namespace building_type_registry_impl {

class ProductionMethod;
class StorageType;

enum class GraphicComparison {
    None,
    LessThan,
    LessThanOrEqual,
    Equal,
    GreaterThan,
    GreaterThanOrEqual
};

enum class WaterAccessType {
    None,
    Well,
    Fountain,
    Reservoir
};

enum class WaterAccessRequirement {
    None,
    ReservoirNetwork,
    WaterSourceAny,
    WaterSourceFreshOnly
};

enum class WaterAccessNodeKind {
    None,
    AqueductConnection
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

enum class GraphicsConditionType {
    None,
    HasWorkers,
    Working,
    WaterAccess,
    FigureSlotOccupied,
    ResourcePositive,
    Desirability
};

struct LaborSeekerPolicy {
    LaborSeekerMode mode = LaborSeekerMode::None;
    int min_houses = 0;
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

struct GraphicsTarget {
    void set_path(std::string path);
    void set_image(std::string image);

    int has_path() const;
    const char *path() const;

    int has_image() const;
    const char *image() const;

private:
    std::string path_;
    std::string image_;
};

struct GraphicsCondition {
    GraphicsConditionType type = GraphicsConditionType::None;
    GraphicComparison comparison = GraphicComparison::None;
    FigureSlot figure_slot = FigureSlot::None;
    int threshold = 0;
    resource_type resource = RESOURCE_NONE;

    int matches(const ::building &building) const;
};

struct WaterAccessNode {
    WaterAccessNodeKind kind = WaterAccessNodeKind::None;
    int x = 0;
    int y = 0;
};

class WaterAccessDefinition {
public:
    void set_type(WaterAccessType type);
    void set_range(int range);
    void set_requirement(WaterAccessRequirement requirement);
    void add_node(WaterAccessNode node);

    int has_provider() const;
    WaterAccessType type() const;
    int range() const;
    WaterAccessRequirement requirement() const;
    const std::vector<WaterAccessNode> &nodes() const;

private:
    int has_type_ = 0;
    int has_range_ = 0;
    WaterAccessType type_ = WaterAccessType::None;
    int range_ = 0;
    WaterAccessRequirement requirement_ = WaterAccessRequirement::None;
    std::vector<WaterAccessNode> nodes_;
};

class LaborDefinition {
public:
    void set_employee_count(int count);
    void set_seeker_policy(LaborSeekerPolicy policy);

    int has_employee_count() const;
    int employee_count() const;
    int has_seeker_policy() const;
    const LaborSeekerPolicy &seeker_policy() const;
    int has_any() const;

private:
    int has_employee_count_ = 0;
    int employee_count_ = 0;
    int has_seeker_policy_ = 0;
    LaborSeekerPolicy seeker_policy_;
};

struct GraphicsVariant {
    GraphicsTarget target;
    std::vector<GraphicsCondition> conditions;

    int matches(const ::building &building) const;
};

class StateDefinition {
public:
    void set_water_access_mode(WaterAccessMode mode);

    WaterAccessMode water_access_mode() const;

private:
    int has_water_access_rule_ = 0;
    WaterAccessMode water_access_mode_ = WaterAccessMode::None;
};

class GraphicsDefinition {
public:
    void mark_default_node();
    GraphicsTarget &default_target();
    const GraphicsTarget &default_target() const;
    GraphicsVariant &add_variant();
    GraphicsVariant *last_variant();
    const GraphicsVariant *last_variant() const;

    int has_path() const;
    int has_default_node() const;
    int has_variants() const;
    const std::vector<GraphicsVariant> &variants() const;
    const GraphicsTarget *resolve_target(const ::building &building) const;
    unsigned char upgrade_level_for(const ::building &building) const;

private:
    GraphicsTarget default_target_;
    std::vector<GraphicsVariant> variants_;
    int has_default_node_ = 0;
};

class BuildingType {
public:
    BuildingType(building_type type, std::string attr);

    void set_state_water_access_mode(WaterAccessMode mode);
    void set_water_access_type(WaterAccessType type);
    void set_water_access_range(int range);
    void set_water_access_requirement(WaterAccessRequirement requirement);
    void add_water_access_node(WaterAccessNode node);
    void mark_graphics_default_node();
    void clear_graphics();
    GraphicsTarget &default_graphics_target();
    GraphicsVariant &add_graphics_variant();
    GraphicsVariant *last_graphics_variant();
    void add_graphics_variant_condition(GraphicsCondition condition);

    void add_spawn_policy(SpawnPolicy policy);
    void set_labor_employee_count(int count);
    void set_labor_seeker_policy(LaborSeekerPolicy policy);
    void add_spawn_group(SpawnDelayGroup group);
    SpawnDelayGroup *last_spawn_group();
    void add_storage_reference(std::string path);
    void add_production_method_reference(std::string path);
    void add_storage_type(const StorageType *storage_type);
    void add_production_method(const ProductionMethod *production_method);

    building_type type() const;
    const char *attr() const;
    const StateDefinition &state() const;
    const WaterAccessDefinition &water_access() const;
    const GraphicsDefinition &graphics() const;
    const GraphicsTarget *resolve_graphics_target(const ::building &building) const;
    WaterAccessMode water_access_mode() const;
    int has_water_access_provider() const;
    int has_graphic() const;
    int has_labor() const;
    const LaborDefinition &labor() const;
    const std::vector<SpawnDelayGroup> &spawn_groups() const;
    const std::vector<std::string> &storage_reference_paths() const;
    const std::vector<std::string> &production_method_reference_paths() const;
    const std::vector<const StorageType *> &storage_types() const;
    const std::vector<const ProductionMethod *> &production_methods() const;
    int has_native_storage() const;
    int has_native_production() const;
    unsigned char upgrade_level_for(const ::building &building) const;

private:
    building_type type_;
    std::string attr_;
    StateDefinition state_;
    WaterAccessDefinition water_access_;
    GraphicsDefinition graphics_;
    LaborDefinition labor_;
    std::vector<SpawnDelayGroup> spawn_groups_;
    std::vector<std::string> storage_reference_paths_;
    std::vector<std::string> production_method_reference_paths_;
    std::vector<const StorageType *> storage_types_;
    std::vector<const ProductionMethod *> production_methods_;
};

} // namespace building_type_registry_impl

#endif // BUILDING_BUILDING_TYPE_H

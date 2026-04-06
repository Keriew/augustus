#ifndef BUILDING_BUILDING_TYPE_H
#define BUILDING_BUILDING_TYPE_H

extern "C" {
#include "building/building.h"
#include "core/direction.h"
#include "figure/type.h"
#include "game/mod_manager.h"
#include "map/point.h"
}

#include <string>
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

enum class GraphicsConditionType {
    None,
    Working,
    WaterAccess,
    Desirability
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
    int threshold = 0;

    int matches(const ::building &building) const;
};

struct GraphicsVariant {
    GraphicsTarget target;
    std::vector<GraphicsCondition> conditions;

    int matches(const ::building &building) const;
};

struct GraphicsOverlay {
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
    void set_default_path(std::string path);
    void set_default_image(std::string image);
    void set_upgrade_rule(int threshold, GraphicComparison comparison);
    void mark_default_node();
    GraphicsVariant &add_variant();
    GraphicsOverlay &add_overlay();
    GraphicsVariant *last_variant();
    const GraphicsVariant *last_variant() const;
    GraphicsOverlay *last_overlay();
    const GraphicsOverlay *last_overlay() const;

    int has_path() const;
    const char *path() const;

    int has_image() const;
    const char *image() const;

    int uses_structured_nodes() const;
    int has_default_node() const;
    int has_variants() const;
    const GraphicsTarget *default_target() const;
    const std::vector<GraphicsVariant> &variants() const;
    const std::vector<GraphicsOverlay> &overlays() const;
    const GraphicsTarget *resolve_target(const ::building &building) const;
    std::vector<const GraphicsTarget *> resolve_overlays(const ::building &building) const;
    unsigned char upgrade_level_for(const ::building &building) const;

private:
    GraphicsTarget default_target_;
    std::vector<GraphicsVariant> variants_;
    std::vector<GraphicsOverlay> overlays_;
    int has_upgrade_rule_ = 0;
    int upgrade_threshold_ = 0;
    GraphicComparison upgrade_comparison_ = GraphicComparison::None;
    int uses_structured_nodes_ = 0;
    int has_default_node_ = 0;
};

class BuildingType {
public:
    BuildingType(building_type type, std::string attr);

    void set_state_water_access_mode(WaterAccessMode mode);
    void set_graphics_path(std::string path);
    void set_graphics_image(std::string image);
    void set_upgrade_rule(int threshold, GraphicComparison comparison);
    void mark_graphics_default_node();
    void clear_graphics();
    GraphicsVariant &add_graphics_variant();
    GraphicsOverlay &add_graphics_overlay();
    GraphicsVariant *last_graphics_variant();
    GraphicsOverlay *last_graphics_overlay();
    void add_graphics_variant_condition(GraphicsCondition condition);
    void add_graphics_overlay_condition(GraphicsCondition condition);

    void add_spawn_policy(SpawnPolicy policy);
    void set_labor_policy(LaborPolicy policy);
    void add_spawn_group(SpawnDelayGroup group);
    SpawnDelayGroup *last_spawn_group();

    building_type type() const;
    const char *attr() const;
    const StateDefinition &state() const;
    const GraphicsDefinition &graphics() const;
    const char *graphics_path() const;
    const char *graphics_image() const;
    const GraphicsTarget *resolve_graphics_target(const ::building &building) const;
    std::vector<const GraphicsTarget *> resolve_graphics_overlays(const ::building &building) const;
    int uses_structured_graphics() const;
    WaterAccessMode water_access_mode() const;
    int has_graphic() const;
    int has_labor_policy() const;
    const LaborPolicy &labor_policy() const;
    const std::vector<SpawnDelayGroup> &spawn_groups() const;
    unsigned char upgrade_level_for(const ::building &building) const;

private:
    building_type type_;
    std::string attr_;
    StateDefinition state_;
    GraphicsDefinition graphics_;
    int has_labor_policy_ = 0;
    LaborPolicy labor_policy_;
    std::vector<SpawnDelayGroup> spawn_groups_;
};

} // namespace building_type_registry_impl

#endif // BUILDING_BUILDING_TYPE_H

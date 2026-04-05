#include "building/building_type.h"

#include <utility>

namespace building_type_registry_impl {

void GraphicsTarget::set_path(std::string path)
{
    path_ = std::move(path);
}

void GraphicsTarget::set_image(std::string image)
{
    image_ = std::move(image);
}

int GraphicsTarget::has_path() const
{
    return !path_.empty();
}

const char *GraphicsTarget::path() const
{
    return path_.c_str();
}

int GraphicsTarget::has_image() const
{
    return !image_.empty();
}

const char *GraphicsTarget::image() const
{
    return image_.c_str();
}

int GraphicsCondition::matches(const ::building &building) const
{
    switch (type) {
        case GraphicsConditionType::Working:
            return building.num_workers > 0 && building.has_water_access;
        case GraphicsConditionType::Desirability:
            switch (comparison) {
                case GraphicComparison::GreaterThan:
                    return building.desirability > threshold;
                case GraphicComparison::GreaterThanOrEqual:
                    return building.desirability >= threshold;
                case GraphicComparison::None:
                default:
                    return 0;
            }
        case GraphicsConditionType::None:
        default:
            return 0;
    }
}

int GraphicsVariant::matches(const ::building &building) const
{
    for (const GraphicsCondition &condition : conditions) {
        if (!condition.matches(building)) {
            return 0;
        }
    }
    return 1;
}

void GraphicsDefinition::set_default_path(std::string path)
{
    default_target_.set_path(std::move(path));
}

void GraphicsDefinition::set_default_image(std::string image)
{
    default_target_.set_image(std::move(image));
}

void GraphicsDefinition::set_upgrade_rule(int threshold, GraphicComparison comparison)
{
    has_upgrade_rule_ = 1;
    upgrade_threshold_ = threshold;
    upgrade_comparison_ = comparison;
}

void GraphicsDefinition::set_water_access_mode(WaterAccessMode mode)
{
    has_water_access_rule_ = 1;
    water_access_mode_ = mode;
}

void GraphicsDefinition::mark_default_node()
{
    uses_structured_nodes_ = 1;
    has_default_node_ = 1;
}

GraphicsVariant &GraphicsDefinition::add_variant()
{
    uses_structured_nodes_ = 1;
    variants_.emplace_back();
    return variants_.back();
}

GraphicsVariant *GraphicsDefinition::last_variant()
{
    return variants_.empty() ? nullptr : &variants_.back();
}

const GraphicsVariant *GraphicsDefinition::last_variant() const
{
    return variants_.empty() ? nullptr : &variants_.back();
}

int GraphicsDefinition::has_path() const
{
    return default_target_.has_path();
}

const char *GraphicsDefinition::path() const
{
    return default_target_.path();
}

int GraphicsDefinition::has_image() const
{
    return default_target_.has_image();
}

const char *GraphicsDefinition::image() const
{
    return default_target_.image();
}

int GraphicsDefinition::uses_structured_nodes() const
{
    return uses_structured_nodes_;
}

int GraphicsDefinition::has_default_node() const
{
    return has_default_node_;
}

int GraphicsDefinition::has_variants() const
{
    return !variants_.empty();
}

const GraphicsTarget *GraphicsDefinition::default_target() const
{
    return default_target_.has_path() ? &default_target_ : nullptr;
}

const std::vector<GraphicsVariant> &GraphicsDefinition::variants() const
{
    return variants_;
}

const GraphicsTarget *GraphicsDefinition::resolve_target(const ::building &building) const
{
    for (const GraphicsVariant &variant : variants_) {
        if (variant.target.has_path() && variant.matches(building)) {
            return &variant.target;
        }
    }
    return default_target_.has_path() ? &default_target_ : nullptr;
}

WaterAccessMode GraphicsDefinition::water_access_mode() const
{
    return has_water_access_rule_ ? water_access_mode_ : WaterAccessMode::None;
}

unsigned char GraphicsDefinition::upgrade_level_for(const ::building &building) const
{
    if (!has_upgrade_rule_) {
        if (uses_structured_nodes_) {
            for (const GraphicsVariant &variant : variants_) {
                if (!variant.target.has_path() || !variant.matches(building)) {
                    continue;
                }

                for (const GraphicsCondition &condition : variant.conditions) {
                    if (condition.type == GraphicsConditionType::Desirability) {
                        return 1;
                    }
                }
                break;
            }
        }
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

BuildingType::BuildingType(building_type type, std::string attr)
    : type_(type)
    , attr_(std::move(attr))
{
}

void BuildingType::set_graphics_path(std::string path)
{
    graphics_.set_default_path(std::move(path));
}

void BuildingType::set_graphics_image(std::string image)
{
    graphics_.set_default_image(std::move(image));
}

void BuildingType::set_upgrade_rule(int threshold, GraphicComparison comparison)
{
    graphics_.set_upgrade_rule(threshold, comparison);
}

void BuildingType::set_graphics_water_access_mode(WaterAccessMode mode)
{
    graphics_.set_water_access_mode(mode);
}

void BuildingType::mark_graphics_default_node()
{
    graphics_.mark_default_node();
}

GraphicsVariant &BuildingType::add_graphics_variant()
{
    return graphics_.add_variant();
}

GraphicsVariant *BuildingType::last_graphics_variant()
{
    return graphics_.last_variant();
}

void BuildingType::add_graphics_condition(GraphicsCondition condition)
{
    GraphicsVariant *variant = graphics_.last_variant();
    if (variant) {
        variant->conditions.push_back(std::move(condition));
    }
}

void BuildingType::add_spawn_policy(SpawnPolicy policy)
{
    if (spawn_groups_.empty()) {
        spawn_groups_.emplace_back();
    }
    spawn_groups_.back().policies.push_back(std::move(policy));
}

void BuildingType::set_labor_policy(LaborPolicy policy)
{
    labor_policy_ = policy;
    has_labor_policy_ = 1;
}

void BuildingType::add_spawn_group(SpawnDelayGroup group)
{
    spawn_groups_.push_back(std::move(group));
}

SpawnDelayGroup *BuildingType::last_spawn_group()
{
    return spawn_groups_.empty() ? nullptr : &spawn_groups_.back();
}

building_type BuildingType::type() const
{
    return type_;
}

const char *BuildingType::attr() const
{
    return attr_.c_str();
}

const GraphicsDefinition &BuildingType::graphics() const
{
    return graphics_;
}

const char *BuildingType::graphics_path() const
{
    return graphics_.path();
}

const char *BuildingType::graphics_image() const
{
    return graphics_.image();
}

const GraphicsTarget *BuildingType::resolve_graphics_target(const ::building &building) const
{
    return graphics_.resolve_target(building);
}

int BuildingType::uses_structured_graphics() const
{
    return graphics_.uses_structured_nodes();
}

WaterAccessMode BuildingType::water_access_mode() const
{
    return graphics_.water_access_mode();
}

int BuildingType::has_graphic() const
{
    return graphics_.has_path();
}

int BuildingType::has_labor_policy() const
{
    return has_labor_policy_;
}

const LaborPolicy &BuildingType::labor_policy() const
{
    return labor_policy_;
}

const std::vector<SpawnDelayGroup> &BuildingType::spawn_groups() const
{
    return spawn_groups_;
}

unsigned char BuildingType::upgrade_level_for(const ::building &building) const
{
    return graphics_.upgrade_level_for(building);
}

} // namespace building_type_registry_impl

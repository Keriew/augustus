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
        case GraphicsConditionType::HasWorkers:
            return building.num_workers > 0;
        case GraphicsConditionType::Working:
            return building.num_workers > 0 && building.has_water_access;
        case GraphicsConditionType::WaterAccess:
            return building.has_water_access;
        case GraphicsConditionType::FigureSlotOccupied:
            switch (figure_slot) {
                case FigureSlot::Primary:
                    return building.figure_id > 0;
                case FigureSlot::Secondary:
                    return building.figure_id2 > 0;
                case FigureSlot::Quaternary:
                    return building.figure_id4 > 0;
                case FigureSlot::None:
                default:
                    return 0;
            }
        case GraphicsConditionType::ResourcePositive:
            return resource > RESOURCE_NONE && resource < RESOURCE_MAX && building.resources[resource] > 0;
        case GraphicsConditionType::Desirability:
            switch (comparison) {
                case GraphicComparison::LessThan:
                    return building.desirability < threshold;
                case GraphicComparison::LessThanOrEqual:
                    return building.desirability <= threshold;
                case GraphicComparison::Equal:
                    return building.desirability == threshold;
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

void LaborDefinition::set_employee_count(int count)
{
    has_employee_count_ = 1;
    employee_count_ = count;
}

void LaborDefinition::set_seeker_policy(LaborSeekerPolicy policy)
{
    has_seeker_policy_ = 1;
    seeker_policy_ = policy;
}

int LaborDefinition::has_employee_count() const
{
    return has_employee_count_;
}

int LaborDefinition::employee_count() const
{
    return employee_count_;
}

int LaborDefinition::has_seeker_policy() const
{
    return has_seeker_policy_;
}

const LaborSeekerPolicy &LaborDefinition::seeker_policy() const
{
    return seeker_policy_;
}

int LaborDefinition::has_any() const
{
    return has_employee_count_ || has_seeker_policy_;
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

void StateDefinition::set_water_access_mode(WaterAccessMode mode)
{
    has_water_access_rule_ = 1;
    water_access_mode_ = mode;
}

WaterAccessMode StateDefinition::water_access_mode() const
{
    return has_water_access_rule_ ? water_access_mode_ : WaterAccessMode::None;
}

GraphicsTarget &GraphicsDefinition::default_target()
{
    return default_target_;
}

const GraphicsTarget &GraphicsDefinition::default_target() const
{
    return default_target_;
}

void GraphicsDefinition::mark_default_node()
{
    has_default_node_ = 1;
}

GraphicsVariant &GraphicsDefinition::add_variant()
{
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

int GraphicsDefinition::has_default_node() const
{
    return has_default_node_;
}

int GraphicsDefinition::has_variants() const
{
    return !variants_.empty();
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
    if (default_target_.has_path()) {
        return &default_target_;
    }
    return nullptr;
}

unsigned char GraphicsDefinition::upgrade_level_for(const ::building &building) const
{
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
    return 0;
}

BuildingType::BuildingType(building_type type, std::string attr)
    : type_(type)
    , attr_(std::move(attr))
{
}

void BuildingType::set_state_water_access_mode(WaterAccessMode mode)
{
    state_.set_water_access_mode(mode);
}

void BuildingType::mark_graphics_default_node()
{
    graphics_.mark_default_node();
}

void BuildingType::clear_graphics()
{
    graphics_ = GraphicsDefinition();
}

GraphicsTarget &BuildingType::default_graphics_target()
{
    return graphics_.default_target();
}

GraphicsVariant &BuildingType::add_graphics_variant()
{
    return graphics_.add_variant();
}

GraphicsVariant *BuildingType::last_graphics_variant()
{
    return graphics_.last_variant();
}

void BuildingType::add_graphics_variant_condition(GraphicsCondition condition)
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

void BuildingType::set_labor_employee_count(int count)
{
    labor_.set_employee_count(count);
}

void BuildingType::set_labor_seeker_policy(LaborSeekerPolicy policy)
{
    labor_.set_seeker_policy(policy);
}

void BuildingType::add_spawn_group(SpawnDelayGroup group)
{
    spawn_groups_.push_back(std::move(group));
}

void BuildingType::add_storage_reference(std::string path)
{
    storage_reference_paths_.push_back(std::move(path));
}

void BuildingType::add_production_method_reference(std::string path)
{
    production_method_reference_paths_.push_back(std::move(path));
}

void BuildingType::add_storage_type(const StorageType *storage_type)
{
    storage_types_.push_back(storage_type);
}

void BuildingType::add_production_method(const ProductionMethod *production_method)
{
    production_methods_.push_back(production_method);
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

const StateDefinition &BuildingType::state() const
{
    return state_;
}

const GraphicsDefinition &BuildingType::graphics() const
{
    return graphics_;
}

const GraphicsTarget *BuildingType::resolve_graphics_target(const ::building &building) const
{
    return graphics_.resolve_target(building);
}

WaterAccessMode BuildingType::water_access_mode() const
{
    return state_.water_access_mode();
}

int BuildingType::has_graphic() const
{
    return graphics_.has_path();
}

int BuildingType::has_labor() const
{
    return labor_.has_any();
}

const LaborDefinition &BuildingType::labor() const
{
    return labor_;
}

const std::vector<SpawnDelayGroup> &BuildingType::spawn_groups() const
{
    return spawn_groups_;
}

const std::vector<std::string> &BuildingType::storage_reference_paths() const
{
    return storage_reference_paths_;
}

const std::vector<std::string> &BuildingType::production_method_reference_paths() const
{
    return production_method_reference_paths_;
}

const std::vector<const StorageType *> &BuildingType::storage_types() const
{
    return storage_types_;
}

const std::vector<const ProductionMethod *> &BuildingType::production_methods() const
{
    return production_methods_;
}

int BuildingType::has_native_storage() const
{
    return !storage_types_.empty();
}

int BuildingType::has_native_production() const
{
    return !production_methods_.empty();
}

unsigned char BuildingType::upgrade_level_for(const ::building &building) const
{
    return graphics_.upgrade_level_for(building);
}

} // namespace building_type_registry_impl

#include "building/production_method.h"

#include <utility>

namespace building_type_registry_impl {

ProductionMethod::ProductionMethod(std::string path)
    : path_(std::move(path))
{
}

const char *ProductionMethod::path() const
{
    return path_.c_str();
}

void ProductionMethod::set_kind(ProductionMethodKind kind)
{
    kind_ = kind;
}

ProductionMethodKind ProductionMethod::kind() const
{
    return kind_;
}

void ProductionMethod::set_output_resource(resource_type resource)
{
    output_resource_ = resource;
}

resource_type ProductionMethod::output_resource() const
{
    return output_resource_;
}

void ProductionMethod::set_batch_size(int batch_size)
{
    batch_size_ = batch_size;
}

int ProductionMethod::batch_size() const
{
    return batch_size_;
}

void ProductionMethod::add_input(ProductionResourceAmount input)
{
    inputs_.push_back(input);
}

const std::vector<ProductionResourceAmount> &ProductionMethod::inputs() const
{
    return inputs_;
}

int ProductionMethod::add_climate_bonus(ClimateProductionBonus bonus)
{
    for (const ClimateProductionBonus &existing : climate_bonuses_) {
        if (existing.climate == bonus.climate) {
            return 0;
        }
    }

    climate_bonuses_.push_back(bonus);
    return 1;
}

const std::vector<ClimateProductionBonus> &ProductionMethod::climate_bonuses() const
{
    return climate_bonuses_;
}

int ProductionMethod::climate_bonus_percent(scenario_climate climate) const
{
    for (const ClimateProductionBonus &bonus : climate_bonuses_) {
        if (bonus.climate == climate) {
            return bonus.percent_delta;
        }
    }
    return 0;
}

int ProductionMethod::is_farm() const
{
    return kind_ == ProductionMethodKind::Farm;
}

int ProductionMethod::is_workshop() const
{
    return kind_ == ProductionMethodKind::Workshop;
}

int ProductionMethod::refreshes_farm_image() const
{
    return is_farm();
}

int ProductionMethod::uses_blessing_multiplier() const
{
    return is_farm();
}

} // namespace building_type_registry_impl

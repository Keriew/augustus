#include "building/production_runtime.h"

#include "building/building_runtime_internal.h"

extern "C" {
#include "building/building.h"
#include "building/image.h"
#include "building/industry.h"
#include "building/monument.h"
#include "building/properties.h"
#include "city/data_private.h"
#include "core/calc.h"
#include "figure/figure.h"
#include "game/resource.h"
#include "game/time.h"
#include "map/building_tiles.h"
#include "scenario/property.h"
}

namespace {

constexpr int kRecordProductionMonths = 12;
constexpr int kMercuryBlessingLoads = 3;

int is_valid_resource_slot(resource_type resource)
{
    return resource > RESOURCE_NONE && resource < RESOURCE_MAX;
}

int get_resource_slot_index(resource_type resource)
{
    if (!is_valid_resource_slot(resource)) {
        return -1;
    }
    return static_cast<int>(resource);
}

void update_farm_image(const building *b)
{
    map_building_tiles_add_farm(
        b->id,
        b->x,
        b->y,
        building_image_get_base_farm_crop(b->type),
        calc_percentage(b->data.industry.progress, building_industry_get_max_progress(b)));
}

} // namespace

int Production::decrement_strike_if_needed(int new_day, int *out_is_striking)
{
    if (out_is_striking) {
        *out_is_striking = 0;
    }

    if (building_->strike_duration_days <= 0) {
        return 0;
    }

    if (out_is_striking) {
        *out_is_striking = 1;
    }

    if (new_day) {
        building_->strike_duration_days--;
        if (city_data.sentiment.value > 50) {
            building_->strike_duration_days -= 3;
        }
        if (city_data.sentiment.value > 65) {
            building_->strike_duration_days = 0;
        }
    }

    if (building_->strike_duration_days == 0) {
        city_data.building.num_striking_industries--;
        figure_delete(figure_get(building_->figure_id4));
        if (out_is_striking) {
            *out_is_striking = 0;
        }
    }
    return 1;
}

void Production::refresh_images() const
{
    if (method_ && method_->refreshes_farm_image()) {
        update_farm_image(building_);
    }
}

int Production::has_raw_materials() const
{
    if (!method_) {
        return 0;
    }

    for (const building_type_registry_impl::ProductionResourceAmount &input : method_->inputs()) {
        if (!is_valid_resource_slot(input.resource)) {
            return 0;
        }
        if (building_->resources[input.resource] < scaled_input_amount(input)) {
            return 0;
        }
    }
    return 1;
}

int Production::effective_monthly_production() const
{
    if (!method_ || method_->output_resource() == RESOURCE_NONE) {
        return 0;
    }

    int monthly_production = resource_base_production_per_month(method_->output_resource());
    monthly_production += (monthly_production * method_->climate_bonus_percent(scenario_property_climate())) / 100;
    return monthly_production;
}

int Production::max_progress() const
{
    if (!building_ || !method_ || method_->output_resource() == RESOURCE_NONE) {
        return 0;
    }

    const int monthly_production = effective_monthly_production();
    if (monthly_production <= 0) {
        return 0;
    }

    const int base_max_progress =
        calc_percentage(GAME_TIME_DAYS_PER_MONTH * 2 * model_get_building(building_->type)->laborers, monthly_production);
    return base_max_progress * output_cart_loads();
}

int Production::efficiency() const
{
    if (building_->state == BUILDING_STATE_MOTHBALLED) {
        return -1;
    }
    if (building_->data.industry.age_months == 0 || method_->output_resource() == RESOURCE_NONE) {
        return -1;
    }

    const int production_for_resource = effective_monthly_production();
    if (production_for_resource <= 0) {
        return -1;
    }
    const int percentage = calc_percentage(building_->data.industry.average_production_per_month, production_for_resource);
    return calc_bound(percentage, 0, 100);
}

int Production::can_start_cycle() const
{
    if (!method_) {
        return 0;
    }
    if (building_->houses_covered <= 0 || building_->num_workers <= 0 || building_->strike_duration_days > 0) {
        return 0;
    }
    if (max_progress() <= 0) {
        return 0;
    }
    if (!has_raw_materials()) {
        return 0;
    }
    if (!resource_is_storable(method_->output_resource()) && building_->data.industry.progress == 0 &&
        !building_has_workshop_for_raw_material_with_room(method_->output_resource(), building_->road_network_id) &&
        !building_monument_get_monument(building_->x, building_->y, method_->output_resource(), building_->road_network_id, 0)) {
        return 0;
    }
    return 1;
}

int Production::update_daily(int new_day, int *out_is_striking)
{
    if (!building_ || !method_) {
        return 0;
    }

    decrement_strike_if_needed(new_day, out_is_striking);

    building_->data.industry.has_raw_materials = 0;
    if (!can_start_cycle()) {
        return 1;
    }

    if (building_->data.industry.curse_days_left) {
        if (new_day) {
            building_->data.industry.curse_days_left--;
        }
        return 1;
    }
    if (building_->data.industry.blessing_days_left && new_day) {
        building_->data.industry.blessing_days_left--;
    }

    int progress = building_->num_workers;
    if (building_->data.industry.blessing_days_left && method_->uses_blessing_multiplier()) {
        progress += building_->num_workers;
    }

    building_->data.industry.has_raw_materials = 1;
    building_->data.industry.progress += progress;

    const int max_value = max_progress();
    if (building_->data.industry.progress > max_value) {
        building_->data.industry.progress = max_value;
    }

    refresh_images();
    return 1;
}

int Production::has_produced_resource() const
{
    const int max_value = max_progress();
    return building_ && method_ && max_value > 0 && building_->data.industry.progress >= max_value;
}

int Production::output_cart_loads() const
{
    return method_ ? method_->batch_size() : 0;
}

int Production::scaled_input_amount(const building_type_registry_impl::ProductionResourceAmount &input) const
{
    return method_ ? input.amount * method_->batch_size() : input.amount;
}

int Production::pending_production_for_stats() const
{
    const int max_value = max_progress();
    if (max_value <= 0) {
        return 0;
    }

    int pending_production_percentage = calc_percentage(building_->data.industry.progress, max_value);
    pending_production_percentage = calc_bound(pending_production_percentage, 0, 100);
    return pending_production_percentage * output_cart_loads();
}

void Production::start_new_production()
{
    if (!building_ || !method_) {
        return;
    }

    const int max_value = max_progress();
    if (max_value > 0 && building_->data.industry.progress >= max_value) {
        building_->data.industry.production_current_month += RESOURCE_ONE_LOAD * output_cart_loads();
        building_->data.industry.progress = 0;
    }

    const int raw_materials_available = has_raw_materials();
    if (raw_materials_available) {
        for (const building_type_registry_impl::ProductionResourceAmount &input : method_->inputs()) {
            if (is_valid_resource_slot(input.resource)) {
                building_->resources[input.resource] -= scaled_input_amount(input);
            }
        }
    }
    building_->data.industry.has_raw_materials = raw_materials_available;
    refresh_images();
}

void Production::advance_stats()
{
    if (!building_ || !method_) {
        return;
    }
    if (building_->state != BUILDING_STATE_IN_USE && building_->state != BUILDING_STATE_MOTHBALLED) {
        return;
    }

    if (building_->data.industry.age_months < kRecordProductionMonths) {
        building_->data.industry.age_months++;
    }

    int sum_months = building_->data.industry.average_production_per_month * (building_->data.industry.age_months - 1);
    const int pending_production_percentage = pending_production_for_stats();
    sum_months += building_->data.industry.production_current_month + pending_production_percentage;
    building_->data.industry.average_production_per_month = sum_months / building_->data.industry.age_months;
    const int leftover_from_average = sum_months % building_->data.industry.age_months;
    building_->data.industry.production_current_month = leftover_from_average - pending_production_percentage;
}

void Production::bless_farm()
{
    if (!building_ || !method_ || !method_->is_farm()) {
        return;
    }

    building_->data.industry.progress = max_progress();
    building_->data.industry.curse_days_left = 0;
    building_->data.industry.blessing_days_left = 16;
    refresh_images();
}

void Production::curse_farm(int big_curse)
{
    if (!building_ || !method_ || !method_->is_farm()) {
        return;
    }

    building_->data.industry.progress = 0;
    building_->data.industry.blessing_days_left = 0;
    building_->data.industry.curse_days_left = big_curse ? 48 : 4;
    refresh_images();
}

void Production::bless_industry()
{
    if (!building_ || !method_ || !method_->is_workshop()) {
        return;
    }
    if (building_->state != BUILDING_STATE_IN_USE || building_->output_resource_id != method_->output_resource()) {
        return;
    }
    if (building_->num_workers <= 0) {
        return;
    }

    for (const building_type_registry_impl::ProductionResourceAmount &input : method_->inputs()) {
        const int resource_slot_index = get_resource_slot_index(input.resource);
        if (resource_slot_index < 0) {
            continue;
        }
        short &resource_slot = building_->resources[resource_slot_index];
        if (resource_slot > 0 &&
            resource_slot < kMercuryBlessingLoads * scaled_input_amount(input)) {
            resource_slot = static_cast<short>(kMercuryBlessingLoads * scaled_input_amount(input));
        }
    }
    if (building_->data.industry.progress) {
        building_->data.industry.progress = max_progress();
    }
}

namespace production_runtime_impl {

std::vector<std::vector<std::unique_ptr<Production>>> g_city_productions;

void reset()
{
    g_city_productions.clear();
}

Production *get_or_create(::building *building, size_t method_index)
{
    if (!building || !building->id) {
        return nullptr;
    }

    building_runtime *runtime = building_runtime_impl::get_or_create_instance(building);
    if (!runtime || !runtime->definition()) {
        return nullptr;
    }

    const std::vector<const building_type_registry_impl::ProductionMethod *> &methods =
        runtime->definition()->production_methods();
    if (method_index >= methods.size()) {
        return nullptr;
    }

    if (g_city_productions.size() <= building->id) {
        g_city_productions.resize(building->id + 1);
    }

    std::vector<std::unique_ptr<Production>> &productions = g_city_productions[building->id];
    if (productions.size() < methods.size()) {
        productions.resize(methods.size());
    }

    std::unique_ptr<Production> &slot = productions[method_index];
    if (!slot || slot->building() != building || slot->method() != methods[method_index]) {
        slot = std::make_unique<Production>(building, methods[method_index], method_index);
    }
    return slot.get();
}

Production *get_or_create_primary(::building *building)
{
    return get_or_create(building, 0);
}

size_t get_method_count(::building *building)
{
    if (!building || !building->id) {
        return 0;
    }

    building_runtime *runtime = building_runtime_impl::get_or_create_instance(building);
    if (!runtime || !runtime->definition()) {
        return 0;
    }
    return runtime->definition()->production_methods().size();
}

void initialize_city()
{
    reset();

    const int total_buildings = building_count();
    for (int id = 1; id < total_buildings; id++) {
        ::building *building = building_get(id);
        if (!building || !building->id) {
            continue;
        }

        const size_t method_count = get_method_count(building);
        for (size_t i = 0; i < method_count; i++) {
            get_or_create(building, i);
        }
    }
}

} // namespace production_runtime_impl

extern "C" void production_runtime_reset(void)
{
    production_runtime_impl::reset();
}

extern "C" void production_runtime_initialize_city(void)
{
    production_runtime_impl::initialize_city();
}

extern "C" int production_runtime_has_native_production(building *b)
{
    return production_runtime_impl::get_or_create_primary(b) ? 1 : 0;
}

extern "C" int production_runtime_get_method_count(building *b)
{
    return static_cast<int>(production_runtime_impl::get_method_count(b));
}

extern "C" int production_runtime_building_has_raw_materials(building *b)
{
    Production *production = production_runtime_impl::get_or_create_primary(b);
    return production ? production->has_raw_materials() : 0;
}

extern "C" int production_runtime_get_max_progress(building *b)
{
    Production *production = production_runtime_impl::get_or_create_primary(b);
    return production ? production->max_progress() : 0;
}

extern "C" int production_runtime_get_efficiency(building *b)
{
    Production *production = production_runtime_impl::get_or_create_primary(b);
    return production ? production->efficiency() : -1;
}

extern "C" int production_runtime_update_building(building *b, int new_day, int *out_is_striking)
{
    Production *production = production_runtime_impl::get_or_create_primary(b);
    return production ? production->update_daily(new_day, out_is_striking) : 0;
}

extern "C" int production_runtime_has_produced_resource(building *b)
{
    Production *production = production_runtime_impl::get_or_create_primary(b);
    return production ? production->has_produced_resource() : 0;
}

extern "C" int production_runtime_get_output_cart_loads(building *b)
{
    Production *production = production_runtime_impl::get_or_create_primary(b);
    return production ? production->output_cart_loads() : 0;
}

extern "C" int production_runtime_start_new_production(building *b)
{
    Production *production = production_runtime_impl::get_or_create_primary(b);
    if (!production) {
        return 0;
    }

    production->start_new_production();
    return 1;
}

extern "C" void production_runtime_advance_stats(building *b)
{
    if (Production *production = production_runtime_impl::get_or_create_primary(b)) {
        production->advance_stats();
    }
}

extern "C" void production_runtime_bless_farm(building *b)
{
    if (Production *production = production_runtime_impl::get_or_create_primary(b)) {
        production->bless_farm();
    }
}

extern "C" void production_runtime_curse_farm(building *b, int big_curse)
{
    if (Production *production = production_runtime_impl::get_or_create_primary(b)) {
        production->curse_farm(big_curse);
    }
}

extern "C" void production_runtime_bless_industry(building *b)
{
    if (Production *production = production_runtime_impl::get_or_create_primary(b)) {
        production->bless_industry();
    }
}

#ifndef BUILDING_PRODUCTION_RUNTIME_H
#define BUILDING_PRODUCTION_RUNTIME_H

#include "building/production_method.h"

extern "C" {
#include "building/building.h"
}

#include <cstddef>
#include <memory>
#include <vector>

class Production {
public:
    Production(::building *building, const building_type_registry_impl::ProductionMethod *method, size_t method_index)
        : building_(building)
        , method_(method)
        , method_index_(method_index)
    {
    }

    ::building *building() const
    {
        return building_;
    }

    const building_type_registry_impl::ProductionMethod *method() const
    {
        return method_;
    }

    size_t method_index() const
    {
        return method_index_;
    }

    int update_daily(int new_day, int *out_is_striking);
    int has_raw_materials() const;
    int max_progress() const;
    int efficiency() const;
    int has_produced_resource() const;
    void start_new_production();
    void advance_stats();
    void bless_farm();
    void curse_farm(int big_curse);
    void bless_industry();

private:
    int can_start_cycle() const;
    int decrement_strike_if_needed(int new_day, int *out_is_striking);
    void refresh_images() const;

    ::building *building_ = nullptr;
    const building_type_registry_impl::ProductionMethod *method_ = nullptr;
    size_t method_index_ = 0;
};

namespace production_runtime_impl {

extern std::vector<std::vector<std::unique_ptr<Production>>> g_city_productions;

void reset();
void initialize_city();
Production *get_or_create(::building *building, size_t method_index);
Production *get_or_create_primary(::building *building);
size_t get_method_count(::building *building);

}

#endif // BUILDING_PRODUCTION_RUNTIME_H

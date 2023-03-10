#include "conditiontype_population_unemployed.h"

#include "city/labor.h"
#include "scenario/conditiontype/comparison_helper.h"

int scenario_condition_population_unemployed_met(scenario_condition_t *condition)
{
    int32_t use_percentage = condition->parameter1;
    int type = condition->parameter2;
    int32_t value = condition->parameter3;

    int unemployed_total = city_labor_workers_unemployed();
    if (use_percentage) {
        unemployed_total = city_labor_unemployment_percentage();
    }

    return comparison_helper_compare_values(type, unemployed_total, (int)value);
}

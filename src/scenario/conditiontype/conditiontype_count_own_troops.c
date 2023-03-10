#include "conditiontype_count_own_troops.h"

#include "city/military.h"
#include "scenario/conditiontype/comparison_helper.h"

int scenario_condition_count_own_troops_met(scenario_condition_t *condition)
{
    int check = condition->parameter1;
    int32_t value = condition->parameter2;
    int32_t in_city_only = condition->parameter3;

    int soldier_count = city_military_total_soldiers();
    if (in_city_only) {
        soldier_count = city_military_total_soldiers_in_city();
    }

    return scenario_event_condition_compare_values(check, soldier_count, (int)value);
}

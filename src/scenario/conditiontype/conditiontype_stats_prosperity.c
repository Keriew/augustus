#include "conditiontype_stats_prosperity.h"

#include "city/ratings.h"
#include "scenario/conditiontype/comparison_helper.h"

int scenario_condition_stats_prosperity_met(scenario_condition_t *condition)
{
    int stat_value = city_rating_prosperity();
    int type = condition->parameter1;
    int32_t value = condition->parameter2;

    return scenario_event_condition_compare_values(type, stat_value, (int)value);
}

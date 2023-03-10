#include "conditiontype_stats_peace.h"

#include "city/ratings.h"
#include "scenario/conditiontype/comparison_helper.h"

int scenario_condition_stats_peace_met(scenario_condition_t *condition)
{
    int stat_value = city_rating_peace();
    int type = condition->parameter1;
    int32_t value = condition->parameter2;

    return comparison_helper_compare_values(type, stat_value, (int)value);
}

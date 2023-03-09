#include "conditiontype_stats_city_health.h"

#include "city/health.h"
#include "scenario/conditiontype/comparison_helper.h"

void scenario_condition_stats_city_health_init(scenario_condition_t *condition)
{
    // Nothing to init.
}

int scenario_condition_stats_city_health_met(scenario_condition_t *condition)
{
    int stat_value = city_health();
    int type = condition->parameter1;
    int32_t value = condition->parameter2;

    return scenario_event_condition_compare_values(type, stat_value, (int)value);
}

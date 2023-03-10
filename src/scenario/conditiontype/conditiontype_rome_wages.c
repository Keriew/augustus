#include "conditiontype_rome_wages.h"

#include "city/labor.h"
#include "scenario/conditiontype/comparison_helper.h"

int scenario_condition_rome_wages_met(scenario_condition_t *condition)
{
    int wages = city_labor_wages_rome();
    int type = condition->parameter1;
    int32_t value = condition->parameter2;

    return comparison_helper_compare_values(type, wages, (int)value);
}

#include "conditiontype_savings.h"

#include "city/emperor.h"
#include "scenario/conditiontype/comparison_helper.h"

int scenario_condition_savings_met(scenario_condition_t *condition)
{
    int funds = city_emperor_personal_savings();
    int type = condition->parameter1;
    int32_t value = condition->parameter2;

    return scenario_event_condition_compare_values(type, funds, (int)value);
}

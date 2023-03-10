#include "conditiontype_difficulty.h"

#include "game/settings.h"
#include "scenario/conditiontype/comparison_helper.h"

int scenario_condition_difficulty_met(scenario_condition_t *condition)
{
    int difficulty = setting_difficulty();
    int type = condition->parameter1;
    int32_t value = condition->parameter2;

    return comparison_helper_compare_values(type, (int)difficulty, (int)value);
}

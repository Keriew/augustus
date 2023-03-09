#include "conditiontype_time.h"

#include "core/random.h"
#include "game/time.h"
#include "scenario/conditiontype/comparison_helper.h"

void scenario_condition_time_init(scenario_condition_t *condition)
{
    int min_months = condition->parameter2;
    int max_months = condition->parameter3;
    if (max_months < min_months) {
        max_months = min_months;
        condition->parameter3 = max_months;
    }

    condition->parameter4 = random_between_from_stdlib(min_months, max_months);
}

int scenario_condition_time_met(scenario_condition_t *condition)
{
    int total_months = game_time_total_months();
    int type = condition->parameter1;
    int32_t min_months = condition->parameter2;
    int32_t max_months = condition->parameter3;
    int32_t target_months = condition->parameter4;

    return scenario_event_condition_compare_values(type, total_months, (int)target_months);
}

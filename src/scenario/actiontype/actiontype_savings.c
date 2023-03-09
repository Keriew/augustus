#include "actiontype_savings.h"

#include "city/emperor.h"
#include "core/random.h"

void scenario_event_action_savings_init(scenario_action_t *action)
{
    action->parameter3 = random_between_from_stdlib(action->parameter1, action->parameter2);
}

int scenario_event_action_savings_execute(scenario_action_t *action)
{
    int32_t adjustment = action->parameter3;
    city_emperor_add_personal_savings(adjustment);

    return 1;
}

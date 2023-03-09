#include "actiontype_city_health.h"

#include "city/health.h"
#include "core/random.h"

void scenario_event_action_city_health_init(scenario_action_t *action)
{
    action->parameter4 = random_between_from_stdlib(action->parameter1, action->parameter2);
}

int scenario_event_action_city_health_execute(scenario_action_t *action)
{
    int32_t is_hard_set = action->parameter3;
    int32_t adjustment = action->parameter4;
    
    if (is_hard_set) {
        city_health_set(adjustment);
    } else {
        city_health_change(adjustment);
    }

    return 1;
}

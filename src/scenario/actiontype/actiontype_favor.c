#include "actiontype_favor.h"

#include "city/ratings.h"

int scenario_event_action_favor_add_execute(scenario_action_t *action)
{
    int32_t adjustment = action->parameter1;
    city_ratings_change_favor(adjustment);

    return 1;
}

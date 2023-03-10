#include "actiontype_request_immediately_start.h"

#include "game/time.h"
#include "scenario/data.h"
#include "scenario/request.h"

int scenario_event_action_request_immediately_start_execute(scenario_action_t *action)
{
    int total_months = game_time_total_months();
    if (total_months < 1) {
        return 0; // Firing an event off at the start of the scenario breaks it. So prevent that.
    }

    int32_t request_id = action->parameter1;
    if (request_id < 0
        || request_id >= MAX_REQUESTS) {
        return 0;
    }

    return scenario_request_force_request_start(request_id);
}

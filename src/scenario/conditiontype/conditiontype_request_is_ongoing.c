#include "conditiontype_request_is_ongoing.h"

#include "scenario/request.h"

void scenario_condition_request_is_ongoing_init(scenario_condition_t *condition)
{
    // Nothing to init.
}

int scenario_condition_request_is_ongoing_met(scenario_condition_t *condition)
{
    int32_t request_id = condition->parameter1;
    int32_t check_for_ongoing = condition->parameter2;
    int is_ongoing = scenario_request_is_ongoing(request_id);

    return check_for_ongoing ? is_ongoing : !is_ongoing;
}

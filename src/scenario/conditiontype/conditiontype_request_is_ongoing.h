#ifndef CONDITION_TYPE_REQUEST_IS_ONGOING_H
#define CONDITION_TYPE_REQUEST_IS_ONGOING_H

#include "scenario/scenario_event_data.h"

void scenario_condition_request_is_ongoing_init(scenario_condition_t *condition);
int scenario_condition_request_is_ongoing_met(scenario_condition_t *condition);

#endif // CONDITION_TYPE_REQUEST_IS_ONGOING_H

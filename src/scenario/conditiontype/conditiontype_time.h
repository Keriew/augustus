#ifndef CONDITION_TYPE_TIME_H
#define CONDITION_TYPE_TIME_H

#include "scenario/scenario_event_data.h"

void scenario_condition_time_init(scenario_condition_t *condition);
int scenario_condition_time_met(scenario_condition_t *condition);

#endif // CONDITION_TYPE_TIME_H

#ifndef CONDITION_TYPE_COUNT_OWN_TROOPS_H
#define CONDITION_TYPE_COUNT_OWN_TROOPS_H

#include "scenario/scenario_event_data.h"

void scenario_condition_count_own_troops_init(scenario_condition_t *condition);
int scenario_condition_count_own_troops_met(scenario_condition_t *condition);

#endif // CONDITION_TYPE_COUNT_OWN_TROOPS_H

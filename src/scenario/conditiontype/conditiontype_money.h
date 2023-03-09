#ifndef CONDITION_TYPE_MONEY_H
#define CONDITION_TYPE_MONEY_H

#include "scenario/scenario_event_data.h"

void scenario_condition_money_init(scenario_condition_t *condition);
int scenario_condition_money_met(scenario_condition_t *condition);

#endif // CONDITION_TYPE_MONEY_H

#ifndef CONDITION_TYPE_POPULATION_UNEMPLOYED_H
#define CONDITION_TYPE_POPULATION_UNEMPLOYED_H

#include "scenario/scenario_event_data.h"

void scenario_condition_population_unemployed_init(scenario_condition_t *condition);
int scenario_condition_population_unemployed_met(scenario_condition_t *condition);

#endif // CONDITION_TYPE_POPULATION_UNEMPLOYED_H

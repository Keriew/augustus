#ifndef CONDITION_TYPE_CITY_POPULATION_H
#define CONDITION_TYPE_CITY_POPULATION_H

#include "scenario/scenario_event_data.h"

void scenario_condition_city_population_init(scenario_condition_t *condition);
int scenario_condition_city_population_met(scenario_condition_t *condition);

#endif // CONDITION_TYPE_CITY_POPULATION_H

#include "conditiontype_city_population.h"

#include "city/data_private.h"
#include "scenario/conditiontype/comparison_helper.h"

void scenario_condition_city_population_init(scenario_condition_t *condition)
{
    // Nothing to init.
}

int scenario_condition_city_population_met(scenario_condition_t *condition)
{
    int type = condition->parameter1;
    int32_t value = condition->parameter2;
    int32_t class = condition->parameter3;

    int population_value_to_use = city_data.population.population;
    if (class == POP_CLASS_PATRICIAN) {
        population_value_to_use = city_data.population.people_in_villas_palaces;
    } else if (class == POP_CLASS_PLEBEIAN) {
        population_value_to_use = city_data.population.population - city_data.population.people_in_villas_palaces;
    } else if (class == POP_CLASS_SLUMS) {
        population_value_to_use = city_data.population.people_in_tents_shacks;
    }

    return scenario_event_condition_compare_values(type, population_value_to_use, (int)value);
}

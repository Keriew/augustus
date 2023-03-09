#include "actiontype_gladiator_revolt.h"

#include "scenario/gladiator_revolt.h"

void scenario_event_action_gladiator_revolt_init(scenario_action_t *action)
{
    // Nothing to init.
}

int scenario_event_action_gladiator_revolt_execute(scenario_action_t *action)
{
    scenario_gladiator_revolt_start_new();

    return 1;
}

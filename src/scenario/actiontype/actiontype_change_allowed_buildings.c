#include "actiontype_change_allowed_buildings.h"

#include "scenario/data.h"
#include "building/menu.h"

void scenario_event_action_change_allowed_buildings_init(scenario_action_t *action)
{
    // Nothing to init.
}

int scenario_event_action_change_allowed_buildings_execute(scenario_action_t *action)
{
    int32_t building = action->parameter1;
    int32_t allowed = action->parameter2;

    scenario.allowed_buildings[building] = allowed;
    building_menu_update();
    
    return 1;
}

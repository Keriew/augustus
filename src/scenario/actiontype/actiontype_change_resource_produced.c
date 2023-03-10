#include "actiontype_change_resource_produced.h"

#include "empire/city.h"
#include "building/menu.h"

int scenario_event_action_change_resource_produced_execute(scenario_action_t *action)
{
    int32_t resource = action->parameter1;
    int32_t new_state = action->parameter2;

    int successfully_changed = empire_city_our_city_change_resource_available(resource, new_state);
    building_menu_update();
    
    return successfully_changed;
}

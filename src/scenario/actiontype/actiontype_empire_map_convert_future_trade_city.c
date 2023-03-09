#include "actiontype_empire_map_convert_future_trade_city.h"

#include "city/message.h"
#include "empire/city.h"
#include "empire/object.h"

void scenario_event_action_empire_map_convert_future_trade_city_init(scenario_action_t *action)
{
    // Nothing to init.
}

int scenario_event_action_empire_map_convert_future_trade_city_execute(scenario_action_t *action)
{
    int32_t target_city_id = action->parameter1;
    int32_t show_message = action->parameter2;

    empire_city *city = empire_city_get(target_city_id);
    if (city->type == EMPIRE_CITY_FUTURE_TRADE) {
        city->type = EMPIRE_CITY_TRADE;
        empire_object_set_expanded(city->empire_object_id, city->type);

        if (show_message) {
            city_message_post(1, MESSAGE_EMPIRE_HAS_EXPANDED, 0, 0);
        }
        return 1;
    }

    return 0;
}

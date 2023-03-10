#include "actiontype_trade_route_amount.h"

#include "city/message.h"
#include "empire/city.h"
#include "empire/trade_route.h"

int scenario_event_action_trade_route_amount_execute(scenario_action_t *action)
{
    int32_t route_id = action->parameter1;
    int32_t resource = action->parameter2;
    int32_t amount = action->parameter3;
    int32_t show_message = action->parameter4;

    trade_route_set_limit(route_id, resource, amount);
    if (show_message
        && empire_city_is_trade_route_open(route_id)) {
        int city_id = empire_city_get_for_trade_route(route_id);
        if (city_id < 0) {
            city_id = 0;
        }
        int last_amount = trade_route_limit(route_id, resource);

        int change = amount - last_amount;
        if (amount > 0 && change > 0) {
            city_message_post(1, MESSAGE_INCREASED_TRADING, city_id, resource);
        } else if (amount > 0 && change < 0) {
            city_message_post(1, MESSAGE_DECREASED_TRADING, city_id, resource);
        } else if (amount <= 0) {
            city_message_post(1, MESSAGE_TRADE_STOPPED, city_id, resource);
        }
    }

    return 1;
}

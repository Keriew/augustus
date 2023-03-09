#include "actiontype_trade_price.h"

#include "city/message.h"
#include "empire/trade_prices.h"

void scenario_event_action_trade_price_init(scenario_action_t *action)
{
    // Nothing to init.
}

int scenario_event_action_trade_price_execute(scenario_action_t *action)
{
    int32_t resource = action->parameter1;
    int32_t adjustment = action->parameter2;
    int32_t show_message = action->parameter3;

    int successfully_changed = trade_price_change(resource, adjustment);
    
    if (successfully_changed
        && show_message) {
        if (adjustment >= 0) {
            city_message_post(1, MESSAGE_PRICE_INCREASED, adjustment, resource);
        } else {
            city_message_post(1, MESSAGE_PRICE_DECREASED, -adjustment, resource);
        }
    }

    return 1;
}

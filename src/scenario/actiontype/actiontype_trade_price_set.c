#include "actiontype_trade_price_set.h"

#include "city/message.h"
#include "empire/trade_prices.h"

int scenario_event_action_trade_price_set_execute(scenario_action_t *action)
{
    int32_t resource = action->parameter1;
    int32_t amount = action->parameter2;
    int32_t set_buy_price = action->parameter3;
    int32_t show_message = action->parameter4;
    
    int current_price = 0;
    if (set_buy_price) {
        current_price = trade_price_base_buy(resource);
    } else {
        current_price = trade_price_base_sell(resource);
    }

    int adjustment = 0;
    if (current_price == amount) {
        return 1;
    } else if (current_price < amount) {
        adjustment = amount - current_price;
    } else {
        adjustment = current_price - amount;
    }

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

#include "actiontype_send_standard_message.h"

#include "city/message.h"
#include "empire/trade_prices.h"

void scenario_event_action_send_standard_message_init(scenario_action_t *action)
{
    // Nothing to init.
}

int scenario_event_action_send_standard_message_execute(scenario_action_t *action)
{
    int32_t text_id = action->parameter1;

    city_message_post(1, text_id, 0, 0);

    return 1;
}

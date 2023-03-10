#include "actiontype_trade_problem_sea.h"

#include "city/data_private.h"
#include "city/message.h"
#include "city/trade.h"

int scenario_event_action_trade_problems_sea_execute(scenario_action_t *action)
{
    int32_t duration = action->parameter1;

    city_data.trade.months_since_last_sea_trade_problem = 0;
    city_trade_start_sea_trade_problems(duration);
    city_message_post(1, MESSAGE_SEA_TRADE_DISRUPTED, 0, 0);

    return 1;
}

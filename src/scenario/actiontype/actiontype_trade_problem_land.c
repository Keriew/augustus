#include "actiontype_trade_problem_land.h"

#include "city/data_private.h"
#include "city/message.h"
#include "city/trade.h"
#include "scenario/property.h"

int scenario_event_action_trade_problems_land_execute(scenario_action_t *action)
{
    int32_t duration = action->parameter1;

    city_data.trade.months_since_last_land_trade_problem = 0;
    city_trade_start_land_trade_problems(duration);
    if (scenario_property_climate() == CLIMATE_DESERT) {
        city_message_post(1, MESSAGE_LAND_TRADE_DISRUPTED_SANDSTORMS, 0, 0);
    } else {
        city_message_post(1, MESSAGE_LAND_TRADE_DISRUPTED_LANDSLIDES, 0, 0);
    }

    return 1;
}

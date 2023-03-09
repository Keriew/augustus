#ifndef ACTION_TYPE_TRADE_ADJUST_ROUTE_AMOUNT_H
#define ACTION_TYPE_TRADE_ADJUST_ROUTE_AMOUNT_H

#include "scenario/scenario_event_data.h"

void scenario_event_action_trade_route_amount_init(scenario_action_t *action);
int scenario_event_action_trade_route_amount_execute(scenario_action_t *action);

#endif // ACTION_TYPE_TRADE_ADJUST_ROUTE_AMOUNT_H

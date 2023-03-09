#ifndef ACTION_TYPE_TRADE_PRICE_H
#define ACTION_TYPE_TRADE_PRICE_H

#include "scenario/scenario_event_data.h"

void scenario_event_action_trade_price_init(scenario_action_t *action);
int scenario_event_action_trade_price_execute(scenario_action_t *action);

#endif // ACTION_TYPE_TRADE_PRICE_H

#ifndef CONDITION_TYPE_TRADE_SELL_PRICE_H
#define CONDITION_TYPE_TRADE_SELL_PRICE_H

#include "scenario/scenario_event_data.h"

void scenario_condition_trade_sell_price_init(scenario_condition_t *condition);
int scenario_condition_trade_sell_price_met(scenario_condition_t *condition);

#endif // CONDITION_TYPE_TRADE_SELL_PRICE_H

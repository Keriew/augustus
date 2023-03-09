#include "conditiontype_trade_sell_price.h"

#include "empire/trade_prices.h"
#include "scenario/conditiontype/comparison_helper.h"

void scenario_condition_trade_sell_price_init(scenario_condition_t *condition)
{
    // Nothing to init.
}

int scenario_condition_trade_sell_price_met(scenario_condition_t *condition)
{
    int32_t resource = condition->parameter1;
    int type = condition->parameter2;
    int32_t value = condition->parameter3;

    int trade_sell_price = trade_price_base_sell(resource);
    return scenario_event_condition_compare_values(type, trade_sell_price, (int)value);
}

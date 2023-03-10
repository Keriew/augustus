#include "actiontype_money.h"

#include "city/finance.h"
#include "core/random.h"

void scenario_event_action_money_add_init(scenario_action_t *action)
{
    action->parameter3 = random_between_from_stdlib(action->parameter1, action->parameter2);
}

int scenario_event_action_money_add_execute(scenario_action_t *action)
{
    int32_t adjustment = action->parameter3;
    city_finance_treasury_add_miscellaneous(adjustment);

    return 1;
}

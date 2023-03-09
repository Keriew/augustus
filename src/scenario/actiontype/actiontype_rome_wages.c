#include "actiontype_rome_wages.h"

#include "city/data_private.h"
#include "city/labor.h"
#include "city/message.h"
#include "core/random.h"

void scenario_event_action_rome_wages_init(scenario_action_t *action)
{
    action->parameter4 = random_between_from_stdlib(action->parameter1, action->parameter2);
}

int scenario_event_action_rome_wages_execute(scenario_action_t *action)
{
    int32_t is_hard_set = action->parameter3;
    int32_t adjustment = action->parameter4;
    
    city_data.labor.months_since_last_wage_change = 0;

    int current_wages = city_labor_wages_rome();
    if (is_hard_set
        && adjustment == current_wages) {
        return 1;
    }

    if (is_hard_set) {
        city_data.labor.wages_rome = adjustment;
        if (adjustment > current_wages) {
            city_message_post(1, MESSAGE_ROME_RAISES_WAGES, 0, 0);
        } else {
            city_message_post(1, MESSAGE_ROME_LOWERS_WAGES, 0, 0);
        }
    } else {
        city_data.labor.wages_rome += adjustment;
        if (adjustment > 0) {
            city_message_post(1, MESSAGE_ROME_RAISES_WAGES, 0, 0);
        } else {
            city_message_post(1, MESSAGE_ROME_LOWERS_WAGES, 0, 0);
        }
    }

    if (city_data.labor.wages_rome < 1) {
        city_data.labor.wages_rome = 1;
    }

    return 1;
}

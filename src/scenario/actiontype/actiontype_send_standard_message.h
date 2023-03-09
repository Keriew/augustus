#ifndef ACTION_TYPE_SEND_STANDARD_MESSAGE_H
#define ACTION_TYPE_SEND_STANDARD_MESSAGE_H

#include "scenario/scenario_event_data.h"

void scenario_event_action_send_standard_message_init(scenario_action_t *action);
int scenario_event_action_send_standard_message_execute(scenario_action_t *action);

#endif // ACTION_TYPE_SEND_STANDARD_MESSAGE_H

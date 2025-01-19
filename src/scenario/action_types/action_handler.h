#ifndef ACTION_HANDLER_H
#define ACTION_HANDLER_H

#include "core/buffer.h"
#include "scenario/scenario_event_data.h"

void scenario_action_type_init(scenario_action_t *action);
int scenario_action_type_execute(scenario_action_t *action);

void scenario_action_type_delete(scenario_action_t *action);
void scenario_action_type_save_state(buffer *buf, const scenario_action_t *action, int link_type, int32_t link_id);
unsigned int scenario_action_type_load_state(buffer *buf, scenario_action_t *action, int *link_type, int32_t *link_id, 
    int is_new_version);
unsigned int scenario_action_type_load_more(unsigned int index, scenario_action_t *action);
int scenario_action_uses_custom_variable(const scenario_action_t *action, int custom_variable_id);

#endif // ACTION_HANDLER_H

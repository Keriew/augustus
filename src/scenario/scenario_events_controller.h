#ifndef SCENARIO_EVENTS_CONTOLLER_H
#define SCENARIO_EVENTS_CONTOLLER_H

#include "core/buffer.h"
#include "scenario/scenario_event_data.h"

void scenario_events_init();
void scenario_events_clear();
scenario_event_t *scenario_event_get(int event_id);
scenario_event_t *scenario_event_create(int repeat_min, int repeat_max, int max_repeats);
int scenario_events_get_count();

void scenario_events_save_state(buffer *buf_events, buffer *buf_conditions, buffer *buf_actions);
void scenario_events_load_state(buffer *buf_events, buffer *buf_conditions, buffer *buf_actions);

void scenario_events_process_all();
void scenario_events_progress_paused(int months_passed);

#endif // SCENARIO_EVENTS_CONTOLLER_H

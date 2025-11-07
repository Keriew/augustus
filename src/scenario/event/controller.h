#ifndef SCENARIO_EVENTS_CONTOLLER_H
#define SCENARIO_EVENTS_CONTOLLER_H

#include "core/buffer.h"
#include "scenario/event/data.h"

typedef enum {
    SCENARIO_EVENTS_VERSION = 1,

    SCENARIO_EVENTS_VERSION_NONE = 0,
    SCENARIO_EVENTS_VERSION_INITIAL = 1,
} scenario_events_version;

unsigned int scenario_formula_add(const uint8_t *formatted_calculation, int min_limit, int max_limit);
void scenario_formula_change(unsigned int id, const uint8_t *formatted_calculation, int min_eval, int max_eval);
const uint8_t *scenario_formula_get_string(unsigned int id);
scenario_formula_t *scenario_formula_get(unsigned int id);
int scenario_formula_evaluate_formula(unsigned int id);

void scenario_events_init(void);

void scenario_events_clear(void);
scenario_event_t *scenario_event_get(int event_id);
scenario_event_t *scenario_event_create(int repeat_min, int repeat_max, int max_repeats);
void scenario_event_delete(scenario_event_t *event);
int scenario_events_get_count(void);

void scenario_events_save_state(buffer *buf_events, buffer *buf_conditions, buffer *buf_actions, buffer *buf_formulas);
void scenario_events_load_state(buffer *buf_events, buffer *buf_conditions, buffer *buf_actions, buffer *buf_formulas, int is_new_version);

void scenario_events_process_all(void);
void scenario_events_progress_paused(int days_passed);
scenario_event_t *scenario_events_get_using_custom_variable(int custom_variable_id);
void scenario_events_migrate_to_resolved_display_names(void);
void scenario_events_migrate_to_formulas(void);
void scenario_events_min_max_migrate_to_formulas(void);

void scenario_events_assign_parent_event_ids(void);

void scenario_events_fetch_event_tiles_to_editor(void);

void scenario_events_migrate_to_grid_slices(void);

#endif // SCENARIO_EVENTS_CONTOLLER_H

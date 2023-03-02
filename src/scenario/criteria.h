#ifndef SCENARIO_CRITERIA_H
#define SCENARIO_CRITERIA_H

#include "core/buffer.h"
#include "scenario/data.h"

typedef enum {
    // There are no criteria. Free play.
    WIN_CRITERIA_STATE_NONE,

    // All criteria are satisfied. Map is won.
    WIN_CRITERIA_STATE_OK,

    // At least one criteria is not satisfied. Play continues.
    WIN_CRITERIA_STATE_FAIL,

    // A condition has been failed irreparably. The game is lost.
    WIN_CRITERIA_STATE_LOSE
} win_criteria_satisfy_state;

void scenario_criteria_clear_ptr(scenario_win_criteria *ptr);
void scenario_criteria_clear(void);

int scenario_criteria_try_add_or_update_ptr(scenario_win_criteria *ptr, win_criteria_type type, int goal, int data);
int scenario_criteria_try_add_or_update(win_criteria_type type, int goal, int data);

int scenario_criteria_toggle(win_criteria_type type, int goal, int data);
int scenario_criteria_disable(win_criteria_type type);

win_criteria_t *scenario_criteria_get_ptr(scenario_win_criteria* scenario_ptr, win_criteria_type type);

win_criteria_satisfy_state scenario_criteria_test_all(void);

int scenario_criteria_population_enabled(void);
int scenario_criteria_population(void);

int scenario_criteria_culture_enabled(void);
int scenario_criteria_culture(void);

int scenario_criteria_prosperity_enabled(void);
int scenario_criteria_prosperity(void);

int scenario_criteria_peace_enabled(void);
int scenario_criteria_peace(void);

int scenario_criteria_favor_enabled(void);
int scenario_criteria_favor(void);

int scenario_criteria_time_limit_enabled(void);
int scenario_criteria_time_limit_years(void);

int scenario_criteria_survival_enabled(void);
int scenario_criteria_survival_years(void);

int scenario_criteria_milestone_year(int percentage);

void scenario_criteria_init_max_year(void);
int scenario_criteria_max_year(void);

void scenario_criteria_save_state(buffer *buf);

void scenario_criteria_load_state(buffer *buf);

void scenario_criteria_load_data(buffer *data, int version, int step);

void scenario_criteria_save_data(buffer *data);

#endif // SCENARIO_CRITERIA_H

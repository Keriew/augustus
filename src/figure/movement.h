#ifndef FIGURE_MOVEMENT_H

#include "figure/figure.h"

#define FIGURE_MOVEMENT_H
#define TICKS_PER_TILE 15
#define ROAM_DECISION_INTERVAL 5
#define FIGURE_REROUTE_DESTINATION_TICKS 120
#define FIGURE_TYPE_COUNT (FIGURE_LAST_DEFINED_TYPE + 1)
#define ROAM_INITIAL_OFFSET 8
#define ROAM_TURN_CW 2
#define ROAM_TURN_CCW -2
#define CC_MAJOR_X 1
#define CC_MAJOR_Y 2
#define TICK_PERCENTAGE_BASE 100 // Shouldn't this be part of movement_internal.h?

void figure_movement_init_roaming(figure *f);

void figure_movement_move_ticks(figure *f, int num_ticks);

void figure_movement_move_ticks_with_percentage(figure* f, int num_ticks, int tick_percentage);

void figure_movement_move_ticks_tower_sentry(figure *f, int num_ticks);

void figure_movement_roam_ticks(figure *f, int num_ticks);

void figure_movement_follow_ticks(figure *f, int num_ticks);

void figure_movement_follow_ticks_with_percentage(figure* f, int num_ticks, int tick_percentage);

void figure_movement_advance_attack(figure *f);

void figure_movement_set_cross_country_direction(
    figure *f, int x_src, int y_src, int x_dst, int y_dst, int is_missile);

void figure_movement_set_cross_country_destination(figure *f, int x_dst, int y_dst);

int figure_movement_move_ticks_cross_country(figure *f, int num_ticks);

int figure_movement_can_launch_cross_country_missile(int x_src, int y_src, int x_dst, int y_dst);

// Function prototypes
roadblock_permission get_permission_for_figure_type(figure *f);

#endif // FIGURE_MOVEMENT_H
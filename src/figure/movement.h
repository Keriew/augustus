// Header Guard
#ifndef MOVEMENT_H
#define MOVEMENT_H

#define TICKS_PER_TILE 15
#define FIGURE_TYPE_COUNT (FIGURE_LAST_DEFINED_TYPE + 1)
#define FIGURE_REROUTE_DESTINATION_TICKS 120

// Public functions
void figure_movement_path(figure *f, int speed);

// Function prototypes
roadblock_permission get_permission_for_figure_type(figure *f);

#endif
#ifndef MOVEMENT_INTERNAL_H
#define MOVEMENT_INTERNAL_H

#include "figure/figure.h"
#define TICK_PERCENTAGE_BASE 100

// Shared helper functions (Defined in movement.c, used by movement_roam.c)
void move_to_next_tile(figure *f);
void advance_tick(figure *f);

// Shared Constants
extern const point_2d DIR_DELTAS[10];

#endif
// Header Guard
#ifndef MOVEMENT_H
#define MOVEMENT_H

#include "figure.h"
#include "building/roadblock.h"
#include "core/direction.h"

#define MOV_PER_TILE 15
#define FIGURE_REROUTE_DESTINATION_TICKS 120
#define MOVEMENT_THRESHOLD 100

// Building HP
#define BUILDING_HP_TERRAIN_MOD 0
#define BUILDING_HP_CLEARABLE    1
#define BUILDING_HP_DEFAULT      10
#define BUILDING_HP_PALISADE     60
#define BUILDING_HP_WALL         200
#define BUILDING_HP_GATEHOUSE    150

// Public functions
void figure_movement_path(figure *f, int speed);
void move_to_next_tile(figure *f);
void advance_mov(figure *f);

extern const point_2d DIR_DELTAS[10];

#endif
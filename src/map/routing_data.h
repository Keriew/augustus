#ifndef MAP_ROUTING_DATA_H
#define MAP_ROUTING_DATA_H

#include "map/grid.h"

enum { // Includes movement cost
    CITIZEN_0_ROAD = 100,
    CITIZEN_1_HIGHWAY = 50,
    CITIZEN_2_PASSABLE_TERRAIN = 100,
    CITIZEN_4_CLEAR_TERRAIN = 200,
    CITIZEN_N1_BLOCKED = 200,
    CITIZEN_N3_AQUEDUCT = 200,
    CITIZEN_N4_RESERVOIR_CONNECTOR = 200,

    NONCITIZEN_0_PASSABLE = 100,
    NONCITIZEN_1_BUILDING = 200,
    NONCITIZEN_2_CLEARABLE = 200,
    NONCITIZEN_3_WALL = 200,
    NONCITIZEN_4_GATEHOUSE = 200,
    NONCITIZEN_5_FORT = 200,
    NONCITIZEN_N1_BLOCKED = 200,

    WATER_0_PASSABLE = 100,
    WATER_N3_LOW_BRIDGE = 100, // Small boats can be added!
    WATER_N1_BLOCKED = 100,
    WATER_N2_MAP_EDGE = 200,

    WALL_0_PASSABLE = 100,
    WALL_N1_BLOCKED = 200,
};

extern grid_i8 terrain_land_citizen;
extern grid_i8 terrain_land_noncitizen;
extern grid_i8 terrain_water;
extern grid_i8 terrain_walls;

#endif // MAP_ROUTING_DATA_H

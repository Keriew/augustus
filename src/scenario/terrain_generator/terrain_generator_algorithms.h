#ifndef SCENARIO_TERRAIN_GENERATOR_ALGORITHMS_H
#define SCENARIO_TERRAIN_GENERATOR_ALGORITHMS_H

#include <stdint.h>
#include "core/calc.h"

int terrain_generator_random_between(int min_value, int max_value);

int terrain_generator_flood_fill_reachable_land(int start_x, int start_y, uint8_t *reachable_land);

void segment_map(void);

const uint16_t *terrain_generator_segments(void);

uint16_t terrain_generator_segment_id_at(int x, int y);

void terrain_generator_random_terrain(void);

void terrain_generator_straight_river(void);

void terrain_generator_generate_river(void);

void terrain_generator_river_map(unsigned int);
#endif // SCENARIO_TERRAIN_GENERATOR_ALGORITHMS_H

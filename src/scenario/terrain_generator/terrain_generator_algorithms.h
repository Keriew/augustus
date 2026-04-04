#ifndef SCENARIO_TERRAIN_GENERATOR_ALGORITHMS_H
#define SCENARIO_TERRAIN_GENERATOR_ALGORITHMS_H

int terrain_generator_random_between(int min_value, int max_value);
int terrain_generator_clamp_int(int value, int min_value, int max_value);

void terrain_generator_random_terrain(void);
void terrain_generator_straight_river(void);
void terrain_generator_generate_river(void);
void terrain_generator_river_map(unsigned int);
#endif // SCENARIO_TERRAIN_GENERATOR_ALGORITHMS_H

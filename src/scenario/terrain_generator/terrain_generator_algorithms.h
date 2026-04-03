#ifndef SCENARIO_TERRAIN_GENERATOR_ALGORITHMS_H
#define SCENARIO_TERRAIN_GENERATOR_ALGORITHMS_H

int terrain_generator_random_between(int min_value, int max_value);
int terrain_generator_clamp_int(int value, int min_value, int max_value);

void terrain_generator_generate_flat_plains(void);
void terrain_generator_generate_river_valley(void);
void terrain_generator_generate_perlin(unsigned int);

#endif // SCENARIO_TERRAIN_GENERATOR_ALGORITHMS_H

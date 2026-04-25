#ifndef SCENARIO_TERRAIN_GENERATOR_H
#define SCENARIO_TERRAIN_GENERATOR_H

typedef enum {
    TERRAIN_GENERATOR_RANDOM = 1,
    TERRAIN_GENERATOR_RIVER = 0,
    TERRAIN_GENERATOR_COUNT = 2
} terrain_generator_algorithm;

void terrain_generator_generate(terrain_generator_algorithm algorithm);

void terrain_generator_set_seed(int enabled, unsigned int seed);

#endif // SCENARIO_TERRAIN_GENERATOR_H

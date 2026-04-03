#include "random.h"

#include "core/random.h"

#include <stdlib.h>
#include <time.h>

#include "map/grid.h"

static grid_u8 random_value;

void map_random_clear(void)
{
    map_grid_clear_u8(random_value.items);
}

void map_random_init(void)
{
    int grid_offset = 0;
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++, grid_offset++) {
            random_generate_next();
            random_value.items[grid_offset] = (uint8_t) random_short();
        }
    }
}

int map_random_get(int grid_offset)
{
    return random_value.items[grid_offset];
}

int map_random_get_from_buffer(buffer *buf, int grid_offset)
{
    buffer_set(buf, grid_offset);
    return buffer_read_u8(buf);
}

void map_random_save_state(buffer *buf)
{
    map_grid_save_state_u8(random_value.items, buf);
}

void map_random_load_state(buffer *buf)
{
    map_grid_load_state_u8(random_value.items, buf);
}

unsigned int generate_seed_value(void)
{
    // Generate a random value between 1 and UINT_MAX. Return the unsingned value directly, as it can be used as a seed for the terrain generator.
    // Do not use time value directly as it is just increasing and might not provide enough randomness if the terrain generator is opened multiple times within the same second. Instead, use a combination of time and a random value from the C library's rand() function to increase randomness.
    unsigned int time_seed = (unsigned int) time(NULL);
    unsigned int rand_seed = (unsigned int) rand();
    unsigned int seed = time_seed ^ rand_seed; // Combine time and rand values using XOR
    seed = seed == 0 ? 1 : seed; // Ensure seed is not zero, as some terrain generators might treat zero as a special case.
    return seed;
}

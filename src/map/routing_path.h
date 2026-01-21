#ifndef MAP_ROUTING_PATH_H
#define MAP_ROUTING_PATH_H

#include <stddef.h>
#include <stdint.h>

#define ROUTING_PATH_DIRECTION_BIT_OFFSET 5
#define ROUTING_PATH_DIRECTION_COUNT_BIT_MASK ((uint8_t) (1u << ROUTING_PATH_DIRECTION_BIT_OFFSET) - 1)

typedef struct {
    unsigned int id;
    unsigned int figure_id;
    unsigned short total_directions;
    uint8_t *directions;

    size_t current_step;
    uint8_t same_direction_count;
} figure_path_data;

int map_routing_get_path(figure_path_data *path, int dst_x, int dst_y, int num_directions);

int map_routing_get_path_on_water(figure_path_data *path, int dst_x, int dst_y, int is_flotsam);

#endif // MAP_ROUTING_PATH_H

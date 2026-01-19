#ifndef MAP_ROUTING_PATH_H
#define MAP_ROUTING_PATH_H

#include <stdint.h>

typedef struct {
    unsigned int id;
    unsigned int figure_id;
    unsigned short total_directions;
    uint8_t *directions;
} figure_path_data;

int map_routing_get_path(figure_path_data *path, int dst_x, int dst_y, int num_directions);

int map_routing_get_path_on_water(figure_path_data *path, int dst_x, int dst_y, int is_flotsam);

#endif // MAP_ROUTING_PATH_H

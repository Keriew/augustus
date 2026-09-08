#include "terrain_generator_algorithms.h"

#include "core/log.h"
#include "map/grid.h"
#include "map/terrain.h"

static uint16_t segments[GRID_SIZE * GRID_SIZE];

static int terrain_tile_is_segment_passable(int grid_offset) {
    return !map_terrain_is(grid_offset, TERRAIN_ROCK | TERRAIN_WATER);
}

const uint16_t *terrain_generator_segments(void) {
    return segments;
}

uint16_t terrain_generator_segment_id_at(int x, int y) {
    if (!map_grid_is_inside(x, y, 1)) {
        return 0;
    }
    return segments[map_grid_offset(x, y)];
}

void segment_map(void) {
    const int width = map_grid_width();
    const int height = map_grid_height();
    static const int dx[4] = {1, -1, 0, 0};
    static const int dy[4] = {0, 0, 1, -1};
    int queue[GRID_SIZE * GRID_SIZE];
    uint16_t component_id = 1;

    map_grid_clear_u16(segments);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int start_offset = map_grid_offset(x, y);

            if (!terrain_tile_is_segment_passable(start_offset)) {
                continue;
            }
            if (segments[start_offset] != 0) {
                continue;
            }

            int queue_start = 0;
            int queue_end = 0;
            segments[start_offset] = component_id;
            queue[queue_end++] = start_offset;

            while (queue_start < queue_end) {
                int current_offset = queue[queue_start++];
                int current_x = map_grid_offset_to_x(current_offset);
                int current_y = map_grid_offset_to_y(current_offset);

                for (int i = 0; i < 4; i++) {
                    int nx = current_x + dx[i];
                    int ny = current_y + dy[i];

                    if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
                        continue;
                    }

                    int next_offset = map_grid_offset(nx, ny);
                    if (segments[next_offset] != 0) {
                        continue;
                    }
                    if (!terrain_tile_is_segment_passable(next_offset)) {
                        continue;
                    }
                    segments[next_offset] = component_id;
                    queue[queue_end++] = next_offset;
                }
            }

            if (component_id < UINT16_MAX) {
                component_id++;
            }
        }
    }
    int final_segment_id = (int) component_id - 1;
    if (final_segment_id > 0) {
        log_info("Terrain segmentation components", 0, final_segment_id);
    } else {
        log_info("Terrain segmentation components", "0", 0);
    }
}

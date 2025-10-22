#ifndef MAP_GRID_H
#define MAP_GRID_H

#include "core/buffer.h"

#include <stdint.h>

enum {
    GRID_SIZE = 162
};
#define MAX_SLICE_SIZE GRID_SIZE * GRID_SIZE 
/**
 * Represents a collection of grid offsets
 *
 * Used to store multiple grid coordinates as a contiguous array of offsets,
 * for operations on groups of tiles - allows easy iteration through uneven shapes
 *
 * @param grid_offsets Array containing the grid offset positions
 * @param size 1-based count of valid entries in grid_offsets, 0 means empty arrray, 1 means [0] is valid, etc.
 */
typedef struct grid_slice {
    int grid_offsets[MAX_SLICE_SIZE];
    int size;
} grid_slice;

typedef struct {
    uint8_t items[GRID_SIZE * GRID_SIZE];
} grid_u8;

typedef struct {
    int8_t items[GRID_SIZE * GRID_SIZE];
} grid_i8;

typedef struct {
    uint16_t items[GRID_SIZE * GRID_SIZE];
} grid_u16;

typedef struct {
    int16_t items[GRID_SIZE * GRID_SIZE];
} grid_i16;

typedef struct {
    uint32_t items[GRID_SIZE * GRID_SIZE];
} grid_u32;

void map_grid_init(int width, int height, int start_offset, int border_size);

grid_slice *map_grid_get_grid_slice(int *grid_offsets, int size);

grid_slice *map_grid_get_grid_slice_from_corners(int start_x, int start_y, int end_x, int end_y);
grid_slice *map_grid_get_grid_slice_from_corner_offsets(int corner_offset_1, int corner_offset_2);
/** @brief Uses a loop instead of first and last position, since a grid slice is not ncessarily an ordered array
 * @returns 1 on success, 0 on failure
 */
int map_grid_get_corner_offsets_from_grid_slice(const grid_slice *slice, int *top_left_offset, int *bottom_right_offset);

int map_grid_is_valid_offset(int grid_offset);

int map_grid_offset(int x, int y);

int map_grid_offset_to_x(int grid_offset);

int map_grid_offset_to_y(int grid_offset);

int map_grid_delta(int x, int y);

/**
 * Adds the specified X and Y to the given offset with error checking
 * @return New grid offset, or -1 if the x/y would wrap around to a different row/column
 */
int map_grid_add_delta(int grid_offset, int x, int y);

int map_grid_direction_delta(int direction);

int map_grid_chess_distance(int offset1, int offset2);

void map_grid_size(int *width, int *height);

int map_grid_width(void);

int map_grid_height(void);

void map_grid_bound(int *x, int *y);

void map_grid_bound_area(int *x_min, int *y_min, int *x_max, int *y_max);

void map_grid_get_area(int x, int y, int size, int radius, int *x_min, int *y_min, int *x_max, int *y_max);

void map_grid_start_end_to_area(int x_start, int y_start, int x_end, int y_end, int *x_min, int *y_min, int *x_max, int *y_max);

int map_grid_is_inside(int x, int y, int size);

const int *map_grid_adjacent_offsets(int size);

void map_grid_get_corner_tiles(int start_x, int start_y, int x, int y, int *c1x, int *c1y, int *c2x, int *c2y);

void map_grid_clear_u8(uint8_t *grid);

void map_grid_clear_i8(int8_t *grid);

void map_grid_clear_u16(uint16_t *grid);

void map_grid_clear_i16(int16_t *grid);

void map_grid_clear_u32(uint32_t *grid);

void map_grid_init_i8(int8_t *grid, int8_t value);

void map_grid_and_u8(uint8_t *grid, uint8_t mask);

void map_grid_and_u16(uint16_t *grid, uint16_t mask);

void map_grid_and_u32(uint32_t *grid, uint32_t mask);

void map_grid_copy_u8(const uint8_t *src, uint8_t *dst);

void map_grid_copy_u16(const uint16_t *src, uint16_t *dst);

void map_grid_copy_u32(const uint32_t *src, uint32_t *dst);


void map_grid_save_state_u8(const uint8_t *grid, buffer *buf);

void map_grid_save_state_i8(const int8_t *grid, buffer *buf);

void map_grid_save_state_u16(const uint16_t *grid, buffer *buf);

void map_grid_save_state_u32_to_u16(const uint32_t *grid, buffer *buf);

void map_grid_save_state_u32(const uint32_t *grid, buffer *buf);

void map_grid_load_state_u8(uint8_t *grid, buffer *buf);

void map_grid_load_state_i8(int8_t *grid, buffer *buf);

void map_grid_load_state_u16(uint16_t *grid, buffer *buf);

void map_grid_load_state_u16_to_u32(uint32_t *grid, buffer *buf);

void map_grid_load_state_u32(uint32_t *grid, buffer *buf);

/**
 * @brief Creates a grid slice representing a rectangular area starting from the given grid offset.
 * All grid points within the specified width and height are included.
 *
 * @param start_grid_offset Grid offset of the top-left corner of the rectangle
 * @param width Width of the rectangle in grid units
 * @param height Height of the rectangle in grid units
 * @return Allocated grid_slice containing rectangle coordinates, or NULL on failure
 */
grid_slice *map_grid_get_grid_slice_rectangle(int start_grid_offset, int width, int height);

/**
 * @brief Creates a grid slice representing area occupied by the given house building.
 *
 * @param building_id id to search for
 * @param check_rubble if true, will check for rubble info grid instead of building grid
 * @return Allocated grid_slice containing rectangle coordinates, or NULL on failure
 */
grid_slice *map_grid_get_grid_slice_house(int building_id, int check_rubble);

/**
 * @brief Creates a grid slice representing a square area starting from the given grid offset.
 * All grid points within the specified size x size area are included.
 *
 * @param start_grid_offset Grid offset of the top-left corner of the square
 * @param size Side length of the square in grid units
 * @return Allocated grid_slice containing square coordinates, or NULL on failure
 */
grid_slice *map_grid_get_grid_slice_square(int start_grid_offset, int size);

/**
 * @brief Creates a grid slice representing a square ring centered at the given grid offset.
 *
 * @param center_grid_offset Grid offset of the ring center
 * @param inner_radius Inner radius of the ring (exclusive, using chess distance)
 * @param outer_radius Outer radius of the ring (inclusive, using chess distance)
 * @return Allocated grid_slice containing ring coordinates, or NULL on failure
 */
grid_slice *map_grid_get_grid_slice_ring(int center_grid_offset, int inner_radius, int outer_radius);

/**
 * @brief Creates a grid slice representing a square centered at the given grid offset.
 * All grid points within the specified radius are included using chess distance, producing a square shape.
 *
 * @param center_grid_offset Grid offset of the circle center
 * @param radius Radius of the shape (using chess distance)
 * @return Allocated grid_slice, or NULL on failure
 */
grid_slice *map_grid_get_grid_slice_from_center(int center_grid_offset, int radius);

#endif // MAP_GRID_H

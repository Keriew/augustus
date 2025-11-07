#ifndef MAP_FIGURE_H
#define MAP_FIGURE_H

#include "core/buffer.h"
#include "figure/figure.h"

/**
 * Returns the first figure at the given offset
 * @param grid_offset Map offset
 * @return Figure ID of first figure at offset
 */
int map_figure_at(int grid_offset);

/**
 * Returns whether there is a figure at the given offset
 * @param grid_offset Map offset
 * @return True if there is a figure, otherwise false
 */
int map_has_figure_at(int grid_offset);

/**
 * Returns whether there is a figure of a given category or category mix at a give offset
 * @param grid_offset Map offset
 * @param category The categories to be checked for e.g. FIGURE_CATEGORY_HOSTILE | FIGURE_CATEGORY_NATIVE
 */
int map_has_figure_category_at(int grid_offset, figure_category category);

/**
 * Calls map_has_figure_category_at for every tile in a given area
 * @params minx, miny, maxx, maxy Specify the area
 * @param category The categories to be checked for
 */
int map_has_figure_category_in_area(int minx, int miny, int maxx, int maxy, figure_category category);

void map_figure_add(figure *f);

void map_figure_update(figure *f);

void map_figure_delete(figure *f);

int map_figure_foreach_until(int grid_offset, int (*callback)(figure *f));

/**
 * Clears the map
 */
void map_figure_clear(void);

void map_figure_save_state(buffer *buf);

void map_figure_load_state(buffer *buf);

#endif // MAP_FIGURE_H

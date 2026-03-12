#ifndef MAP_FIGURE_H
#define MAP_FIGURE_H

#include "core/buffer.h"
#include "figure/figure.h"
#include "map/grid.h"

/**
 * Returns the first figure at the given offset
 * @param grid_offset Map offset
 * @return Figure ID of first figure at offset
 */
unsigned int map_figure_at(int grid_offset);

/**
 * Returns whether there is a figure at the given offset
 * @param grid_offset Map offset
 * @return True if there is a figure, otherwise false
 */
int map_has_figure_at(int grid_offset);

/**
 * Returns whether there is a figure of a given category or category mix at a given offset
 * @param grid_offset Map offset
 * @param category The categories to be checked for e.g. FIGURE_CATEGORY_HOSTILE | FIGURE_CATEGORY_NATIVE
 * @return 1 if there's a figure of category on grid_offset else 0
 */
int map_has_figure_category_at(int grid_offset, figure_category category);

/**
 * Calls map_has_figure_category_at for every tile in the given slice
 * @param slice The grid slice to check
 * @param category The categories to be checked for
 * @return 1 if there is a figure of category in the slice, otherwise 0
 */
int map_has_figure_category_in_area(grid_slice *slice, figure_category category);

/**
 * Counts the figures of a given category at a given grid_offset
 * @param grid_offset Map offset
 * @param category The categories to be checked for
 * @return Count of figures of category on grid_offset
 */
int map_count_figures_category_at(int grid_offset, figure_category category);

/**
 * Calls map_count_figures_category_at for every tile in the given slice
 * @param slice The grid slice to check
 * @param category The categories to be checked for
 * @return Count of figures of category in the slice
 */
int map_count_figures_category_in_area(grid_slice *slice, figure_category category);

/**
 * Kills all figures of a given category at a given grid_offset
 * @param grid_offset Map offset
 * @param category The categories to be checked for
 */
void map_kill_figures_category_at(int grid_offset, figure_category category);

/**
 * Calls map_kill_figures_category_at for every tile in the given slice
 * @param slice The grid slice to process
 * @param category The categories to be checked for
 */
void map_kill_figures_category_in_area(grid_slice *slice, figure_category category);

void map_figure_add(figure *f);

void map_figure_update(figure *f);

void map_figure_delete(figure *f);

int map_figure_foreach_until(int grid_offset, int (*callback)(figure *f));

void map_figure_foreach(int grid_offset, void (*callback)(figure *f));

/**
 * Clears the map
 */
void map_figure_clear(void);

void map_figure_save_state(buffer *buf);

void map_figure_load_state(buffer *buf);

#endif // MAP_FIGURE_H

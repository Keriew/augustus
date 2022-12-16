#ifndef FIGURE_VISITED_BUILDINGS_H
#define FIGURE_VISITED_BUILDINGS_H

#include "core/buffer.h"

typedef struct {
    int index;
    int building_id;
    int prev_index;
} visited_building;

/**
 * Initializes the visited buildings list
 */
void figure_visited_buildings_init(void);

/**
 * Creates a new visited building entry
 * @return The new entry or 0 on error
 */
visited_building *figure_visited_buildings_add(void);

/**
 * Gets a visisted building entry from an index
 * @param index The index to fetch
 * @return The respective entry
 */
visited_building *figure_visited_buildings_get(int index);

/**
 * Removes an entire list of visited buildings, started at index.
 *
 * The function recursively removes the current index and all last indexes associated to the index
 * To prevent dangling lists, make sure you remove from the last actual index for the list!
 *
 * @param index The index of the last element on the list to remove
 */
void figure_visited_buildings_remove_list(int index);

void figure_visited_buildings_migrate_ship_info(void);

/**
 * Save state to buffer
 * @param buf Buffer
 */
void figure_visited_buildings_save_state(buffer *buf);

/**
 * Load state from buffer
 * @param buf Buffer
 */
void figure_visited_buildings_load_state(buffer *buf);

#endif // FIGURE_VISITED_BUILDINGS_H

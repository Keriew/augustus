#ifndef EDITOR_TOOL_H
#define EDITOR_TOOL_H

#include "map/point.h"
#include "map/grid.h"

typedef enum {
    TOOL_GRASS = 0,
    TOOL_TREES = 1,
    TOOL_WATER = 2,
    TOOL_EARTHQUAKE_POINT = 3,
    TOOL_SHRUB = 4,
    TOOL_ROCKS = 5,
    TOOL_MEADOW = 6,
    TOOL_ACCESS_RAMP = 9,
    TOOL_ROAD = 10,
    TOOL_RAISE_LAND = 11,
    TOOL_LOWER_LAND = 12,
    TOOL_INVASION_POINT = 13,
    TOOL_ENTRY_POINT = 15,
    TOOL_EXIT_POINT = 16,
    TOOL_RIVER_ENTRY_POINT = 18,
    TOOL_RIVER_EXIT_POINT = 19,
    TOOL_NATIVE_HUT = 21,
    TOOL_NATIVE_CENTER = 22,
    TOOL_NATIVE_FIELD = 23,
    TOOL_FISHING_POINT = 24,
    TOOL_HERD_POINT = 25,
    TOOL_NATIVE_HUT_ALT = 26,
    TOOL_NATIVE_DECORATION = 27,
    TOOL_NATIVE_MONUMENT = 28,
    TOOL_NATIVE_WATCHTOWER = 29,
    TOOL_NATIVE_RUINS = 30,
    TOOL_SELECT_LAND = 31
} tool_type;

tool_type editor_tool_type(void);
int editor_tool_is_active(void);
void editor_tool_deactivate(void);
void editor_tool_set_type(tool_type tool);
void editor_tool_set_with_id(tool_type tool, int id);

int editor_tool_is_brush(void);
int editor_tool_brush_size(void);
void editor_tool_set_brush_size(int size);
void editor_tool_foreach_brush_tile(void (*callback)(const void *user_data, int dx, int dy), const void *user_data);

int editor_tool_is_updatable(void);

int editor_tool_is_in_use(void);

void editor_tool_start_use(const map_tile *tile);

void editor_tool_update_use(const map_tile *tile);

void editor_tool_end_use(const map_tile *tile);

/**
 * @brief Set callback for TOOL_SELECT_LAND to receive the selected grid_slice
 * @param callback Function to be called when selection is completed, receives grid_slice pointer
 */
void editor_tool_set_selection_callback(void (*callback)(grid_slice *selection));

/**
 * @brief Get the current land selection from TOOL_SELECT_LAND
 * @return grid_slice pointer with selected tiles, or NULL if no selection or tool not active
 * @note The returned pointer is dynamically allocated and must be freed by the caller
 */
grid_slice *editor_tool_get_land_selection(void);

grid_slice *editor_tool_get_existing_land_selection(void);

void editor_tool_set_land_selection(grid_slice *selection);

void editor_tool_set_existing_land_selection(grid_slice *selection);

void editor_tool_clear_existing_land_selection(void);

void editor_tool_clear_land_selection(void);

/**
 * @brief Get the start tile for updatable tools (TOOL_ROAD, TOOL_SELECT_LAND)
 * @return Pointer to the start map_tile, or NULL if tool not in use
 */
const map_tile *editor_tool_get_start_tile(void);

/**
 * @brief Get the start and end grid offsets from the land selection
 * @param start_offset Pointer to store the start grid offset (can be NULL)
 * @param end_offset Pointer to store the end grid offset (can be NULL)
 * @note The offsets represent opposite corners of the selected rectangle
 */
void editor_tool_get_selection_offsets(int *start_offset, int *end_offset);

#endif // EDITOR_TOOL_H

#include "construction_routed.h"

#include "core/calc.h"
#include "building/building.h"
#include "building/connectable.h"
#include "building/construction.h"
#include "building/image.h"
#include "building/properties.h"
#include "game/undo.h"
#include "map/building.h"
#include "map/building_tiles.h"
#include "map/grid.h"
#include "map/image.h"
#include "map/property.h"
#include "map/routing.h"
#include "map/routing_terrain.h"
#include "map/terrain.h"
#include "map/tiles.h"
#include "graphics/window.h"

#include <stdlib.h>

#define GATE_PREVIEW_MAX 100

static struct {
    int grid_offsets[GATE_PREVIEW_MAX];
    int images[GATE_PREVIEW_MAX];
    int count;
} gate_preview;

void building_construction_restore_gate_previews(void)
{
    for (int i = 0; i < gate_preview.count; i++) {
        map_image_set(gate_preview.grid_offsets[i], gate_preview.images[i]);
    }
    gate_preview.count = 0;
}

static int place_routed_building(int x_start, int y_start, int x_end, int y_end, routed_building_type type, int *items, int measure_only)
{
    static const int direction_indices[8][4] = {
        {0, 2, 6, 4},
        {0, 2, 6, 4},
        {2, 4, 0, 6},
        {2, 4, 0, 6},
        {4, 6, 2, 0},
        {4, 6, 2, 0},
        {6, 0, 4, 2},
        {6, 0, 4, 2}
    };
    *items = 0;
    int grid_offset = map_grid_offset(x_end, y_end);
    int distance = 0;
    int guard = 0;
    // reverse routing
    while (1) {
        if (++guard >= 400) {
            return 0;
        }
        distance = map_routing_distance(grid_offset);
        if (distance <= 0) {
            return 0;
        }
        switch (type) {
            default:
            case ROUTED_BUILDING_ROAD:
                if (map_routing_is_gate_transformable(grid_offset)) {
                    building *wall_b = building_get(map_building_at(grid_offset));
                    int gate_type = building_connectable_gate_type(wall_b->type);
                    if (gate_type) {
                        if (!measure_only) {
                            game_undo_add_building(wall_b);
                            wall_b->state = BUILDING_STATE_DELETED_BY_PLAYER;
                            building *gate_b = building_create(gate_type, x_end, y_end);
                            game_undo_add_building(gate_b);
                            map_building_tiles_add(gate_b->id, gate_b->x, gate_b->y, gate_b->size,
                                building_image_get(gate_b), TERRAIN_BUILDING);
                            map_terrain_add(grid_offset, TERRAIN_ROAD);
                        } else if (gate_preview.count < GATE_PREVIEW_MAX) {
                            // Show gate ghost during preview: save original image, swap type to get gate image
                            gate_preview.grid_offsets[gate_preview.count] = grid_offset;
                            gate_preview.images[gate_preview.count] = map_image_at(grid_offset);
                            gate_preview.count++;
                            map_terrain_add(grid_offset, TERRAIN_ROAD); // temporary, restored next frame
                            building_type saved_type = wall_b->type;
                            wall_b->type = gate_type;
                            map_image_set(grid_offset, building_image_get(wall_b));
                            wall_b->type = saved_type;
                        }
                        *items += 1;
                        break;
                    }
                }
                *items += map_tiles_set_road(x_end, y_end);
                break;
            case ROUTED_BUILDING_AQUEDUCT:
                *items += map_building_tiles_add_aqueduct(x_end, y_end);
                break;
            case ROUTED_BUILDING_AQUEDUCT_WITHOUT_GRAPHIC:
                *items += 1;
                break;
            case ROUTED_BUILDING_HIGHWAY:
                *items += map_tiles_set_highway(x_end, y_end);
                break;
        }
        int direction = calc_general_direction(x_end, y_end, x_start, y_start);
        if (direction == DIR_8_NONE) {
            return 1; // destination reached
        }
        int routed = 0;
        for (int i = 0; i < 4; i++) {
            int index = direction_indices[direction][i];
            int new_grid_offset = grid_offset + map_grid_direction_delta(index);
            int new_dist = map_routing_distance(new_grid_offset);
            if (new_dist > 0 && new_dist < distance) {
                grid_offset = new_grid_offset;
                x_end = map_grid_offset_to_x(grid_offset);
                y_end = map_grid_offset_to_y(grid_offset);
                routed = 1;
                break;
            }
        }
        if (!routed) {
            return 0;
        }
    }
}

int building_construction_place_road(int measure_only, int x_start, int y_start, int x_end, int y_end)
{
    building_construction_restore_gate_previews();
    game_undo_restore_map(0);

    int start_offset = map_grid_offset(x_start, y_start);
    int end_offset = map_grid_offset(x_end, y_end);
    int forbidden_terrain_mask =
        TERRAIN_TREE | TERRAIN_ROCK | TERRAIN_WATER |
        TERRAIN_SHRUB | TERRAIN_GARDEN | TERRAIN_ELEVATION |
        TERRAIN_RUBBLE | TERRAIN_BUILDING | TERRAIN_WALL;
    if (map_terrain_is(start_offset, forbidden_terrain_mask)) {
        if (!(map_routing_is_gate_transformable(start_offset))) {
            return 0;
        }
    }
    if (map_terrain_is(end_offset, forbidden_terrain_mask)) {
        if (!(map_routing_is_gate_transformable(end_offset))) {
            return 0;
        }
    }

    int items_placed = 0;
    if (map_routing_calculate_distances_for_building(ROUTED_BUILDING_ROAD, x_start, y_start) &&
            place_routed_building(x_start, y_start, x_end, y_end, ROUTED_BUILDING_ROAD, &items_placed, measure_only)) {
        if (!measure_only) {
            map_property_clear_constructing_and_deleted();
            building_connectable_update_connections();
            map_tiles_update_all_roads();
            map_routing_update_land();
            window_invalidate();
        }
    }
    return items_placed;
}

int building_construction_place_highway(int measure_only, int x_start, int y_start, int x_end, int y_end)
{
    game_undo_restore_map(0);
    int start_offset = map_grid_offset(x_start, y_start);
    int end_offset = map_grid_offset(x_end, y_end);
    int forbidden_terrain_mask =
        TERRAIN_TREE | TERRAIN_ROCK | TERRAIN_WATER | TERRAIN_BUILDING |
        TERRAIN_SHRUB | TERRAIN_GARDEN | TERRAIN_ELEVATION |
        TERRAIN_RUBBLE | TERRAIN_ACCESS_RAMP;
    if (map_terrain_is(start_offset, forbidden_terrain_mask)) {
        return 0;
    }
    if (map_terrain_is(end_offset, forbidden_terrain_mask)) {
        return 0;
    }

    int items_placed = 0;
    if (map_routing_calculate_distances_for_building(ROUTED_BUILDING_HIGHWAY, x_start, y_start) &&
        place_routed_building(x_start, y_start, x_end, y_end, ROUTED_BUILDING_HIGHWAY, &items_placed, measure_only)) {
        map_tiles_update_all_plazas();
        if (!measure_only) {
            map_routing_update_land();
            window_invalidate();
        }
    }
    return items_placed;
}

int building_construction_place_aqueduct(int x_start, int y_start, int x_end, int y_end, int *cost)
{
    game_undo_restore_map(0);

    int item_cost = model_get_building(BUILDING_AQUEDUCT)->cost;
    *cost = 0;
    int blocked = 0;
    int grid_offset = map_grid_offset(x_start, y_start);
    if (map_terrain_is(grid_offset, TERRAIN_ROAD)) {
        if (map_property_is_plaza_earthquake_or_overgrown_garden(grid_offset)) {
            blocked = 1;
        }
        if (map_terrain_count_directly_adjacent_with_types(grid_offset, TERRAIN_ROAD | TERRAIN_AQUEDUCT)) {
            blocked = 1;
        }
    } else if (map_terrain_is(grid_offset, TERRAIN_NOT_CLEAR) && !map_terrain_is(grid_offset, TERRAIN_HIGHWAY)) {
        blocked = 1;
    }
    grid_offset = map_grid_offset(x_end, y_end);
    if (map_terrain_is(grid_offset, TERRAIN_ROAD)) {
        if (map_property_is_plaza_earthquake_or_overgrown_garden(grid_offset)) {
            blocked = 1;
        }
        if (map_terrain_count_directly_adjacent_with_types(grid_offset, TERRAIN_ROAD | TERRAIN_AQUEDUCT)) {
            blocked = 1;
        }

    } else if (map_terrain_is(grid_offset, TERRAIN_NOT_CLEAR) && !map_terrain_is(grid_offset, TERRAIN_HIGHWAY)) {
        blocked = 1;
    }
    if (blocked) {
        return 0;
    }
    if (!map_routing_calculate_distances_for_building(ROUTED_BUILDING_AQUEDUCT, x_start, y_start)) {
        return 0;
    }
    int num_items;
    place_routed_building(x_start, y_start, x_end, y_end, ROUTED_BUILDING_AQUEDUCT, &num_items, 0);
    *cost = item_cost * num_items;
    return 1;
}

int building_construction_place_aqueduct_for_reservoir(
    int measure_only, int x_start, int y_start, int x_end, int y_end, int *items)
{
    routed_building_type type = measure_only ? ROUTED_BUILDING_AQUEDUCT_WITHOUT_GRAPHIC : ROUTED_BUILDING_AQUEDUCT;
    return place_routed_building(x_start, y_start, x_end, y_end, type, items, 0);
}

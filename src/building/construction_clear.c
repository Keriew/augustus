#include "construction_clear.h"

#include "building/building.h"
#include "building/construction.h"
#include "building/monument.h"
#include "city/warning.h"
#include "core/config.h"
#include "core/string.h"
#include "figure/roamer_preview.h"
#include "figuretype/migrant.h"
#include "game/undo.h"
#include "graphics/color.h"
#include "graphics/window.h"
#include "map/aqueduct.h"
#include "map/bridge.h"
#include "map/building.h"
#include "map/building_tiles.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/routing_terrain.h"
#include "map/terrain.h"
#include "map/tiles.h"
#include "translation/translation.h"
#include "window/popup_dialog.h"

#include <string.h>

static struct {
    int x_start;
    int y_start;
    int x_end;
    int y_end;
    int bridge_confirmed;
    int fort_confirmed;
    int monument_confirmed;
    int repair_confirmed;
    int repair_cost;
    int repairable_buildings[1000];
} confirm;
static int repair_land_confirmed(int measure_only, int x_start, int y_start, int x_end, int y_end, int *buildings_count);
static building *get_deletable_building(int grid_offset)
{
    int building_id = map_building_at(grid_offset);
    if (!building_id) {
        return 0;
    }
    building *b = building_main(building_get(building_id));
    if (b->type == BUILDING_BURNING_RUIN || b->type == BUILDING_NATIVE_CROPS ||
        b->type == BUILDING_NATIVE_HUT || b->type == BUILDING_NATIVE_HUT_ALT ||
        b->type == BUILDING_NATIVE_MEETING || b->type == BUILDING_NATIVE_MONUMENT ||
        b->type == BUILDING_NATIVE_DECORATION || b->type == BUILDING_NATIVE_WATCHTOWER) {
        return 0;
    }
    if (b->state == BUILDING_STATE_DELETED_BY_PLAYER || b->is_deleted) {
        return 0;
    }
    return b;
}

static int clear_land_confirmed(int measure_only, int x_start, int y_start, int x_end, int y_end)
{
    int items_placed = 0;
    game_undo_restore_building_state();
    game_undo_restore_map(0);
    int x_min, x_max, y_min, y_max;
    map_grid_start_end_to_area(x_start, y_start, x_end, y_end, &x_min, &y_min, &x_max, &y_max);

    int visual_feedback_on_delete = 1;
    int highways_removed = 0;

    for (int y = y_min; y <= y_max; y++) {
        for (int x = x_min; x <= x_max; x++) {
            int grid_offset = map_grid_offset(x, y);
            if (measure_only && visual_feedback_on_delete) {
                building *b = get_deletable_building(grid_offset);
                if (map_property_is_deleted(grid_offset) || (b && map_property_is_deleted(b->grid_offset))) {
                    continue;
                }
                map_building_tiles_mark_deleting(grid_offset);
                if (map_terrain_is(grid_offset, TERRAIN_BUILDING)) {
                    if (b) {
                        items_placed++;
                    }
                } else if (map_terrain_is(grid_offset, TERRAIN_ROCK | TERRAIN_ELEVATION | TERRAIN_ACCESS_RAMP)) {
                    continue;
                } else if (map_terrain_is(grid_offset, TERRAIN_WATER)) { // keep the "bridge is free" bug from C3
                    continue;
                } else if (map_terrain_is(grid_offset, TERRAIN_AQUEDUCT)) {
                    items_placed++;
                } else if (map_terrain_is(grid_offset, TERRAIN_HIGHWAY)) {
                    int next_highways_removed = map_tiles_clear_highway(grid_offset, measure_only);
                    highways_removed += next_highways_removed;
                    items_placed += next_highways_removed;
                } else if (map_terrain_is(grid_offset, TERRAIN_NOT_CLEAR)) {
                    items_placed++;
                }
                continue;
            }
            if (map_terrain_is(grid_offset, TERRAIN_ROCK | TERRAIN_ELEVATION | TERRAIN_ACCESS_RAMP)) {
                continue;
            }
            if (map_terrain_is(grid_offset, TERRAIN_BUILDING) && !map_is_bridge(grid_offset)) {
                building *b = get_deletable_building(grid_offset);
                if (!b) {
                    continue;
                }
                if (b->type == BUILDING_FORT_GROUND || building_is_fort(b->type)) {
                    if (!measure_only && confirm.fort_confirmed != 1) {
                        continue;
                    }
                    if (!measure_only && confirm.fort_confirmed == 1) {
                        game_undo_disable();
                    }
                }
                if (building_monument_is_monument(b)) {
                    if (!measure_only && confirm.monument_confirmed != 1) {
                        continue;
                    }
                    if (!measure_only && confirm.monument_confirmed == 1) {
                        game_undo_disable();
                    }
                }
                if (b->house_size && b->house_population && !measure_only) {
                    figure *homeless = figure_create_homeless(b, b->house_population);
                    b->house_population = 0;
                    b->figure_id = homeless->id;
                }
                if (b->state != BUILDING_STATE_DELETED_BY_PLAYER) {
                    if (b->type == BUILDING_SHIPYARD && b->figure_id) {
                        figure *f = figure_get(b->figure_id);
                        f->state = FIGURE_STATE_DEAD;
                    }
                    items_placed++;
                    game_undo_add_building(b);
                }
                b->state = BUILDING_STATE_DELETED_BY_PLAYER;
                b->is_deleted = 1;
                building *space = b;
                for (int i = 0; i < 9; i++) {
                    if (space->prev_part_building_id <= 0) {
                        break;
                    }
                    space = building_get(space->prev_part_building_id);
                    game_undo_add_building(space);
                    space->state = BUILDING_STATE_DELETED_BY_PLAYER;
                }
                space = b;
                for (int i = 0; i < 9; i++) {
                    space = building_next(space);
                    if (space->id <= 0) {
                        break;
                    }
                    game_undo_add_building(space);
                    space->state = BUILDING_STATE_DELETED_BY_PLAYER;
                }
            } else if (map_terrain_is(grid_offset, TERRAIN_AQUEDUCT)) {
                map_terrain_remove(grid_offset, TERRAIN_CLEARABLE & ~TERRAIN_HIGHWAY);
                items_placed++;
                map_aqueduct_remove(grid_offset);
            } else if (map_terrain_is(grid_offset, TERRAIN_WATER)) { //only bridges fall here
                if (!measure_only && map_bridge_count_figures(grid_offset) > 0) {
                    city_warning_show(WARNING_PEOPLE_ON_BRIDGE, NEW_WARNING_SLOT);
                } else if (confirm.bridge_confirmed == 1) {
                    map_bridge_remove(grid_offset, measure_only);
                    items_placed++;
                }
            } else if (map_terrain_is(grid_offset, TERRAIN_HIGHWAY)) {
                int next_highways_removed = map_tiles_clear_highway(grid_offset, measure_only);
                highways_removed += next_highways_removed;
                items_placed += next_highways_removed;
            } else if (map_terrain_is(grid_offset, TERRAIN_NOT_CLEAR)) {
                if (map_terrain_is(grid_offset, TERRAIN_ROAD | TERRAIN_GARDEN)) {
                    map_property_clear_plaza_earthquake_or_overgrown_garden(grid_offset);
                }
                if (map_terrain_is(grid_offset, TERRAIN_RUBBLE) && !measure_only) {
                    //rubble state handling:

                    if (map_building_rubble_building_id(grid_offset)) {
                        int rubble_id = map_building_rubble_building_id(grid_offset);
                        building *rubble_building = building_get(map_building_rubble_building_id(grid_offset));
                        if (rubble_id) {
                            if (rubble_building->state == BUILDING_STATE_RUBBLE ||
                                    rubble_building->type == BUILDING_BURNING_RUIN) {
                                int ruins_left = map_building_ruins_left(rubble_id);
                                if (!ruins_left) { //dont remove buildings until their last rubble is gone
                                    rubble_building->state = BUILDING_STATE_DELETED_BY_GAME;
                                }
                            } else if (rubble_building->state == BUILDING_STATE_UNUSED) {
                                // intentional fallthrough - unused buildings are corrupt if they exist on the grid. 
                                // dont change state, just remove reference on the grid - addressed after if {} block 
                            } else {
                                rubble_building->state = BUILDING_STATE_DELETED_BY_GAME;
                            }
                        }
                        map_building_set_rubble_grid_building_id(grid_offset, 0, 1); // remove rubble marker
                    }
                }
                map_terrain_remove(grid_offset, TERRAIN_CLEARABLE);
                items_placed++;
            }
        }
    }
    if (!measure_only || !visual_feedback_on_delete) {
        int radius;
        if (x_max - x_min <= y_max - y_min) {
            radius = y_max - y_min + 3;
        } else {
            radius = x_max - x_min + 3;
        }
        if (highways_removed) {
            x_min -= 1;
            y_min -= 1;
            x_max += 1;
            y_max += 1;
        }
        map_tiles_update_region_empty_land(x_min, y_min, x_max, y_max);
        map_tiles_update_region_meadow(x_min, y_min, x_max, y_max);
        map_tiles_update_region_rubble(x_min, y_min, x_max, y_max);
        map_tiles_update_all_gardens();
        map_tiles_update_area_roads(x_min, y_min, radius);
        map_tiles_update_area_highways(x_min - 1, y_min - 1, radius);
        map_tiles_update_all_plazas();
        map_tiles_update_area_walls(x_min, y_min, radius);
        map_tiles_update_region_aqueducts(x_min - 3, y_min - 3, x_max + 3, y_max + 3);
    }
    if (!measure_only) {
        map_routing_update_land();
        map_routing_update_walls();
        map_routing_update_water();
        building_update_state();
        figure_roamer_preview_reset(BUILDING_CLEAR_LAND);
        window_invalidate();
    }
    return items_placed;
}

static void confirm_delete_fort(int accepted, int checked)
{
    if (accepted == 1) {
        confirm.fort_confirmed = 1;
    } else {
        confirm.fort_confirmed = -1;
    }
    clear_land_confirmed(0, confirm.x_start, confirm.y_start, confirm.x_end, confirm.y_end);
}

static void confirm_delete_bridge(int accepted, int checked)
{
    if (accepted == 1) {
        confirm.bridge_confirmed = 1;
    } else {
        confirm.bridge_confirmed = -1;
    }
    clear_land_confirmed(0, confirm.x_start, confirm.y_start, confirm.x_end, confirm.y_end);
}

static void confirm_delete_monument(int accepted, int checked)
{
    if (accepted == 1) {
        confirm.monument_confirmed = 1;
    } else {
        confirm.monument_confirmed = -1;
    }
    clear_land_confirmed(0, confirm.x_start, confirm.y_start, confirm.x_end, confirm.y_end);
}

static void confirm_repair_buildings(int accepted, int checked)
{
    if (accepted == 1) {
        confirm.repair_confirmed = 1;
    } else {
        confirm.repair_confirmed = -1;
    }
    if (accepted == 1) {
        repair_land_confirmed(0, confirm.x_start, confirm.y_start, confirm.x_end, confirm.y_end, 0);
    }
}

int building_construction_clear_land(int measure_only, int x_start, int y_start, int x_end, int y_end)
{
    confirm.fort_confirmed = 0;
    confirm.bridge_confirmed = 0;
    confirm.monument_confirmed = 0;
    confirm.repair_confirmed = 0;
    if (measure_only) {
        return clear_land_confirmed(measure_only, x_start, y_start, x_end, y_end);
    }

    int x_min, x_max, y_min, y_max;
    map_grid_start_end_to_area(x_start, y_start, x_end, y_end, &x_min, &y_min, &x_max, &y_max);

    int ask_confirm_bridge = 0;
    int ask_confirm_fort = 0;
    int ask_confirm_monument = 0;
    for (int y = y_min; y <= y_max; y++) {
        for (int x = x_min; x <= x_max; x++) {
            int grid_offset = map_grid_offset(x, y);
            int building_id = map_building_at(grid_offset);
            if (building_id) {
                building *b = building_get(building_id);
                if (building_is_fort(b->type) || b->type == BUILDING_FORT_GROUND) {
                    ask_confirm_fort = 1;
                }
                if (building_monument_is_monument(b)) {
                    if (building_monument_type_is_mini_monument(b->type)) {
                        confirm.monument_confirmed = 1;
                    } else {
                        ask_confirm_monument = 1;
                    }
                }
            }
            if (map_is_bridge(grid_offset)) {
                ask_confirm_bridge = 1;
            }

        }
    }
    confirm.x_start = x_start;
    confirm.y_start = y_start;
    confirm.x_end = x_end;
    confirm.y_end = y_end;
    if (ask_confirm_fort) {
        window_popup_dialog_show(POPUP_DIALOG_DELETE_FORT, confirm_delete_fort, 2);
        return -1;
    } else if (ask_confirm_monument) {
        window_popup_dialog_show_confirmation(translation_for(TR_CONFIRM_DELETE_MONUMENT), 0, 0, confirm_delete_monument);
        return -1;
    } else if (ask_confirm_bridge) {
        window_popup_dialog_show(POPUP_DIALOG_DELETE_BRIDGE, confirm_delete_bridge, 2);
        return -1;
    } else {
        return clear_land_confirmed(measure_only, x_start, y_start, x_end, y_end);
    }
}

color_t building_construction_clear_color(void)
{
    if (building_construction_type() == BUILDING_CLEAR_LAND) {
        return COLOR_MASK_RED;
    } else if (building_construction_type() == BUILDING_REPAIR_LAND) {
        return COLOR_MASK_GREEN;
    }
    return COLOR_MASK_NONE;
}

static int was_building_counted(int building_id, int count_of_processed)
{
    for (int i = 0; i < count_of_processed; i++) {
        if (confirm.repairable_buildings[i] == building_id) {
            return 1;
        }
    }
    return 0;
}

static int repair_land_confirmed(int measure_only, int x_start, int y_start, int x_end, int y_end, int *buildings_count)
{
    grid_slice *slice = map_grid_get_grid_slice_from_corners(x_start, y_start, x_end, y_end);
    int repairable_buildings = 0;
    int repair_cost = 0;
    // Measure phase - count buildings and calculate cost
    for (int i = 0; i < slice->size; i++) {
        int grid_offset = slice->grid_offsets[i];
        if (measure_only) {
            map_property_mark_deleted(grid_offset);
        }
        int building_id = map_building_rubble_building_id(grid_offset);
        if (building_id) {
            building *b = building_get(building_id);
            if (building_can_repair(b)) {
                if (b->type == BUILDING_WAREHOUSE_SPACE) { // swap the b pointer for the main warehouse building
                    b = building_get(map_building_rubble_building_id(b->data.rubble.og_grid_offset));
                }
                if (!was_building_counted(b->id, repairable_buildings)) {
                    if (measure_only) {
                        repair_cost += building_repair_cost(b);
                    } else {
                        repair_cost += building_repair(b);// Actually perform the repair
                    }
                    confirm.repairable_buildings[repairable_buildings] = b->id;
                    repairable_buildings++;
                }
            } else {
                if (building_monument_is_limited(b->type)) {
                    city_warning_show(WARNING_REPAIR_MONUMENT, NEW_WARNING_SLOT);
                } else if (b->type == BUILDING_AQUEDUCT) {
                    city_warning_show(WARNING_REPAIR_AQUEDUCT, NEW_WARNING_SLOT);
                } else {
                    city_warning_show(WARNING_REPAIR_IMPOSSIBLE, NEW_WARNING_SLOT);
                }
            }
        }
    }
    if (buildings_count) {
        *buildings_count = repairable_buildings;
    }
    return repair_cost;
}

int building_construction_repair_land(int measure_only, int x_start, int y_start, int x_end, int y_end, int *buildings_count)
{
    confirm.repair_confirmed = 0;
    memset(confirm.repairable_buildings, 0, sizeof(confirm.repairable_buildings)); // reset the array
    if (measure_only) {
        return repair_land_confirmed(measure_only, x_start, y_start, x_end, y_end, buildings_count);
    }

    int repairable_buildings = 0;    // First, measure to see if there are any repairable buildings and get the cost
    int repair_cost = repair_land_confirmed(1, x_start, y_start, x_end, y_end, &repairable_buildings);

    if (repairable_buildings > 0) {        // Store the coordinates and cost for the confirmation callback
        confirm.x_start = x_start;
        confirm.y_start = y_start;
        confirm.x_end = x_end;
        confirm.y_end = y_end;
        confirm.repair_cost = repair_cost;

        static uint8_t big_buffer[120];
        memset(big_buffer, 0, sizeof(big_buffer)); // Clear buffer
        const uint8_t *custom_text = translation_for(TR_CONFIRM_REPAIR_BUILDINGS_TITLE);

        int offset = 0;
        const uint8_t *prefix = translation_for(TR_CONFIRM_REPAIR_BUILDINGS);
        string_copy(prefix, &big_buffer[offset], sizeof(big_buffer) - offset);
        offset += string_length(prefix);
        big_buffer[offset++] = ' ';

        offset += string_from_int(&big_buffer[offset], repair_cost, 0);
        big_buffer[offset++] = ' ';

        const uint8_t *currency = lang_get_string(6, 0);
        string_copy(currency, &big_buffer[offset], sizeof(big_buffer) - offset);
        offset += string_length(currency);

        big_buffer[offset++] = '?';
        big_buffer[offset] = '\0';
        const uint8_t *pointer = big_buffer;

        window_popup_dialog_show_confirmation(custom_text, pointer, 0, confirm_repair_buildings);
        return repair_cost;
    } else {
        return 0;// No buildings to repair, return 0 cost
    }
}
#include "building.h"

#include "building/clone.h"
#include "building/construction.h"
#include "building/construction_building.h"
#include "building/construction_clear.h"
#include "building/data_transfer.h"
#include "building/destruction.h"
#include "building/distribution.h"
#include "building/industry.h"
#include "building/granary.h"
#include "building/menu.h"
#include "building/model.h"
#include "building/monument.h"
#include "building/properties.h"
#include "building/rotation.h"
#include "building/state.h"
#include "building/storage.h"
#include "building/variant.h"
#include "building/type.h"
#include "city/buildings.h"
#include "city/finance.h"
#include "city/population.h"
#include "city/warning.h"
#include "core/array.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/log.h"
#include "figure/figure.h"
#include "figure/formation_legion.h"
#include "figuretype/missile.h"
#include "game/difficulty.h"
#include "game/save_version.h"
#include "game/undo.h"
#include "map/building.h"
#include "map/building_tiles.h"
#include "map/bridge.h"
#include "map/desirability.h"
#include "map/elevation.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/random.h"
#include "map/routing_terrain.h"
#include "map/terrain.h"
#include "map/tiles.h"

#define BUILDING_ARRAY_SIZE_STEP 2000

#define WATER_DESIRABILITY_RANGE 3
#define WATER_DESIRABILITY_BONUS 15

static struct {
    array(building) buildings;
    building *first_of_type[BUILDING_TYPE_MAX];
    building *last_of_type[BUILDING_TYPE_MAX];
} data;

static struct {
    int created_sequence;
    int incorrect_houses;
    int unfixable_houses;
} extra;

building *building_get(int id)
{
    return array_item(data.buildings, id);
}
int building_can_repair_type(building_type type)
{
    if (building_monument_is_limited(type) || type == BUILDING_AQUEDUCT || building_is_fort(type)) {
        return 0; // limited monuments and aqueducts cannot be repaired at the moment, aqueducts require a rework,
    }   //and limited monuments are too complex to easily repair, and arent a common occurrence
    // forts have the complexity of holding formations, so are also currently excluded
    building_type repair_type = building_clone_type_from_building_type(type);
    if (repair_type == BUILDING_NONE) {
        return 0;
    } else {
        return 1;
    }
}
int building_dist(int x, int y, int w, int h, building *b)
{
    int size = building_properties_for_type(b->type)->size;
    int dist = calc_box_distance(x, y, w, h, b->x, b->y, size, size);
    return dist;
}

void building_get_from_buffer(buffer *buf, int id, building *b, int includes_building_size, int save_version,
    int buffer_offset)
{
    buffer_set(buf, 0);
    int building_buf_size = BUILDING_STATE_ORIGINAL_BUFFER_SIZE;
    int buf_skip = 0;

    if (includes_building_size) {
        building_buf_size = buffer_read_i32(buf);
        buf_skip = 4;
    }
    buf_skip += buffer_offset;
    buffer_set(buf, building_buf_size * id + buf_skip);
    building_state_load_from_buffer(buf, b, building_buf_size, save_version, 1);
}

int building_count(void)
{
    return data.buildings.size;
}

int building_find(building_type type)
{
    for (building *b = data.first_of_type[type]; b; b = b->next_of_type) {
        if (b->state == BUILDING_STATE_IN_USE) {
            return b->id;
        }
    }
    return 0;
}

building *building_first_of_type(building_type type)
{
    return data.first_of_type[type];
}

building *building_main(building *b)
{
    for (int guard = 0; guard < 9; guard++) {
        if (b->prev_part_building_id <= 0) {
            return b;
        }
        b = array_item(data.buildings, b->prev_part_building_id);
    }
    return array_first(data.buildings);
}

building *building_next(building *b)
{
    return array_item(data.buildings, b->next_part_building_id);
}

static void fill_adjacent_types(building *b)
{
    building *first = data.first_of_type[b->type];
    building *last = data.last_of_type[b->type];
    if (!first || !last) {
        b->prev_of_type = 0;
        b->next_of_type = 0;
        data.first_of_type[b->type] = b;
        data.last_of_type[b->type] = b;
    } else if (b->id < first->id) {
        first->prev_of_type = b;
        b->next_of_type = first;
        b->prev_of_type = 0;
        data.first_of_type[b->type] = b;
    } else if (b->id > last->id) {
        last->next_of_type = b;
        b->prev_of_type = last;
        b->next_of_type = 0;
        data.last_of_type[b->type] = b;
    } else if (b != first && b != last) {
        int id = b->id - 1;
        while (id) {
            building *prev = building_get(id);
            if (prev->state != BUILDING_STATE_UNUSED && prev->type == b->type) {
                b->prev_of_type = prev;
                b->next_of_type = prev->next_of_type;
                b->next_of_type->prev_of_type = b;
                prev->next_of_type = b;
                break;
            }
            id--;
        }
    }
}

static void remove_adjacent_types(building *b)
{
    building *first = data.first_of_type[b->type];
    building *last = data.last_of_type[b->type];
    if (b == first && b == last) {
        data.first_of_type[b->type] = 0;
        data.last_of_type[b->type] = 0;
    } else if (b == first) {
        data.first_of_type[b->type] = b->next_of_type;
        if (b->next_of_type) {
            b->next_of_type->prev_of_type = 0;
        }
    } else if (b == last) {
        data.last_of_type[b->type] = b->prev_of_type;
        if (b->prev_of_type) {
            b->prev_of_type->next_of_type = 0;
        }
    } else {
        b->prev_of_type->next_of_type = b->next_of_type;
        b->next_of_type->prev_of_type = b->prev_of_type;
    }
    b->prev_of_type = 0;
    b->next_of_type = 0;
}

building *building_create(building_type type, int x, int y)
{
    building *b;
    array_new_item_after_index(data.buildings, 1, b);
    if (!b) {
        city_warning_show(WARNING_DATA_LIMIT_REACHED, NEW_WARNING_SLOT);
        return array_first(data.buildings);
    }

    const building_properties *props = building_properties_for_type(type);

    b->state = BUILDING_STATE_CREATED;
    b->faction_id = 1;
    b->type = type;
    b->size = props->size;
    b->created_sequence = extra.created_sequence++;
    b->sentiment.house_happiness = 100;

    fill_adjacent_types(b);

    // house size
    if (type >= BUILDING_HOUSE_SMALL_TENT && type <= BUILDING_HOUSE_MEDIUM_INSULA) {
        b->house_size = 1;
    } else if (type >= BUILDING_HOUSE_LARGE_INSULA && type <= BUILDING_HOUSE_MEDIUM_VILLA) {
        b->house_size = 2;
    } else if (type >= BUILDING_HOUSE_LARGE_VILLA && type <= BUILDING_HOUSE_MEDIUM_PALACE) {
        b->house_size = 3;
    } else if (type >= BUILDING_HOUSE_LARGE_PALACE && type <= BUILDING_HOUSE_LUXURY_PALACE) {
        b->house_size = 4;
    }

    // subtype
    if (building_is_house(type)) {
        b->subtype.house_level = type - BUILDING_HOUSE_VACANT_LOT;
    }

    b->output_resource_id = resource_get_from_industry(type);

    if (type == BUILDING_GRANARY) {
        b->resources[RESOURCE_NONE] = FULL_GRANARY;
    }

    // Set it as accepting all available goods
    for (resource_type r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        b->accepted_goods[r] = building_distribution_resource_is_handled(r, type);
    }

    // Exception for Venus temples which should never accept wine by default to prevent unwanted evolutions
    if (type == BUILDING_SMALL_TEMPLE_VENUS || type == BUILDING_LARGE_TEMPLE_VENUS) {
        b->accepted_goods[RESOURCE_WINE] = 0;
    }

    if (type == BUILDING_WAREHOUSE || type == BUILDING_HIPPODROME) {
        b->subtype.orientation = building_rotation_get_rotation();
    }

    // Most roadblock-like buildings should allow everything by default
    if (building_type_is_roadblock(b->type) && b->type != BUILDING_ROADBLOCK && b->type != BUILDING_GATEHOUSE &&
        b->type != BUILDING_PALISADE_GATE && config_get(CONFIG_GP_CH_GATES_DEFAULT_TO_PASS_ALL_WALKERS)) {
        b->data.roadblock.exceptions = ROADBLOCK_PERMISSION_ALL;
    }
    if (building_type_is_bridge(b->type) || b->type == BUILDING_GRANARY || b->type == BUILDING_WAREHOUSE) {
        //bridges and other passable buildings should allow all walkers by default
        b->data.roadblock.exceptions = ROADBLOCK_PERMISSION_ALL;
    }

    if (b->type == BUILDING_MARKET && config_get(CONFIG_GP_CH_MARKETS_DONT_ACCEPT)) {
        building_distribution_unaccept_all_goods(b);
    } else if (b->type == BUILDING_MARKET && !config_get(CONFIG_GP_CH_MARKETS_DONT_ACCEPT)) {
        building_distribution_accept_all_goods(b);
    }

    b->x = x;
    b->y = y;
    b->grid_offset = map_grid_offset(x, y);
    b->house_figure_generation_delay = map_random_get(b->grid_offset) & 0x7f;
    b->figure_roam_direction = b->house_figure_generation_delay & 6;
    b->fire_proof = props->fire_proof;
    b->is_close_to_water = building_is_close_to_water(b);

    return b;
}

void building_change_type(building *b, building_type type)
{
    if (b->type == type) {
        return;
    }
    remove_adjacent_types(b);
    b->type = type;
    fill_adjacent_types(b);
}

static void building_delete(building *b)
{
    building_clear_related_data(b);
    remove_adjacent_types(b);
    int id = b->id;
    memset(b, 0, sizeof(building));
    b->id = id;

    array_trim(data.buildings);
}

void building_clear_related_data(building *b)
{
    if (b->storage_id) {
        building_storage_delete(b->storage_id);
        b->storage_id = 0;
    }
    if (building_is_fort(b->type)) {
        formation_legion_delete_for_fort(b);
    }
    if (b->type == BUILDING_TRIUMPHAL_ARCH) {
        city_buildings_remove_triumphal_arch();
        building_menu_update();
    }
    if (building_monument_is_unfinished_monument(b)) {
        building_monument_remove_all_deliveries(b->id);
    }
}

building *building_restore_from_undo(building *to_restore)
{
    building *b = array_item(data.buildings, to_restore->id);
    memcpy(b, to_restore, sizeof(building));
    if (b->id >= data.buildings.size) {
        data.buildings.size = b->id + 1;
    }
    fill_adjacent_types(b);
    return b;
}

void building_trim(void)
{
    array_trim(data.buildings);
}

int building_was_tent(const building *b)
{
    return b->data.rubble.og_type == BUILDING_HOUSE_LARGE_TENT || b->data.rubble.og_type == BUILDING_HOUSE_SMALL_TENT;
}

int building_is_storage(building_type b_type)
{
    return b_type == BUILDING_GRANARY || b_type == BUILDING_WAREHOUSE;
}

int building_is_still_burning(building *b)
{
    int hot = (b->type == BUILDING_BURNING_RUIN);
    int grid_offset = hot ? b->data.rubble.og_grid_offset : b->grid_offset;
    int size = hot ? b->data.rubble.og_size : b->size;
    grid_slice *b_area = map_grid_get_grid_slice_square(grid_offset, size);
    for (int i = 0; i < b_area->size; i++) {
        int offset = b_area->grid_offsets[i];
        if (map_has_figure_at(offset)) {  // also check for prefects on the tile - their presence prevents rebuilding
            return 1;
        }
        if (building_get(map_building_at(offset))->type == BUILDING_BURNING_RUIN) {
            if (building_get(map_building_at(offset))->state == BUILDING_STATE_RUBBLE) {
                continue; // extinguished tile
            }
            return 1;
        }
    }
    return 0;
}

int building_can_repair(building *b)
{
    if (!b) {
        return 0;
    }
    if (b->type == BUILDING_BURNING_RUIN) {
        if (building_is_still_burning(b)) {
            return 0;
        }
        if (!building_can_repair_type(b->data.rubble.og_type)) {
            return 0;
        } else {
            return 1;
        }
    } else {
        if (b->state != BUILDING_STATE_RUBBLE) {
            return 0;
        } else {
            return building_can_repair_type(b->type);
        }
    }
}

int building_repair_cost(building *b)
{
    int og_grid_offset = 0, og_size = 0, og_type = 0;
    if (!b || !building_can_repair(b)) {
        return 0;
    }
    int is_ruin = b->type == BUILDING_BURNING_RUIN || // ruins and collapsed warehouse parts all use rubble data 
        b->type == BUILDING_WAREHOUSE_SPACE || b->type == BUILDING_WAREHOUSE;

    og_grid_offset = is_ruin ? b->data.rubble.og_grid_offset : b->grid_offset;
    og_size = is_ruin ? b->data.rubble.og_size : b->size;
    og_type = is_ruin ? b->data.rubble.og_type : b->type;

    if (building_is_house(og_type)) {
        grid_slice *house_slice = map_grid_get_grid_slice_house(b->id, 1);
        int clear_cost = house_slice->size * (11 + 3); // 10.5 per new house tile + 3 per rubble tile to clear
        return clear_cost;
    }
    if (b->type == BUILDING_WAREHOUSE_SPACE) {
        og_size = 1; // dont charge for clearing the whole warehouse, just the collapsed part, otherwise its *9
    }
    grid_slice *grid_slice = map_grid_get_grid_slice_square(og_grid_offset, og_size); // wont work correctly for hippo
    int clear_cost = building_construction_prepare_terrain(grid_slice, CLEAR_MODE_RUBBLE, COST_MEASURE);
    int placement_cost = model_get_building(og_type)->cost;
    if (og_type == BUILDING_WAREHOUSE && b->type == BUILDING_WAREHOUSE_SPACE) {
        placement_cost = 0; // collapsed warehouse parts only need clearing cost, no placement cost
    }
    return clear_cost + placement_cost + placement_cost / 20; // +5% fee on a building price
}

int building_repair(building *b)
{
    if (!b) {
        return 0;
    }
    if (b->type == BUILDING_BURNING_RUIN && building_is_still_burning(b)) {
        city_warning_show(WARNING_REPAIR_BURNING, NEW_WARNING_SLOT);
        return 0;
    }
    if (!building_can_repair_type(b->type) && !building_can_repair_type(b->data.rubble.og_type)) {
        if (building_monument_is_limited(b->type) || building_monument_is_limited(b->data.rubble.og_type)) {
            city_warning_show(WARNING_REPAIR_MONUMENT, NEW_WARNING_SLOT);
        } else if (b->type == BUILDING_AQUEDUCT || b->data.rubble.og_type == BUILDING_AQUEDUCT) {
            city_warning_show(WARNING_REPAIR_AQUEDUCT, NEW_WARNING_SLOT);
        } else {
            city_warning_show(WARNING_REPAIR_IMPOSSIBLE, NEW_WARNING_SLOT);
        }
        return 0;
    }
    // flags and placeholders
    int og_size = 0, og_grid_offset = 0, og_orientation = 0, og_storage_id, wall = 0, is_house_lot = 0, success = 0;
    building_type og_type = BUILDING_NONE;

    if (b->type == BUILDING_WAREHOUSE_SPACE) { // collapsed warehouse parts use the main warehouse data
        building *main_warehouse = building_get(map_building_rubble_building_id(b->data.rubble.og_grid_offset));
        if (main_warehouse) {
            b = main_warehouse;
        }
    }
    // TODO: wrap warehouse and granary repairs into a separate helper, to better control the storage transfer
    // --- Handle rubble recovery ---
    if (b->data.rubble.og_size || b->data.rubble.og_grid_offset ||        // if there's rubble data, take it from there
        b->data.rubble.og_orientation || b->data.rubble.og_type) {
        og_size = b->data.rubble.og_size;
        og_grid_offset = b->data.rubble.og_grid_offset;
        og_orientation = b->data.rubble.og_orientation;
        og_type = b->data.rubble.og_type;
    }
    // --- Special handling for warehouse coordinates ---
    // For warehouses, og_grid_offset points to the tower (entrance) corner, not the top-left
    // We need to convert it to the top-left corner based on the orientation
    if (og_type == BUILDING_WAREHOUSE && og_grid_offset && og_orientation >= 0) {
        // Warehouse tower offset positions based on orientation (matches construction_building.c)
        int x_offset[9] = { 0, 0, 1, 1, 0, 2, 1, 2, 2 };
        int y_offset[9] = { 0, 1, 0, 1, 2, 0, 2, 1, 2 };
        int corner = building_rotation_get_corner(2 * og_orientation);

        // Convert from tower position to top-left corner
        int tower_x = map_grid_offset_to_x(og_grid_offset);
        int tower_y = map_grid_offset_to_y(og_grid_offset);
        int top_left_x = tower_x - x_offset[corner];
        int top_left_y = tower_y - y_offset[corner];
        og_grid_offset = map_grid_offset(top_left_x, top_left_y);
    }

    building_data_transfer_backup();
    building_data_transfer_copy(b, 1);
    //  Resolve placement data 
    int grid_offset = og_grid_offset ? og_grid_offset : b->grid_offset;
    int x = map_grid_offset_to_x(grid_offset);
    int y = map_grid_offset_to_y(grid_offset);
    int size = og_size ? og_size : b->size;
    int type = og_type ? og_type : b->type;
    size = (og_type == BUILDING_WAREHOUSE) ? 3 : size;
    building_type type_to_place = og_type ? og_type : b->type;

    if (building_is_house(type) || type == 1) {
        is_house_lot = 1;
        building_change_type(b, BUILDING_HOUSE_VACANT_LOT);
    }
    int placement_cost = 0;
    og_storage_id = b->storage_id; //store the original storage id before clearing it
    // --- Clear terrain & place building ---
    grid_slice *grid_slice = map_grid_get_grid_slice_square(grid_offset, size);
    if (building_construction_nearby_enemy_type(grid_slice) != FIGURE_NONE) {
        city_warning_show(WARNING_ENEMY_NEARBY, NEW_WARNING_SLOT);
        building_data_transfer_restore_and_clear_backup();
        return 0;
    }
    map_terrain_backup(); // backup the terrain in case of failure
    int cleared = building_construction_prepare_terrain(grid_slice, CLEAR_MODE_RUBBLE, COST_PROCESS);
    if (is_house_lot) {
        success = building_construction_fill_vacant_lots(grid_slice);
    } else if (type_to_place == BUILDING_WALL || type_to_place == BUILDING_TOWER) {
        wall = 1;
        for (int i = 0; i < grid_slice->size; i++) {

            success = building_construction_place_wall(grid_slice->grid_offsets[i]);
            placement_cost += model_get_building(BUILDING_WALL)->cost * success; // TODO: confirm if wall cost is stored in model
        }
        if (type_to_place == BUILDING_TOWER) {
            map_tiles_update_all_walls(); // towers affect wall connections
            success = building_construction_place_building(type_to_place, x, y, 1);
        }
    } else {
        if (type_to_place == BUILDING_GATEHOUSE) {
            wall = 1;
        }
        success = building_construction_place_building(type_to_place, x, y, 1);
    }
    building *new_building = building_get(map_building_at(map_grid_offset(x, y)));
    if (!success || !cleared) {
        map_terrain_restore(); // restore terrain on failure
        city_finance_process_construction(-cleared); // refund clearing cost
        city_warning_show(WARNING_REPAIR_IMPOSSIBLE, NEW_WARNING_SLOT);
        return 0;
    }
    if (building_is_storage(type_to_place) && b->storage_id) {
        if (new_building->storage_id != og_storage_id) {
            /*int storage_restore = */building_storage_change_building(b->storage_id, new_building->id);
            // TODO: if storage_restore, refresh orders of cart depots. Requires merge of new depot code
            b->storage_id = 0; // remove reference to the storage we just deleted
        }
    }
    placement_cost += model_get_building(type_to_place)->cost * success;
    int full_cost = (placement_cost + placement_cost / 20);// +5%

    city_finance_process_construction(full_cost);
    new_building->subtype.orientation = og_orientation;
    map_building_set_rubble_grid_building_id(grid_offset, 0, size); // remove rubble marker
    building_data_transfer_paste(new_building, 1);
    if (new_building->state == BUILDING_STATE_RUBBLE) {
        new_building->state = BUILDING_STATE_CREATED;
    }
    building_data_transfer_restore_and_clear_backup();
    figure_create_explosion_cloud(new_building->x, new_building->y, og_size, 1);
    if (wall) {
        map_tiles_update_all_walls(); // towers affect wall connections
    }
    b->state = BUILDING_STATE_DELETED_BY_GAME; // mark old building as deleted
    game_undo_disable(); // not accounting for undoing repairs
    return full_cost;
}

void building_update_state(void)
{
    int land_recalc = 0;
    int wall_recalc = 0;
    int road_recalc = 0;
    int aqueduct_recalc = 0;
    building *b;
    array_foreach(data.buildings, b)
    {
        if (b->state == BUILDING_STATE_CREATED) {
            b->state = BUILDING_STATE_IN_USE;
        }
        if (b->state == BUILDING_STATE_IN_USE && b->house_size) {
            continue;
        }
        if (b->state == BUILDING_STATE_UNDO || b->state == BUILDING_STATE_DELETED_BY_PLAYER) {
            if (b->type == BUILDING_TOWER || b->type == BUILDING_GATEHOUSE) {
                wall_recalc = 1;
                road_recalc = 1;
            } else if (b->type == BUILDING_RESERVOIR) {
                aqueduct_recalc = 1;
            } else if (b->type == BUILDING_GRANARY || building_type_is_bridge(b->type)) {
                road_recalc = 1;
            } else if ((b->type >= BUILDING_GRAND_TEMPLE_CERES && b->type <= BUILDING_GRAND_TEMPLE_VENUS) ||
                b->type == BUILDING_PANTHEON || b->type == BUILDING_LIGHTHOUSE) {
                road_recalc = 1;
            }
            map_building_tiles_remove(b->id, b->x, b->y);
            if (building_type_is_roadblock(b->type) && b->size == 1 && !building_type_is_bridge(b->type)) {
                // Leave the road behind the deleted roadblock
                // except for bridges - they are coded as size 1 too
                map_terrain_add(b->grid_offset, TERRAIN_ROAD);
                road_recalc = 1;
            }
            land_recalc = 1;
            building_delete(b);
        } else if (b->state == BUILDING_STATE_RUBBLE) {
            if (b->house_size) {
                city_population_remove_home_removed(b->house_population);
                b->house_population = 0;
            }
            if (building_is_fort(b->type) || b->type == BUILDING_FORT_GROUND) {
                b->state = BUILDING_STATE_DELETED_BY_GAME;
                map_building_tiles_remove(b->id, b->x, b->y);
                map_building_set_rubble_grid_building_id(b->grid_offset, 0, b->size);
            }
            // building_delete(b); // keep the rubbled building as a reference for reconstruction
        } else if (b->state == BUILDING_STATE_DELETED_BY_GAME) {
            building_delete(b);
        } else if (b->immigrant_figure_id) {
            const figure *f = figure_get(b->immigrant_figure_id);
            if (f->state != FIGURE_STATE_ALIVE || (unsigned int) f->destination_building_id != array_index) {
                b->immigrant_figure_id = 0;
            }
        }
    }
    if (wall_recalc) {
        map_tiles_update_all_walls();
    }
    if (aqueduct_recalc) {
        map_tiles_update_all_aqueducts(0);
    }
    if (land_recalc) {
        map_routing_update_land();
    }
    if (road_recalc) {
        map_tiles_update_all_roads();
        map_tiles_update_all_highways();
    }
}

void building_update_desirability(void)
{
    building *b;
    array_foreach(data.buildings, b)
    {
        if (b->state != BUILDING_STATE_IN_USE) {
            continue;
        }

        // Use wider type to prevent 8-bit overflow
        int desirability = map_desirability_get_max(b->x, b->y, b->size);

        if (b->is_close_to_water) {
            desirability += 10;
        }

        switch (map_elevation_at(b->grid_offset)) {
            case 0: break;
            case 1: desirability += 10; break;
            case 2: desirability += 12; break;
            case 3: desirability += 14; break;
            case 4: desirability += 16; break;
            default: desirability += 18; break;
        }

        // Clamp before assigning to 8-bit signed int
        if (desirability > 100) {
            desirability = 100;
        } else if (desirability < -100) {
            desirability = -100;
        }

        b->desirability = (int8_t) desirability;
    }
}

int building_is_active(const building *b)
{
    if (b->state != BUILDING_STATE_IN_USE) {
        return 0;
    }
    if (building_is_house(b->type)) {
        return b->house_size > 0 && b->house_population > 0;
    }
    if (building_monument_is_unfinished_monument(b)) {
        return 0;
    }
    switch (b->type) {
        case BUILDING_RESERVOIR:
        case BUILDING_FOUNTAIN:
            return b->has_water_access;
        case BUILDING_ORACLE:
        case BUILDING_NYMPHAEUM:
        case BUILDING_SMALL_MAUSOLEUM:
        case BUILDING_LARGE_MAUSOLEUM:
            return b->monument.phase == MONUMENT_FINISHED;
        case BUILDING_WHARF:
            return b->num_workers > 0 && b->data.industry.fishing_boat_id;
        case BUILDING_DOCK:
            return b->num_workers > 0 && b->has_water_access;
        default:
            return b->num_workers > 0;
    }
}

int building_is_primary_product_producer(building_type type)
{
    return building_is_raw_resource_producer(type) || building_is_farm(type) || type == BUILDING_WHARF;
}

int building_is_house(building_type type)
{
    return type >= BUILDING_HOUSE_VACANT_LOT && type <= BUILDING_HOUSE_LUXURY_PALACE;
}

// For Venus GT base bonus
int building_is_statue_garden_temple(building_type type)
{
    const building_properties *props = building_properties_for_type(type);
    return props->venus_gt_bonus;
}

int building_is_ceres_temple(building_type type)
{
    return (type == BUILDING_SMALL_TEMPLE_CERES || type == BUILDING_LARGE_TEMPLE_CERES);
}

int building_is_neptune_temple(building_type type)
{
    return (type == BUILDING_SMALL_TEMPLE_NEPTUNE || type == BUILDING_LARGE_TEMPLE_NEPTUNE);
}

int building_is_mercury_temple(building_type type)
{
    return (type == BUILDING_SMALL_TEMPLE_MERCURY || type == BUILDING_LARGE_TEMPLE_MERCURY);
}

int building_is_mars_temple(building_type type)
{
    return (type == BUILDING_SMALL_TEMPLE_MARS || type == BUILDING_LARGE_TEMPLE_MARS);
}

int building_is_venus_temple(building_type type)
{
    return (type == BUILDING_SMALL_TEMPLE_VENUS || type == BUILDING_LARGE_TEMPLE_VENUS);
}

// All buildings capable of collecting and storing goods as a market
int building_has_supplier_inventory(building_type type)
{
    return (type == BUILDING_MARKET ||
        type == BUILDING_MESS_HALL ||
        type == BUILDING_CARAVANSERAI ||
        type == BUILDING_SMALL_TEMPLE_CERES ||
        type == BUILDING_LARGE_TEMPLE_CERES ||
        type == BUILDING_SMALL_TEMPLE_VENUS ||
        type == BUILDING_LARGE_TEMPLE_VENUS ||
        type == BUILDING_TAVERN);
}

int building_is_fort(building_type type)
{
    return type == BUILDING_FORT_LEGIONARIES ||
        type == BUILDING_FORT_JAVELIN ||
        type == BUILDING_FORT_MOUNTED ||
        type == BUILDING_FORT_AUXILIA_INFANTRY ||
        type == BUILDING_FORT_ARCHERS;
}

int building_mothball_toggle(building *b)
{
    if (b->state == BUILDING_STATE_IN_USE) {
        b->state = BUILDING_STATE_MOTHBALLED;
        b->num_workers = 0;
    } else if (b->state == BUILDING_STATE_MOTHBALLED) {
        b->state = BUILDING_STATE_IN_USE;
    }
    return b->state;
}

int building_mothball_set(building *b, int mothball)
{
    if (mothball) {
        if (b->state == BUILDING_STATE_IN_USE) {
            b->state = BUILDING_STATE_MOTHBALLED;
            b->num_workers = 0;
        }
    } else if (b->state == BUILDING_STATE_MOTHBALLED) {
        b->state = BUILDING_STATE_IN_USE;
    }
    return b->state;

}

unsigned char building_stockpiling_toggle(building *b)
{
    b->data.industry.is_stockpiling = !b->data.industry.is_stockpiling;
    return b->data.industry.is_stockpiling;
}

int building_get_levy(const building *b)
{
    int levy = b->monthly_levy;
    if (levy <= 0) {
        return 0;
    }
    if (building_monument_type_is_monument(b->type) && b->monument.phase != MONUMENT_FINISHED) {
        return 0;
    }
    if (b->state != BUILDING_STATE_IN_USE && levy && !b->prev_part_building_id) {
        return 0;
    }
    if (b->prev_part_building_id) {
        return 0;
    }


    // Pantheon base bonus
    if (building_monument_working(BUILDING_PANTHEON) &&
        ((b->type >= BUILDING_SMALL_TEMPLE_CERES && b->type <= BUILDING_LARGE_TEMPLE_VENUS) ||
            (b->type >= BUILDING_GRAND_TEMPLE_CERES && b->type <= BUILDING_GRAND_TEMPLE_VENUS) ||
            b->type == BUILDING_ORACLE || b->type == BUILDING_NYMPHAEUM || b->type == BUILDING_SMALL_MAUSOLEUM ||
            b->type == BUILDING_LARGE_MAUSOLEUM)) {
        levy = (levy / 4) * 3;
    }

    // Mars module 1 bonus
    if (building_monument_gt_module_is_active(MARS_MODULE_1_MESS_HALL)) {
        if (building_is_fort(b->type)) {
            levy = (levy / 4) * 3;
        }
    }

    return difficulty_adjust_levies(levy);
}

int building_get_tourism(const building *b)
{
    return b->is_tourism_venue;
}

int building_get_laborers(building_type type)
{
    const model_building *model = model_get_building(type);
    int workers = model->laborers;
    // Neptune GT bonus
    if (type == BUILDING_FOUNTAIN && building_monument_working(BUILDING_GRAND_TEMPLE_NEPTUNE)) {
        workers /= 2;
        if (workers == 0) {
            workers = 1;
        }
    }
    return workers;
}

void building_totals_add_corrupted_house(int unfixable)
{
    extra.incorrect_houses++;
    if (unfixable) {
        extra.unfixable_houses++;
    }
}

static void initialize_new_building(building *b, unsigned int position)
{
    b->id = position;
}

static int building_in_use(const building *b)
{
    return b->state != BUILDING_STATE_UNUSED || game_undo_contains_building(b->id);
}

void building_clear_all(void)
{
    memset(data.first_of_type, 0, sizeof(data.first_of_type));
    memset(data.last_of_type, 0, sizeof(data.last_of_type));

    if (!array_init(data.buildings, BUILDING_ARRAY_SIZE_STEP, initialize_new_building, building_in_use) ||
        !array_next(data.buildings)) { // Ignore first building
        log_error("Unable to allocate enough memory for the building array. The game will now crash.", 0, 0);
    }

    extra.created_sequence = 0;
    extra.incorrect_houses = 0;
    extra.unfixable_houses = 0;
}

void building_make_immune_cheat(void)
{
    building *b;
    array_foreach(data.buildings, b)
    {
        b->fire_proof = 1;
    }
}

int building_is_close_to_water(const building *b)
{
    return map_terrain_exists_tile_in_radius_with_type(b->x, b->y, b->size, WATER_DESIRABILITY_RANGE, TERRAIN_WATER);
}

void building_save_state(buffer *buf, buffer *highest_id, buffer *highest_id_ever,
    buffer *sequence, buffer *corrupt_houses)
{
    int buf_size = sizeof(int32_t) + data.buildings.size * BUILDING_STATE_CURRENT_BUFFER_SIZE;
    uint8_t *buf_data = malloc(buf_size);
    buffer_init(buf, buf_data, buf_size);
    buffer_write_i32(buf, BUILDING_STATE_CURRENT_BUFFER_SIZE);
    building *b;
    array_foreach(data.buildings, b)
    {
        building_state_save_to_buffer(buf, b);
    }
    buffer_write_i32(highest_id, data.buildings.size);
    buffer_write_i32(highest_id_ever, data.buildings.size);
    buffer_skip(highest_id_ever, 4);
    buffer_write_i32(sequence, extra.created_sequence);

    buffer_write_i32(corrupt_houses, extra.incorrect_houses);
    buffer_write_i32(corrupt_houses, extra.unfixable_houses);
}

void building_load_state(buffer *buf, buffer *sequence, buffer *corrupt_houses, int save_version)
{
    int building_buf_size = BUILDING_STATE_ORIGINAL_BUFFER_SIZE;
    size_t buf_size = buf->size;

    if (save_version > SAVE_GAME_LAST_STATIC_VERSION) {
        building_buf_size = buffer_read_i32(buf);
        buf_size -= 4;
    }

    int buildings_to_load = (int) buf_size / building_buf_size;

    if (!array_init(data.buildings, BUILDING_ARRAY_SIZE_STEP, initialize_new_building, building_in_use) ||
        !array_expand(data.buildings, buildings_to_load)) {
        log_error("Unable to allocate enough memory for the building array. The game will now crash.", 0, 0);
    }

    memset(data.first_of_type, 0, sizeof(data.first_of_type));
    memset(data.last_of_type, 0, sizeof(data.last_of_type));

    int highest_id_in_use = 0;

    for (int i = 0; i < buildings_to_load; i++) {
        building *b = array_next(data.buildings);
        building_state_load_from_buffer(buf, b, building_buf_size, save_version, 0);
        if (b->state != BUILDING_STATE_UNUSED) {
            highest_id_in_use = i;
            fill_adjacent_types(b);
        }
    }

    // Fix messy old hack that assigned type BUILDING_GARDENS to building 0
    building *b = array_first(data.buildings);
    if (b->state == BUILDING_STATE_UNUSED && b->type == BUILDING_GARDENS) {
        b->type = BUILDING_NONE;
    }

    data.buildings.size = highest_id_in_use + 1;

    extra.created_sequence = buffer_read_i32(sequence);

    extra.incorrect_houses = buffer_read_i32(corrupt_houses);
    extra.unfixable_houses = buffer_read_i32(corrupt_houses);
}

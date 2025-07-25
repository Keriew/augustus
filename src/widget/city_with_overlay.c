#include "city_with_overlay.h"

#include "assets/assets.h"
#include "building/animation.h"
#include "building/construction.h"
#include "building/granary.h"
#include "building/industry.h"
#include "building/model.h"
#include "building/storage.h"
#include "building/type.h"
#include "city/view.h"
#include "core/config.h"
#include "core/log.h"
#include "figure/roamer_preview.h"
#include "game/resource.h"
#include "game/state.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/renderer.h"
#include "map/bridge.h"
#include "map/building.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/image.h"
#include "map/image_context.h"
#include "map/property.h"
#include "map/random.h"
#include "map/tiles.h"
#include "map/terrain.h"
#include "widget/city_bridge.h"
#include "widget/city_building_ghost.h"
#include "widget/city_figure.h"
#include "widget/city_overlay.h"
#include "widget/city_overlay_education.h"
#include "widget/city_overlay_entertainment.h"
#include "widget/city_overlay_health.h"
#include "widget/city_overlay_other.h"
#include "widget/city_overlay_risks.h"
#include "widget/city_without_overlay.h"
#include "widget/city_draw_highway.h"

static const city_overlay *overlay = 0;
static float scale = SCALE_NONE;
static unsigned int city_roamer_preview_selected_building_id = ((unsigned int) -1); //NO_POSITION default

#define SELECTED_BUILDING_COLOR_MASK COLOR_MASK_SKY_BLUE
#define OFFSET(x,y) (x + GRID_SIZE * y)

static const int ADJACENT_OFFSETS[2][4][7] = {
    {
        {OFFSET(-1, 0), OFFSET(-1, -1), OFFSET(-1, -2), OFFSET(0, -2), OFFSET(1, -2)},
        {OFFSET(0, -1), OFFSET(1, -1), OFFSET(2, -1), OFFSET(2, 0), OFFSET(2, 1)},
        {OFFSET(1, 0), OFFSET(1, 1), OFFSET(1, 2), OFFSET(0, 2), OFFSET(-1, 2)},
        {OFFSET(0, 1), OFFSET(-1, 1), OFFSET(-2, 1), OFFSET(-2, 0), OFFSET(-2, -1)}
    },
    {
        {OFFSET(-1, 0), OFFSET(-1, -1), OFFSET(-1, -2), OFFSET(-1, -3), OFFSET(0, -3),  OFFSET(1, -3), OFFSET(2, -3)},
        {OFFSET(0, -1), OFFSET(1, -1), OFFSET(2, -1), OFFSET(3, -1), OFFSET(3, 0),  OFFSET(3, 1), OFFSET(3, 2)},
        {OFFSET(1, 0), OFFSET(1, 1), OFFSET(1, 2), OFFSET(1, 3), OFFSET(0, 3),  OFFSET(-1, 3), OFFSET(-2, 3)},
        {OFFSET(0, 1), OFFSET(-1, 1), OFFSET(-2, 1), OFFSET(-3, 1), OFFSET(-3, 0),  OFFSET(-3, -1), OFFSET(-3, -2)}
    }
};

static const city_overlay *get_city_overlay(void)
{
    switch (game_state_overlay()) {
        case OVERLAY_FIRE:
            return city_overlay_for_fire();
        case OVERLAY_CRIME:
            return city_overlay_for_crime();
        case OVERLAY_DAMAGE:
            return city_overlay_for_damage();
        case OVERLAY_PROBLEMS:
            return city_overlay_for_problems();
        case OVERLAY_NATIVE:
            return city_overlay_for_native();
        case OVERLAY_ENTERTAINMENT:
            return city_overlay_for_entertainment();
        case OVERLAY_THEATER:
            return city_overlay_for_theater();
        case OVERLAY_AMPHITHEATER:
            return city_overlay_for_amphitheater();
        case OVERLAY_ARENA:
            return city_overlay_for_arena();
        case OVERLAY_COLOSSEUM:
            return city_overlay_for_colosseum();
        case OVERLAY_HIPPODROME:
            return city_overlay_for_hippodrome();
        case OVERLAY_TAVERN:
            return city_overlay_for_tavern();
        case OVERLAY_EDUCATION:
            return city_overlay_for_education();
        case OVERLAY_SCHOOL:
            return city_overlay_for_school();
        case OVERLAY_LIBRARY:
            return city_overlay_for_library();
        case OVERLAY_ACADEMY:
            return city_overlay_for_academy();
        case OVERLAY_HEALTH:
            return city_overlay_for_health();
        case OVERLAY_BARBER:
            return city_overlay_for_barber();
        case OVERLAY_BATHHOUSE:
            return city_overlay_for_bathhouse();
        case OVERLAY_CLINIC:
            return city_overlay_for_clinic();
        case OVERLAY_HOSPITAL:
            return city_overlay_for_hospital();
        case OVERLAY_SICKNESS:
            return city_overlay_for_sickness();
        case OVERLAY_RELIGION:
            return city_overlay_for_religion();
        case OVERLAY_TAX_INCOME:
            return city_overlay_for_tax_income();
        case OVERLAY_EFFICIENCY:
            return city_overlay_for_efficiency();
        case OVERLAY_FOOD_STOCKS:
            return city_overlay_for_food_stocks();
        case OVERLAY_WATER:
            return city_overlay_for_water();
        case OVERLAY_SENTIMENT:
            return city_overlay_for_sentiment();
        case OVERLAY_DESIRABILITY:
            return city_overlay_for_desirability();
        case OVERLAY_ROADS:
            return city_overlay_for_roads();
        case OVERLAY_LEVY:
            return city_overlay_for_levy();
        case OVERLAY_EMPLOYMENT:
            return city_overlay_for_employment();
        case OVERLAY_MOTHBALL:
            return city_overlay_for_mothball();
        case OVERLAY_ENEMY:
            return city_overlay_for_enemy();
        case OVERLAY_LOGISTICS:
            return city_overlay_for_logistics();
        case OVERLAY_STORAGES:
            return city_overlay_for_storages();
        default:
            return 0;
    }
}

static int select_city_overlay(void)
{
    if (!overlay || overlay->type != game_state_overlay()) {
        overlay = get_city_overlay();
    }
    return overlay != 0;
}

void city_with_overlay_update(void)
{
    select_city_overlay();
}

static color_t get_building_color_mask(const building *b)
{
    color_t color_mask = COLOR_MASK_NONE;
    const model_building *model = model_get_building(b->type);
    int labor_needed = model->laborers;
    if (!labor_needed && b->type != BUILDING_WAREHOUSE_SPACE) {
        // account for warehouse case
        color_mask = COLOR_MASK_NONE;
    } else {
        switch (b->type) {
            //buildings that have labor but no walkers
            case BUILDING_LATRINES:
            case BUILDING_FOUNTAIN:
                color_mask = COLOR_MASK_NONE;
                //all other buildings
            default:
                color_mask = SELECTED_BUILDING_COLOR_MASK;
        }
    }
    return color_mask;
}

static int is_building_selected(const building *b)
{
    if (!config_get(CONFIG_UI_HIGHLIGHT_SELECTED_BUILDING)) { // if option not selected in config, abandon
        return 0;
    }
    int main_part_id = building_main(b)->id; //check if side or main part is selected
    if (b->id == city_roamer_preview_selected_building_id || main_part_id == city_roamer_preview_selected_building_id) {
        return 1;
    } else {
        return 0;
    }

}

static int is_drawable_farmhouse(int grid_offset, int map_orientation)
{
    if (!map_property_is_draw_tile(grid_offset)) {
        return 0;
    }
    int xy = map_property_multi_tile_xy(grid_offset);
    if (map_orientation == DIR_0_TOP && xy == EDGE_X0Y1) {
        return 1;
    }
    if (map_orientation == DIR_2_RIGHT && xy == EDGE_X0Y0) {
        return 1;
    }
    if (map_orientation == DIR_4_BOTTOM && xy == EDGE_X1Y0) {
        return 1;
    }
    if (map_orientation == DIR_2_RIGHT && xy == EDGE_X1Y1) {
        return 1;
    }
    return 0;
}

static int is_drawable_farm_corner(int grid_offset)
{
    if (!map_property_is_draw_tile(grid_offset)) {
        return 0;
    }

    int map_orientation = city_view_orientation();
    int xy = map_property_multi_tile_xy(grid_offset);
    if (map_orientation == DIR_0_TOP && xy == EDGE_X0Y2) {
        return 1;
    } else if (map_orientation == DIR_2_RIGHT && xy == EDGE_X0Y0) {
        return 1;
    } else if (map_orientation == DIR_4_BOTTOM && xy == EDGE_X2Y0) {
        return 1;
    } else if (map_orientation == DIR_6_LEFT && xy == EDGE_X2Y2) {
        return 1;
    }
    return 0;
}

static int draw_building_as_deleted(building *b)
{
    b = building_main(b);
    return b->id && (b->is_deleted || map_property_is_deleted(b->grid_offset));
}

static int is_multi_tile_terrain(int grid_offset)
{
    return !map_building_at(grid_offset) && map_property_multi_tile_size(grid_offset) > 1;
}

static int has_adjacent_deletion(int grid_offset)
{
    int size = map_property_multi_tile_size(grid_offset);
    int total_adjacent_offsets = size * 2 + 1;
    const int *adjacent_offset = ADJACENT_OFFSETS[size - 2][city_view_orientation() / 2];
    for (int i = 0; i < total_adjacent_offsets; ++i) {
        if (map_property_is_deleted(grid_offset + adjacent_offset[i]) ||
            draw_building_as_deleted(building_get(map_building_at(grid_offset + adjacent_offset[i])))) {
            return 1;
        }
    }
    return 0;
}

static void draw_flattened_building_footprint(const building *b, int x, int y, int image_offset, color_t color_mask)
{
    int image_base = image_group(GROUP_TERRAIN_OVERLAY) + image_offset;
    if (b->house_size) {
        image_base += 4;
    }
    if (building_type_is_bridge(b->type)) {//dont draw bridges
        return;
    }
    if (b->size == 1) {
        image_draw_isometric_footprint_from_draw_tile(image_base, x, y, color_mask, scale);
    } else if (b->size == 2) {
        const int x_tile_offset[] = { 30, 0, 60, 30 };
        const int y_tile_offset[] = { -15, 0, 0, 15 };
        for (int i = 0; i < 4; i++) {
            image_draw_isometric_footprint_from_draw_tile(image_base + i,
                x + x_tile_offset[i], y + y_tile_offset[i], color_mask, scale);
        }
    } else if (b->size == 3) {
        const int image_tile_offset[] = { 0, 1, 2, 1, 3, 2, 3, 3, 3 };
        const int x_tile_offset[] = { 60, 30, 90, 0, 60, 120, 30, 90, 60 };
        const int y_tile_offset[] = { -30, -15, -15, 0, 0, 0, 15, 15, 30 };
        for (int i = 0; i < 9; i++) {
            image_draw_isometric_footprint_from_draw_tile(image_base + image_tile_offset[i],
                x + x_tile_offset[i], y + y_tile_offset[i], color_mask, scale);
        }
    } else if (b->size == 4) {
        const int image_tile_offset[] = { 0, 1, 2, 1, 3, 2, 1, 3, 3, 2, 3, 3, 3, 3, 3, 3 };
        const int x_tile_offset[] = {
            90,
            60, 120,
            30, 90, 150,
            0, 60, 120, 180,
            30, 90, 150,
            60, 120,
            90
        };
        const int y_tile_offset[] = {
            -45,
            -30, -30,
            -15, -15, -15,
            0, 0, 0, 0,
            15, 15, 15,
            30, 30,
            45
        };
        for (int i = 0; i < 16; i++) {
            image_draw_isometric_footprint_from_draw_tile(image_base + image_tile_offset[i],
                x + x_tile_offset[i], y + y_tile_offset[i], color_mask, scale);
        }
    } else if (b->size == 5) {
        const int image_tile_offset[] = { 0, 1, 2, 1, 3, 2, 1, 3, 3, 2, 1, 3, 3, 3, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 };
        const int x_tile_offset[] = {
            120,
            90, 150,
            60, 120, 180,
            30, 90, 150, 210,
            0, 60, 120, 180, 240,
            30, 90, 150, 210,
            60, 120, 180,
            90, 150,
            120
        };
        const int y_tile_offset[] = {
            -60,
            -45, -45,
            -30, -30, -30,
            -15, -15, -15, -15,
            0, 0, 0, 0, 0,
            15, 15, 15, 15,
            30, 30, 30,
            45, 45,
            60
        };
        for (int i = 0; i < 25; i++) {
            image_draw_isometric_footprint_from_draw_tile(image_base + image_tile_offset[i],
                x + x_tile_offset[i], y + y_tile_offset[i], color_mask, scale);
        }
    } else if (b->size == 7) {
        const int image_tile_offset[] = { 0, 1, 2, 1, 3, 2, 1, 3, 3, 2, 1, 3, 3, 3, 2, 1, 3, 3, 3, 3, 2, 1,
            3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 };
        const int x_tile_offset[] = {
            180,
            150, 210,
            120, 180, 240,
            90, 150, 210, 270,
            60, 120, 180, 240, 300,
            30, 90, 150, 210, 270, 330,
            0, 60, 120, 180, 240, 300, 360,
            30, 90, 150, 210, 270, 330,
            60, 120, 180, 240, 300,
            90, 150, 210, 270,
            120, 180, 240,
            150, 210,
            180,
        };
        const int y_tile_offset[] = {
            -90,
            -75,-75,
            -60,-60,-60,
            -45, -45,-45,-45,
            -30, -30, -30,-30,-30,
            -15, -15, -15, -15,-15,-15,
            0, 0, 0, 0, 0, 0, 0,
            15, 15, 15, 15, 15, 15,
            30, 30, 30, 30, 30,
            45, 45, 45, 45,
            60, 60, 60,
            75, 75,
            90,
        };
        for (int i = 0; i < 49; i++) {
            image_draw_isometric_footprint_from_draw_tile(image_base + image_tile_offset[i],
                x + x_tile_offset[i], y + y_tile_offset[i], color_mask, scale);
        }
    }
}

void city_with_overlay_draw_building_footprint(int x, int y, int grid_offset, int image_offset)
{
    int building_id = map_building_at(grid_offset);
    if (!building_id) {
        return;
    }
    building *b = building_get(building_id);
    color_t color_mask = 0;
    if (overlay->type == OVERLAY_PROBLEMS) {
        city_overlay_problems_prepare_building(b);
    }
    if (overlay->show_building(b)) {
        if (is_building_selected(b)) {
            color_mask = get_building_color_mask(b);
        }
        if (building_is_farm(b->type)) {
            if (is_drawable_farmhouse(grid_offset, city_view_orientation())) {
                image_draw_isometric_footprint_from_draw_tile(map_image_at(grid_offset), x, y, color_mask, scale);
            } else if (map_property_is_draw_tile(grid_offset)) {
                image_draw_isometric_footprint_from_draw_tile(map_image_at(grid_offset), x, y, color_mask, scale);
            }
        } else {
            image_draw_isometric_footprint_from_draw_tile(map_image_at(grid_offset), x, y, color_mask, scale);
        }
    } else {
        int draw = 1;
        if (b->size == 3 && building_is_farm(b->type)) {
            draw = is_drawable_farm_corner(grid_offset);
        }
        if (draw) {
            draw_flattened_building_footprint(b, x, y, image_offset, color_mask);
        }
    }
}

static void draw_roamer_frequency(int x, int y, int grid_offset)
{
    int travel_frequency = figure_roamer_preview_get_frequency(grid_offset);
    if (travel_frequency > 0 && travel_frequency <= FIGURE_ROAMER_PREVIEW_MAX_PASSAGES) {
        static color_t frequency_colors[] = {
            0x663377ff, 0x662266ee, 0x661155dd, 0x660044cc, 0x660033c4, 0x660022bb, 0x660011a4, 0x66000088
        };
        image_draw(image_group(GROUP_TERRAIN_FLAT_TILE), x, y, frequency_colors[travel_frequency - 1], scale);
    } else if (travel_frequency == FIGURE_ROAMER_PREVIEW_ENTRY_TILE) {
        image_blend_footprint_color(x, y, COLOR_MASK_RED, scale);
    } else if (travel_frequency == FIGURE_ROAMER_PREVIEW_EXIT_TILE) {
        image_blend_footprint_color(x, y, COLOR_MASK_GREEN, scale);
    } else if (travel_frequency == FIGURE_ROAMER_PREVIEW_ENTRY_EXIT_TILE) {
        image_draw_isometric_footprint(image_group(GROUP_TERRAIN_FLAT_TILE),
            x, y, COLOR_MASK_PINK, scale);
    }
}

static void draw_footprint(int x, int y, int grid_offset)
{
    building_construction_record_view_position(x, y, grid_offset);
    if (grid_offset < 0) {
        return;
    } else if (overlay->draw_custom_footprint && overlay->draw_custom_footprint(x, y, scale, grid_offset)) {
        return;
    }
    if (map_property_is_draw_tile(grid_offset)) {
        int terrain = map_terrain_get(grid_offset);
        if (terrain & TERRAIN_HIGHWAY && !(terrain & TERRAIN_GATEHOUSE)) {
            city_draw_highway_footprint(x, y, scale, grid_offset);
        } else if (terrain & (TERRAIN_AQUEDUCT | TERRAIN_WALL)) {
            if (terrain & TERRAIN_ROAD) {
                // Draw the equivalent road tile.
                int image_id = image_group(GROUP_TERRAIN_ROAD);
                if (map_tiles_is_paved_road(grid_offset)) {
                    const terrain_image *img = map_image_context_get_paved_road(grid_offset);
                    image_id += img->group_offset + img->item_offset;
                } else {
                    const terrain_image *img = map_image_context_get_dirt_road(grid_offset);
                    image_id += img->group_offset + img->item_offset + 49;
                }
                image_draw_isometric_footprint_from_draw_tile(image_id, x, y, 0, scale);
            } else {
                // display grass
                int image_id = image_group(GROUP_TERRAIN_GRASS_1) + (map_random_get(grid_offset) & 7);
                image_draw_isometric_footprint_from_draw_tile(image_id, x, y, 0, scale);
            }
        } else if ((terrain & TERRAIN_ROAD) && !(terrain & TERRAIN_BUILDING)) {
            image_draw_isometric_footprint_from_draw_tile(map_image_at(grid_offset), x, y, 0, scale);
        } else if ((terrain & TERRAIN_BUILDING) && !map_is_bridge(grid_offset)) {
            city_with_overlay_draw_building_footprint(x, y, grid_offset, 0);
        } else {
            image_draw_isometric_footprint_from_draw_tile(map_image_at(grid_offset), x, y, 0, scale);
        }
    }
    if (config_get(CONFIG_UI_SHOW_GRID) && map_property_is_draw_tile(grid_offset)
                                        && !map_building_at(grid_offset) && scale <= 2.0f) {
        //grid is drawn by the renderer directly at zoom > 200%
        static int grid_id = 0;
        if (!grid_id) {
            grid_id = assets_get_image_id("UI", "Grid_Full");
        }
        image_draw(grid_id, x, y, COLOR_GRID, scale);
    }
    draw_roamer_frequency(x, y, grid_offset);
}

static void draw_overlay_column(int x, int y, int height, column_color_type color_type)
{
    int image_id = image_group(GROUP_OVERLAY_COLUMN);
    if (height > 10) {
        height = 10;
    }
    switch (color_type) {
        case COLUMN_COLOR_RED:
            image_id += 9;
            break;
        case COLUMN_COLOR_RED_TO_GREEN:
            image_id += height - (height % 3);
            break;
        case COLUMN_COLOR_GREEN_TO_RED:
            image_id += 9 - height + (height % 3);
            break;
        default:
            break;
    }

    int capital_height = image_get(image_id)->height;
    // base
    image_draw(image_id + 2, x + 9, y - 8, 0, scale);
    if (height) {
        // column
        for (int i = 1; i < height; i++) {
            image_draw(image_id + 1, x + 17, y - 8 - 10 * i + 13, 0, scale);
        }
        // capital
        image_draw(image_id, x + 5, y - 8 - capital_height - 10 * (height - 1) + 13, 0, scale);
    }
}
static void draw_depot_resource(building *b, int x, int y, color_t color_mask)
{
    int img_id;

    if (b->num_workers > 0) {
        switch (b->data.depot.current_order.resource_type) {
            case RESOURCE_VEGETABLES:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Vegetables");
                break;
            case RESOURCE_FRUIT:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Fruit");
                break;
            case RESOURCE_MEAT:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Meat");
                break;
            case RESOURCE_FISH:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Fish");
                break;
            case RESOURCE_VINES:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Grapes");
                break;
            case RESOURCE_POTTERY:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Pottery");
                break;
            case RESOURCE_FURNITURE:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Furniture");
                break;
            case RESOURCE_OIL:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Oil");
                break;
            case RESOURCE_WINE:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Wine");
                break;
            case RESOURCE_MARBLE:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Marble");
                break;
            case RESOURCE_WEAPONS:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Weapons");
                break;
            case RESOURCE_CLAY:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Clay");
                break;
            case RESOURCE_TIMBER:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Timber");
                break;
            case RESOURCE_OLIVES:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Olives");
                break;
            case RESOURCE_IRON:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Iron");
                break;
            case RESOURCE_GOLD:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Gold");
                break;
            case RESOURCE_SAND:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Sand");
                break;
            case RESOURCE_STONE:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Stone");
                break;
            case RESOURCE_BRICKS:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Bricks");
                break;
            case RESOURCE_WHEAT:
            default:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Wheat");
                break;
        }
    } else {
        img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Cat");
    }
    image_draw(img_id, x + 11, y, COLOR_MASK_NONE, scale);
}
static void draw_permissions_flag(building *b, int x, int y, color_t color_mask)
{
    if (b->has_plague) {
        return;
    }
    static int base_permission_image[8];
    if (!base_permission_image[0]) {
        base_permission_image[0] = 0xdeadbeef; // Invalid image ID, just to confirm the other values have been set
        base_permission_image[1] = assets_get_image_id("UI", "Warehouse_Flag_Market");
        base_permission_image[2] = assets_get_image_id("UI", "Warehouse_Flag_Land");
        base_permission_image[3] = assets_get_image_id("UI", "Warehouse_Flag_Market_Land");
        base_permission_image[4] = assets_get_image_id("UI", "Warehouse_Flag_Sea");
        base_permission_image[5] = assets_get_image_id("UI", "Warehouse_Flag_Market_Sea");
        base_permission_image[6] = assets_get_image_id("UI", "Warehouse_Flag_Land_Sea");
        base_permission_image[7] = assets_get_image_id("UI", "Warehouse_Flag_All");
    }
    const building_storage *storage = building_storage_get(b->storage_id);
    int flag_permission_mask = 0x7;
    int permissions = (~storage->permissions) & flag_permission_mask;
    if (!permissions) {
        return;
    }
    image_draw(base_permission_image[permissions] + b->data.warehouse.flag_frame, x, y, color_mask, scale);

    building_animation_advance_storage_flag(b, base_permission_image[permissions]);
}

static void draw_warehouse_ornaments(int x, int y, color_t color_mask)
{
    image_draw(image_group(GROUP_BUILDING_WAREHOUSE) + 17, x - 4, y - 42, color_mask, scale);
}

static void draw_building_top(int grid_offset, building *b, int x, int y)
{
    color_t color_mask = draw_building_as_deleted(b) ? COLOR_MASK_RED : 0;
    if (is_building_selected(b)) {
        color_mask = get_building_color_mask(b);
    }

    if (building_is_farm(b->type)) {
        if (is_drawable_farmhouse(grid_offset, city_view_orientation())) {
            image_draw_isometric_top_from_draw_tile(map_image_at(grid_offset), x, y, color_mask, scale);
        } else if (map_property_is_draw_tile(grid_offset)) {
            image_draw_isometric_top_from_draw_tile(map_image_at(grid_offset), x, y, color_mask, scale);
        }
        return;
    }
    if (b->type == BUILDING_GRANARY) {
        const image *img = image_get(map_image_at(grid_offset));
        if (img->animation) {
            image_draw(image_group(GROUP_BUILDING_GRANARY) + 1,
                x + img->animation->sprite_offset_x, y + img->animation->sprite_offset_y - 30 - (img->height - 90),
                color_mask, scale);
        }
        if (b->resources[RESOURCE_NONE] < FULL_GRANARY) {
            image_draw(image_group(GROUP_BUILDING_GRANARY) + 2, x + 33, y - 60, color_mask, scale);
            if (b->resources[RESOURCE_NONE] < THREEQUARTERS_GRANARY) {
                image_draw(image_group(GROUP_BUILDING_GRANARY) + 3, x + 56, y - 50, color_mask, scale);
            }
            if (b->resources[RESOURCE_NONE] < HALF_GRANARY) {
                image_draw(image_group(GROUP_BUILDING_GRANARY) + 4, x + 91, y - 50, color_mask, scale);
            }
            if (b->resources[RESOURCE_NONE] < QUARTER_GRANARY) {
                image_draw(image_group(GROUP_BUILDING_GRANARY) + 5, x + 117, y - 62, color_mask, scale);
            }
        }
        draw_permissions_flag(b, x + 81, y - 101, color_mask);
    }
    if (b->type == BUILDING_WAREHOUSE) {
        image_draw(image_group(GROUP_BUILDING_WAREHOUSE) + 17, x - 4, y - 42, color_mask, scale);
        draw_warehouse_ornaments(x, y, color_mask);
        draw_permissions_flag(b, x + 19, y - 56, color_mask);
    }
    if (b->type == BUILDING_DEPOT) {
        draw_depot_resource(b, x, y, color_mask);
    }

    image_draw_isometric_top_from_draw_tile(map_image_at(grid_offset), x, y, color_mask, scale);
}

void city_with_overlay_draw_building_top(int x, int y, int grid_offset)
{
    building *b = building_get(map_building_at(grid_offset));
    if (overlay->show_building(b)) {
        draw_building_top(grid_offset, b, x, y);
    } else {
        int column_height = overlay->get_column_height(b);
        if (column_height != NO_COLUMN) {
            int draw = 1;
            if (building_is_farm(b->type)) {
                draw = is_drawable_farm_corner(grid_offset);
            }
            if (draw) {
                draw_overlay_column(x, y, column_height, overlay->column_type);
            }
        }
    }
}

static void draw_top(int x, int y, int grid_offset)
{
    if (overlay->draw_custom_top && overlay->draw_custom_top(x, y, scale, grid_offset)) {
        return;
    }
    if (!map_property_is_draw_tile(grid_offset)) {
        return;
    }
    if (map_terrain_is(grid_offset, TERRAIN_BUILDING) && map_building_at(grid_offset)) {
        city_with_overlay_draw_building_top(x, y, grid_offset);
    } else if (!map_terrain_is(grid_offset, TERRAIN_BUILDING)) {
        if (!map_terrain_is(grid_offset, TERRAIN_WALL | TERRAIN_AQUEDUCT | TERRAIN_ROAD)) {
            color_t color_mask = 0;
            if (map_property_is_deleted(grid_offset) && !is_multi_tile_terrain(grid_offset)) {
                color_mask = COLOR_MASK_RED;
            }
            // terrain
            image_draw_isometric_top_from_draw_tile(map_image_at(grid_offset), x, y, color_mask, scale);
        }
    }
}
static void draw_animation(int x, int y, int grid_offset)
{
    int draw = 0;
    if (map_building_at(grid_offset)) {
        int btype = building_get(map_building_at(grid_offset))->type;
        switch (overlay->type) {
            case OVERLAY_FIRE:
            case OVERLAY_CRIME:
            case OVERLAY_ENEMY:
                if (btype == BUILDING_PREFECTURE || btype == BUILDING_BURNING_RUIN) {
                    draw = 1;
                }
                break;
            case OVERLAY_DAMAGE:
                if (btype == BUILDING_ENGINEERS_POST) {
                    draw = 1;
                }
                break;
            case OVERLAY_WATER:
                if (btype == BUILDING_RESERVOIR || btype == BUILDING_FOUNTAIN) {
                    draw = 1;
                }
                break;
            case OVERLAY_FOOD_STOCKS:
                if (btype == BUILDING_MARKET || btype == BUILDING_GRANARY) {
                    draw = 1;
                }
                break;
        }
    }

    int image_id = map_image_at(grid_offset);
    const image *img = image_get(image_id);
    if (map_is_bridge(grid_offset)) {
        city_draw_bridge(x, y, scale, grid_offset);
    } else if (img->animation && draw) {
        if (map_property_is_draw_tile(grid_offset)) {
            building *b = building_get(map_building_at(grid_offset));
            int color_mask = draw_building_as_deleted(b) ? COLOR_MASK_RED : 0;
            if (is_building_selected(b)) {
                color_mask = get_building_color_mask(b);
            }
            if (b->type == BUILDING_GRANARY) {
                if (img->animation) {
                    image_draw(image_group(GROUP_BUILDING_GRANARY) + 1,
                        x + img->animation->sprite_offset_x,
                        y + 60 + img->animation->sprite_offset_y - img->height,
                        color_mask, scale);
                }
                if (b->resources[RESOURCE_NONE] < FULL_GRANARY) {
                    image_draw(image_group(GROUP_BUILDING_GRANARY) + 2, x + 33, y - 60, color_mask, scale);
                }
                if (b->resources[RESOURCE_NONE] < THREEQUARTERS_GRANARY) {
                    image_draw(image_group(GROUP_BUILDING_GRANARY) + 3, x + 56, y - 50, color_mask, scale);
                }
                if (b->resources[RESOURCE_NONE] < HALF_GRANARY) {
                    image_draw(image_group(GROUP_BUILDING_GRANARY) + 4, x + 91, y - 50, color_mask, scale);
                }
                if (b->resources[RESOURCE_NONE] < QUARTER_GRANARY) {
                    image_draw(image_group(GROUP_BUILDING_GRANARY) + 5, x + 117, y - 62, color_mask, scale);
                }
                draw_permissions_flag(b, x + 81, y - 101, color_mask);
            } else {
                int animation_offset = building_animation_offset(b, image_id, grid_offset);
                if (animation_offset > 0) {
                    if (animation_offset > img->animation->num_sprites) {
                        animation_offset = img->animation->num_sprites;
                    }
                    int y_offset = img->top ? img->top->original.height - FOOTPRINT_HALF_HEIGHT : 0;
                    image_draw(image_id + img->animation->start_offset + animation_offset,
                        x + img->animation->sprite_offset_x,
                        y + img->animation->sprite_offset_y - y_offset,
                        color_mask, scale);
                }
            }
        }
    }
}

static void draw_figures(int x, int y, int grid_offset)
{
    int figure_id = map_figure_at(grid_offset);
    while (figure_id) {
        figure *f = figure_get(figure_id);
        if (!f->is_ghost && overlay->show_figure(f)) {
            city_draw_figure(f, x, y, scale, 0);
        }
        figure_id = f->next_figure_id_on_same_tile;
    }
}

static void draw_elevated_figures(int x, int y, int grid_offset)
{
    int figure_id = map_figure_at(grid_offset);
    while (figure_id > 0) {
        figure *f = figure_get(figure_id);
        if (((f->use_cross_country && !f->is_ghost && !f->dont_draw_elevated) || f->height_adjusted_ticks) && overlay->show_figure(f)) {
            city_draw_figure(f, x, y, scale, 0);
        } else if (f->building_id == city_roamer_preview_selected_building_id) { //figure from selected building
            if (config_get(CONFIG_UI_SHOW_ROAMING_PATH)) {
                int highlight = FIGURE_HIGHLIGHT_GREEN;
                if (f->type == FIGURE_MARKET_SUPPLIER || f->type == FIGURE_DELIVERY_BOY) {
                    highlight = FIGURE_HIGHLIGHT_RED; //green highlight makes market supplier look indistinguishable
                }
                city_draw_figure(f, x, y, scale, highlight);
            }

        }
        figure_id = f->next_figure_id_on_same_tile;
    }
}

static int should_draw_top_before_deletion(int grid_offset)
{
    return is_multi_tile_terrain(grid_offset) && has_adjacent_deletion(grid_offset);
}

static void deletion_draw_terrain_top(int x, int y, int grid_offset)
{
    if (should_draw_top_before_deletion(grid_offset)) {
        draw_top(x, y, grid_offset);
    }
}

static void deletion_draw_animations(int x, int y, int grid_offset)
{
    if (map_property_is_deleted(grid_offset) || draw_building_as_deleted(building_get(map_building_at(grid_offset)))) {
        image_blend_footprint_color(x, y, COLOR_MASK_RED, scale);
    }
    if (!should_draw_top_before_deletion(grid_offset)) {
        draw_top(x, y, grid_offset);
    }
    draw_animation(x, y, grid_offset);
}

static void draw_custom_layer(int x, int y, int grid_offset)
{
    overlay->draw_custom_layer(x, y, scale, grid_offset);
}

void city_with_overlay_draw(const map_tile *tile, unsigned int roamer_preview_building_id)
{
    if (!select_city_overlay()) {
        return;
    }

    scale = city_view_get_scale() / 100.0f;
    city_roamer_preview_selected_building_id = roamer_preview_building_id;
    int x, y, width, height;
    city_view_get_viewport(&x, &y, &width, &height);
    graphics_fill_rect(x, y, width, height, COLOR_BLACK);
    int should_mark_deleting = city_building_ghost_mark_deleting(tile);
    city_view_foreach_valid_map_tile(draw_footprint);
    if (!should_mark_deleting) {
        city_view_foreach_valid_map_tile_row(
            draw_figures,
            draw_top,
            draw_animation
        );
        city_building_ghost_draw(tile);
        city_view_foreach_valid_map_tile(draw_elevated_figures);
    } else {
        city_view_foreach_valid_map_tile(draw_figures);
        city_view_foreach_valid_map_tile(deletion_draw_terrain_top);
        city_view_foreach_valid_map_tile(deletion_draw_animations);
        city_building_ghost_draw(tile);
        city_view_foreach_valid_map_tile(draw_elevated_figures);
    }
    if (overlay->draw_custom_layer) {
        city_view_foreach_valid_map_tile(draw_custom_layer);
    }
}

int city_with_overlay_get_tooltip_text(tooltip_context *c, int grid_offset)
{
    int overlay_type = overlay->type;
    int building_id = map_building_at(grid_offset);
    if (overlay->get_tooltip_for_building && !building_id) {
        return 0;
    }
    int overlay_requires_house =
        overlay_type != OVERLAY_WATER && overlay_type != OVERLAY_FIRE && overlay_type != OVERLAY_LEVY &&
        overlay_type != OVERLAY_DAMAGE && overlay_type != OVERLAY_NATIVE && overlay_type != OVERLAY_DESIRABILITY &&
        overlay_type != OVERLAY_PROBLEMS && overlay_type != OVERLAY_MOTHBALL && overlay_type != OVERLAY_ENEMY &&
        overlay_type != OVERLAY_LOGISTICS && overlay_type != OVERLAY_SICKNESS && overlay_type != OVERLAY_EFFICIENCY &&
        overlay_type != OVERLAY_HEALTH && overlay_type != OVERLAY_EMPLOYMENT;
    building *b = building_get(building_id);
    if (overlay_requires_house && !b->house_size) {
        return 0;
    }
    if (overlay->get_tooltip_for_building) {
        return overlay->get_tooltip_for_building(c, b);
    } else if (overlay->get_tooltip_for_grid_offset) {
        return overlay->get_tooltip_for_grid_offset(c, grid_offset);
    }
    return 0;
}

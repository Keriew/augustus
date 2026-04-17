#include "other.h"

#include "assets/assets.h"
#include "building/animation.h"
#include "building/building.h"
#include "building/industry.h"
#include "building/monument.h"
#include "building/properties.h"
#include "building/roadblock.h"
#include "building/rotation.h"
#include "building/storage.h"
#include "city/constants.h"
#include "city/finance.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/lang.h"
#include "core/string.h"
#include "game/resource.h"
#include "game/state.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/text.h"
#include "map/bridge.h"
#include "map/building.h"
#include "map/desirability.h"
#include "map/grid.h"
#include "map/image.h"
#include "map/property.h"
#include "map/random.h"
#include "map/terrain.h"
#include "map/water_supply.h"
#include "scenario/property.h"
#include "translation/translation.h"
#include "widget/city/draw.h"
#include "widget/city/highway.h"

#include <stdio.h>

static struct {
    int show_reservoir_range;
    int show_fountain_well_range;
    color_t reservoir_range_color;
} water_building_ghost_settings;

static int show_building_religion(const building *b)
{
    return
        b->type == BUILDING_ORACLE || b->type == BUILDING_LARARIUM || b->type == BUILDING_SMALL_TEMPLE_CERES ||
        b->type == BUILDING_SMALL_TEMPLE_NEPTUNE || b->type == BUILDING_SMALL_TEMPLE_MERCURY ||
        b->type == BUILDING_SMALL_TEMPLE_MARS || b->type == BUILDING_SMALL_TEMPLE_VENUS ||
        b->type == BUILDING_LARGE_TEMPLE_CERES || b->type == BUILDING_LARGE_TEMPLE_NEPTUNE ||
        b->type == BUILDING_LARGE_TEMPLE_MERCURY || b->type == BUILDING_LARGE_TEMPLE_MARS ||
        b->type == BUILDING_SMALL_MAUSOLEUM || b->type == BUILDING_LARGE_MAUSOLEUM ||
        b->type == BUILDING_LARGE_TEMPLE_VENUS || b->type == BUILDING_GRAND_TEMPLE_CERES ||
        b->type == BUILDING_GRAND_TEMPLE_NEPTUNE || b->type == BUILDING_GRAND_TEMPLE_MERCURY ||
        b->type == BUILDING_GRAND_TEMPLE_MARS || b->type == BUILDING_GRAND_TEMPLE_VENUS ||
        b->type == BUILDING_PANTHEON || b->type == BUILDING_NYMPHAEUM ||
        b->type == BUILDING_SHRINE_CERES || b->type == BUILDING_SHRINE_MARS ||
        b->type == BUILDING_SHRINE_MERCURY || b->type == BUILDING_SHRINE_VENUS ||
        b->type == BUILDING_SHRINE_NEPTUNE;
}

static int show_building_food_stocks(const building *b)
{
    return b->type == BUILDING_MARKET || b->type == BUILDING_WHARF || b->type == BUILDING_GRANARY ||
        b->type == BUILDING_CARAVANSERAI || b->type == BUILDING_MESS_HALL;
}

static int show_building_tax_income(const building *b)
{
    return b->type == BUILDING_FORUM || b->type == BUILDING_SENATE;
}

static int show_building_water(const building *b)
{
    return b->house_size > 0 || b->type == BUILDING_WELL || b->type == BUILDING_FOUNTAIN || b->type == BUILDING_RESERVOIR ||
        (b->type == BUILDING_GRAND_TEMPLE_NEPTUNE && building_monument_gt_module_is_active(NEPTUNE_MODULE_2_CAPACITY_AND_WATER));
}

static int show_building_sentiment(const building *b)
{
    return b->house_size > 0;
}

static int show_building_roads(const building *b)
{
    return building_type_is_roadblock(b->type);
}

static int show_building_mothball(const building *b)
{
    return b->state == BUILDING_STATE_MOTHBALLED;
}

static int show_building_logistics(const building *b)
{
    return b->type == BUILDING_WAREHOUSE || b->type == BUILDING_WAREHOUSE_SPACE ||
        b->type == BUILDING_GRANARY || b->type == BUILDING_DOCK ||
        b->type == BUILDING_DEPOT || b->type == BUILDING_LIGHTHOUSE ||
        b->type == BUILDING_ARMOURY;
}

static int show_building_storages(const building *b)
{
    b = building_main((building *) b);
    return (b->storage_id > 0 && building_storage_get(b->storage_id))
        || b->type == BUILDING_DEPOT || b->type == BUILDING_DOCK;
}

static int show_building_none(const building *b)
{
    return 0;
}

static int show_figure_religion(const figure *f)
{
    return f->type == FIGURE_PRIEST || f->type == FIGURE_PRIEST_SUPPLIER;
}

static int show_figure_efficiency(const figure *f)
{
    return f->type == FIGURE_CART_PUSHER || f->type == FIGURE_LABOR_SEEKER;
}

static int show_figure_food_stocks(const figure *f)
{
    switch (f->type) {
        case FIGURE_MARKET_SUPPLIER:
        case FIGURE_MARKET_TRADER:
        case FIGURE_CARAVANSERAI_SUPPLIER:
        case FIGURE_CARAVANSERAI_COLLECTOR:
        case FIGURE_MESS_HALL_SUPPLIER:
        case FIGURE_MESS_HALL_FORT_SUPPLIER:
        case FIGURE_MESS_HALL_COLLECTOR:
        case FIGURE_DELIVERY_BOY:
        case FIGURE_FISHING_BOAT:
            return 1;

        case FIGURE_CART_PUSHER:
            return resource_is_food(f->resource_id);

        case FIGURE_WAREHOUSEMAN:
        {
            building *b = building_get(f->building_id);
            return b->type == BUILDING_GRANARY;
        }
        default:
            return 0;
    }
}

static int show_figure_tax_income(const figure *f)
{
    return f->type == FIGURE_TAX_COLLECTOR;
}

static int show_figure_logistics(const figure *f)
{
    switch (f->type) {
        case FIGURE_CART_PUSHER:
        case FIGURE_WAREHOUSEMAN:
        case FIGURE_DEPOT_CART_PUSHER:
        case FIGURE_DOCKER:
        case FIGURE_LIGHTHOUSE_SUPPLIER:
        case FIGURE_TRADE_CARAVAN:
        case FIGURE_TRADE_CARAVAN_DONKEY:
        case FIGURE_TRADE_SHIP:
        case FIGURE_NATIVE_TRADER:
        case FIGURE_MESS_HALL_FORT_SUPPLIER:
            return 1;
        default:
            return 0;
    }
}

static int show_figure_employment(const figure *f)
{
    return f->type == FIGURE_LABOR_SEEKER;
}

static int show_figure_none(const figure *f)
{
    return 0;
}

static int get_column_height_religion(const building *b)
{
    return b->house_size && b->data.house.num_gods ? b->data.house.num_gods * 18 / 10 : NO_COLUMN;
}

static int get_column_height_efficiency(const building *b)
{
    int percentage = building_get_efficiency(b);
    if (percentage == -1) {
        return NO_COLUMN;
    }
    return calc_bound(percentage / 10, 1, 10);
}

static int get_column_height_food_stocks(const building *b)
{
    if (b->house_size && model_get_house(b->subtype.house_level)->food_types) {
        int pop = b->house_population;
        int stocks = 0;
        for (resource_type r = RESOURCE_MIN_FOOD; r < RESOURCE_MAX_FOOD; r++) {
            if (resource_is_inventory(r)) {
                stocks += b->resources[r];
            }
        }
        int pct_stocks = calc_percentage(stocks, pop);
        if (pct_stocks <= 0) {
            return 10;
        } else if (pct_stocks < 100) {
            return 5;
        } else if (pct_stocks <= 200) {
            return 1;
        }
    }
    return NO_COLUMN;
}

static int get_column_height_levy(const building *b)
{
    int levy = building_get_levy(b);
    int height = calc_percentage(levy, PANTHEON_LEVY_MONTHLY) / 10;
    height = calc_bound(height, 1, 10);
    return levy ? height : NO_COLUMN;
}

static int get_column_height_tax_income(const building *b)
{
    if (b->house_size) {
        int pct = calc_adjust_with_percentage(b->tax_income_or_storage / 2, city_finance_tax_percentage());
        if (pct > 0) {
            return pct / 25;
        }
    }
    return NO_COLUMN;
}

static int get_column_height_employment(const building *b)
{
    int full_staff = building_get_laborers(b->type);
    int pct_staff = calc_percentage(b->num_workers, full_staff);

    int height = 100 - pct_staff;
    if (height == 0) {
        return NO_COLUMN;
    }
    return full_staff ? height / 10 : NO_COLUMN;
}

static void add_god(tooltip_context *c, int god_id)
{
    int index = c->num_extra_texts;
    c->extra_text_groups[index] = 59;
    c->extra_text_ids[index] = 11 + god_id;
    c->num_extra_texts++;
}

static int get_tooltip_religion(tooltip_context *c, int grid_offset)
{
    building *b = building_get(map_building_at(grid_offset));
    if (!b->house_size) {
        return 0;
    }
    if (b->house_pantheon_access) {
        c->translation_key = TR_TOOLTIP_OVERLAY_PANTHEON_ACCESS;
        return 0;
    }

    if (b->data.house.num_gods < 5) {
        if (b->data.house.temple_ceres) {
            add_god(c, GOD_CERES);
        }
        if (b->data.house.temple_neptune) {
            add_god(c, GOD_NEPTUNE);
        }
        if (b->data.house.temple_mercury) {
            add_god(c, GOD_MERCURY);
        }
        if (b->data.house.temple_mars) {
            add_god(c, GOD_MARS);
        }
        if (b->data.house.temple_venus) {
            add_god(c, GOD_VENUS);
        }
    }
    if (b->data.house.num_gods <= 0) {
        return 12;
    } else if (b->data.house.num_gods == 1) {
        return 13;
    } else if (b->data.house.num_gods == 2) {
        return 14;
    } else if (b->data.house.num_gods == 3) {
        return 15;
    } else if (b->data.house.num_gods == 4) {
        return 16;
    } else if (b->data.house.num_gods == 5) {
        return 17;
    } else {
        return 18; // >5 gods, shouldn't happen...
    }
}

static int get_tooltip_efficiency(tooltip_context *c, int grid_offset)
{
    building *b = building_get(map_building_at(grid_offset));
    int efficiency = building_get_efficiency(b);
    if (efficiency == -1) {
        return 0;
    }
    if (efficiency == 0) {
        c->translation_key = TR_TOOLTIP_OVERLAY_EFFICIENCY_0;
    } else if (efficiency < 25) {
        c->translation_key = TR_TOOLTIP_OVERLAY_EFFICIENCY_1;
    } else if (efficiency < 50) {
        c->translation_key = TR_TOOLTIP_OVERLAY_EFFICIENCY_2;
    } else if (efficiency < 80) {
        c->translation_key = TR_TOOLTIP_OVERLAY_EFFICIENCY_3;
    } else if (efficiency < 95) {
        c->translation_key = TR_TOOLTIP_OVERLAY_EFFICIENCY_4;
    } else {
        c->translation_key = TR_TOOLTIP_OVERLAY_EFFICIENCY_5;
    }
    return 0;
}

static int get_tooltip_food_stocks(tooltip_context *c, int grid_offset)
{
    building *b = building_get(map_building_at(grid_offset));
    if (b->house_population <= 0) {
        return 0;
    }
    if (!model_get_house(b->subtype.house_level)->food_types) {
        return 104;
    } else {
        int stocks_present = 0;
        for (resource_type r = RESOURCE_MIN_FOOD; r < RESOURCE_MAX_FOOD; r++) {
            if (resource_is_inventory(r)) {
                stocks_present += b->resources[r];
            }
        }
        int stocks_per_pop = calc_percentage(stocks_present, b->house_population);
        if (stocks_per_pop <= 0) {
            return 4;
        } else if (stocks_per_pop < 100) {
            return 5;
        } else if (stocks_per_pop <= 200) {
            return 6;
        } else {
            return 7;
        }
    }
}

static int get_tooltip_tax_income(tooltip_context *c, int grid_offset)
{
    building *b = building_get(map_building_at(grid_offset));
    int denarii = calc_adjust_with_percentage(b->tax_income_or_storage / 2, city_finance_tax_percentage());
    if (denarii > 0) {
        c->has_numeric_prefix = 1;
        c->numeric_prefix = denarii;
        return 45;
    } else if (b->house_tax_coverage > 0) {
        return 44;
    } else {
        return 43;
    }
}

static int get_tooltip_employment(tooltip_context *c, int grid_offset)
{
    building *b = building_get(map_building_at(grid_offset));
    int full = building_get_laborers(b->type);
    int missing = full - b->num_workers;

    if (full >= 1) {
        if (missing == 0) {
            c->translation_key = TR_TOOLTIP_OVERLAY_EMPLOYMENT_FULL;
        } else if (missing <= 1) {
            c->has_numeric_prefix = 1;
            c->numeric_prefix = missing;
            c->translation_key = TR_TOOLTIP_OVERLAY_EMPLOYMENT_MISSING_1;
            return 1;
        } else if (missing >= 2 && b->state == BUILDING_STATE_MOTHBALLED) {
            c->has_numeric_prefix = 1;
            c->numeric_prefix = missing;
            c->translation_key = TR_TOOLTIP_OVERLAY_EMPLOYMENT_MOTHBALL;
            return 1;
        } else {
            c->has_numeric_prefix = 1;
            c->numeric_prefix = missing;
            c->translation_key = TR_TOOLTIP_OVERLAY_EMPLOYMENT_MISSING_2;
            return 1;
        }
    }
    return 0;
}

static int get_tooltip_water(tooltip_context *c, int grid_offset)
{
    if (map_terrain_is(grid_offset, TERRAIN_RESERVOIR_RANGE)) {
        if (map_terrain_is(grid_offset, TERRAIN_FOUNTAIN_RANGE)) {
            return 2;
        } else {
            return 1;
        }
    } else if (map_terrain_is(grid_offset, TERRAIN_FOUNTAIN_RANGE)) {
        return 3;
    }
    return 0;
}

static int get_tooltip_desirability(tooltip_context *c, int grid_offset)
{
    int desirability;
    if (map_terrain_is(grid_offset, TERRAIN_BUILDING)) {
        int building_id = map_building_at(grid_offset);
        building *b = building_get(building_id);
        desirability = b->desirability;
    } else {
        desirability = map_desirability_get(grid_offset);
    }
    if (desirability < 0) {
        return 91;
    } else if (desirability == 0) {
        return 92;
    } else {
        return 93;
    }
}

static int get_tooltip_depot_orders(tooltip_context *c, int grid_offset)
{
    int building_id = map_building_at(grid_offset);
    building *b = building_get(building_id);
    if (b->type == BUILDING_DEPOT) {
        static uint8_t result[256];
        order depot_order = b->data.depot.current_order;
        int condition_type = TR_ORDER_CONDITION_NEVER + depot_order.condition.condition_type;
        const uint8_t *order_string = lang_get_string(CUSTOM_TRANSLATION, condition_type);
        const uint8_t *moving_resource = lang_get_string(CUSTOM_TRANSLATION, TR_TOOLTIP_DEPOT_MOVED);
        const uint8_t *resource_name = resource_get_data(depot_order.resource_type)->text;
        char threshold_str[16] = "\n";
        if (condition_type > TR_ORDER_CONDITION_ALWAYS) {
            snprintf(threshold_str, sizeof(threshold_str), " %d", depot_order.condition.threshold);
        }
        building *b_src = building_get(depot_order.src_storage_id);
        building *b_dst = building_get(depot_order.dst_storage_id);

        const uint8_t *src_type = lang_get_string(28, b_src->type);
        const uint8_t *dst_type = lang_get_string(28, b_dst->type);
        char src_info[64];
        char dst_info[64];
        snprintf(src_info, sizeof(src_info), "%s %d", (const char *) src_type, b_src->storage_id);
        snprintf(dst_info, sizeof(dst_info), "%s %d", (const char *) dst_type, b_dst->storage_id);
        const uint8_t *direction_arrow = lang_get_string(CUSTOM_TRANSLATION, TR_TOOLTIP_DEPOT_ORDER_TO);

        snprintf((char *) result, sizeof(result),
            "%s %s\n"
            "%s%s\n" //double \n doesnt get rendered, and neither does \n after a space. 
            "%s %s %s",
            (const char *) moving_resource, (const char *) resource_name,
            (const char *) order_string, threshold_str,
            src_info, (const char *) direction_arrow, dst_info);

        c->precomposed_text = (const uint8_t *) result;

        return 1;
    }

    return 0;
}

static int get_tooltip_levy(tooltip_context *c, const building *b)
{
    int levy = building_get_levy(b);
    if (levy > 0) {
        c->has_numeric_prefix = 1;
        c->numeric_prefix = levy;
        c->translation_key = TR_TOOLTIP_OVERLAY_LEVY;
        return 1;
    }
    return 0;
}

static int get_offset_tooltip_levy(tooltip_context *c, int grid_offset)
{
    if (map_terrain_is(grid_offset, TERRAIN_BUILDING)) {
        return get_tooltip_levy(c, building_get(map_building_at(grid_offset)));
    }
    if (map_terrain_is(grid_offset, TERRAIN_HIGHWAY)) {
        c->has_numeric_prefix = 1;
        c->numeric_prefix = 1;
        c->translation_key = TR_TOOLTIP_OVERLAY_LEVY_PER_TILE;
        return 1;
    }
    return 0;
}

static int get_tooltip_sentiment(tooltip_context *c, int grid_offset)
{
    if (!map_terrain_is(grid_offset, TERRAIN_BUILDING)) {
        return 0;
    }
    int building_id = map_building_at(grid_offset);
    building *b = building_get(building_id);
    if (!b || !b->house_population) {
        return 0;
    }
    int happiness = b->sentiment.house_happiness;
    int sentiment_text_id = TR_BUILDING_WINDOW_HOUSE_SENTIMENT_1;
    if (happiness > 0) {
        sentiment_text_id = happiness / 10 + TR_BUILDING_WINDOW_HOUSE_SENTIMENT_2;
    }
    c->translation_key = sentiment_text_id;
    return 1;
}

const city_overlay *city_overlay_for_religion(void)
{
    static city_overlay overlay = {
        .type = OVERLAY_RELIGION,
        .column_type = COLUMN_COLOR_GREEN_TO_RED,
        .show_building = show_building_religion,
        .show_figure = show_figure_religion,
        .get_column_height = get_column_height_religion,
        .get_tooltip = get_tooltip_religion
    };
    return &overlay;
}

const city_overlay *city_overlay_for_efficiency(void)
{
    static city_overlay overlay = {
        .type = OVERLAY_EFFICIENCY,
        .column_type = COLUMN_COLOR_GREEN_TO_RED,
        .show_building = show_building_roads,
        .show_figure = show_figure_efficiency,
        .get_column_height = get_column_height_efficiency,
        .get_tooltip = get_tooltip_efficiency
    };
    return &overlay;
}

const city_overlay *city_overlay_for_food_stocks(void)
{
    static city_overlay overlay = {
        .type = OVERLAY_FOOD_STOCKS,
        .column_type = COLUMN_COLOR_RED,
        .show_building = show_building_food_stocks,
        .show_figure = show_figure_food_stocks,
        .get_column_height = get_column_height_food_stocks,
        .get_tooltip = get_tooltip_food_stocks
    };
    return &overlay;
}

const city_overlay *city_overlay_for_tax_income(void)
{
    static city_overlay overlay = {
        .type = OVERLAY_TAX_INCOME,
        .column_type = COLUMN_COLOR_GREEN_TO_RED,
        .show_building = show_building_tax_income,
        .show_figure = show_figure_tax_income,
        .get_column_height = get_column_height_tax_income,
        .get_tooltip = get_tooltip_tax_income
    };
    return &overlay;
}

const city_overlay *city_overlay_for_employment(void)
{
    static city_overlay overlay = {
        .type = OVERLAY_EMPLOYMENT,
        .column_type = COLUMN_COLOR_RED_TO_GREEN,
        .show_building = show_building_none,
        .show_figure = show_figure_employment,
        .get_column_height = get_column_height_employment,
        .get_tooltip = get_tooltip_employment
    };
    return &overlay;
}

static int should_draw_graph(int grid_offset)
{
    if (map_terrain_is(grid_offset, TERRAIN_WATER)) {
        return map_terrain_is(grid_offset, TERRAIN_BUILDING) && !map_is_bridge(grid_offset);
    }
    if (map_terrain_is(grid_offset, TERRAIN_ROCK | TERRAIN_ELEVATION | TERRAIN_ACCESS_RAMP)) {
        return 0;
    }
    return !map_property_is_plaza_earthquake_or_overgrown_garden(grid_offset) ||
        map_terrain_is(grid_offset, TERRAIN_ROAD | TERRAIN_GARDEN);
}

static int has_well_access(int grid_offset)
{
    // If we are in a building that doesn't have well access, we know for sure the tile doesn't have well access,
    // so we can skip the more expensive terrain checks.
    const building *b = building_get(map_building_at(grid_offset));
    if (b->id > 0 && !b->has_well_access) {
         return 0;
    }

    // Store the last well found to avoid redundant checks for consecutive tiles with the same well access.
    static const building *last_well;
    int radius = map_water_supply_well_radius();

    if (last_well && map_grid_chess_distance(last_well->grid_offset, grid_offset) <= radius) {
        return 1;
    }

    // Check every well
    for (building *well = building_first_of_type(BUILDING_WELL); well; well = well->next_of_type) {
        if (well == last_well) {
            continue;
        }
        if (well->state == BUILDING_STATE_IN_USE && map_grid_chess_distance(well->grid_offset, grid_offset) <= radius) {
            last_well = well;
            return 1;
        }
    }

    return 0;
}

static int is_inhabited_building(int grid_offset)
{
    building *b = building_get(map_building_at(grid_offset));
    return b && b->house_population > 0;
}

static void blend_color_to_footprint(int x, int y, int size, color_t color, float scale)
{
    int total_steps = size * 2 - 1;
    int tiles = 1;

    for (int step = 1; step <= total_steps; step++) {
        if (tiles % 2) {
            image_draw(image_group(GROUP_TERRAIN_FLAT_TILE), x, y, color, scale);
        }
        int y_offset = 15 + 15 * (tiles % 2);

        for (int i = 1; i <= tiles / 2; i++) {
            image_draw(image_group(GROUP_TERRAIN_FLAT_TILE), x, y - y_offset, color, scale);
            image_draw(image_group(GROUP_TERRAIN_FLAT_TILE), x, y + y_offset, color, scale);
            y_offset += 30;
        }
        x += 30;
        step < size ? tiles++ : tiles--;
    }
}

static void redraw_water_building(building *b, int x, int y, float scale, int grid_offset)
{
    if (!water_building_ghost_settings.show_reservoir_range &&
        (b->type == BUILDING_RESERVOIR || b->type == BUILDING_GRAND_TEMPLE_NEPTUNE)) {
        return;
    }
    if (!water_building_ghost_settings.show_fountain_well_range &&
        (b->type == BUILDING_WELL || b->type == BUILDING_FOUNTAIN)) {
        return;
    }

    int image_id = map_image_at(grid_offset);
    color_t color_mask = city_draw_get_color_mask(grid_offset, 0);
    image_draw_isometric_footprint_from_draw_tile(image_id, x, y, color_mask, scale);
    image_draw_isometric_top_from_draw_tile(image_id, x, y, city_draw_get_color_mask(grid_offset, 1), scale);
    int animation_offset = building_animation_offset(b, image_id, grid_offset);
    if (animation_offset > 0) {
        const image *img = image_get(image_id);
        int y_offset = img->top ? img->top->original.height - FOOTPRINT_HALF_HEIGHT : 0;
        if (animation_offset > img->animation->num_sprites) {
            animation_offset = img->animation->num_sprites;
        }
        if (b->type == BUILDING_GRAND_TEMPLE_NEPTUNE) {
            int image_id = assets_get_image_id("Monuments", "Neptune Module 2 Fountain");
            image_draw(image_id + ((animation_offset - 1) % 5), x + 98, y + 87 - y_offset, color_mask, scale);
        }
        image_draw(image_id + img->animation->start_offset + animation_offset,
            x + img->animation->sprite_offset_x,
            y + img->animation->sprite_offset_y - y_offset,
            color_mask, scale);
    }
}

static void draw_water_graph(int x, int y, float scale, int grid_offset)
{
    if (!water_building_ghost_settings.show_reservoir_range && !water_building_ghost_settings.show_fountain_well_range) {
        return;
    }

    if (!should_draw_graph(grid_offset)) {
        return;
    }

    building *b = building_get(map_building_at(grid_offset));

    if (show_building_water(b) && !is_inhabited_building(grid_offset)) {
        if (map_property_is_draw_tile(grid_offset)) {
            redraw_water_building(b, x, y, scale, grid_offset);
        }
        return;
    }

    if (water_building_ghost_settings.show_reservoir_range &&
        (!show_building_water(b) || (is_inhabited_building(grid_offset) && !water_building_ghost_settings.show_fountain_well_range)) &&
        map_terrain_is(grid_offset, TERRAIN_RESERVOIR_RANGE) && !map_terrain_is(grid_offset, TERRAIN_AQUEDUCT)) {
        color_t color_to_use = map_terrain_is(grid_offset, TERRAIN_ROAD) ?
            ALPHA_MASK_SEMI_TRANSPARENT : water_building_ghost_settings.reservoir_range_color;
        image_draw_isometric_footprint_from_draw_tile(assets_lookup_image_id(ASSET_UI_RESERVOIR_RANGE), x, y,
            color_to_use, scale);
    }

    if (water_building_ghost_settings.show_reservoir_range && map_terrain_is(grid_offset, TERRAIN_AQUEDUCT)) {
        int image_id = map_image_at(grid_offset);
        color_t color_mask = city_draw_get_color_mask(grid_offset, 1);
        image_draw_isometric_top_from_draw_tile(image_id, x, y, color_mask, scale);
    }

    if (!water_building_ghost_settings.show_fountain_well_range) {
        return;
    }

    if (is_inhabited_building(grid_offset)) {
        if (!map_property_is_draw_tile(grid_offset)) {
            return;
        }
        color_t water_color = COLOR_MASK_GRAY;
        
        switch (model_get_house(b->subtype.house_level)->water) {
            default:
            case 1:
                if (b->has_well_access) {
                    water_color = COLOR_MASK_DARK_BLUE;
                }
                // intentional fallthrough
            case 2:
                if (b->house_size && b->has_water_access) {
                    water_color = COLOR_MASK_BLUE;
                }
                break;
        }

        blend_color_to_footprint(x, y, b->house_size, water_color, scale);
        int image_id = map_image_at(grid_offset);
        image_draw_isometric_top_from_draw_tile(image_id, x, y, city_draw_get_color_mask(grid_offset, 1), scale);
        image_draw_set_isometric_top_from_draw_tile(image_id, x, y, water_color, scale);

        return;
    }

    if (map_terrain_is(grid_offset, TERRAIN_FOUNTAIN_RANGE)) {
        image_draw_isometric_footprint_from_draw_tile(assets_lookup_image_id(ASSET_UI_FOUNTAIN_RANGE), x, y,
            COLOR_MASK_BLUE, scale);
    } else if (has_well_access(grid_offset)) {
        image_draw_isometric_footprint_from_draw_tile(assets_lookup_image_id(ASSET_UI_FOUNTAIN_RANGE), x, y,
            COLOR_MASK_DARK_BLUE, scale);
    }
}

const city_overlay *city_overlay_for_water(void)
{
    water_building_ghost_settings.show_reservoir_range = 1;
    water_building_ghost_settings.show_fountain_well_range = 1;
    water_building_ghost_settings.reservoir_range_color = COLOR_MASK_NONE;

    static city_overlay overlay = {
        .type = OVERLAY_WATER,
        .show_building = show_building_water,
        .show_figure = show_figure_none,
        .get_tooltip = get_tooltip_water,
        .draw_layer = draw_water_graph
    };
    return &overlay;
}

const city_overlay *city_overlay_for_water_building_ghost(int show_reservoir_range, int show_fountain_well_ranges)
{
    if (!show_reservoir_range && !show_fountain_well_ranges) {
        return 0;
    }

    water_building_ghost_settings.show_reservoir_range = show_reservoir_range;
    water_building_ghost_settings.show_fountain_well_range = show_fountain_well_ranges;
    water_building_ghost_settings.reservoir_range_color = ALPHA_FONT_SEMI_TRANSPARENT;

    static city_overlay overlay = {
        .draw_layer = draw_water_graph
    };
    return &overlay;
}

static color_t get_color_for_percentage(int percentage)
{
    static const color_t percentage_colors[] = {
        0xccde4939, 0xccff695a, 0xccf7714a, 0xccf7aa52, 0xccf7d35a,
        0xccdeeb4a, 0xccb5eb5a, 0xcc7beb73, 0xcc5adbce, 0xcc5ac3ef
    };
    return percentage_colors[calc_bound(percentage / 10, 0, 9)];
}

static void draw_sentiment_graph(int x, int y, float scale, int grid_offset)
{
    if (!map_terrain_is(grid_offset, TERRAIN_BUILDING)) {
        return;
    }
    int building_id = map_building_at(grid_offset);
    building *b = building_get(building_id);
    if (!b || !b->house_population || b->is_deleted || map_property_is_deleted(b->grid_offset)) {
        return;
    }
    if (map_property_is_draw_tile(grid_offset)) {
        color_t color = get_color_for_percentage(b->sentiment.house_happiness);
        int image_id = map_image_at(grid_offset);
        blend_color_to_footprint(x, y, b->house_size, color, scale);
        image_draw_isometric_top_from_draw_tile(image_id, x, y, city_draw_get_color_mask(grid_offset, 1), scale);
        image_draw_set_isometric_top_from_draw_tile(image_id, x, y, color, scale);
    }
}

const city_overlay *city_overlay_for_sentiment(void)
{
    static city_overlay overlay = {
        .type = OVERLAY_SENTIMENT,
        .show_building = show_building_sentiment,
        .show_figure = show_figure_none,
        .get_tooltip = get_tooltip_sentiment,
        .draw_layer = draw_sentiment_graph
    };
    return &overlay;
}

static int get_desirability_image_offset(int desirability)
{
    if (desirability < -10) {
        return 0;
    } else if (desirability < -5) {
        return 1;
    } else if (desirability < 0) {
        return 2;
    } else if (desirability == 1) {
        return 3;
    } else if (desirability < 5) {
        return 4;
    } else if (desirability < 10) {
        return 5;
    } else if (desirability < 15) {
        return 6;
    } else if (desirability < 20) {
        return 7;
    } else if (desirability < 25) {
        return 8;
    } else {
        return 9;
    }
}

static void draw_desirability_graph(int x, int y, float scale, int grid_offset)
{
    if (!should_draw_graph(grid_offset)) {
        return;
    }
    if (map_terrain_is(grid_offset, TERRAIN_BUILDING) && is_inhabited_building(grid_offset)) {
        if (map_property_is_draw_tile(grid_offset)) {
            building *b = building_get(map_building_at(grid_offset));
            color_t desirability_color = get_color_for_percentage(get_desirability_image_offset(b->desirability) * 10);
            blend_color_to_footprint(x, y, b->house_size, desirability_color, scale);
            int image_id = map_image_at(grid_offset);
            image_draw_isometric_top_from_draw_tile(image_id, x, y, city_draw_get_color_mask(grid_offset, 1), scale);
            image_draw_set_isometric_top_from_draw_tile(image_id, x, y, desirability_color, scale);
        }
    } else {
        int desirability;
        if (map_building_at(grid_offset)) {
            building *b = building_get(map_building_at(grid_offset));
            desirability = b->desirability;
        } else {
            desirability = map_desirability_get(grid_offset);
        }
        if (desirability) {
            int offset = get_desirability_image_offset(desirability);
            image_draw_isometric_footprint_from_draw_tile(image_group(GROUP_TERRAIN_DESIRABILITY) + offset, x, y,
                ALPHA_FONT_SEMI_TRANSPARENT, scale);
            image_draw_isometric_top_from_draw_tile(image_group(GROUP_TERRAIN_DESIRABILITY) + offset, x, y,
                ALPHA_FONT_SEMI_TRANSPARENT, scale);
        }
    }
}

const city_overlay *city_overlay_for_desirability(void)
{
    static city_overlay overlay = {
        .type = OVERLAY_DESIRABILITY,
        .show_building = show_building_sentiment,
        .show_figure = show_figure_none,
        .get_tooltip = get_tooltip_desirability,
        .draw_layer = draw_desirability_graph
    };
    return &overlay;
}

const city_overlay *city_overlay_for_roads(void)
{
    static city_overlay overlay = {
        .type = OVERLAY_ROADS,
        .show_building = show_building_roads,
        .show_figure = show_figure_none
    };
    return &overlay;
}

const city_overlay *city_overlay_for_levy(void)
{
    static city_overlay overlay = {
        .type = OVERLAY_LEVY,
        .column_type = COLUMN_COLOR_GREEN,
        .show_building = show_building_none,
        .show_figure = show_figure_none,
        .get_column_height = get_column_height_levy,
        .get_tooltip = get_offset_tooltip_levy
    };
    return &overlay;
}

const city_overlay *city_overlay_for_mothball(void)
{
    static city_overlay overlay = {
        .type = OVERLAY_MOTHBALL,
        .show_building = show_building_mothball,
        .show_figure = show_figure_none
    };
    return &overlay;
}

static void draw_storage_ids(int x, int y, float scale, int grid_offset)
{
    if (!map_terrain_is(grid_offset, TERRAIN_BUILDING)) {
        return;
    }
    int building_id = map_building_at(grid_offset);
    building *b = building_get(building_id);
    if (!b || b->is_deleted || map_property_is_deleted(b->grid_offset) || !b->storage_id ||
        !map_property_is_draw_tile(grid_offset)) {
        return;
    }
    uint8_t number[10];
    string_from_int(number, b->storage_id, 0);
    int text_width = text_get_width(number, FONT_SMALL_PLAIN);
    int box_width = text_width + 10;
    int box_height = 22;
    if (b->type == BUILDING_GRANARY) {
        x += 90;
        y += 15;
    } else if (b->type == BUILDING_WAREHOUSE) {
        switch (building_rotation_get_building_orientation(b->subtype.orientation)) {
            case 6:
                x -= 30;
                break;
            case 4:
                x += 30;
                y -= 30;
                break;
            case 2:
                x += 90;
                break;
            case 0:
            default:
                x += 30;
                y += 30;
                break;
        }
    }
    x = (int) (x / scale);
    y = (int) (y / scale);
    x -= box_width / 2;
    y -= box_height / 2;
    graphics_draw_rect(x, y, box_width, box_height, COLOR_BLACK);
    graphics_fill_rect(x + 1, y + 1, box_width - 2, box_height - 2, COLOR_WHITE);
    text_draw(number, x + 5, y + 6, FONT_SMALL_PLAIN, COLOR_BLACK);
}

const city_overlay *city_overlay_for_logistics(void)
{
    static city_overlay overlay = {
        .type = OVERLAY_LOGISTICS,
        .show_building = show_building_logistics,
        .show_figure = show_figure_logistics,
        .get_tooltip = get_tooltip_depot_orders,
        .draw_layer = draw_storage_ids
    };
    return &overlay;
}

const city_overlay *city_overlay_for_storages(void)
{
    static city_overlay overlay = {
        .type = OVERLAY_STORAGES,
        .show_building = show_building_storages,
        .show_figure = show_figure_none,
        .draw_layer = draw_storage_ids
    };
    return &overlay;
}

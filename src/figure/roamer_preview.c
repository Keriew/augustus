#include "roamer_preview.h"

#include "building/industry.h"
#include "building/properties.h"
#include "core/config.h"
#include "figure/figure.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "map/grid.h"
#include "map/road_access.h"

#include <string.h>

#define TOTAL_ROAMERS 4

static struct {
    figure roamer;
    grid_u8 travelled_tiles;
    int active;
} data;

static figure_type building_type_to_figure_type(building_type type)
{
    switch (type) {
        case BUILDING_MARKET:
            return FIGURE_MARKET_TRADER;
        case BUILDING_SENATE:
        case BUILDING_SENATE_UPGRADED:
        case BUILDING_FORUM:
        case BUILDING_FORUM_UPGRADED:
            return FIGURE_TAX_COLLECTOR;
        case BUILDING_SCHOOL:
            return FIGURE_SCHOOL_CHILD;
        case BUILDING_LIBRARY:
            return FIGURE_LIBRARIAN;
        case BUILDING_ACADEMY:
            return FIGURE_TEACHER;
        case BUILDING_ENGINEERS_POST:
            return FIGURE_ENGINEER;
        case BUILDING_PREFECTURE:
            return FIGURE_PREFECT;
        case BUILDING_TAVERN:
            return FIGURE_BARKEEP;
        case BUILDING_THEATER:
            return FIGURE_ACTOR;
        case BUILDING_AMPHITHEATER:
            return FIGURE_GLADIATOR;
        case BUILDING_ARENA:
        case BUILDING_COLOSSEUM:
            return FIGURE_LION_TAMER;
        case BUILDING_HIPPODROME:
            return FIGURE_CHARIOTEER;
        case BUILDING_BATHHOUSE:
            return FIGURE_BATHHOUSE_WORKER;
        case BUILDING_BARBER:
            return FIGURE_BARBER;
        case BUILDING_DOCTOR:
            return FIGURE_DOCTOR;
        case BUILDING_HOSPITAL:
            return FIGURE_SURGEON;
        case BUILDING_PANTHEON:
        case BUILDING_GRAND_TEMPLE_CERES:
        case BUILDING_GRAND_TEMPLE_MARS:
        case BUILDING_GRAND_TEMPLE_MERCURY:
        case BUILDING_GRAND_TEMPLE_NEPTUNE:
        case BUILDING_GRAND_TEMPLE_VENUS:
        case BUILDING_LARGE_TEMPLE_CERES:
        case BUILDING_LARGE_TEMPLE_MARS:
        case BUILDING_LARGE_TEMPLE_MERCURY:
        case BUILDING_LARGE_TEMPLE_NEPTUNE:
        case BUILDING_LARGE_TEMPLE_VENUS:
        case BUILDING_SMALL_TEMPLE_CERES:
        case BUILDING_SMALL_TEMPLE_MARS:
        case BUILDING_SMALL_TEMPLE_MERCURY:
        case BUILDING_SMALL_TEMPLE_NEPTUNE:
        case BUILDING_SMALL_TEMPLE_VENUS:
            return FIGURE_PRIEST;
        case BUILDING_MISSION_POST:
            return FIGURE_MISSIONARY;
        case BUILDING_BARRACKS:
        case BUILDING_MILITARY_ACADEMY:
        case BUILDING_MESS_HALL:
        case BUILDING_WAREHOUSE:
        case BUILDING_GRANARY:
        case BUILDING_TOWER:
        case BUILDING_ACTOR_COLONY:
        case BUILDING_GLADIATOR_SCHOOL:
        case BUILDING_LION_HOUSE:
        case BUILDING_CHARIOT_MAKER:
        case BUILDING_WHEAT_FARM:
        case BUILDING_PIG_FARM:
        case BUILDING_FRUIT_FARM:
        case BUILDING_OLIVE_FARM:
        case BUILDING_VINES_FARM:
        case BUILDING_VEGETABLE_FARM:
        case BUILDING_CLAY_PIT:
        case BUILDING_TIMBER_YARD:
        case BUILDING_IRON_MINE:
        case BUILDING_MARBLE_QUARRY:
        case BUILDING_POTTERY_WORKSHOP:
        case BUILDING_OIL_WORKSHOP:
        case BUILDING_WINE_WORKSHOP:
        case BUILDING_FURNITURE_WORKSHOP:
        case BUILDING_WEAPONS_WORKSHOP:
        case BUILDING_WORKCAMP:
        case BUILDING_ARCHITECT_GUILD:
        case BUILDING_WHARF:
        case BUILDING_DOCK:
        case BUILDING_SHIPYARD:
        case BUILDING_CARAVANSERAI:
        case BUILDING_LIGHTHOUSE:
        case BUILDING_WATCHTOWER:
            return FIGURE_LABOR_SEEKER;
        default:
            return FIGURE_NONE;
    }
}

static int roam_length_for_figure_type(figure_type type)
{
    switch (type) {
        case FIGURE_TAX_COLLECTOR:
        case FIGURE_ACTOR:
        case FIGURE_GLADIATOR:
        case FIGURE_LION_TAMER:
        case FIGURE_CHARIOTEER:
            return 512;
        case FIGURE_SCHOOL_CHILD:
        case FIGURE_MISSIONARY:
            return 192;
        case FIGURE_ENGINEER:
        case FIGURE_PREFECT:
            return 640;
        default:
            return 384;
    }
}

static int figure_enters_exits_building(figure_type type)
{
    switch (type) {
        case FIGURE_TAX_COLLECTOR:
        case FIGURE_ENGINEER:
        case FIGURE_PREFECT:
            return 1;
        default:
            return 0;
    }
}

static void init_roaming(figure *f, int roam_dir, int x, int y)
{
    f->progress_on_tile = 15;
    f->roam_choose_destination = 0;
    f->roam_ticks_until_next_turn = -1;
    f->roam_turn_direction = 2;
    f->roam_length = 0;

    if (config_get(CONFIG_GP_CH_ROAMERS_DONT_SKIP_CORNERS)) {
        f->disallow_diagonal = 1;
    }
    switch (roam_dir) {
        case DIR_0_TOP: y -= 8; break;
        case DIR_2_RIGHT: x += 8; break;
        case DIR_4_BOTTOM: y += 8; break;
        case DIR_6_LEFT: x -= 8; break;
    }
    map_grid_bound(&x, &y);
    int x_road, y_road;
    if (map_closest_road_within_radius(x, y, 1, 6, &x_road, &y_road)) {
        f->destination_x = x_road;
        f->destination_y = y_road;
    } else {
        f->roam_choose_destination = 1;
    }
}

void figure_roamer_preview_create(building_type b_type, int grid_offset, int x, int y)
{
    if (!config_get(CONFIG_UI_SHOW_ROAMING_PATH)) {
        figure_roamer_preview_reset();
        return;
    }

    figure_roamer_preview_reset();

    figure_type fig_type = building_type_to_figure_type(b_type);
    if (fig_type == FIGURE_NONE) {
        return;
    }

    if (fig_type == FIGURE_LABOR_SEEKER && config_get(CONFIG_GP_CH_GLOBAL_LABOUR)) {
        return;
    }

    data.active = 1;

    int b_size = building_is_farm(b_type) ? 3 : building_properties_for_type(b_type)->size;

    map_point road;
    if (!map_has_road_access(x, y, b_size, &road)) {
        return;
    }

    int figure_walks_into_building = figure_enters_exits_building(fig_type);

    int x_road, y_road;
    int has_closest_road = map_closest_road_within_radius(x, y, b_size, 2, &x_road, &y_road);

    if (figure_walks_into_building && !has_closest_road) {
        return;
    }

    int roam_length = roam_length_for_figure_type(fig_type);

    int num_ticks = fig_type == FIGURE_CHARIOTEER || fig_type == FIGURE_SCHOOL_CHILD ? 2 : 1;
    int should_return = fig_type == FIGURE_SCHOOL_CHILD ? 0 : 1;

    figure *f = &data.roamer;

    for (int i = 0; i < TOTAL_ROAMERS; i++) {
        memset(f, 0, sizeof(figure));
        
        f->source_x = f->destination_x = f->previous_tile_x = road.x;
        f->source_y = f->destination_y = f->previous_tile_y = road.y;
        f->terrain_usage = TERRAIN_USAGE_ROADS;
        f->cross_country_x = 15 * road.x;
        f->cross_country_y = 15 * road.y;
        f->progress_on_tile = 15;
        f->direction = DIR_0_TOP;
        f->faction_id = FIGURE_FACTION_ROAMER_PREVIEW;
        f->type = fig_type;
        f->max_roam_length = roam_length;

        if (figure_walks_into_building) {
            f->x = x_road;
            f->y = y_road;
        } else {
            f->x = road.x;
            f->y = road.y;
        }
        f->grid_offset = map_grid_offset(f->x, f->y);
        if (map_grid_is_valid_offset(f->grid_offset)) {
            data.travelled_tiles.items[f->grid_offset] = FIGURE_ROAMER_PREVIEW_EXIT_TILE;
        }
        init_roaming(f, i * 2, x, y);
        while (++f->roam_length <= f->max_roam_length) {
            figure_movement_roam_ticks(f, num_ticks);
        }
        figure_route_remove(f);
        if (!should_return || !has_closest_road) {
            continue;
        }
        f->destination_x = x_road;
        f->destination_y = y_road;
        while (f->direction != DIR_FIGURE_AT_DESTINATION &&
            f->direction != DIR_FIGURE_REROUTE && f->direction != DIR_FIGURE_LOST) {
            figure_movement_move_ticks(f, num_ticks);
        }
        figure_route_remove(f);
        if (f->direction == DIR_FIGURE_AT_DESTINATION) {
            int tile_type = data.travelled_tiles.items[f->grid_offset];
            data.travelled_tiles.items[f->grid_offset] = tile_type < FIGURE_ROAMER_PREVIEW_EXIT_TILE ?
                FIGURE_ROAMER_PREVIEW_ENTRY_TILE : FIGURE_ROAMER_PREVIEW_ENTRY_EXIT_TILE;
        }
    }
}

void figure_roamer_preview_reset(void)
{
    if (data.active) {
        memset(&data.roamer, 0, sizeof(figure));
        map_grid_clear_u8(data.travelled_tiles.items);
        data.active = 0;
    }
}

void figure_roamer_preview_add_grid_offset_to_travelled_path(int grid_offset)
{
    if (map_grid_is_valid_offset(grid_offset) &&
        data.travelled_tiles.items[grid_offset] < FIGURE_ROAMER_PREVIEW_MAX_PASSAGES) {
        data.travelled_tiles.items[grid_offset]++;
    }
}

int figure_roamer_preview_get_frequency(int grid_offset)
{
    return map_grid_is_valid_offset(grid_offset) ? data.travelled_tiles.items[grid_offset] : 0;
}
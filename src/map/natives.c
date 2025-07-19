#include "natives.h"

#include "assets/assets.h"
#include "building/building.h"
#include "building/image.h"
#include "building/list.h"
#include "building/properties.h"
#include "building/roadblock.h"
#include "city/buildings.h"
#include "city/military.h"
#include "core/calc.h"
#include "core/image.h"
#include "core/image_group_editor.h"
#include "map/building.h"
#include "map/building_tiles.h"
#include "map/data.h"
#include "map/grid.h"
#include "map/image.h"
#include "map/property.h"
#include "map/random.h"
#include "map/terrain.h"
#include "scenario/data.h" // TODO remove this dependency
#include "scenario/property.h"

static void mark_native_land(int x, int y, int size, int radius)
{
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, size, radius, &x_min, &y_min, &x_max, &y_max);
    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            map_property_mark_native_land(map_grid_offset(xx, yy));
        }
    }
}

static int has_building_on_native_land(int x, int y, int size, int radius)
{
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, size, radius, &x_min, &y_min, &x_max, &y_max);
    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            int building_id = map_building_at(map_grid_offset(xx, yy));
            if (building_id > 0) {
                int type = building_get(building_id)->type;
                if (type != BUILDING_MISSION_POST &&
                    type != BUILDING_NATIVE_HUT &&
                    type != BUILDING_NATIVE_HUT_ALT &&
                    type != BUILDING_NATIVE_MEETING &&
                    type != BUILDING_NATIVE_CROPS &&
                    type != BUILDING_NATIVE_DECORATION &&
                    type != BUILDING_NATIVE_MONUMENT &&
                    type != BUILDING_NATIVE_WATCHTOWER &&
                    (!building_type_is_roadblock(type) || type == BUILDING_PALISADE_GATE ||
                        type == BUILDING_TRIUMPHAL_ARCH || type == BUILDING_GATEHOUSE)) {
                    return 1;
                }
            } else if (map_terrain_is(map_grid_offset(xx, yy), TERRAIN_AQUEDUCT | TERRAIN_WALL | TERRAIN_GARDEN)) {
                return 1;
            }
        }
    }
    return 0;
}

static void determine_meeting_center(void)
{
    building_type native_hut_kind[] = { BUILDING_NATIVE_HUT, BUILDING_NATIVE_HUT_ALT };

    for (int kind_idx = 0; kind_idx < sizeof(native_hut_kind) / sizeof(native_hut_kind[0]); ++kind_idx) {
        // Determine closest meeting center for hut
        for (building *b = building_first_of_type(native_hut_kind[kind_idx]); b; b = b->next_of_type) {
            if (b->state != BUILDING_STATE_IN_USE) {
                continue;
            }
            int min_dist = 1000;
            int min_meeting_id = 0;
            for (building *m = building_first_of_type(BUILDING_NATIVE_MEETING); m; m = m->next_of_type) {
                int dist = calc_maximum_distance(b->x, b->y, m->x, m->y);
                if (dist < min_dist) {
                    min_dist = dist;
                    min_meeting_id = m->id;
                }
            }
            b->subtype.native_meeting_center_id = min_meeting_id;
        }
    }
}

int native_hut_alt_get_image_id() {
    switch (scenario_property_climate()) {
        case CLIMATE_NORTHERN:
            return assets_get_image_id("Terrain_Maps", "Native_Hut_Northern_01");
        case CLIMATE_DESERT:
            return assets_get_image_id("Terrain_Maps", "Native_Hut_Southern_01");
        default:
            return assets_get_image_id("Terrain_Maps", "Native_Hut_Central_01");
    }
}

void map_natives_init(void)
{
    int image_hut = scenario.native_images.hut;
    int image_meeting = scenario.native_images.meeting;
    int image_crops = scenario.native_images.crops;
    int scenario_image_alt_hut = scenario.native_images.alt_hut;
    int scenario_image_decoration = scenario.native_images.decoration;
    int scenario_image_monument = scenario.native_images.monument;
    int scenario_image_watchtower = scenario.native_images.watchtower;
    int native_image = image_group(GROUP_BUILDING_NATIVE);
    int native_hut_alt_image = native_hut_alt_get_image_id();
    int grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {
            if (!map_terrain_is(grid_offset, TERRAIN_BUILDING) || map_building_at(grid_offset)) {
                continue;
            }

            int random_bit = map_random_get(grid_offset) & 1;
            int type;
            int image_id = map_image_at(grid_offset);
            if (image_id == image_hut) {
                type = BUILDING_NATIVE_HUT;
                map_image_set(grid_offset, native_image);
            } else if (image_id == image_hut + 1) {
                type = BUILDING_NATIVE_HUT;
                map_image_set(grid_offset, native_image + 1);
            } else if (scenario_image_alt_hut != 0 && image_id == scenario_image_alt_hut) {
                type = BUILDING_NATIVE_HUT_ALT;
                map_image_set(grid_offset, native_hut_alt_image);
            } else if (scenario_image_alt_hut != 0 && image_id == scenario_image_alt_hut + 1) {
                type = BUILDING_NATIVE_HUT_ALT;
                map_image_set(grid_offset, native_hut_alt_image + 1);
            } else if (scenario_image_decoration != 0 && image_id == scenario_image_decoration) {
                type = BUILDING_NATIVE_DECORATION;
                map_image_set(grid_offset, building_image_get_for_type(type));
            } else if (scenario_image_monument != 0 && image_id == scenario_image_monument) {
                type = BUILDING_NATIVE_MONUMENT;
                map_image_set(grid_offset, building_image_get_for_type(type));
            } else if (scenario_image_watchtower != 0 && image_id == scenario_image_watchtower) {
                type = BUILDING_NATIVE_WATCHTOWER;
                map_image_set(grid_offset, building_image_get_for_type(type));
            } else if (image_id == image_meeting) {
                type = BUILDING_NATIVE_MEETING;
                map_image_set(grid_offset, native_image + 2);
                map_image_set(grid_offset + map_grid_delta(1, 0), native_image + 2);
                map_image_set(grid_offset + map_grid_delta(0, 1), native_image + 2);
                map_image_set(grid_offset + map_grid_delta(1, 1), native_image + 2);
            } else if (image_id == image_crops) {
                type = BUILDING_NATIVE_CROPS;
                map_image_set(grid_offset, image_group(GROUP_BUILDING_FARM_CROPS) + random_bit);
            } else { //unknown building
                map_building_tiles_remove(0, x, y);
                continue;
            }
            building *b = building_create(type, x, y);
            map_building_set(grid_offset, b->id);
            b->state = BUILDING_STATE_IN_USE;
            switch (type) {
                case BUILDING_NATIVE_CROPS:
                    b->data.industry.progress = random_bit;
                    break;
                case BUILDING_NATIVE_MEETING:
                    b->sentiment.native_anger = 100;
                    map_building_set(grid_offset + map_grid_delta(1, 0), b->id);
                    map_building_set(grid_offset + map_grid_delta(0, 1), b->id);
                    map_building_set(grid_offset + map_grid_delta(1, 1), b->id);
                    mark_native_land(b->x, b->y, 2, 6);
                    break;
                case BUILDING_NATIVE_HUT:
                case BUILDING_NATIVE_HUT_ALT:
                case BUILDING_NATIVE_WATCHTOWER:
                    b->sentiment.native_anger = 100;
                    b->figure_spawn_delay = random_bit;
                    mark_native_land(b->x, b->y, 1, 3);
                    break;
                case BUILDING_NATIVE_MONUMENT:
                    map_building_tiles_add(b->id, b->x, b->y, b->size, building_image_get_for_type(type), TERRAIN_BUILDING);
                    break;
            }
        }
    }

    determine_meeting_center();
}

void map_natives_init_editor(void)
{
    int image_hut = scenario.native_images.hut;
    int image_meeting = scenario.native_images.meeting;
    int image_crops = scenario.native_images.crops;
    int native_image = image_group(GROUP_EDITOR_BUILDING_NATIVE);
    int native_hut_alt_image = native_hut_alt_get_image_id();
    int scenario_image_alt_hut = scenario.native_images.alt_hut;
    int scenario_image_decoration = scenario.native_images.decoration;
    int scenario_image_monument = scenario.native_images.monument;
    int scenario_image_watchtower = scenario.native_images.watchtower;

    int grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {
            if (!map_terrain_is(grid_offset, TERRAIN_BUILDING) || map_building_at(grid_offset)) {
                continue;
            }

            int type;
            int image_id = map_image_at(grid_offset);
            if (image_id == image_hut) {
                type = BUILDING_NATIVE_HUT;
                map_image_set(grid_offset, native_image);
            } else if (image_id == image_hut + 1) {
                type = BUILDING_NATIVE_HUT;
                map_image_set(grid_offset, native_image + 1);
            } else if (scenario_image_alt_hut != 0 && image_id == scenario_image_alt_hut) {
                type = BUILDING_NATIVE_HUT_ALT;
                map_image_set(grid_offset, native_hut_alt_image);
            } else if (scenario_image_alt_hut != 0 && image_id == scenario_image_alt_hut + 1) {
                type = BUILDING_NATIVE_HUT_ALT;
                map_image_set(grid_offset, native_hut_alt_image + 1);
            } else if (scenario_image_decoration != 0 && image_id == scenario_image_decoration) {
                type = BUILDING_NATIVE_DECORATION;
                map_image_set(grid_offset, building_image_get_for_type(type));
            } else if (scenario_image_monument != 0 && image_id == scenario_image_monument) {
                type = BUILDING_NATIVE_MONUMENT;
                map_image_set(grid_offset, building_image_get_for_type(type));
            } else if (scenario_image_watchtower != 0 && image_id == scenario_image_watchtower) {
                type = BUILDING_NATIVE_WATCHTOWER;
                map_image_set(grid_offset, building_image_get_for_type(type));
            } else if (image_id == image_meeting) {
                type = BUILDING_NATIVE_MEETING;
                map_image_set(grid_offset, native_image + 2);
                map_image_set(grid_offset + map_grid_delta(1, 0), native_image + 2);
                map_image_set(grid_offset + map_grid_delta(0, 1), native_image + 2);
                map_image_set(grid_offset + map_grid_delta(1, 1), native_image + 2);
            } else if (image_id == image_crops) {
                type = BUILDING_NATIVE_CROPS;
                map_image_set(grid_offset, image_group(GROUP_EDITOR_BUILDING_CROPS));
            } else { //unknown building
                map_building_tiles_remove(0, x, y);
                continue;
            }
            building *b = building_create(type, x, y);
            b->state = BUILDING_STATE_IN_USE;
            map_building_set(grid_offset, b->id);
            if (type == BUILDING_NATIVE_MEETING) {
                map_building_set(grid_offset + map_grid_delta(1, 0), b->id);
                map_building_set(grid_offset + map_grid_delta(0, 1), b->id);
                map_building_set(grid_offset + map_grid_delta(1, 1), b->id);
            }
            if (type == BUILDING_NATIVE_MONUMENT) {
                map_building_tiles_add(b->id, b->x, b->y, b->size, building_image_get_for_type(type), TERRAIN_BUILDING);
            }
        }
    }
}

void map_natives_check_land(int update_behavior)
{
    map_property_clear_all_native_land();
    if (update_behavior) {
        city_military_decrease_native_attack_duration();
    }

    building_type native_buildings[] = { BUILDING_NATIVE_HUT, BUILDING_NATIVE_HUT_ALT,
        BUILDING_NATIVE_MEETING, BUILDING_NATIVE_WATCHTOWER };

    for (int i = 0; i < sizeof(native_buildings) / sizeof(native_buildings[0]); i++) {
        building_type type = native_buildings[i];
        int size = building_properties_for_type(type)->size;
        int radius = size * 3;
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->state != BUILDING_STATE_IN_USE) {
                continue;
            }
            if (b->sentiment.native_anger >= 100) {
                mark_native_land(b->x, b->y, size, radius);
                if (!city_military_natives_are_retreating() && update_behavior && 
                    has_building_on_native_land(b->x, b->y, size, radius)) {
                    city_military_start_native_attack();
                }
            } else if (update_behavior) {
                b->sentiment.native_anger++;
            }
        }
    }
}

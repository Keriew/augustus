#include "water_supply.h"

#include "building/water_access_runtime.h"

extern "C" {
#include "building/building.h"
#include "building/list.h"
#include "map/building.h"
#include "map/grid.h"
#include "map/terrain.h"
}

extern "C" void map_water_supply_update_buildings(void)
{
    water_access_runtime_refresh();
}

extern "C" void map_water_supply_update_reservoir_fountain(void)
{
    water_access_runtime_refresh();
}

extern "C" int map_water_supply_has_aqueduct_access(int grid_offset)
{
    return water_access_runtime_reservoir_has_network_access(grid_offset);
}

extern "C" void map_water_supply_refresh_large_statue(building *b)
{
    water_access_runtime_refresh_large_statue(b);
}

extern "C" int map_water_supply_is_building_unnecessary(int building_id, int radius)
{
    building *b = building_get(building_id);
    int num_houses = 0;
    int x_min = 0;
    int y_min = 0;
    int x_max = 0;
    int y_max = 0;
    map_grid_get_area(b->x, b->y, 1, radius, &x_min, &y_min, &x_max, &y_max);

    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            int grid_offset = map_grid_offset(xx, yy);
            unsigned int found_building_id = map_building_at(grid_offset);
            if (found_building_id && building_get(found_building_id)->house_size) {
                num_houses++;
                if (!water_access_runtime_tile_has_access(grid_offset, WATER_ACCESS_RUNTIME_TYPE_FOUNTAIN) &&
                    !map_terrain_is(grid_offset, TERRAIN_FOUNTAIN_RANGE)) {
                    return BUILDING_NECESSARY;
                }
            }
        }
    }
    return num_houses ? BUILDING_UNNECESSARY_FOUNTAIN : BUILDING_UNNECESSARY_NO_HOUSES;
}

extern "C" int map_water_supply_fountain_radius(void)
{
    return water_access_runtime_range_for_building(BUILDING_FOUNTAIN);
}

extern "C" int map_water_supply_reservoir_radius(void)
{
    return water_access_runtime_range_for_building(BUILDING_RESERVOIR);
}

extern "C" int map_water_supply_well_radius(void)
{
    return water_access_runtime_range_for_building(BUILDING_WELL);
}

extern "C" int map_water_supply_latrines_radius(void)
{
    return water_access_runtime_range_for_building(BUILDING_LATRINES);
}

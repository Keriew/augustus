#ifndef MAP_WATER_SUPPLY_H
#define MAP_WATER_SUPPLY_H

void map_water_supply_update_buildings(void);
void map_water_supply_update_reservoir_fountain(void);
int map_water_supply_has_aqueduct_access(int grid_offset);

enum {
    BUILDING_NECESSARY = 0,
    BUILDING_UNNECESSARY_FOUNTAIN = 1,
    BUILDING_UNNECESSARY_NO_HOUSES = 2
};

int map_water_supply_is_building_unnecessary(int building_id, int radius);
int map_water_supply_fountain_radius(void);
int map_water_supply_reservoir_radius(void);
int map_water_supply_well_radius(void);
int map_water_supply_latrines_radius(void);

#endif // MAP_WATER_SUPPLY_H

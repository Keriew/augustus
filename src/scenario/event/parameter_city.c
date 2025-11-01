#include "scenario/event/parameter_city.h"

#include "building/count.h"
#include "building/granary.h"
#include "building/warehouse.h"
#include "city/constants.h"
#include "city/emperor.h"
#include "city/finance.h"
#include "city/health.h"
#include "city/labor.h"
#include "city/data.h"
#include "city/population.h"
#include "city/ratings.h"
#include "city/resource.h"
#include "core/calc.h"
#include "empire/city.h"
#include "empire/trade_route.h"
#include "figure/figure.h"
#include "figure/formation.h"
#include "map/grid.h"
#include "map/terrain.h"
#include "scenario/event/parameter_data.h"
#include "game/settings.h"
#include "window/advisors.h"
#include "window/editor/select_city_trade_route.h"
#include "parameter_city.h"

#define RESOURCE_ALL_BUYS RESOURCE_MAX + 1 // max +1 indicates all resources that this trade route buys
#define RESOURCE_ALL_SELLS RESOURCE_MAX + 2 // max +2 indicates all resources that this trade route sells
//above mirros the defines in select_city_trade_route.c

static int resource_count(scenario_action_t *action)
{
    resource_type resource = action->parameter3;
    storage_types storage_type = action->parameter4;
    int respect_settings = action->parameter5;
    switch (storage_type) {
        case STORAGE_TYPE_GRANARIES:
            return building_granaries_count_available_resource(resource, respect_settings, 0);
        case STORAGE_TYPE_WAREHOUSES:
            return  building_warehouses_count_available_resource(resource, respect_settings, 0);
        case STORAGE_TYPE_ALL:
        default:
            return city_resource_get_total_amount(resource, respect_settings);
    }

}

static int building_coverage(scenario_action_t *action)
{
    building_type type = action->parameter3;
    switch (type) {
        case BUILDING_TAVERN:
            return window_advisors_get_tavern_coverage();
        case BUILDING_THEATER:
            return window_advisors_get_theater_coverage();
        case BUILDING_ARENA:
            return window_advisors_get_arena_coverage();
        case BUILDING_AMPHITHEATER:
            return window_advisors_get_amphitheater_coverage();
        case BUILDING_HIPPODROME:
            return window_advisors_get_hippodrome_coverage();
        case BUILDING_COLOSSEUM:
            return window_advisors_get_colosseum_coverage();
        case BUILDING_BATHHOUSE:
            return window_advisors_get_bathhouse_coverage();
        case BUILDING_BARBER:
            return window_advisors_get_barber_coverage();
        case BUILDING_DOCTOR:
            return window_advisors_get_clinic_coverage();
        case BUILDING_HOSPITAL:
            return window_advisors_get_hospital_coverage();
        case BUILDING_SCHOOL:
            return window_advisors_get_school_coverage();
        case BUILDING_LIBRARY:
            return window_advisors_get_library_coverage();
        case BUILDING_ACADEMY:
            return window_advisors_get_academy_coverage();
        default:
            return 0;
    }
}

static int unemployment_rate(scenario_action_t *action)
{
    int is_absolute = action->parameter3;
    if (is_absolute) {
        return city_labor_workers_unemployed();
    } else {
        return city_labor_unemployment_percentage();
    }
}

static int population_by_housing_type(scenario_action_t *action)
{
    house_level level = action->parameter3 - 10; // convert from building_type to house_level
    int is_absolute = action->parameter4;
    int is_group = ((int) level >= (int) HOUSE_GROUP_TENT);
    int total_pop = city_population();
    if (!total_pop) {
        return 0;
    }
    if (!is_group) {
        int pop_at_level = city_population_at_level(level);
        if (is_absolute) {
            return pop_at_level;
        }
        return calc_percentage(pop_at_level, total_pop);
    }
    int min = 0;
    int max = 0;
    int total = 0;
    int level_as_int = (int) level; // Store the int value separately
    switch (level_as_int) { // Use the int variable in switch
        case HOUSE_GROUP_TENT:   min = HOUSE_SMALL_TENT;   max = HOUSE_LARGE_TENT;   break;
        case HOUSE_GROUP_SHACK:  min = HOUSE_SMALL_SHACK;  max = HOUSE_LARGE_SHACK;  break;
        case HOUSE_GROUP_HOVEL:  min = HOUSE_SMALL_HOVEL;  max = HOUSE_LARGE_HOVEL;  break;
        case HOUSE_GROUP_CASA:   min = HOUSE_SMALL_CASA;   max = HOUSE_LARGE_CASA;   break;
        case HOUSE_GROUP_INSULA: min = HOUSE_SMALL_INSULA; max = HOUSE_GRAND_INSULA; break;
        case HOUSE_GROUP_VILLA:  min = HOUSE_SMALL_VILLA;  max = HOUSE_GRAND_VILLA;  break;
        case HOUSE_GROUP_PALACE: min = HOUSE_SMALL_PALACE; max = HOUSE_LUXURY_PALACE; break;
        default:
            return 0;
    }

    for (int i = min; i <= max; i++) {
        total += city_population_at_level(i);
    }
    return is_absolute ? total : calc_percentage(total, total_pop);
}

static int population_by_age(scenario_action_t *action)
{
    int age_group = action->parameter3;
    int is_absolute = action->parameter4;
    int total_pop = city_population();
    int value = 0;
    if (!total_pop) {
        return 0;
    }
    if (age_group >= 0 && age_group < 10) { // decennial age groups
        value = city_population_in_age_decennium(age_group);
    } else if (age_group >= 10) {
        switch (age_group) { // non-decennial age groups
            case 10:
                value = city_population_school_age();
                break;
            case 11:
                value = city_population_academy_age();
                break;
            case 12:
                value = city_labor_workers_employed();
                break;
            case 13:
                value = city_population_retired_people();
                break;
        }
    }
    return is_absolute ? value : calc_percentage(value, total_pop);
}

static int get_building_count(scenario_action_t *action)
{
    building_type type = action->parameter3;
    int active_only = action->parameter4;
    int total_count = 0;
    switch (type) {
        case BUILDING_MENU_FARMS:
            total_count = building_set_count_farms(active_only);
            break;
        case BUILDING_MENU_RAW_MATERIALS:
            total_count = building_set_count_raw_materials(active_only);
            break;
        case BUILDING_MENU_WORKSHOPS:
            total_count = building_set_count_workshops(active_only);
            break;
        case BUILDING_MENU_SMALL_TEMPLES:
            total_count = building_set_count_small_temples(active_only);
            break;
        case BUILDING_MENU_LARGE_TEMPLES:
            total_count = building_set_count_large_temples(active_only);
            break;
        case BUILDING_MENU_GRAND_TEMPLES:
            total_count = building_count_grand_temples_active();
            break;
        case BUILDING_MENU_TREES:
            total_count = building_set_count_deco_trees();
            break;
        case BUILDING_MENU_PATHS:
            total_count = building_set_count_deco_paths();
            break;
        case BUILDING_MENU_PARKS:
            total_count = building_set_count_deco_statues();
            break;
        case BUILDING_ANY:
            total_count = building_count_any_total(active_only);
            break;
        case BUILDING_ROAD:
            total_count = building_count_roads();
            break;
        case BUILDING_HIGHWAY:
            total_count = building_count_highway();
            break;
        case BUILDING_PLAZA:
            total_count = building_count_plaza();
            break;
        case BUILDING_GARDENS:
            total_count = building_count_gardens(0);
            break;
        case BUILDING_OVERGROWN_GARDENS:
            total_count = building_count_gardens(1);
            break;
        case BUILDING_LOW_BRIDGE:
            total_count = building_count_bridges(0);
            break;
        case BUILDING_SHIP_BRIDGE:
            total_count = building_count_bridges(1);
            break;
        default:
            total_count = active_only ? building_count_active(type) : building_count_total(type);
            break;
    }
    return total_count;
}

static int get_player_soldiers_count(scenario_action_t *action)
{
    figure_type type = action->parameter3;
    return formation_legion_count_alive_soldiers_by_type(type);
}

static int get_enemy_troops_count(scenario_action_t *action)
{
    enemy_class_t enemy_class = action->parameter3;
    int count = 0;
    for (int i = 1; i < figure_count(); i++) {    // Iterate through all figures to count enemy troops
        figure *f = figure_get(i);
        if (!figure_is_enemy(f) || figure_is_dead(f)) {
            continue;
        }
        switch (enemy_class) {
            case ENEMY_CLASS_MELEE:
                count += figure_is_melee_enemy(f);
                break;
            case ENEMY_CLASS_RANGED:
                count += figure_is_ranged_enemy(f);
                break;
            case ENEMY_CLASS_MOUNTED:
                count += figure_is_mounted_enemy(f);
                break;
            case ENEMY_CLASS_ALL:
                count++;
                break;
        }
    }
    return count;
}

static int get_terrain_tiles_count(scenario_action_t *action)
{
    // TODO: this probably should go as global somewhere
    int terrain_type = action->parameter3;
    int count = 0;
    int width, height;
    map_grid_size(&width, &height);
    for (int y = 0; y < height; y++) {    // Iterate through all map tiles
        for (int x = 0; x < width; x++) {
            int grid_offset = map_grid_offset(x, y);
            if (map_terrain_is(grid_offset, terrain_type)) {
                count++;
            }
        }
    }
    return count;
}

static int city_trade_quota_fill_percentage(scenario_action_t *action)
{
    // Decode the encoded route+resource value from parameter4
    int encoded_value = action->parameter4;
    unsigned int trade_route_id = window_editor_select_city_trade_route_decode_route_id(encoded_value);
    unsigned int resource_id = window_editor_select_city_trade_route_decode_resource_id(encoded_value);
    unsigned int is_absolute = action->parameter5;
    int traded = 0;
    int limit = 0;
    int city_id = empire_city_get_for_trade_route(trade_route_id);

    if (resource_id == RESOURCE_ALL_BUYS || resource_id == RESOURCE_ALL_SELLS) {
        for (resource_type r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
            int buys = empire_city_buys_resource(city_id, r);
            int sells = empire_city_sells_resource(city_id, r);

            if ((resource_id == RESOURCE_ALL_BUYS && buys) || (resource_id == RESOURCE_ALL_SELLS && sells)) {
                traded += trade_route_traded(trade_route_id, r);
                limit += trade_route_limit(trade_route_id, r);
            }
        }
    } else {
        traded = trade_route_traded(trade_route_id, resource_id);
        limit = trade_route_limit(trade_route_id, resource_id);
    }
    if (is_absolute) {
        return traded;
    }
    return limit == 0 ? 0 : calc_percentage(traded, limit);
}



int scenario_event_parameter_city_for_action(scenario_action_t *action)
{
    city_property_t type = action->parameter2;
    switch (type) { // Simple properties - direct return values
        case CITY_PROPERTY_DIFFICULTY:
            return setting_difficulty();
        case CITY_PROPERTY_MONEY:
            return city_finance_treasury();
        case CITY_PROPERTY_POPULATION:
            return city_population();
        case CITY_PROPERTY_SAVINGS:
            return city_emperor_personal_savings();
        case CITY_PROPERTY_YEAR_FINANCE_BALANCE:
            return city_finance_overview_last_year()->net_in_out;
        case CITY_PROPERTY_STATS_FAVOR:
            return city_rating_favor();
        case CITY_PROPERTY_STATS_PROSPERITY:
            return city_rating_prosperity();
        case CITY_PROPERTY_STATS_CULTURE:
            return city_rating_culture();
        case CITY_PROPERTY_STATS_PEACE:
            return city_rating_peace();
        case CITY_PROPERTY_STATS_CITY_HEALTH:
            return city_health();
        case CITY_PROPERTY_ROME_WAGES:
            return city_labor_wages_rome();
        case CITY_PROPERTY_CITY_WAGES:
            return city_labor_wages();

            // Complex properties - require additional parameters
        case CITY_PROPERTY_RESOURCE_STOCK:
            return resource_count(action);
        case CITY_PROPERTY_SERVICE_COVERAGE:
            return building_coverage(action);
        case CITY_PROPERTY_POPS_UNEMPLOYMENT:
            return unemployment_rate(action);
        case CITY_PROPERTY_POPS_HOUSING_TYPE:
            return population_by_housing_type(action);
        case CITY_PROPERTY_POPS_BY_AGE:
            return population_by_age(action);
        case CITY_PROPERTY_BUILDING_COUNT:
            return get_building_count(action);
        case CITY_PROPERTY_TROOPS_COUNT_PLAYER:
            return get_player_soldiers_count(action);
        case CITY_PROPERTY_TROOPS_COUNT_ENEMY:
            return get_enemy_troops_count(action);
        case CITY_PROPERTY_TERRAIN_COUNT_TILES:
            return get_terrain_tiles_count(action);
        case CITY_PROPERTY_QUOTA_FILL:
            return city_trade_quota_fill_percentage(action);
        case CITY_PROPERTY_NONE:
        case CITY_PROPERTY_MAX:
        default:
            return 0;
    }
}

// Function returning parameter info for each city property
city_property_info_t city_property_get_param_info(city_property_t type)
{
    city_property_info_t info = { 0, {PARAMETER_TYPE_UNDEFINED} }; // default: no params

    switch (type) {
        // --- Complex properties: have enum-based parameter requirements ---
        case CITY_PROPERTY_RESOURCE_STOCK:
            info.count = 3;
            info.param_types[0] = PARAMETER_TYPE_RESOURCE;
            info.param_types[1] = PARAMETER_TYPE_STORAGE_TYPE;
            info.param_types[2] = PARAMETER_TYPE_BOOLEAN; // respect settings
            info.param_keys[0] = TR_PARAMETER_TYPE_RESOURCE;
            info.param_keys[1] = TR_PARAMETER_TYPE_STORAGE_TYPE;
            info.param_keys[2] = TR_PARAMETER_RESPECT_SETTINGS;
            info.param_names[0] = "resource";
            info.param_names[1] = "storage_type";
            info.param_names[2] = "respect_settings";
            break;

        case CITY_PROPERTY_SERVICE_COVERAGE:
            info.count = 1;
            info.param_types[0] = PARAMETER_TYPE_COVERAGE_BUILDINGS;
            info.param_keys[0] = TR_CITY_PROPERTY_SERVICE_COVERAGE;
            info.param_names[0] = "coverage";
            break;

        case CITY_PROPERTY_POPS_UNEMPLOYMENT:
            info.count = 1;
            info.param_types[0] = PARAMETER_TYPE_PERCENTAGE;
            info.param_keys[0] = TR_PARAMETER_PERCENTAGE;
            info.param_names[0] = "percentage_type";
            break;

        case CITY_PROPERTY_POPS_HOUSING_TYPE:
            info.count = 2;
            info.param_types[0] = PARAMETER_TYPE_HOUSING_TYPE;
            info.param_types[1] = PARAMETER_TYPE_PERCENTAGE;
            info.param_keys[0] = TR_CITY_PROPERTY_POPS_HOUSING_TYPE;
            info.param_keys[1] = TR_PARAMETER_PERCENTAGE;
            info.param_names[0] = "housing_type";
            info.param_names[1] = "percentage_type";
            break;

        case CITY_PROPERTY_POPS_BY_AGE:
            info.count = 2;
            info.param_types[0] = PARAMETER_TYPE_AGE_GROUP;
            info.param_types[1] = PARAMETER_TYPE_PERCENTAGE;
            info.param_keys[0] = TR_CITY_PROPERTY_POPS_BY_AGE;
            info.param_keys[1] = TR_PARAMETER_PERCENTAGE;
            info.param_names[0] = "age_group";
            info.param_names[1] = "percentage_type";
            break;

        case CITY_PROPERTY_BUILDING_COUNT:
            info.count = 2;
            info.param_types[0] = PARAMETER_TYPE_BUILDING;
            info.param_types[1] = PARAMETER_TYPE_BOOLEAN; // active only or all
            info.param_keys[0] = TR_PARAMETER_TYPE_ALLOWED_BUILDING;
            info.param_keys[1] = TR_CITY_PROPERTY_ACTIVE_ONLY;
            info.param_names[0] = "building";
            info.param_names[1] = "active_only";
            break;

        case CITY_PROPERTY_TROOPS_COUNT_PLAYER:
            info.count = 1;
            info.param_types[0] = PARAMETER_TYPE_PLAYER_TROOPS;
            info.param_keys[0] = TR_CITY_PROPERTY_TROOPS_COUNT_PLAYER;
            info.param_names[0] = "troop_type";
            break;

        case CITY_PROPERTY_TROOPS_COUNT_ENEMY:
            info.count = 1;
            info.param_types[0] = PARAMETER_TYPE_ENEMY_CLASS;
            info.param_keys[0] = TR_CITY_PROPERTY_TROOPS_COUNT_ENEMY;
            info.param_names[0] = "enemy_class";
            break;

        case CITY_PROPERTY_TERRAIN_COUNT_TILES:
            info.count = 1;
            info.param_types[0] = PARAMETER_TYPE_TERRAIN;
            info.param_keys[0] = TR_PARAMETER_TERRAIN;
            info.param_names[0] = "terrain_type";
            break;
        case CITY_PROPERTY_QUOTA_FILL:
            info.count = 3;
            info.param_types[0] = PARAMETER_TYPE_ROUTE;
            info.param_types[1] = PARAMETER_TYPE_ROUTE_RESOURCE;
            info.param_types[2] = PARAMETER_TYPE_PERCENTAGE;
            info.param_keys[0] = TR_PARAMETER_TYPE_ROUTE;
            info.param_keys[1] = TR_PARAMETER_TYPE_RESOURCE;
            info.param_keys[2] = TR_PARAMETER_PERCENTAGE;
            info.param_names[0] = "route";
            info.param_names[1] = "resource";
            info.param_names[2] = "percentage_type";
            break;
            // --- Invalid / none ---
        case CITY_PROPERTY_NONE:
        case CITY_PROPERTY_MAX:
        default:
            info.count = 0;
            break;
    }

    return info;
}


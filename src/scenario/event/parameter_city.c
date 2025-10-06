#include "scenario/event/parameter_city.h"

#include "city/constants.h"
#include "city/emperor.h"
#include "city/finance.h"
#include "city/health.h"
#include "city/labor.h"
#include "city/data.h"
#include "city/population.h"
#include "city/ratings.h"
#include "scenario/event/parameter_data.h"
#include "game/settings.h"

int process_parameter(city_property_t type)
{
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
            // TODO: Handle resource stock parameter (requires resource type)
            return 0;
        case CITY_PROPERTY_SERVICE_COVERAGE:
            // TODO: Handle service coverage parameter (requires service type)
            return 0;
        case CITY_PROPERTY_POPS_UNEMPLOYMENT:
            // TODO: Handle unemployment parameter (requires population class)
            return 0;
        case CITY_PROPERTY_POPS_HOUSING_TYPE:
            // TODO: Handle housing type parameter (requires housing type)
            return 0;
        case CITY_PROPERTY_POPS_BY_AGE:
            // TODO: Handle population by age parameter (requires age group)
            return 0;
        case CITY_PROPERTY_BUILDING_COUNT:
            // TODO: Handle active building count parameter (requires building type)
            return 0;
        case CITY_PROPERTY_TROOPS_COUNT_PLAYER:
            // TODO: Handle player troops count parameter (requires troop type)
            return 0;
        case CITY_PROPERTY_TROOPS_COUNT_ENEMY:
            // TODO: Handle enemy troops count parameter (requires troop type)
            return 0;
        case CITY_PROPERTY_TERRAIN_COUNT_TILES:
            // TODO: Handle terrain tiles count parameter (requires terrain type)
            return 0;

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
            break;

        case CITY_PROPERTY_SERVICE_COVERAGE:
            info.count = 1;
            info.param_types[0] = PARAMETER_TYPE_COVERAGE_BUILDINGS;
            break;

        case CITY_PROPERTY_POPS_UNEMPLOYMENT:
            info.count = 1;
            info.param_types[0] = PARAMETER_TYPE_PERCENTAGE;
            break;

        case CITY_PROPERTY_POPS_HOUSING_TYPE:
            info.count = 2;
            info.param_types[0] = PARAMETER_TYPE_HOUSING_TYPE;
            info.param_types[1] = PARAMETER_TYPE_PERCENTAGE;
            break;

        case CITY_PROPERTY_POPS_BY_AGE:
            info.count = 2;
            info.param_types[0] = PARAMETER_TYPE_AGE_GROUP;
            info.param_types[1] = PARAMETER_TYPE_PERCENTAGE;
            break;

        case CITY_PROPERTY_BUILDING_COUNT:
            info.count = 2;
            info.param_types[0] = PARAMETER_TYPE_BUILDING;
            info.param_types[1] = PARAMETER_TYPE_BOOLEAN; // active only or all
            break;

        case CITY_PROPERTY_TROOPS_COUNT_PLAYER:
            info.count = 1;
            info.param_types[0] = PARAMETER_TYPE_PLAYER_TROOPS;
            break;

        case CITY_PROPERTY_TROOPS_COUNT_ENEMY:
            info.count = 1;
            info.param_types[0] = PARAMETER_TYPE_ENEMY_CLASS;
            break;

        case CITY_PROPERTY_TERRAIN_COUNT_TILES:
            info.count = 1;
            info.param_types[0] = PARAMETER_TYPE_TERRAIN;
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

int city_property_execute(city_property_t type, int value)
{
    return process_parameter(type);
}
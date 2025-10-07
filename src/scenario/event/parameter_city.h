#ifndef SCENARIO_EVENT_CITY_PROPERTY_H
#define SCENARIO_EVENT_CITY_PROPERTY_H

#include "scenario/event/parameter_data.h"

typedef enum {
    CITY_PROPERTY_NONE,
    CITY_PROPERTY_DIFFICULTY,
    CITY_PROPERTY_MONEY,
    CITY_PROPERTY_POPULATION,
    CITY_PROPERTY_SAVINGS,
    CITY_PROPERTY_YEAR_FINANCE_BALANCE,
    CITY_PROPERTY_RESOURCE_STOCK, //complex
    // stats
    CITY_PROPERTY_STATS_FAVOR,
    CITY_PROPERTY_STATS_PROSPERITY,
    CITY_PROPERTY_STATS_CULTURE,
    CITY_PROPERTY_STATS_PEACE,
    CITY_PROPERTY_STATS_CITY_HEALTH,
    // service coverage
    CITY_PROPERTY_SERVICE_COVERAGE,//complex
    // Population
    CITY_PROPERTY_POPS_UNEMPLOYMENT,//complex
    CITY_PROPERTY_POPS_HOUSING_TYPE,//complex
    CITY_PROPERTY_POPS_BY_AGE,//complex
    CITY_PROPERTY_ROME_WAGES,
    CITY_PROPERTY_CITY_WAGES,
    // buildings
    CITY_PROPERTY_BUILDING_COUNT,
    //troops
    CITY_PROPERTY_TROOPS_COUNT_PLAYER,//complex
    CITY_PROPERTY_TROOPS_COUNT_ENEMY,//complex
    //terrain
    CITY_PROPERTY_TERRAIN_COUNT_TILES,//complex
    CITY_PROPERTY_MAX,
} city_property_t;

// Struct describing how many and what kind of extra parameters a property requires
typedef struct
{
    int count;                     // how many parameters needed
    parameter_type param_types[3]; // what enum type each param comes from (up to 3)
    int param_keys[3];  // translation keys for parameter types, if applicable
} city_property_info_t;

/**
 * Get parameter info for a city property
 * @param type The city property type
 * @return Info struct with count and parameter types needed
 */
city_property_info_t city_property_get_param_info(city_property_t type);

#endif // SCENARIO_EVENT_CITY_PROPERTY_H
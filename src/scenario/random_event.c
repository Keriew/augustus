#include "random_event.h"

#include "building/destruction.h"
#include "building/monument.h"
#include "city/data_private.h"
#include "city/finance.h"
#include "city/gods.h"
#include "city/health.h"
#include "city/labor.h"
#include "city/message.h"
#include "city/population.h"
#include "city/trade.h"
#include "core/config.h"
#include "core/random.h"
#include "game/difficulty.h"
#include "scenario/data.h"
#include "scenario/property.h"

enum {
    //NONE = 0                      // 98   98/128*100=76.56%   //before 102/128*100=79.69
    EVENT_ROME_RAISES_WAGES = 1,    // 3    3/128*100=2.34%
    EVENT_ROME_LOWERS_WAGES = 2,    // 3    3/128*100=2.34%
    EVENT_LAND_TRADE_DISRUPTED = 3, // 5    5/128*100=3.91%
    EVENT_LAND_SEA_DISRUPTED = 4,   // 4    4/128*100=3.13%
    EVENT_CONTAMINATED_WATER = 5,   // 2    2/128*100=1.56%
    EVENT_IRON_MINE_COLLAPSED = 6,  // 4    4/128*100=3.13%
    EVENT_CLAY_PIT_FLOODED = 7,     // 5    5/128*100=3.91%
    EVENT_QUARRY_COLLAPSED = 8      // 4    4/128*100=3.13%

};

#define COOLDOWN_MONTHS_ROME_WAGE_CHANGE 12

static const int RANDOM_EVENT_PROBABILITY[128] = {
    0, 0, 1, 0, 0, 0, 4, 0, 8, 0, 0, 3, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 0, 8, 0, 0, 6,
    0, 0, 2, 0, 0, 0, 7, 0, 5, 0, 0, 7, 0, 0, 0, 0,
    0, 7, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
    6, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0,
    0, 7, 0, 1, 6, 0, 0, 0, 0, 0, 2, 0, 0, 4, 0, 8,
    0, 0, 3, 0, 7, 4, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0
};

static const int mines[] = {
    BUILDING_IRON_MINE,
    BUILDING_GOLD_MINE
};

static const int pits[] = {
    BUILDING_CLAY_PIT,
    BUILDING_SAND_PIT
};

static const int quarries[] = {
    BUILDING_STONE_QUARRY,
    BUILDING_MARBLE_QUARRY
};

static void raise_wages(void)
{
    if (scenario.random_events.raise_wages &&
        city_data.labor.months_since_last_wage_change > COOLDOWN_MONTHS_ROME_WAGE_CHANGE) {
        if (city_labor_raise_wages_rome()) {
            city_data.labor.months_since_last_wage_change = 0;
            city_message_post(1, MESSAGE_ROME_RAISES_WAGES, 0, 0);
        }
    }
}

static void lower_wages(void)
{
    if (scenario.random_events.lower_wages &&
        city_data.labor.months_since_last_wage_change > COOLDOWN_MONTHS_ROME_WAGE_CHANGE) {
        if (city_labor_lower_wages_rome()) {
            city_data.labor.months_since_last_wage_change = 0;
            city_message_post(1, MESSAGE_ROME_LOWERS_WAGES, 0, 0);
        }
    }
}

static void disrupt_land_trade(void)
{
    if (scenario.random_events.land_trade_problem) {
        if (city_trade_has_land_trade_route() &&
            city_data.trade.months_since_last_land_trade_problem > difficulty_random_event_cooldown_months()) {
            city_trade_start_land_trade_problems(48);
            city_data.trade.months_since_last_land_trade_problem = 0;
            if (scenario_property_climate() == CLIMATE_DESERT) {
                city_message_post(1, MESSAGE_LAND_TRADE_DISRUPTED_SANDSTORMS, 0, 0);
            } else {
                city_message_post(1, MESSAGE_LAND_TRADE_DISRUPTED_LANDSLIDES, 0, 0);
            }
        }
    }
}

static void disrupt_sea_trade(void)
{
    if (scenario.random_events.sea_trade_problem) {
        if (city_trade_has_sea_trade_route() &&
            city_data.trade.months_since_last_sea_trade_problem > difficulty_random_event_cooldown_months()) {
            city_trade_start_sea_trade_problems(48);
            city_data.trade.months_since_last_sea_trade_problem = 0;
            city_message_post(1, MESSAGE_SEA_TRADE_DISRUPTED, 0, 0);
        }
    }
}

static void contaminate_water(void)
{
    if (scenario.random_events.contaminated_water) {
        if (city_population() > 200 &&
            city_data.health.months_since_last_contaminated_water > difficulty_random_event_cooldown_months()) {
            int change;
            int health_rate = city_health();
            if (health_rate > 80) {
                change = -40;
            } else if (health_rate > 60) {
                change = -30;
            } else {
                change = -20;
            }
            city_health_change(change);
            city_data.health.months_since_last_contaminated_water = 0;
            city_message_post(1, MESSAGE_CONTAMINATED_WATER, 0, 0);
        }
    }
}

static void destroy_iron_mine(void)
{
    if (scenario.random_events.iron_mine_collapse &&
        city_data.building.months_since_last_destroyed_iron_mine > difficulty_random_event_cooldown_months()) {
        int building_type = mines[random_byte() % (sizeof(mines) / sizeof(mines[0]))];
        if (!building_find(building_type)) {
            building_type = 0;
            for (int i = 0; i < sizeof(mines) / sizeof(mines[0]); i++) {
                if (building_find(mines[i])) {
                    building_type = mines[i];
                    break;
                }
            }
        }
        if (!building_type) {
            return;
        }
        if (config_get(CONFIG_GP_CH_RANDOM_COLLAPSES_TAKE_MONEY)) {
            city_finance_process_sundry(250);
            city_message_post(1, MESSAGE_MINE_COLLAPSED, 0, 0);
        } else {
            int grid_offset = building_destroy_first_of_type(building_type);
            if (grid_offset) {
                city_message_post(1, MESSAGE_MINE_COLLAPSED, 0, grid_offset);
            }
        }
        city_data.building.months_since_last_destroyed_iron_mine = 0;
    }
}

static void destroy_clay_pit(void)
{
    if (scenario.random_events.clay_pit_flooded &&
        city_data.building.months_since_last_flooded_clay_pit > difficulty_random_event_cooldown_months()) {
        int building_type = pits[random_byte() % (sizeof(pits) / sizeof(pits[0]))];
        if (!building_find(building_type)) {
            building_type = 0;
            for (int i = 0; i < sizeof(pits) / sizeof(pits[0]); i++) {
                if (building_find(pits[i])) {
                    building_type = pits[i];
                    break;
                }
            }
        }
        if (!building_type) {
            return;
        }
        if (config_get(CONFIG_GP_CH_RANDOM_COLLAPSES_TAKE_MONEY)) {
            city_finance_process_sundry(250);
            city_message_post(1, MESSAGE_PIT_COLLAPSED, 0, 0);
        } else {
            int grid_offset = building_destroy_first_of_type(building_type);
            if (grid_offset) {
                city_message_post(1, MESSAGE_PIT_COLLAPSED, 0, grid_offset);
            }
        }
        city_data.building.months_since_last_flooded_clay_pit = 0;
    }
}

static void destroy_quarry(void)
{
    if (scenario.random_events.quarry_collapse &&
        city_data.building.months_since_last_destroyed_quarry > difficulty_random_event_cooldown_months()) {
        int building_type = quarries[random_byte() % (sizeof(quarries) / sizeof(quarries[0]))];
        if (!building_find(building_type)) {
            building_type = 0;
            for (int i = 0; i < sizeof(quarries) / sizeof(quarries[0]); i++) {
                if (building_find(quarries[i])) {
                    building_type = quarries[i];
                    break;
                }
            }
        }
        if (!building_type) {
            return;
        }
        if (config_get(CONFIG_GP_CH_RANDOM_COLLAPSES_TAKE_MONEY)) {
            city_finance_process_sundry(250);
            city_message_post(1, MESSAGE_QUARRY_COLLAPSED, 0, 0);
        } else {
            int grid_offset = building_destroy_first_of_type(building_type);
            if (grid_offset) {
                city_message_post(1, MESSAGE_QUARRY_COLLAPSED, 0, grid_offset);
            }
        }
        city_data.building.months_since_last_destroyed_quarry = 0;
    }
}

static void increase_month_since_last_random_event(void)
{
    city_data.labor.months_since_last_wage_change++;
    city_data.trade.months_since_last_land_trade_problem++;
    city_data.trade.months_since_last_sea_trade_problem++;
    city_data.health.months_since_last_contaminated_water++;
    city_data.building.months_since_last_destroyed_iron_mine++;
    city_data.building.months_since_last_flooded_clay_pit++;
    city_data.building.months_since_last_destroyed_quarry++;
}

static int all_gods_happy(void)
{
    for (int god = 0; god < MAX_GODS; god++) {
        if (city_god_happiness(god) < 60) {
            return 0;
        }
    }
    return 1;
}

void scenario_random_event_process(void)
{
    increase_month_since_last_random_event();
    int skip_event = building_monument_working(BUILDING_PANTHEON) && all_gods_happy();
    int event = RANDOM_EVENT_PROBABILITY[random_byte()];
    switch (event) {
        case EVENT_ROME_RAISES_WAGES:
            raise_wages();
            break;
        case EVENT_ROME_LOWERS_WAGES:
            lower_wages();
            break;
        case EVENT_LAND_TRADE_DISRUPTED:
            disrupt_land_trade();
            break;
        case EVENT_LAND_SEA_DISRUPTED:
            disrupt_sea_trade();
            break;
        case EVENT_CONTAMINATED_WATER:
            if (!skip_event) {
                contaminate_water();
            }
            break;
        case EVENT_IRON_MINE_COLLAPSED:
            if (!skip_event) {
                destroy_iron_mine();
            }
            break;
        case EVENT_CLAY_PIT_FLOODED:
            if (!skip_event) {
                destroy_clay_pit();
            }
            break;
        case EVENT_QUARRY_COLLAPSED:
            if (!skip_event) {
                destroy_quarry();
            }
            break;
    }
}

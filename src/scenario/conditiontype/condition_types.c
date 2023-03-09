#include "condition_types.h"

#include "scenario/conditiontype/conditiontype_building_count_active.h"
#include "scenario/conditiontype/conditiontype_city_population.h"
#include "scenario/conditiontype/conditiontype_count_own_troops.h"
#include "scenario/conditiontype/conditiontype_difficulty.h"
#include "scenario/conditiontype/conditiontype_money.h"
#include "scenario/conditiontype/conditiontype_population_unemployed.h"
#include "scenario/conditiontype/conditiontype_request_is_ongoing.h"
#include "scenario/conditiontype/conditiontype_rome_wages.h"
#include "scenario/conditiontype/conditiontype_savings.h"
#include "scenario/conditiontype/conditiontype_stats_city_health.h"
#include "scenario/conditiontype/conditiontype_stats_culture.h"
#include "scenario/conditiontype/conditiontype_stats_favor.h"
#include "scenario/conditiontype/conditiontype_stats_peace.h"
#include "scenario/conditiontype/conditiontype_stats_prosperity.h"
#include "scenario/conditiontype/conditiontype_time.h"
#include "scenario/conditiontype/conditiontype_trade_sell_price.h"

void scenario_condition_init(scenario_condition_t *condition)
{
    switch (condition->type) {
        case CONDITION_TYPE_BUILDING_COUNT_ACTIVE:
            scenario_condition_building_count_active_init(condition);
            break;
        case CONDITION_TYPE_CITY_POPULATION:
            scenario_condition_city_population_init(condition);
            break;
        case CONDITION_TYPE_COUNT_OWN_TROOPS:
            scenario_condition_count_own_troops_init(condition);
            break;
        case CONDITION_TYPE_DIFFICULTY:
            scenario_condition_difficulty_init(condition);
            break;
        case CONDITION_TYPE_MONEY:
            scenario_condition_money_init(condition);
            break;
        case CONDITION_TYPE_POPS_UNEMPLOYMENT:
            scenario_condition_population_unemployed_init(condition);
            break;
        case CONDITION_TYPE_REQUEST_IS_ONGOING:
            scenario_condition_request_is_ongoing_init(condition);
            break;
        case CONDITION_TYPE_ROME_WAGES:
            scenario_condition_rome_wages_init(condition);
            break;
        case CONDITION_TYPE_SAVINGS:
            scenario_condition_savings_init(condition);
            break;
        case CONDITION_TYPE_STATS_CITY_HEALTH:
            scenario_condition_stats_city_health_init(condition);
            break;
        case CONDITION_TYPE_STATS_CULTURE:
            scenario_condition_stats_culture_init(condition);
            break;
        case CONDITION_TYPE_STATS_FAVOR:
            scenario_condition_stats_favor_init(condition);
            break;
        case CONDITION_TYPE_STATS_PEACE:
            scenario_condition_stats_peace_init(condition);
            break;
        case CONDITION_TYPE_STATS_PROSPERITY:
            scenario_condition_stats_prosperity_init(condition);
            break;
        case CONDITION_TYPE_TIME_PASSED:
            scenario_condition_time_init(condition);
            break;
        case CONDITION_TYPE_TRADE_SELL_PRICE:
            scenario_condition_trade_sell_price_init(condition);
            break;
        default:
            break;
    }
}

int scenario_condition_is_met(scenario_condition_t *condition)
{
    switch (condition->type) {
        case CONDITION_TYPE_BUILDING_COUNT_ACTIVE: return scenario_condition_building_count_active_met(condition);
        case CONDITION_TYPE_CITY_POPULATION: return scenario_condition_city_population_met(condition);
        case CONDITION_TYPE_COUNT_OWN_TROOPS: return scenario_condition_count_own_troops_met(condition);
        case CONDITION_TYPE_DIFFICULTY: return scenario_condition_difficulty_met(condition);
        case CONDITION_TYPE_MONEY: return scenario_condition_money_met(condition);
        case CONDITION_TYPE_POPS_UNEMPLOYMENT: return scenario_condition_population_unemployed_met(condition);
        case CONDITION_TYPE_REQUEST_IS_ONGOING: return scenario_condition_request_is_ongoing_met(condition);
        case CONDITION_TYPE_ROME_WAGES: return scenario_condition_rome_wages_met(condition);
        case CONDITION_TYPE_SAVINGS: return scenario_condition_savings_met(condition);
        case CONDITION_TYPE_STATS_CITY_HEALTH: return scenario_condition_stats_city_health_met(condition);
        case CONDITION_TYPE_STATS_CULTURE: return scenario_condition_stats_culture_met(condition);
        case CONDITION_TYPE_STATS_FAVOR: return scenario_condition_stats_favor_met(condition);
        case CONDITION_TYPE_STATS_PEACE: return scenario_condition_stats_peace_met(condition);
        case CONDITION_TYPE_STATS_PROSPERITY: return scenario_condition_stats_prosperity_met(condition);
        case CONDITION_TYPE_TIME_PASSED: return scenario_condition_time_met(condition);
        case CONDITION_TYPE_TRADE_SELL_PRICE: return scenario_condition_trade_sell_price_met(condition);
        default:
            return 0;
            break;
    }
}

void scenario_condition_save_state(buffer *buf, scenario_condition_t *condition, int link_type, int32_t link_id)
{
    buffer_write_i16(buf, link_type);
    buffer_write_i32(buf, link_id);
    buffer_write_i16(buf, condition->type);
    buffer_write_i32(buf, condition->parameter1);
    buffer_write_i32(buf, condition->parameter2);
    buffer_write_i32(buf, condition->parameter3);
    buffer_write_i32(buf, condition->parameter4);
    buffer_write_i32(buf, condition->parameter5);
}

void scenario_condition_load_state(buffer *buf,
    int version, scenario_condition_t *condition, int *link_type, int32_t *link_id)
{
    *link_type = buffer_read_i16(buf);
    *link_id = buffer_read_i32(buf);
    condition->type = buffer_read_i16(buf);
    condition->parameter1 = buffer_read_i32(buf);
    condition->parameter2 = buffer_read_i32(buf);
    condition->parameter3 = buffer_read_i32(buf);
    condition->parameter4 = buffer_read_i32(buf);
    condition->parameter5 = buffer_read_i32(buf);
}

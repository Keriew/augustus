#include "action_types.h"

#include "scenario/actiontype/actiontype_change_allowed_buildings.h"
#include "scenario/actiontype/actiontype_change_resource_produced.h"
#include "scenario/actiontype/actiontype_city_health.h"
#include "scenario/actiontype/actiontype_favor.h"
#include "scenario/actiontype/actiontype_empire_map_convert_future_trade_city.h"
#include "scenario/actiontype/actiontype_gladiator_revolt.h"
#include "scenario/actiontype/actiontype_money.h"
#include "scenario/actiontype/actiontype_request_immediately_start.h"
#include "scenario/actiontype/actiontype_rome_wages.h"
#include "scenario/actiontype/actiontype_savings.h"
#include "scenario/actiontype/actiontype_send_standard_message.h"
#include "scenario/actiontype/actiontype_trade_price.h"
#include "scenario/actiontype/actiontype_trade_problem_land.h"
#include "scenario/actiontype/actiontype_trade_problem_sea.h"
#include "scenario/actiontype/actiontype_trade_route_amount.h"
#include "scenario/actiontype/actiontype_trade_price_set.h"

void scenario_action_init(scenario_action_t *action)
{
    switch (action->type) {
        case ACTION_TYPE_ADJUST_CITY_HEALTH:
            scenario_event_action_city_health_init(action);
            break;
        case ACTION_TYPE_ADJUST_MONEY:
            scenario_event_action_money_add_init(action);
            break;
        case ACTION_TYPE_ADJUST_ROME_WAGES:
            scenario_event_action_rome_wages_init(action);
            break;
        case ACTION_TYPE_ADJUST_SAVINGS:
            scenario_event_action_savings_add_init(action);
            break;
        default:
            break;
    }
}

int scenario_action_execute(scenario_action_t *action)
{
    switch (action->type) {
        case ACTION_TYPE_ADJUST_CITY_HEALTH: return scenario_event_action_city_health_execute(action);
        case ACTION_TYPE_ADJUST_FAVOR: return scenario_event_action_favor_add_execute(action);
        case ACTION_TYPE_ADJUST_ROME_WAGES: return scenario_event_action_rome_wages_execute(action);
        case ACTION_TYPE_ADJUST_MONEY: return scenario_event_action_money_add_execute(action);
        case ACTION_TYPE_ADJUST_SAVINGS: return scenario_event_action_savings_add_execute(action);
        case ACTION_TYPE_CHANGE_ALLOWED_BUILDINGS: return scenario_event_action_change_allowed_buildings_execute(action);
        case ACTION_TYPE_CHANGE_RESOURCE_PRODUCED: return scenario_event_action_change_resource_produced_execute(action);
        case ACTION_TYPE_EMPIRE_MAP_CONVERT_FUTURE_TRADE_CITY: return scenario_event_action_empire_map_convert_future_trade_city_execute(action);
        case ACTION_TYPE_GLADIATOR_REVOLT: return scenario_event_action_gladiator_revolt_execute(action);
        case ACTION_TYPE_REQUEST_IMMEDIATELY_START: return scenario_event_action_request_immediately_start_execute(action);
        case ACTION_TYPE_SEND_STANDARD_MESSAGE: return scenario_event_action_send_standard_message_execute(action);
        case ACTION_TYPE_TRADE_ADJUST_PRICE: return scenario_event_action_trade_price_adjust_execute(action);
        case ACTION_TYPE_TRADE_ADJUST_ROUTE_AMOUNT: return scenario_event_action_trade_route_amount_execute(action);
        case ACTION_TYPE_TRADE_PROBLEM_LAND: return scenario_event_action_trade_problems_land_execute(action);
        case ACTION_TYPE_TRADE_PROBLEM_SEA: return scenario_event_action_trade_problems_sea_execute(action);
        case ACTION_TYPE_TRADE_SET_PRICE: return scenario_event_action_trade_price_set_execute(action);
        default:
            return 0;
            break;
    }
}

void scenario_action_save_state(buffer *buf, scenario_action_t *action, int link_type, int32_t link_id)
{
    buffer_write_i16(buf, link_type);
    buffer_write_i32(buf, link_id);
    buffer_write_i16(buf, action->type);
    buffer_write_i32(buf, action->parameter1);
    buffer_write_i32(buf, action->parameter2);
    buffer_write_i32(buf, action->parameter3);
    buffer_write_i32(buf, action->parameter4);
    buffer_write_i32(buf, action->parameter5);
}

void scenario_action_load_state(buffer *buf,
    int version, scenario_action_t *action, int *link_type, int32_t *link_id)
{
    *link_type = buffer_read_i16(buf);
    *link_id = buffer_read_i32(buf);
    action->type = buffer_read_i16(buf);
    action->parameter1 = buffer_read_i32(buf);
    action->parameter2 = buffer_read_i32(buf);
    action->parameter3 = buffer_read_i32(buf);
    action->parameter4 = buffer_read_i32(buf);
    action->parameter5 = buffer_read_i32(buf);
}

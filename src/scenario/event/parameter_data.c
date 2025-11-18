#include "parameter_data.h"

#include "building/menu.h"
#include "building/properties.h"
#include "city/constants.h"
#include "city/ratings.h"
#include "city/resource.h"
#include "core/array.h"
#include "core/lang.h"
#include "core/string.h"
#include "core/xml_parser.h"
#include "empire/city.h"
#include "figure/formation.h"
#include "game/resource.h"
#include "game/state.h"
#include "map/terrain.h"
#include "scenario/event/controller.h"
#include "scenario/event/parameter_city.h"
#include "scenario/custom_messages.h"
#include "scenario/custom_variable.h"
#include "scenario/invasion.h"
#include "scenario/request.h"
#include "window/editor/select_city_trade_route.h"

#define UNLIMITED 1000000000 //fits in 32bit signed/unsigned int
#define NEGATIVE_UNLIMITED -1000000000 //fits in 32bit signed int

static scenario_condition_data_t scenario_condition_data[CONDITION_TYPE_MAX] = {
    [CONDITION_TYPE_TIME_PASSED] = {.type = CONDITION_TYPE_TIME_PASSED,
                                        .xml_attr = {.name = "time",           .type = PARAMETER_TYPE_TEXT,             .key = TR_CONDITION_TYPE_TIME_PASSED },
                                        .xml_parm1 = {.name = "check",          .type = PARAMETER_TYPE_CHECK,            .min_limit = 1,         .max_limit = 6,                     .key = TR_PARAMETER_TYPE_CHECK },
                                        .xml_parm2 = {.name = "min",            .type = PARAMETER_TYPE_MIN_MAX_NUMBER,           .min_limit = 0,         .max_limit = UNLIMITED,     .key = TR_PARAMETER_TYPE_MIN_MAX_NUMBER_MIN },
                                        .xml_parm3 = {.name = "max",            .type = PARAMETER_TYPE_MIN_MAX_NUMBER,           .min_limit = 0,         .max_limit = UNLIMITED,     .key = TR_PARAMETER_TYPE_MIN_MAX_NUMBER_MAX }, },
    [CONDITION_TYPE_DIFFICULTY] = {.type = CONDITION_TYPE_DIFFICULTY,
                                        .xml_attr = {.name = "difficulty",     .type = PARAMETER_TYPE_TEXT,             .key = TR_CONDITION_TYPE_DIFFICULTY },
                                        .xml_parm1 = {.name = "check",          .type = PARAMETER_TYPE_CHECK,            .min_limit = 1,         .max_limit = 6,     .key = TR_PARAMETER_TYPE_CHECK },
                                        .xml_parm2 = {.name = "value",          .type = PARAMETER_TYPE_DIFFICULTY,       .min_limit = 0,         .max_limit = 4,     .key = TR_PARAMETER_TYPE_DIFFICULTY }, },
    [CONDITION_TYPE_MONEY] = {.type = CONDITION_TYPE_MONEY,
                                        .xml_attr = {.name = "money",          .type = PARAMETER_TYPE_TEXT,             .key = TR_CONDITION_TYPE_MONEY },
                                        .xml_parm1 = {.name = "check",          .type = PARAMETER_TYPE_CHECK,            .min_limit = 1,         .max_limit = 6,     .key = TR_PARAMETER_TYPE_CHECK },
                                        .xml_parm2 = {.name = "value",          .type = PARAMETER_TYPE_FORMULA,           .min_limit = -10000,    .max_limit = UNLIMITED,     .key = TR_PARAMETER_TYPE_FORMULA }, },
    [CONDITION_TYPE_SAVINGS] = {.type = CONDITION_TYPE_SAVINGS,
                                        .xml_attr = {.name = "savings",        .type = PARAMETER_TYPE_TEXT,             .key = TR_CONDITION_TYPE_SAVINGS },
                                        .xml_parm1 = {.name = "check",          .type = PARAMETER_TYPE_CHECK,            .min_limit = 1,         .max_limit = 6,     .key = TR_PARAMETER_TYPE_CHECK },
                                        .xml_parm2 = {.name = "value",          .type = PARAMETER_TYPE_FORMULA,           .min_limit = 0,         .max_limit = UNLIMITED,   .key = TR_PARAMETER_TYPE_FORMULA }, },
    [CONDITION_TYPE_STATS_FAVOR] = {.type = CONDITION_TYPE_STATS_FAVOR,
                                        .xml_attr = {.name = "stats_favor",    .type = PARAMETER_TYPE_TEXT,             .key = TR_CONDITION_TYPE_STATS_FAVOR },
                                        .xml_parm1 = {.name = "check",          .type = PARAMETER_TYPE_CHECK,            .min_limit = 1,         .max_limit = 6,     .key = TR_PARAMETER_TYPE_CHECK },
                                        .xml_parm2 = {.name = "value",          .type = PARAMETER_TYPE_FORMULA,           .min_limit = 0,         .max_limit = 100,   .key = TR_PARAMETER_TYPE_FORMULA }, },
    [CONDITION_TYPE_STATS_PROSPERITY] = {.type = CONDITION_TYPE_STATS_PROSPERITY,
                                        .xml_attr = {.name = "stats_prosperity",     .type = PARAMETER_TYPE_TEXT,       .key = TR_CONDITION_TYPE_STATS_PROSPERITY },
                                        .xml_parm1 = {.name = "check",          .type = PARAMETER_TYPE_CHECK,            .min_limit = 1,         .max_limit = 6,     .key = TR_PARAMETER_TYPE_CHECK },
                                        .xml_parm2 = {.name = "value",          .type = PARAMETER_TYPE_FORMULA,           .min_limit = 0,         .max_limit = 100,   .key = TR_PARAMETER_TYPE_FORMULA }, },
    [CONDITION_TYPE_STATS_CULTURE] = {.type = CONDITION_TYPE_STATS_CULTURE,
                                        .xml_attr = {.name = "stats_culture",        .type = PARAMETER_TYPE_TEXT,       .key = TR_CONDITION_TYPE_STATS_CULTURE },
                                        .xml_parm1 = {.name = "check",          .type = PARAMETER_TYPE_CHECK,            .min_limit = 1,         .max_limit = 6,     .key = TR_PARAMETER_TYPE_CHECK },
                                        .xml_parm2 = {.name = "value",          .type = PARAMETER_TYPE_FORMULA,           .min_limit = 0,         .max_limit = 100,   .key = TR_PARAMETER_TYPE_FORMULA }, },
    [CONDITION_TYPE_STATS_PEACE] = {.type = CONDITION_TYPE_STATS_PEACE,
                                        .xml_attr = {.name = "stats_peace",          .type = PARAMETER_TYPE_TEXT,       .key = TR_CONDITION_TYPE_STATS_PEACE },
                                        .xml_parm1 = {.name = "check",          .type = PARAMETER_TYPE_CHECK,            .min_limit = 1,         .max_limit = 6,     .key = TR_PARAMETER_TYPE_CHECK },
                                        .xml_parm2 = {.name = "value",          .type = PARAMETER_TYPE_FORMULA,           .min_limit = 0,         .max_limit = 100,   .key = TR_PARAMETER_TYPE_FORMULA }, },
    [CONDITION_TYPE_TRADE_SELL_PRICE] = {.type = CONDITION_TYPE_TRADE_SELL_PRICE,
                                        .xml_attr = {.name = "trade_sell_price",     .type = PARAMETER_TYPE_TEXT,       .key = TR_CONDITION_TYPE_TRADE_SELL_PRICE },
                                        .xml_parm1 = {.name = "resource",       .type = PARAMETER_TYPE_RESOURCE,         .key = TR_PARAMETER_TYPE_RESOURCE },
                                        .xml_parm2 = {.name = "check",          .type = PARAMETER_TYPE_CHECK,            .min_limit = 1,         .max_limit = 6,             .key = TR_PARAMETER_TYPE_CHECK },
                                        .xml_parm3 = {.name = "value",          .type = PARAMETER_TYPE_FORMULA,           .min_limit = 0,         .max_limit = UNLIMITED,     .key = TR_PARAMETER_TYPE_FORMULA }, },
    [CONDITION_TYPE_POPS_UNEMPLOYMENT] = {.type = CONDITION_TYPE_POPS_UNEMPLOYMENT,
                                        .xml_attr = {.name = "population_unemployed",     .type = PARAMETER_TYPE_TEXT,  .key = TR_CONDITION_TYPE_POPS_UNEMPLOYMENT },
                                        .xml_parm1 = {.name = "percentage",     .type = PARAMETER_TYPE_BOOLEAN,          .key = TR_PARAMETER_USE_PERCENTAGE },
                                        .xml_parm2 = {.name = "check",          .type = PARAMETER_TYPE_CHECK,            .min_limit = 1,         .max_limit = 6,             .key = TR_PARAMETER_TYPE_CHECK },
                                        .xml_parm3 = {.name = "value",          .type = PARAMETER_TYPE_FORMULA,           .min_limit = 0,         .max_limit = UNLIMITED,     .key = TR_PARAMETER_TYPE_FORMULA }, },
    [CONDITION_TYPE_ROME_WAGES] = {.type = CONDITION_TYPE_ROME_WAGES,
                                        .xml_attr = {.name = "rome_wages",     .type = PARAMETER_TYPE_TEXT,             .key = TR_CONDITION_TYPE_ROME_WAGES },
                                        .xml_parm1 = {.name = "check",          .type = PARAMETER_TYPE_CHECK,            .min_limit = 1,         .max_limit = 6,         .key = TR_PARAMETER_TYPE_CHECK },
                                        .xml_parm2 = {.name = "value",          .type = PARAMETER_TYPE_FORMULA,           .min_limit = 0,         .max_limit = 10000,     .key = TR_PARAMETER_TYPE_FORMULA }, },
    [CONDITION_TYPE_CITY_POPULATION] = {.type = CONDITION_TYPE_CITY_POPULATION,
                                        .xml_attr = {.name = "city_population",     .type = PARAMETER_TYPE_TEXT,        .key = TR_CONDITION_TYPE_CITY_POPULATION },
                                        .xml_parm1 = {.name = "check",          .type = PARAMETER_TYPE_CHECK,            .min_limit = 1,         .max_limit = 6,             .key = TR_PARAMETER_TYPE_CHECK },
                                        .xml_parm2 = {.name = "value",          .type = PARAMETER_TYPE_FORMULA,           .min_limit = 0,         .max_limit = UNLIMITED,     .key = TR_PARAMETER_TYPE_FORMULA },
                                        .xml_parm3 = {.name = "class",          .type = PARAMETER_TYPE_POP_CLASS,        .min_limit = 1,         .max_limit = 3,             .key = TR_PARAMETER_TYPE_POP_CLASS }, },
    [CONDITION_TYPE_BUILDING_COUNT_ACTIVE] = {.type = CONDITION_TYPE_BUILDING_COUNT_ACTIVE,
                                        .xml_attr = {.name = "building_count_active",     .type = PARAMETER_TYPE_TEXT,  .key = TR_CONDITION_TYPE_BUILDING_COUNT_ACTIVE },
                                        .xml_parm1 = {.name = "check",          .type = PARAMETER_TYPE_CHECK,            .min_limit = 1,         .max_limit = 6,             .key = TR_PARAMETER_TYPE_CHECK },
                                        .xml_parm2 = {.name = "value",          .type = PARAMETER_TYPE_FORMULA,           .min_limit = 0,         .max_limit = UNLIMITED,     .key = TR_PARAMETER_TYPE_FORMULA },
                                        .xml_parm3 = {.name = "building",       .type = PARAMETER_TYPE_BUILDING_COUNTING,                        .key = TR_PARAMETER_TYPE_BUILDING_COUNTING }, },
    [CONDITION_TYPE_STATS_CITY_HEALTH] = {.type = CONDITION_TYPE_STATS_CITY_HEALTH,
                                        .xml_attr = {.name = "stats_health",          .type = PARAMETER_TYPE_TEXT,      .key = TR_CONDITION_TYPE_STATS_CITY_HEALTH },
                                        .xml_parm1 = {.name = "check",          .type = PARAMETER_TYPE_CHECK,            .min_limit = 1,         .max_limit = 6,       .key = TR_PARAMETER_TYPE_CHECK },
                                        .xml_parm2 = {.name = "value",          .type = PARAMETER_TYPE_FORMULA,           .min_limit = 0,         .max_limit = 100,     .key = TR_PARAMETER_TYPE_FORMULA }, },
    [CONDITION_TYPE_COUNT_OWN_TROOPS] = {.type = CONDITION_TYPE_COUNT_OWN_TROOPS,
                                        .xml_attr = {.name = "count_own_troops",     .type = PARAMETER_TYPE_TEXT,       .key = TR_CONDITION_TYPE_COUNT_OWN_TROOPS },
                                        .xml_parm1 = {.name = "check",          .type = PARAMETER_TYPE_CHECK,            .min_limit = 1,         .max_limit = 6,             .key = TR_PARAMETER_TYPE_CHECK },
                                        .xml_parm2 = {.name = "value",          .type = PARAMETER_TYPE_FORMULA,           .min_limit = 0,         .max_limit = UNLIMITED,     .key = TR_PARAMETER_TYPE_FORMULA },
                                        .xml_parm3 = {.name = "in_city_only",   .type = PARAMETER_TYPE_BOOLEAN,          .min_limit = 0,         .max_limit = 1,             .key = TR_PARAMETER_IN_CITY_ONLY }, },
    [CONDITION_TYPE_REQUEST_IS_ONGOING] = {.type = CONDITION_TYPE_REQUEST_IS_ONGOING,
                                        .xml_attr = {.name = "request_is_ongoing",     .type = PARAMETER_TYPE_TEXT,     .key = TR_CONDITION_TYPE_REQUEST_IS_ONGOING },
                                        .xml_parm1 = {.name = "request_id",          .type = PARAMETER_TYPE_REQUEST,           .min_limit = 0,         .max_limit = UNLIMITED,     .key = TR_PARAMETER_TYPE_REQUEST },
                                        .xml_parm2 = {.name = "check_for_ongoing",   .type = PARAMETER_TYPE_BOOLEAN,     .min_limit = 0,         .max_limit = 1,      .key = TR_PARAMETER_CHECK_FOR_ONGOING }, },
    [CONDITION_TYPE_TAX_RATE] = {.type = CONDITION_TYPE_TAX_RATE,
                                        .xml_attr = {.name = "tax_rate",       .type = PARAMETER_TYPE_TEXT,             .key = TR_CONDITION_TYPE_TAX_RATE },
                                        .xml_parm1 = {.name = "check",          .type = PARAMETER_TYPE_CHECK,            .min_limit = 1,         .max_limit = 6,         .key = TR_PARAMETER_TYPE_CHECK },
                                        .xml_parm2 = {.name = "value",          .type = PARAMETER_TYPE_FORMULA,           .min_limit = 0,         .max_limit = 25,       .key = TR_PARAMETER_TYPE_FORMULA }, },
    [CONDITION_TYPE_BUILDING_COUNT_ANY] = {.type = CONDITION_TYPE_BUILDING_COUNT_ANY,
                                        .xml_attr = {.name = "building_count_any",     .type = PARAMETER_TYPE_TEXT,  .key = TR_CONDITION_TYPE_BUILDING_COUNT_ANY },
                                        .xml_parm1 = {.name = "check",          .type = PARAMETER_TYPE_CHECK,            .min_limit = 1,         .max_limit = 6,             .key = TR_PARAMETER_TYPE_CHECK },
                                        .xml_parm2 = {.name = "value",          .type = PARAMETER_TYPE_FORMULA,           .min_limit = 0,         .max_limit = UNLIMITED,     .key = TR_PARAMETER_TYPE_FORMULA },
                                        .xml_parm3 = {.name = "building",       .type = PARAMETER_TYPE_BUILDING_COUNTING,                        .key = TR_PARAMETER_TYPE_BUILDING_COUNTING }, },
    [CONDITION_TYPE_CUSTOM_VARIABLE_CHECK] = {.type = CONDITION_TYPE_CUSTOM_VARIABLE_CHECK,
                                        .xml_attr = {.name = "variable_check",       .type = PARAMETER_TYPE_TEXT,             .key = TR_CONDITION_TYPE_CUSTOM_VARIABLE_CHECK },
                                        .xml_parm1 = {.name = "variable_uid",   .type = PARAMETER_TYPE_CUSTOM_VARIABLE,  .min_limit = 0,         .max_limit = 99,       .key = TR_PARAMETER_TYPE_CUSTOM_VARIABLE },
                                        .xml_parm2 = {.name = "check",          .type = PARAMETER_TYPE_CHECK,            .min_limit = 1,         .max_limit = 6,        .key = TR_PARAMETER_TYPE_CHECK },
                                        .xml_parm3 = {.name = "value",          .type = PARAMETER_TYPE_FORMULA,           .min_limit = NEGATIVE_UNLIMITED,               .max_limit = UNLIMITED,       .key = TR_PARAMETER_TYPE_FORMULA }, },
    [CONDITION_TYPE_TRADE_ROUTE_OPEN] = {.type = CONDITION_TYPE_TRADE_ROUTE_OPEN,
                                        .xml_attr = {.name = "trade_route_open",     .type = PARAMETER_TYPE_TEXT,       .key = TR_CONDITION_TYPE_TRADE_ROUTE_OPEN },
                                        .xml_parm1 = {.name = "target_city",          .type = PARAMETER_TYPE_ROUTE,      .key = TR_PARAMETER_TYPE_ROUTE },
                                        .xml_parm2 = {.name = "check_for_open",       .type = PARAMETER_TYPE_BOOLEAN,    .min_limit = 0,         .max_limit = 1,             .key = TR_PARAMETER_CHECK_FOR_OPEN }, },
    [CONDITION_TYPE_TRADE_ROUTE_PRICE] = {.type = CONDITION_TYPE_TRADE_ROUTE_PRICE,
                                        .xml_attr = {.name = "trade_route_price",    .type = PARAMETER_TYPE_TEXT,       .key = TR_CONDITION_TYPE_TRADE_ROUTE_PRICE },
                                        .xml_parm1 = {.name = "target_city",    .type = PARAMETER_TYPE_ROUTE,            .key = TR_PARAMETER_TYPE_ROUTE },
                                        .xml_parm2 = {.name = "check",          .type = PARAMETER_TYPE_CHECK,            .min_limit = 1,         .max_limit = 6,             .key = TR_PARAMETER_TYPE_CHECK },
                                        .xml_parm3 = {.name = "value",          .type = PARAMETER_TYPE_FORMULA,           .min_limit = 0,         .max_limit = UNLIMITED,     .key = TR_PARAMETER_TYPE_FORMULA }, },
    [CONDITION_TYPE_RESOURCE_STORED_COUNT] = {.type = CONDITION_TYPE_RESOURCE_STORED_COUNT,
                                        .xml_attr = {.name = "resource_stored_count",     .type = PARAMETER_TYPE_TEXT,       .key = TR_CONDITION_TYPE_RESOURCE_STORED_COUNT },
                                        .xml_parm1 = {.name = "resource",       .type = PARAMETER_TYPE_RESOURCE,         .key = TR_PARAMETER_TYPE_RESOURCE },
                                        .xml_parm2 = {.name = "check",          .type = PARAMETER_TYPE_CHECK,            .min_limit = 1,         .max_limit = 6,             .key = TR_PARAMETER_TYPE_CHECK },
                                        .xml_parm3 = {.name = "value",          .type = PARAMETER_TYPE_FORMULA,           .min_limit = 0,         .max_limit = UNLIMITED,     .key = TR_PARAMETER_TYPE_FORMULA },
                                        .xml_parm4 = {.name = "storage_type",   .type = PARAMETER_TYPE_STORAGE_TYPE,     .key = TR_PARAMETER_TYPE_STORAGE_TYPE }, },
    [CONDITION_TYPE_RESOURCE_STORAGE_AVAILABLE] = {.type = CONDITION_TYPE_RESOURCE_STORAGE_AVAILABLE,
                                        .xml_attr = {.name = "resource_storage_available",     .type = PARAMETER_TYPE_TEXT,       .key = TR_CONDITION_TYPE_RESOURCE_STORAGE_AVAILABLE },
                                        .xml_parm1 = {.name = "resource",            .type = PARAMETER_TYPE_RESOURCE,         .key = TR_PARAMETER_TYPE_RESOURCE },
                                        .xml_parm2 = {.name = "check",               .type = PARAMETER_TYPE_CHECK,            .min_limit = 1,         .max_limit = 6,             .key = TR_PARAMETER_TYPE_CHECK },
                                        .xml_parm3 = {.name = "value",               .type = PARAMETER_TYPE_FORMULA,           .min_limit = 0,         .max_limit = UNLIMITED,     .key = TR_PARAMETER_TYPE_FORMULA },
                                        .xml_parm4 = {.name = "storage_type",        .type = PARAMETER_TYPE_STORAGE_TYPE,     .key = TR_PARAMETER_TYPE_STORAGE_TYPE },
                                        .xml_parm5 = {.name = "respect_settings",    .type = PARAMETER_TYPE_BOOLEAN,          .min_limit = 0,         .max_limit = 1,             .key = TR_PARAMETER_RESPECT_SETTINGS }, },
    [CONDITION_TYPE_BUILDING_COUNT_AREA] = {.type = CONDITION_TYPE_BUILDING_COUNT_AREA,
                                        .xml_attr = {.name = "building_count_area", .type = PARAMETER_TYPE_TEXT,             .key = TR_CONDITION_TYPE_BUILDING_COUNT_AREA },
                                        .xml_parm1 = {.name = "grid_offset",         .type = PARAMETER_TYPE_GRID_SLICE,           .min_limit = 0,         .max_limit = UNLIMITED,     .key = TR_PARAMETER_GRID_OFFSET_CORNER1 },
                                        .xml_parm2 = {.name = "grid_offset2",        .type = PARAMETER_TYPE_GRID_SLICE,           .min_limit = 0,         .max_limit = UNLIMITED,     .key = TR_PARAMETER_GRID_OFFSET_CORNER2 },
                                        .xml_parm3 = {.name = "building",            .type = PARAMETER_TYPE_BUILDING,         .key = TR_PARAMETER_TYPE_BUILDING_COUNTING },
                                        .xml_parm4 = {.name = "check",               .type = PARAMETER_TYPE_CHECK,            .min_limit = 1,         .max_limit = 6,             .key = TR_PARAMETER_TYPE_CHECK },
                                        .xml_parm5 = {.name = "value",               .type = PARAMETER_TYPE_FORMULA,           .min_limit = 0,         .max_limit = UNLIMITED,     .key = TR_PARAMETER_TYPE_FORMULA }, },
    [CONDITION_TYPE_CHECK_FORMULA] = {.type = CONDITION_TYPE_CHECK_FORMULA,
                                        .xml_attr = {.name = "check_formulas",       .type = PARAMETER_TYPE_TEXT,             .key = TR_CONDITION_TYPE_CHECK_FORMULA },
                                        .xml_parm1 = {.name = "formula",            .type = PARAMETER_TYPE_FORMULA,          .min_limit = NEGATIVE_UNLIMITED,         .max_limit = UNLIMITED,       .key = TR_PARAMETER_TYPE_FORMULA },
                                        .xml_parm2 = {.name = "check",              .type = PARAMETER_TYPE_CHECK,            .min_limit = 1,         .max_limit = 6,        .key = TR_PARAMETER_TYPE_CHECK },
                                        .xml_parm3 = {.name = "formula",            .type = PARAMETER_TYPE_FORMULA,          .min_limit = NEGATIVE_UNLIMITED,         .max_limit = UNLIMITED,       .key = TR_PARAMETER_TYPE_FORMULA }, },
    [CONDITION_TYPE_TERRAIN_IN_AREA] = {.type = CONDITION_TYPE_TERRAIN_IN_AREA,
                                        .xml_attr = {.name = "terrain_in_area",      .type = PARAMETER_TYPE_TEXT,             .key = TR_CONDITION_TYPE_TERRAIN_IN_AREA },
                                        .xml_parm1 = {.name = "grid_offset",         .type = PARAMETER_TYPE_GRID_SLICE,           .min_limit = 0,         .max_limit = UNLIMITED,     .key = TR_PARAMETER_GRID_OFFSET_CORNER1 },
                                        .xml_parm2 = {.name = "grid_offset2",        .type = PARAMETER_TYPE_GRID_SLICE,           .min_limit = 0,         .max_limit = UNLIMITED,     .key = TR_PARAMETER_GRID_OFFSET_CORNER2 },
                                        .xml_parm3 = {.name = "terrain_type",        .type = PARAMETER_TYPE_TERRAIN,          .key = TR_PARAMETER_TERRAIN },
                                        .xml_parm4 = {.name = "check",               .type = PARAMETER_TYPE_CHECK,            .min_limit = 1,         .max_limit = 6,             .key = TR_PARAMETER_TYPE_CHECK },
                                        .xml_parm5 = {.name = "value",               .type = PARAMETER_TYPE_FORMULA,          .min_limit = 0,         .max_limit = UNLIMITED,     .key = TR_PARAMETER_TYPE_FORMULA }, },



};

scenario_condition_data_t *scenario_events_parameter_data_get_conditions_xml_attributes(condition_types type)
{
    return &scenario_condition_data[type];
}

static scenario_action_data_t scenario_action_data[ACTION_TYPE_MAX] = {
    [ACTION_TYPE_ADJUST_FAVOR] = {.type = ACTION_TYPE_ADJUST_FAVOR,
                                        .xml_attr = {.name = "favor_add",          .type = PARAMETER_TYPE_TEXT,     .key = TR_ACTION_TYPE_ADJUST_FAVOR },
                                        .xml_parm1 = {.name = "amount",         .type = PARAMETER_TYPE_FORMULA,           .min_limit = -100,      .max_limit = 100,     .key = TR_PARAMETER_TYPE_FORMULA }, },
    [ACTION_TYPE_ADJUST_MONEY] = {.type = ACTION_TYPE_ADJUST_MONEY,
                                        .xml_attr = {.name = "money_add",          .type = PARAMETER_TYPE_TEXT,     .key = TR_ACTION_TYPE_ADJUST_MONEY },
                                        .xml_parm1 = {.name = "formula" ,        .type = PARAMETER_TYPE_FORMULA,        .min_limit = NEGATIVE_UNLIMITED,      .max_limit = UNLIMITED,      .key = TR_PARAMETER_TYPE_FORMULA },},
    [ACTION_TYPE_ADJUST_SAVINGS] = {.type = ACTION_TYPE_ADJUST_SAVINGS,
                                        .xml_attr = {.name = "savings_add",        .type = PARAMETER_TYPE_TEXT,     .key = TR_ACTION_TYPE_ADJUST_SAVINGS },
                                        .xml_parm1 = {.name = "formula" ,        .type = PARAMETER_TYPE_FORMULA,      .min_limit = NEGATIVE_UNLIMITED,      .max_limit = UNLIMITED,        .key = TR_PARAMETER_TYPE_FORMULA }, },
    [ACTION_TYPE_TRADE_ADJUST_PRICE] = {.type = ACTION_TYPE_TRADE_ADJUST_PRICE,
                                        .xml_attr = {.name = "trade_price_adjust",    .type = PARAMETER_TYPE_TEXT,      .key = TR_ACTION_TYPE_TRADE_ADJUST_PRICE },
                                        .xml_parm1 = {.name = "resource",       .type = PARAMETER_TYPE_RESOURCE,         .key = TR_PARAMETER_TYPE_RESOURCE },
                                        .xml_parm2 = {.name = "amount",         .type = PARAMETER_TYPE_FORMULA,           .min_limit = -10000,      .max_limit = 10000,     .key = TR_PARAMETER_TYPE_FORMULA },
                                        .xml_parm3 = {.name = "show_message",   .type = PARAMETER_TYPE_BOOLEAN,          .min_limit = 0,           .max_limit = 1,         .key = TR_PARAMETER_SHOW_MESSAGE }, },
    [ACTION_TYPE_TRADE_PROBLEM_LAND] = {.type = ACTION_TYPE_TRADE_PROBLEM_LAND,
                                        .xml_attr = {.name = "trade_problems_land",    .type = PARAMETER_TYPE_TEXT,     .key = TR_ACTION_TYPE_TRADE_PROBLEM_LAND },
                                        .xml_parm1 = {.name = "duration",       .type = PARAMETER_TYPE_FORMULA,           .min_limit = 0,      .max_limit = 10000,     .key = TR_PARAMETER_TYPE_FORMULA }, },
    [ACTION_TYPE_TRADE_PROBLEM_SEA] = {.type = ACTION_TYPE_TRADE_PROBLEM_SEA,
                                        .xml_attr = {.name = "trade_problems_sea",    .type = PARAMETER_TYPE_TEXT,      .key = TR_ACTION_TYPE_TRADE_PROBLEM_SEA },
                                        .xml_parm1 = {.name = "duration",       .type = PARAMETER_TYPE_FORMULA,           .min_limit = 0,      .max_limit = 10000,     .key = TR_PARAMETER_TYPE_FORMULA }, },
    [ACTION_TYPE_TRADE_ADJUST_ROUTE_AMOUNT] = {.type = ACTION_TYPE_TRADE_ADJUST_ROUTE_AMOUNT,
                                        .xml_attr = {.name = "trade_route_amount",    .type = PARAMETER_TYPE_TEXT,      .key = TR_ACTION_TYPE_TRADE_ADJUST_ROUTE_AMOUNT },
                                        .xml_parm1 = {.name = "target_city",    .type = PARAMETER_TYPE_ROUTE,            .key = TR_PARAMETER_TYPE_ROUTE },
                                        .xml_parm2 = {.name = "resource",       .type = PARAMETER_TYPE_RESOURCE,         .key = TR_PARAMETER_TYPE_RESOURCE },
                                        .xml_parm3 = {.name = "amount",         .type = PARAMETER_TYPE_FORMULA,           .min_limit = 0,      .max_limit = UNLIMITED,     .key = TR_PARAMETER_TYPE_FORMULA },
                                        .xml_parm4 = {.name = "show_message",   .type = PARAMETER_TYPE_BOOLEAN,          .min_limit = 0,      .max_limit = 1,          .key = TR_PARAMETER_SHOW_MESSAGE }, },
    [ACTION_TYPE_ADJUST_ROME_WAGES] = {.type = ACTION_TYPE_ADJUST_ROME_WAGES,
                                        .xml_attr = {.name = "change_rome_wages",     .type = PARAMETER_TYPE_TEXT,      .key = TR_ACTION_TYPE_ADJUST_ROME_WAGES },
                                        .xml_parm1 = {.name = "formula" ,        .type = PARAMETER_TYPE_FORMULA,      .min_limit = -10000,      .max_limit = 10000,        .key = TR_PARAMETER_TYPE_FORMULA },
                                        .xml_parm2 = {.name = "set_to_value",   .type = PARAMETER_TYPE_BOOLEAN,          .min_limit = 0,      .max_limit = 1,      .key = TR_PARAMETER_SET_TO_VALUE },},
    [ACTION_TYPE_GLADIATOR_REVOLT] = {.type = ACTION_TYPE_GLADIATOR_REVOLT,
                                        .xml_attr = {.name = "gladiator_revolt",     .type = PARAMETER_TYPE_TEXT,       .key = TR_ACTION_TYPE_GLADIATOR_REVOLT }, },
    [ACTION_TYPE_CHANGE_RESOURCE_PRODUCED] = {.type = ACTION_TYPE_CHANGE_RESOURCE_PRODUCED,
                                        .xml_attr = {.name = "change_resource_produced",    .type = PARAMETER_TYPE_TEXT,     .key = TR_ACTION_TYPE_CHANGE_RESOURCE_PRODUCED },
                                        .xml_parm1 = {.name = "resource",       .type = PARAMETER_TYPE_RESOURCE,         .key = TR_PARAMETER_TYPE_RESOURCE },
                                        .xml_parm2 = {.name = "produced",       .type = PARAMETER_TYPE_BOOLEAN,          .min_limit = 0,           .max_limit = 1,      .key = TR_PARAMETER_PRODUCED }, },
    [ACTION_TYPE_CHANGE_ALLOWED_BUILDINGS] = {.type = ACTION_TYPE_CHANGE_ALLOWED_BUILDINGS,
                                        .xml_attr = {.name = "change_allowed_buildings",    .type = PARAMETER_TYPE_TEXT,     .key = TR_ACTION_TYPE_CHANGE_ALLOWED_BUILDINGS },
                                        .xml_parm1 = {.name = "building",       .type = PARAMETER_TYPE_ALLOWED_BUILDING,      .key = TR_PARAMETER_TYPE_ALLOWED_BUILDING },
                                        .xml_parm2 = {.name = "allowed",        .type = PARAMETER_TYPE_BOOLEAN,          .min_limit = 0,           .max_limit = 1,      .key = TR_PARAMETER_ALLOWED }, },
    [ACTION_TYPE_SEND_STANDARD_MESSAGE] = {.type = ACTION_TYPE_SEND_STANDARD_MESSAGE,
                                        .xml_attr = {.name = "send_standard_message",    .type = PARAMETER_TYPE_TEXT,     .key = TR_ACTION_TYPE_SEND_STANDARD_MESSAGE },
                                        .xml_parm1 = {.name = "text_id",        .type = PARAMETER_TYPE_STANDARD_MESSAGE,   .key = TR_PARAMETER_TYPE_STANDARD_MESSAGE }, },
    [ACTION_TYPE_ADJUST_CITY_HEALTH] = {.type = ACTION_TYPE_ADJUST_CITY_HEALTH,
                                        .xml_attr = {.name = "city_health",    .type = PARAMETER_TYPE_TEXT,               .key = TR_ACTION_TYPE_ADJUST_CITY_HEALTH },
                                        .xml_parm1 = {.name = "formula" ,        .type = PARAMETER_TYPE_FORMULA,      .min_limit = -100,      .max_limit = 100,        .key = TR_PARAMETER_TYPE_FORMULA },
                                        .xml_parm2 = {.name = "set_to_value",   .type = PARAMETER_TYPE_BOOLEAN,          .min_limit = 0,      .max_limit = 1,          .key = TR_PARAMETER_SET_TO_VALUE },},
    [ACTION_TYPE_TRADE_SET_PRICE] = {.type = ACTION_TYPE_TRADE_SET_PRICE,
                                        .xml_attr = {.name = "trade_price_set",    .type = PARAMETER_TYPE_TEXT,         .key = TR_ACTION_TYPE_TRADE_SET_PRICE },
                                        .xml_parm1 = {.name = "resource",       .type = PARAMETER_TYPE_RESOURCE,         .key = TR_PARAMETER_TYPE_RESOURCE },
                                        .xml_parm2 = {.name = "amount",         .type = PARAMETER_TYPE_FORMULA,           .min_limit = 0,           .max_limit = UNLIMITED,     .key = TR_PARAMETER_TYPE_FORMULA },
                                        .xml_parm3 = {.name = "set_buy_price",  .type = PARAMETER_TYPE_BOOLEAN,          .min_limit = 0,           .max_limit = 1,      .key = TR_PARAMETER_SET_BUY_PRICE },
                                        .xml_parm4 = {.name = "show_message",   .type = PARAMETER_TYPE_BOOLEAN,          .min_limit = 0,           .max_limit = 1,      .key = TR_PARAMETER_SHOW_MESSAGE }, },
    [ACTION_TYPE_EMPIRE_MAP_CONVERT_FUTURE_TRADE_CITY] = {.type = ACTION_TYPE_EMPIRE_MAP_CONVERT_FUTURE_TRADE_CITY,
                                        .xml_attr = {.name = "empire_map_convert_future_trade_city",     .type = PARAMETER_TYPE_TEXT,     .key = TR_ACTION_TYPE_EMPIRE_MAP_CONVERT_FUTURE_TRADE_CITY },
                                        .xml_parm1 = {.name = "target_city",    .type = PARAMETER_TYPE_FUTURE_CITY,      .key = TR_PARAMETER_TYPE_FUTURE_CITY },
                                        .xml_parm2 = {.name = "show_message",   .type = PARAMETER_TYPE_BOOLEAN,          .min_limit = 0,           .max_limit = 1,      .key = TR_PARAMETER_SHOW_MESSAGE }, },
    [ACTION_TYPE_REQUEST_IMMEDIATELY_START] = {.type = ACTION_TYPE_REQUEST_IMMEDIATELY_START,
                                        .xml_attr = {.name = "request_immediately_start",     .type = PARAMETER_TYPE_TEXT,     .key = TR_ACTION_TYPE_REQUEST_IMMEDIATELY_START },
                                        .xml_parm1 = {.name = "request_id",     .type = PARAMETER_TYPE_REQUEST,           .min_limit = 0,           .max_limit = UNLIMITED,     .key = TR_PARAMETER_TYPE_REQUEST }, },
    [ACTION_TYPE_SHOW_CUSTOM_MESSAGE] = {.type = ACTION_TYPE_SHOW_CUSTOM_MESSAGE,
                                        .xml_attr = {.name = "show_custom_message",           .type = PARAMETER_TYPE_TEXT,     .key = TR_ACTION_TYPE_SHOW_CUSTOM_MESSAGE },
                                        .xml_parm1 = {.name = "message_uid",    .type = PARAMETER_TYPE_CUSTOM_MESSAGE,   .key = TR_PARAMETER_TYPE_CUSTOM_MESSAGE }, },
    [ACTION_TYPE_TAX_RATE_SET] = {.type = ACTION_TYPE_TAX_RATE_SET,
                                        .xml_attr = {.name = "tax_rate_set",   .type = PARAMETER_TYPE_TEXT,             .key = TR_ACTION_TYPE_TAX_RATE_SET },
                                        .xml_parm1 = {.name = "amount",         .type = PARAMETER_TYPE_FORMULA,           .min_limit = 0,      .max_limit = 25,     .key = TR_PARAMETER_TYPE_FORMULA }, },
    [ACTION_TYPE_CHANGE_CUSTOM_VARIABLE] = {.type = ACTION_TYPE_CHANGE_CUSTOM_VARIABLE,
                                        .xml_attr = {.name = "change_variable",        .type = PARAMETER_TYPE_TEXT,      .key = TR_ACTION_TYPE_CHANGE_CUSTOM_VARIABLE },
                                        .xml_parm1 = {.name = "variable_uid",   .type = PARAMETER_TYPE_CUSTOM_VARIABLE,  .min_limit = 0,      .max_limit = 99,     .key = TR_PARAMETER_TYPE_CUSTOM_VARIABLE },
                                        .xml_parm2 = {.name = "value",          .type = PARAMETER_TYPE_FORMULA,           .min_limit = NEGATIVE_UNLIMITED,          .max_limit = UNLIMITED,     .key = TR_PARAMETER_TYPE_FORMULA },
                                        .xml_parm3 = {.name = "set_to_value",   .type = PARAMETER_TYPE_BOOLEAN,          .min_limit = 0,      .max_limit = 1,      .key = TR_PARAMETER_SET_TO_VALUE }, },
    [ACTION_TYPE_TRADE_ADJUST_ROUTE_OPEN_PRICE] = {.type = ACTION_TYPE_TRADE_ADJUST_ROUTE_OPEN_PRICE,
                                        .xml_attr = {.name = "change_trade_route_open_price",    .type = PARAMETER_TYPE_TEXT,      .key = TR_ACTION_TYPE_TRADE_ADJUST_ROUTE_OPEN_PRICE },
                                        .xml_parm1 = {.name = "target_city",    .type = PARAMETER_TYPE_ROUTE,            .key = TR_PARAMETER_TYPE_ROUTE },
                                        .xml_parm2 = {.name = "amount",         .type = PARAMETER_TYPE_FORMULA,           .min_limit = NEGATIVE_UNLIMITED,          .max_limit = UNLIMITED,     .key = TR_PARAMETER_TYPE_FORMULA },
                                        .xml_parm3 = {.name = "set_to_value",   .type = PARAMETER_TYPE_BOOLEAN,          .min_limit = 0,      .max_limit = 1,      .key = TR_PARAMETER_SET_TO_VALUE },
                                        .xml_parm4 = {.name = "show_message",   .type = PARAMETER_TYPE_BOOLEAN,          .min_limit = 0,      .max_limit = 1,      .key = TR_PARAMETER_SHOW_MESSAGE }, },
    [ACTION_TYPE_CHANGE_CITY_RATING] = {.type = ACTION_TYPE_CHANGE_CITY_RATING,
                                        .xml_attr = {.name = "change_city_rating",   .type = PARAMETER_TYPE_TEXT,             .key = TR_ACTION_TYPE_CHANGE_CITY_RATING },
                                        .xml_parm1 = {.name = "rating",         .type = PARAMETER_TYPE_RATING_TYPE,      .min_limit = 0,           .max_limit = 4,      .key = TR_PARAMETER_TYPE_RATING_TYPE },
                                        .xml_parm2 = {.name = "amount",         .type = PARAMETER_TYPE_FORMULA,           .min_limit = -100,        .max_limit = 100,    .key = TR_PARAMETER_TYPE_FORMULA },
                                        .xml_parm3 = {.name = "set_to_value",   .type = PARAMETER_TYPE_BOOLEAN,          .min_limit = 0,           .max_limit = 1,      .key = TR_PARAMETER_SET_TO_VALUE }, },
    [ACTION_TYPE_CHANGE_RESOURCE_STOCKPILES] = {.type = ACTION_TYPE_CHANGE_RESOURCE_STOCKPILES,
                                        .xml_attr = {.name = "change_resource_stockpiles",    .type = PARAMETER_TYPE_TEXT,             .key = TR_ACTION_TYPE_CHANGE_RESOURCE_STOCKPILES },
                                        .xml_parm1 = {.name = "resource",           .type = PARAMETER_TYPE_RESOURCE,         .key = TR_PARAMETER_TYPE_RESOURCE },
                                        .xml_parm2 = {.name = "amount",             .type = PARAMETER_TYPE_FORMULA,           .min_limit = NEGATIVE_UNLIMITED,           .max_limit = UNLIMITED,     .key = TR_PARAMETER_TYPE_FORMULA },
                                        .xml_parm3 = {.name = "storage_type",       .type = PARAMETER_TYPE_STORAGE_TYPE,     .key = TR_PARAMETER_TYPE_STORAGE_TYPE },
                                        .xml_parm4 = {.name = "respect_settings",   .type = PARAMETER_TYPE_BOOLEAN,          .min_limit = 0,                            .max_limit = 1,             .key = TR_PARAMETER_RESPECT_SETTINGS }, },
    [ACTION_TYPE_TRADE_ROUTE_SET_OPEN] = {.type = ACTION_TYPE_TRADE_ROUTE_SET_OPEN,
                                        .xml_attr = {.name = "trade_route_set_open",    .type = PARAMETER_TYPE_TEXT,        .key = TR_ACTION_TYPE_TRADE_ROUTE_SET_OPEN },
                                        .xml_parm1 = {.name = "target_city",    .type = PARAMETER_TYPE_ROUTE,            .key = TR_PARAMETER_TYPE_ROUTE },
                                        .xml_parm2 = {.name = "apply_cost",     .type = PARAMETER_TYPE_BOOLEAN,          .min_limit = 0,      .max_limit = 1,      .key = TR_PARAMETER_APPLY_COST }, },
    [ACTION_TYPE_TRADE_ROUTE_ADD_NEW_RESOURCE] = {.type = ACTION_TYPE_TRADE_ROUTE_ADD_NEW_RESOURCE,
                                        .xml_attr = {.name = "trade_route_add_new_resource",    .type = PARAMETER_TYPE_TEXT,      .key = TR_ACTION_TYPE_TRADE_ROUTE_ADD_NEW_RESOURCE },
                                        .xml_parm1 = {.name = "target_city",    .type = PARAMETER_TYPE_ROUTE,            .key = TR_PARAMETER_TYPE_ROUTE },
                                        .xml_parm2 = {.name = "resource",       .type = PARAMETER_TYPE_RESOURCE,         .key = TR_PARAMETER_TYPE_RESOURCE },
                                        .xml_parm3 = {.name = "amount",         .type = PARAMETER_TYPE_FORMULA,           .min_limit = 0,      .max_limit = UNLIMITED,     .key = TR_PARAMETER_TYPE_FORMULA },
                                        .xml_parm4 = {.name = "add_as_buying",  .type = PARAMETER_TYPE_BOOLEAN,          .min_limit = 0,      .max_limit = 1,      .key = TR_PARAMETER_ADD_AS_BUYING },
                                        .xml_parm5 = {.name = "show_message",   .type = PARAMETER_TYPE_BOOLEAN,          .min_limit = 0,      .max_limit = 1,      .key = TR_PARAMETER_SHOW_MESSAGE }, },
    [ACTION_TYPE_TRADE_SET_BUY_PRICE_ONLY] = {.type = ACTION_TYPE_TRADE_SET_BUY_PRICE_ONLY,
                                        .xml_attr = {.name = "trade_set_buy_price_only",    .type = PARAMETER_TYPE_TEXT,         .key = TR_ACTION_TYPE_TRADE_SET_BUY_PRICE_ONLY },
                                        .xml_parm1 = {.name = "resource",       .type = PARAMETER_TYPE_RESOURCE,         .key = TR_PARAMETER_TYPE_RESOURCE },
                                        .xml_parm2 = {.name = "amount",         .type = PARAMETER_TYPE_FORMULA,           .min_limit = 0,           .max_limit = UNLIMITED,     .key = TR_PARAMETER_TYPE_FORMULA }, },
    [ACTION_TYPE_TRADE_SET_SELL_PRICE_ONLY] = {.type = ACTION_TYPE_TRADE_SET_SELL_PRICE_ONLY,
                                        .xml_attr = {.name = "trade_set_sell_price_only",    .type = PARAMETER_TYPE_TEXT,         .key = TR_ACTION_TYPE_TRADE_SET_SELL_PRICE_ONLY },
                                        .xml_parm1 = {.name = "resource",       .type = PARAMETER_TYPE_RESOURCE,         .key = TR_PARAMETER_TYPE_RESOURCE },
                                        .xml_parm2 = {.name = "amount",         .type = PARAMETER_TYPE_FORMULA,           .min_limit = 0,           .max_limit = UNLIMITED,     .key = TR_PARAMETER_TYPE_FORMULA }, },
    [ACTION_TYPE_BUILDING_FORCE_COLLAPSE] = {.type = ACTION_TYPE_BUILDING_FORCE_COLLAPSE,
                                        .xml_attr = {.name = "building_force_collapse",    .type = PARAMETER_TYPE_TEXT,         .key = TR_ACTION_TYPE_BUILDING_FORCE_COLLAPSE },
                                        .xml_parm1 = {.name = "grid_offset1",    .type = PARAMETER_TYPE_GRID_SLICE,           .min_limit = 0,           .max_limit = UNLIMITED,     .key = TR_PARAMETER_GRID_OFFSET_CORNER1 },
                                        .xml_parm2 = {.name = "grid_offset2",   .type = PARAMETER_TYPE_GRID_SLICE,           .min_limit = 0,           .max_limit = UNLIMITED,     .key = TR_PARAMETER_GRID_OFFSET_CORNER2 },
                                        .xml_parm3 = {.name = "building",       .type = PARAMETER_TYPE_BUILDING,         .key = TR_PARAMETER_TYPE_BUILDING_COUNTING },
                                        .xml_parm4 = {.name = "destroy_all",    .type = PARAMETER_TYPE_BOOLEAN,          .min_limit = 0,      .max_limit = 1,      .key = TR_PARAMETER_DESTROY_ALL }, },
    [ACTION_TYPE_INVASION_IMMEDIATE] = {.type = ACTION_TYPE_INVASION_IMMEDIATE,
                                        .xml_attr = {.name = "invasion_start_immediate",    .type = PARAMETER_TYPE_TEXT,             .key = TR_ACTION_TYPE_INVASION_IMMEDIATE },
                                        .xml_parm1 = {.name = "attack_type",                 .type = PARAMETER_TYPE_INVASION_TYPE,    .key = TR_PARAMETER_TYPE_INVASION_TYPE },
                                        .xml_parm2 = {.name = "size",                        .type = PARAMETER_TYPE_FORMULA,           .min_limit = 1,           .max_limit = 200,     .key = TR_PARAMETER_TYPE_INVASION_SIZE },
                                        .xml_parm3 = {.name = "invasion_point",              .type = PARAMETER_TYPE_FORMULA,           .min_limit = 1,           .max_limit = 9,       .key = TR_PARAMETER_TYPE_INVASION_POINT },
                                        .xml_parm4 = {.name = "target_type",                 .type = PARAMETER_TYPE_TARGET_TYPE,      .key = TR_PARAMETER_TYPE_TARGET_TYPE },
                                        .xml_parm5 = {.name = "enemy_type",                  .type = PARAMETER_TYPE_ENEMY_TYPE,       .key = TR_PARAMETER_TYPE_ENEMY_TYPE }, },
    [ACTION_TYPE_CAUSE_BLESSING] = {.type = ACTION_TYPE_CAUSE_BLESSING,
                                        .xml_attr = {.name = "cause_blessing",       .type = PARAMETER_TYPE_TEXT,      .key = TR_ACTION_TYPE_CAUSE_BLESSING },
                                        .xml_parm1 = {.name = "god",                 .type = PARAMETER_TYPE_GOD,       .key = TR_PARAMETER_TYPE_GOD }, },
    [ACTION_TYPE_CAUSE_MINOR_CURSE] = {.type = ACTION_TYPE_CAUSE_MINOR_CURSE,
                                        .xml_attr = {.name = "cause_minor_curse",    .type = PARAMETER_TYPE_TEXT,      .key = TR_ACTION_TYPE_CAUSE_MINOR_CURSE },
                                        .xml_parm1 = {.name = "god",                 .type = PARAMETER_TYPE_GOD,       .key = TR_PARAMETER_TYPE_GOD }, },
    [ACTION_TYPE_CAUSE_MAJOR_CURSE] = {.type = ACTION_TYPE_CAUSE_MAJOR_CURSE,
                                        .xml_attr = {.name = "cause_major_curse",    .type = PARAMETER_TYPE_TEXT,      .key = TR_ACTION_TYPE_CAUSE_MAJOR_CURSE },
                                        .xml_parm1 = {.name = "god",                 .type = PARAMETER_TYPE_GOD,       .key = TR_PARAMETER_TYPE_GOD }, },
    [ACTION_TYPE_CHANGE_CLIMATE] = {.type = ACTION_TYPE_CHANGE_CLIMATE,
                                        .xml_attr = {.name = "change_climate",      .type = PARAMETER_TYPE_TEXT,      .key = TR_ACTION_TYPE_CHANGE_CLIMATE },
                                        .xml_parm1 = {.name = "climate",            .type = PARAMETER_TYPE_CLIMATE,   .key = TR_PARAMETER_TYPE_CLIMATE }, },
    [ACTION_TYPE_CHANGE_TERRAIN] = {.type = ACTION_TYPE_CHANGE_TERRAIN,
                                        .xml_attr = {.name = "change_terrain",      .type = PARAMETER_TYPE_TEXT,       .key = TR_ACTION_TYPE_CHANGE_TERRAIN },
                                        .xml_parm1 = {.name = "grid_offset1",       .type = PARAMETER_TYPE_GRID_SLICE,           .min_limit = 0,           .max_limit = UNLIMITED,     .key = TR_PARAMETER_GRID_OFFSET_CORNER1 },
                                        .xml_parm2 = {.name = "grid_offset2",       .type = PARAMETER_TYPE_GRID_SLICE,           .min_limit = 0,           .max_limit = UNLIMITED,     .key = TR_PARAMETER_GRID_OFFSET_CORNER2 },
                                        .xml_parm3 = {.name = "terrain",            .type = PARAMETER_TYPE_TERRAIN,    .key = TR_PARAMETER_TERRAIN },
                                        .xml_parm4 = {.name = "add",                .type = PARAMETER_TYPE_BOOLEAN,    .min_limit = 0,      .max_limit = 1,      .key = TR_PARAMETER_ADD }, },
    [ACTION_TYPE_CHANGE_MODEL_DATA] = {.type = ACTION_TYPE_CHANGE_MODEL_DATA,
                                        .xml_attr = {.name = "change_model_data",   .type = PARAMETER_TYPE_TEXT,       .key = TR_ACTION_TYPE_CHANGE_MODEL_DATA },
                                        .xml_parm1 = {.name = "model",              .type =
                                        PARAMETER_TYPE_MODEL,   .key = TR_PARAMETER_MODEL },
                                        .xml_parm2 = {.name = "data_type",          .type = PARAMETER_TYPE_DATA_TYPE,          .key = TR_PARAMETER_DATA_TYPE },
                                        .xml_parm3 = {.name = "amount",        .type = PARAMETER_TYPE_FORMULA,          .min_limit = NEGATIVE_UNLIMITED,      .max_limit = UNLIMITED, .key = TR_PARAMETER_TYPE_FORMULA },
                                        .xml_parm4 = {.name = "set_to_value",      .type = PARAMETER_TYPE_BOOLEAN,            .min_limit = 0,           .max_limit = 1,                    .key = TR_PARAMETER_SET_TO_VALUE }, },
    [ACTION_TYPE_CHANGE_CUSTOM_VARIABLE_VISIBILITY] = {.type = ACTION_TYPE_CHANGE_CUSTOM_VARIABLE_VISIBILITY,
                                        .xml_attr = {.name = "change_variable_visibility",        .type = PARAMETER_TYPE_TEXT,      .key = TR_ACTION_TYPE_CHANGE_CUSTOM_VARIABLE_VISIBILITY },
                                        .xml_parm1 = {.name = "variable_uid",   .type = PARAMETER_TYPE_CUSTOM_VARIABLE,  .min_limit = 0,      .max_limit = 99,     .key = TR_PARAMETER_TYPE_CUSTOM_VARIABLE },
                                        .xml_parm2 = {.name = "value",          .type = PARAMETER_TYPE_BOOLEAN, .min_limit = 0,      .max_limit = 1,      .key = TR_PARAMETER_TYPE_BOOLEAN }, },

    [ACTION_TYPE_CUSTOM_VARIABLE_FORMULA] = {.type = ACTION_TYPE_CUSTOM_VARIABLE_FORMULA,
                                        .xml_attr = {.name = "variable_formula",       .type = PARAMETER_TYPE_TEXT,             .key = TR_ACTION_TYPE_CUSTOM_VARIABLE_FORMULA },
                                        .xml_parm1 = {.name = "variable_uid",   .type = PARAMETER_TYPE_CUSTOM_VARIABLE,  .min_limit = 0,      .max_limit = 99,     .key = TR_PARAMETER_TYPE_CUSTOM_VARIABLE },
                                        .xml_parm2 = {.name = "formula",        .type = PARAMETER_TYPE_FORMULA,          .min_limit = NEGATIVE_UNLIMITED,      .max_limit = UNLIMITED, .key = TR_PARAMETER_TYPE_FORMULA }, },
    [ACTION_TYPE_CUSTOM_VARIABLE_CITY_PROPERTY] = {.type = ACTION_TYPE_CUSTOM_VARIABLE_CITY_PROPERTY,
                                        .xml_attr = {.name = "variable_city_property",  .type = PARAMETER_TYPE_TEXT,             .key = TR_EDITOR_ACTION_TYPE_CITY_PROPERTY },
                                        .xml_parm1 = {.name = "variable_uid",            .type = PARAMETER_TYPE_CUSTOM_VARIABLE,  .min_limit = 0,      .max_limit = 99,     .key = TR_PARAMETER_TYPE_CUSTOM_VARIABLE },
                                        .xml_parm2 = {.name = "property",                .type = PARAMETER_TYPE_CITY_PROPERTY,    .key = TR_EDITOR_PARAMETER_CITY_PROPERTY},
                                        .xml_parm3 = {.name = "flexible",                .type = PARAMETER_TYPE_FLEXIBLE  ,           .key = TR_PLACEHOLDER },
                                        .xml_parm4 = {.name = "flexible",                .type = PARAMETER_TYPE_FLEXIBLE  ,           .key = TR_PLACEHOLDER },
                                        .xml_parm5 = {.name = "flexible",                .type = PARAMETER_TYPE_FLEXIBLE  ,           .key = TR_PLACEHOLDER }, },
    [ACTION_TYPE_GOD_SENTIMENT_CHANGE] = {.type = ACTION_TYPE_GOD_SENTIMENT_CHANGE,
                                        .xml_attr = {.name = "change_god_sentiment",    .type = PARAMETER_TYPE_TEXT,      .key = TR_ACTION_TYPE_GOD_SENTIMENT },
                                        .xml_parm1 = {.name = "god",                 .type = PARAMETER_TYPE_GOD,       .key = TR_PARAMETER_TYPE_GOD },
                                        .xml_parm2 = {.name = "amount",              .type = PARAMETER_TYPE_FORMULA,  .min_limit = -100,        .max_limit = 100,     .key = TR_PARAMETER_TYPE_FORMULA },
                                        .xml_parm3 = {.name = "set_to_value",        .type = PARAMETER_TYPE_BOOLEAN,  .min_limit = 0,        .max_limit = 1,       .key = TR_PARAMETER_SET_TO_VALUE }, },
    [ACTION_TYPE_POP_SENTIMENT_CHANGE] = {.type = ACTION_TYPE_POP_SENTIMENT_CHANGE,
                                        .xml_attr = {.name = "change_pop_sentiment",    .type = PARAMETER_TYPE_TEXT,      .key = TR_ACTION_TYPE_POP_SENTIMENT},
                                        .xml_parm1 = {.name = "amount",              .type = PARAMETER_TYPE_FORMULA,  .min_limit = -100,     .max_limit = 100,     .key = TR_PARAMETER_TYPE_FORMULA },
                                        .xml_parm2 = {.name = "set_to_value",        .type = PARAMETER_TYPE_BOOLEAN,  .min_limit = 0,        .max_limit = 1,       .key = TR_PARAMETER_SET_TO_VALUE }, },
    [ACTION_TYPE_WIN] = {.type = ACTION_TYPE_WIN,
                                        .xml_attr = {.name = "win",    .type = PARAMETER_TYPE_TEXT,      .key = TR_ACTION_TYPE_WIN }, },
    [ACTION_TYPE_LOSE] = {.type = ACTION_TYPE_LOSE,
                                        .xml_attr = {.name = "lose",   .type = PARAMETER_TYPE_TEXT,      .key = TR_ACTION_TYPE_LOSE }, },
    [ACTION_TYPE_CHANGE_RANK] = {.type = ACTION_TYPE_CHANGE_RANK,
                                        .xml_attr = {.name = "change_rank",     .type = PARAMETER_TYPE_TEXT,      .key = TR_ACTION_TYPE_CHANGE_RANK },
                                        .xml_parm1 = {.name = "rank",           .type = PARAMETER_TYPE_RANK,      .key = TR_PARAMETER_TYPE_RANK }, },
    [ACTION_TYPE_CHANGE_PRODUCTION_RATE] = {.type = ACTION_TYPE_CHANGE_PRODUCTION_RATE,
                                        .xml_attr = {.name = "change_production_rate",  .type = PARAMETER_TYPE_TEXT,    .key = TR_ACTION_TYPE_PRODUCTION_RATE},
                                        .xml_parm1 = {.name = "resource",       .type = PARAMETER_TYPE_RESOURCE,   .key = TR_PARAMETER_TYPE_RESOURCE },
                                        .xml_parm2 = {.name = "rate",         .type = PARAMETER_TYPE_FORMULA,            .min_limit = 0,
                                            .max_limit = UNLIMITED,     .key = TR_PARAMETER_TYPE_NUMBER },
                                        .xml_parm3 = {.name = "set_to_value",      .type = PARAMETER_TYPE_BOOLEAN,      .min_limit = 0,
                                            .max_limit = 1,         .key = TR_PARAMETER_SET_TO_VALUE }, },
    [ACTION_TYPE_CHANGE_HOUSE_MODEL_DATA] = {.type = ACTION_TYPE_CHANGE_HOUSE_MODEL_DATA,
                                        .xml_attr = {.name = "change_house_model_data",  .type = PARAMETER_TYPE_TEXT,    .key = TR_ACTION_TYPE_CHANGE_HOUSE_MODEL_DATA},
                                        .xml_parm1 = {.name = "housing_level",       .type = PARAMETER_TYPE_HOUSING_TYPE,   .key = TR_PARAMETER_TYPE_HOUSING_TYPE },
                                        .xml_parm2 = {.name = "house_data_type",     .type = PARAMETER_TYPE_HOUSE_DATA_TYPE, .key = TR_PARAMETER_DATA_TYPE},
                                        .xml_parm3 = {.name = "value",         .type = PARAMETER_TYPE_FORMULA,            .min_limit = 0,
                                            .max_limit = UNLIMITED,     .key = TR_PARAMETER_TYPE_NUMBER },
                                        .xml_parm4 = {.name = "set_to_value",      .type = PARAMETER_TYPE_BOOLEAN,      .min_limit = 0,
                                            .max_limit = 1,         .key = TR_PARAMETER_SET_TO_VALUE }, }
};

scenario_action_data_t *scenario_events_parameter_data_get_actions_xml_attributes(action_types type)
{
    return &scenario_action_data[type];
}

parameter_type scenario_events_parameter_data_get_action_parameter_type(
    action_types action_type, int parameter_index, int *min_limit, int *max_limit)
{
    if (action_type < 0 || action_type >= ACTION_TYPE_MAX) {
        return PARAMETER_TYPE_UNDEFINED;
    }

    const scenario_action_data_t *action = &scenario_action_data[action_type];

    switch (parameter_index) {
        case 1:
            *min_limit = action->xml_parm1.min_limit;
            *max_limit = action->xml_parm1.max_limit;
            return action->xml_parm1.type;
        case 2:
            *min_limit = action->xml_parm2.min_limit;
            *max_limit = action->xml_parm2.max_limit;
            return action->xml_parm2.type;
        case 3:
            *min_limit = action->xml_parm3.min_limit;
            *max_limit = action->xml_parm3.max_limit;
            return action->xml_parm3.type;
        case 4:
            *min_limit = action->xml_parm4.min_limit;
            *max_limit = action->xml_parm4.max_limit;
            return action->xml_parm4.type;
        case 5:
            *min_limit = action->xml_parm5.min_limit;
            *max_limit = action->xml_parm5.max_limit;
        default:
            *min_limit = 0;
            *max_limit = 0;
            return PARAMETER_TYPE_UNDEFINED;
    }
}

parameter_type scenario_events_parameter_data_get_condition_parameter_type(
    condition_types condition_type, int parameter_index, int *min_limit, int *max_limit)
{
    if (condition_type < 0 || condition_type >= CONDITION_TYPE_MAX) {
        return PARAMETER_TYPE_UNDEFINED;
    }

    const scenario_condition_data_t *condition = &scenario_condition_data[condition_type];

    switch (parameter_index) {
        case 1:
            *min_limit = condition->xml_parm1.min_limit;
            *max_limit = condition->xml_parm1.max_limit;
            return condition->xml_parm1.type;
        case 2:
            *min_limit = condition->xml_parm2.min_limit;
            *max_limit = condition->xml_parm2.max_limit;
            return condition->xml_parm2.type;
        case 3:
            *min_limit = condition->xml_parm3.min_limit;
            *max_limit = condition->xml_parm3.max_limit;
            return condition->xml_parm3.type;
        case 4:
            *min_limit = condition->xml_parm4.min_limit;
            *max_limit = condition->xml_parm4.max_limit;
            return condition->xml_parm4.type;
        case 5:
            *min_limit = condition->xml_parm5.min_limit;
            *max_limit = condition->xml_parm5.max_limit;
            return condition->xml_parm5.type;
        default:
            *min_limit = 0;
            *max_limit = 0;
            return PARAMETER_TYPE_UNDEFINED;
    }
}

typedef struct {
    int type;
    translation_key key;
} sorting_attr_t;

static scenario_condition_data_t *scenario_condition_data_alphabetical[CONDITION_TYPE_MAX - 1];
static scenario_action_data_t *scenario_action_data_alphabetical[ACTION_TYPE_MAX - 1];

static int compare_lower(const void *va, const void *vb)
{
    const sorting_attr_t *a = (const sorting_attr_t *) va;
    const sorting_attr_t *b = (const sorting_attr_t *) vb;

    const uint8_t *name_a = translation_for(a->key);
    const uint8_t *name_b = translation_for(b->key);
    return string_compare(name_a, name_b);
}

void scenario_events_parameter_data_sort_alphabetically(void)
{
    sorting_attr_t conditions[CONDITION_TYPE_MAX - 1];
    sorting_attr_t actions[ACTION_TYPE_MAX - 1];
    for (int i = 1; i < CONDITION_TYPE_MAX; i++) {
        conditions[i - 1].type = scenario_condition_data[i].type;
        conditions[i - 1].key = scenario_condition_data[i].xml_attr.key;
    }
    for (int i = 1; i < ACTION_TYPE_MAX; i++) {
        actions[i - 1].type = scenario_action_data[i].type;
        actions[i - 1].key = scenario_action_data[i].xml_attr.key;
    }

    qsort(conditions, CONDITION_TYPE_MAX - 1, sizeof(sorting_attr_t), compare_lower);
    qsort(actions, ACTION_TYPE_MAX - 1, sizeof(sorting_attr_t), compare_lower);

    for (int i = 0; i < CONDITION_TYPE_MAX - 1; i++) {
        scenario_condition_data_alphabetical[i] = scenario_events_parameter_data_get_conditions_xml_attributes(conditions[i].type);
    }
    for (int i = 0; i < ACTION_TYPE_MAX - 1; i++) {
        scenario_action_data_alphabetical[i] = scenario_events_parameter_data_get_actions_xml_attributes(actions[i].type);
    }
}

scenario_condition_data_t *scenario_events_parameter_data_get_conditions_xml_attributes_alphabetical(int index)
{
    return scenario_condition_data_alphabetical[index];
}

scenario_action_data_t *scenario_events_parameter_data_get_actions_xml_attributes_alphabetical(int index)
{
    return scenario_action_data_alphabetical[index];
}

static special_attribute_mapping_t special_attribute_mappings_check[] = {
    {.type = PARAMETER_TYPE_CHECK,                .text = "eq",                            .value = COMPARISON_TYPE_EQUAL,               .key = TR_PARAMETER_VALUE_COMPARISON_TYPE_EQUAL },
    {.type = PARAMETER_TYPE_CHECK,                .text = "lte",                           .value = COMPARISON_TYPE_EQUAL_OR_LESS,       .key = TR_PARAMETER_VALUE_COMPARISON_TYPE_EQUAL_OR_LESS },
    {.type = PARAMETER_TYPE_CHECK,                .text = "gte",                           .value = COMPARISON_TYPE_EQUAL_OR_MORE,       .key = TR_PARAMETER_VALUE_COMPARISON_TYPE_EQUAL_OR_MORE },
    {.type = PARAMETER_TYPE_CHECK,                .text = "neq",                           .value = COMPARISON_TYPE_NOT_EQUAL,           .key = TR_PARAMETER_VALUE_COMPARISON_TYPE_NOT_EQUAL },
    {.type = PARAMETER_TYPE_CHECK,                .text = "lt",                            .value = COMPARISON_TYPE_LESS_THAN,           .key = TR_PARAMETER_VALUE_COMPARISON_TYPE_LESS_THAN },
    {.type = PARAMETER_TYPE_CHECK,                .text = "gt",                            .value = COMPARISON_TYPE_GREATER_THAN,        .key = TR_PARAMETER_VALUE_COMPARISON_TYPE_GREATER_THAN },
};

#define SPECIAL_ATTRIBUTE_MAPPINGS_CHECK_SIZE (sizeof(special_attribute_mappings_check) / sizeof(special_attribute_mapping_t))

static special_attribute_mapping_t special_attribute_mappings_difficulty[] = {
    {.type = PARAMETER_TYPE_DIFFICULTY,           .text = "very_easy",                     .value = DIFFICULTY_VERY_EASY,               .key = TR_PARAMETER_VALUE_DIFFICULTY_VERY_EASY },
    {.type = PARAMETER_TYPE_DIFFICULTY,           .text = "easy",                          .value = DIFFICULTY_EASY,                    .key = TR_PARAMETER_VALUE_DIFFICULTY_EASY },
    {.type = PARAMETER_TYPE_DIFFICULTY,           .text = "normal",                        .value = DIFFICULTY_NORMAL,                  .key = TR_PARAMETER_VALUE_DIFFICULTY_NORMAL },
    {.type = PARAMETER_TYPE_DIFFICULTY,           .text = "hard",                          .value = DIFFICULTY_HARD,                    .key = TR_PARAMETER_VALUE_DIFFICULTY_HARD },
    {.type = PARAMETER_TYPE_DIFFICULTY,           .text = "very_hard",                     .value = DIFFICULTY_VERY_HARD,               .key = TR_PARAMETER_VALUE_DIFFICULTY_VERY_HARD },
};

#define SPECIAL_ATTRIBUTE_MAPPINGS_CHECK_DIFFICULTY (sizeof(special_attribute_mappings_difficulty) / sizeof(special_attribute_mapping_t))

static special_attribute_mapping_t special_attribute_mappings_boolean[] = {
    {.type = PARAMETER_TYPE_BOOLEAN,              .text = "false",                         .value = 0,                                  .key = TR_PARAMETER_VALUE_BOOLEAN_FALSE },
    {.type = PARAMETER_TYPE_BOOLEAN,              .text = "true",                          .value = 1,                                  .key = TR_PARAMETER_VALUE_BOOLEAN_TRUE },
};

#define SPECIAL_ATTRIBUTE_MAPPINGS_BOOLEAN_SIZE (sizeof(special_attribute_mappings_boolean) / sizeof(special_attribute_mapping_t))

static special_attribute_mapping_t special_attribute_mappings_pop_class[] = {
    {.type = PARAMETER_TYPE_POP_CLASS,            .text = "all",                           .value = POP_CLASS_ALL,                      .key = TR_PARAMETER_VALUE_POP_CLASS_ALL },
    {.type = PARAMETER_TYPE_POP_CLASS,            .text = "patrician",                     .value = POP_CLASS_PATRICIAN,                .key = TR_PARAMETER_VALUE_POP_CLASS_PATRICIAN },
    {.type = PARAMETER_TYPE_POP_CLASS,            .text = "plebeian",                      .value = POP_CLASS_PLEBEIAN,                 .key = TR_PARAMETER_VALUE_POP_CLASS_PLEBEIAN },
    {.type = PARAMETER_TYPE_POP_CLASS,            .text = "slums",                         .value = POP_CLASS_SLUMS,                    .key = TR_PARAMETER_VALUE_POP_CLASS_SLUMS },
};

#define SPECIAL_ATTRIBUTE_MAPPINGS_POP_CLASS_SIZE (sizeof(special_attribute_mappings_pop_class) / sizeof(special_attribute_mapping_t))

static special_attribute_mapping_t special_attribute_mappings_buildings[BUILDING_TYPE_MAX];
static unsigned int special_attribute_mappings_building_type_size;

static special_attribute_mapping_t special_attribute_mappings_allowed_buildings[BUILDING_TYPE_MAX];
static unsigned int special_attribute_mappings_allowed_buildings_size;

static special_attribute_mapping_t special_attribute_mappings_model_buildings[BUILDING_TYPE_MAX];
static unsigned int special_attribute_mappings_model_buildings_size;

static special_attribute_mapping_t special_attribute_mappings_standard_message[] = {
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "none",                      .value = 0,                    .key = TR_PARAMETER_VALUE_NONE },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "caesar_debt",               .value = MESSAGE_CITY_IN_DEBT,                    .key = TR_PARAMETER_VALUE_MESSAGE_CITY_IN_DEBT },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "caesar_debt_again",         .value = MESSAGE_CITY_IN_DEBT_AGAIN,                    .key = TR_PARAMETER_VALUE_MESSAGE_CITY_IN_DEBT_AGAIN },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "caesar_ebt_still",          .value = MESSAGE_CITY_STILL_IN_DEBT,                    .key = TR_PARAMETER_VALUE_MESSAGE_CITY_STILL_IN_DEBT },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "caesar_wrath",              .value = MESSAGE_CAESAR_WRATH,                    .key = TR_PARAMETER_VALUE_MESSAGE_CAESAR_WRATH },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "caesar_army_continue",      .value = MESSAGE_CAESAR_ARMY_CONTINUE,                    .key = TR_PARAMETER_VALUE_MESSAGE_CAESAR_ARMY_CONTINUE },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "caesar_army_retreat",       .value = MESSAGE_CAESAR_ARMY_RETREAT,                    .key = TR_PARAMETER_VALUE_MESSAGE_CAESAR_ARMY_RETREAT },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "local_uprising",            .value = MESSAGE_DISTANT_BATTLE,                    .key = TR_PARAMETER_VALUE_MESSAGE_DISTANT_BATTLE },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "local_uprising",            .value = MESSAGE_ENEMIES_CLOSING,                    .key = TR_PARAMETER_VALUE_MESSAGE_ENEMIES_CLOSING },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "local_uprising",            .value = MESSAGE_ENEMIES_AT_THE_DOOR,                    .key = TR_PARAMETER_VALUE_MESSAGE_ENEMIES_AT_THE_DOOR },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "small_festival",            .value = MESSAGE_SMALL_FESTIVAL,                    .key = TR_PARAMETER_VALUE_MESSAGE_SMALL_FESTIVAL },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "large_festival",            .value = MESSAGE_LARGE_FESTIVAL,                    .key = TR_PARAMETER_VALUE_MESSAGE_LARGE_FESTIVAL },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "grand_festival",            .value = MESSAGE_GRAND_FESTIVAL,                    .key = TR_PARAMETER_VALUE_MESSAGE_GRAND_FESTIVAL },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "gods_unhappy",              .value = MESSAGE_GODS_UNHAPPY,                    .key = TR_PARAMETER_VALUE_MESSAGE_GODS_UNHAPPY },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "gladiator_revolt",          .value = MESSAGE_GLADIATOR_REVOLT,                    .key = TR_PARAMETER_VALUE_MESSAGE_GLADIATOR_REVOLT },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "gladiator_revolt_over",     .value = MESSAGE_GLADIATOR_REVOLT_FINISHED,                    .key = TR_PARAMETER_VALUE_MESSAGE_GLADIATOR_REVOLT_FINISHED },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "emperor_change",            .value = MESSAGE_EMPEROR_CHANGE,                    .key = TR_PARAMETER_VALUE_MESSAGE_EMPEROR_CHANGE },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "land_trade_sandstorms",     .value = MESSAGE_LAND_TRADE_DISRUPTED_SANDSTORMS,                    .key = TR_PARAMETER_VALUE_MESSAGE_LAND_TRADE_DISRUPTED_SANDSTORMS },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "land_trade_landslides",     .value = MESSAGE_LAND_TRADE_DISRUPTED_LANDSLIDES,                    .key = TR_PARAMETER_VALUE_MESSAGE_LAND_TRADE_DISRUPTED_LANDSLIDES },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "land_trade_storms",         .value = MESSAGE_SEA_TRADE_DISRUPTED,                    .key = TR_PARAMETER_VALUE_MESSAGE_SEA_TRADE_DISRUPTED },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "rome_raises_wages",         .value = MESSAGE_ROME_RAISES_WAGES,                    .key = TR_PARAMETER_VALUE_MESSAGE_ROME_RAISES_WAGES },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "rome_lowers_wages",         .value = MESSAGE_ROME_LOWERS_WAGES,                    .key = TR_PARAMETER_VALUE_MESSAGE_ROME_LOWERS_WAGES },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "contaminated_water",        .value = MESSAGE_CONTAMINATED_WATER,                    .key = TR_PARAMETER_VALUE_MESSAGE_CONTAMINATED_WATER },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "empire_expanded",           .value = MESSAGE_EMPIRE_HAS_EXPANDED,                    .key = TR_PARAMETER_VALUE_MESSAGE_EMPIRE_HAS_EXPANDED },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "wrath_of_ceres",            .value = MESSAGE_WRATH_OF_CERES,                    .key = TR_PARAMETER_VALUE_MESSAGE_WRATH_OF_CERES },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "wrath_of_neptune_2",        .value = MESSAGE_WRATH_OF_NEPTUNE_NO_SEA_TRADE,                    .key = TR_PARAMETER_VALUE_MESSAGE_WRATH_OF_NEPTUNE_NO_SEA_TRADE },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "wrath_of_mercury",          .value = MESSAGE_WRATH_OF_MERCURY,                    .key = TR_PARAMETER_VALUE_MESSAGE_WRATH_OF_MERCURY },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "wrath_of_mars_2",           .value = MESSAGE_WRATH_OF_MARS_NO_MILITARY,                    .key = TR_PARAMETER_VALUE_MESSAGE_WRATH_OF_MARS_NO_MILITARY },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "wrath_of_venus",            .value = MESSAGE_WRATH_OF_VENUS,                    .key = TR_PARAMETER_VALUE_MESSAGE_WRATH_OF_VENUS },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "wrath_of_neptune",          .value = MESSAGE_WRATH_OF_NEPTUNE,                    .key = TR_PARAMETER_VALUE_MESSAGE_WRATH_OF_NEPTUNE },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "wrath_of_mars",             .value = MESSAGE_WRATH_OF_MARS,                    .key = TR_PARAMETER_VALUE_MESSAGE_WRATH_OF_MARS },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "ceres_upset",               .value = MESSAGE_CERES_IS_UPSET,                    .key = TR_PARAMETER_VALUE_MESSAGE_CERES_IS_UPSET },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "neptune_upset",             .value = MESSAGE_NEPTUNE_IS_UPSET,                    .key = TR_PARAMETER_VALUE_MESSAGE_NEPTUNE_IS_UPSET },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "mercury_upset",             .value = MESSAGE_MERCURY_IS_UPSET,                    .key = TR_PARAMETER_VALUE_MESSAGE_MERCURY_IS_UPSET },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "mars_upset",                .value = MESSAGE_MARS_IS_UPSET,                    .key = TR_PARAMETER_VALUE_MESSAGE_MARS_IS_UPSET },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "venus_upset",               .value = MESSAGE_VENUS_IS_UPSET,                    .key = TR_PARAMETER_VALUE_MESSAGE_VENUS_IS_UPSET },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "blessing_ceres",            .value = MESSAGE_BLESSING_FROM_CERES,                    .key = TR_PARAMETER_VALUE_MESSAGE_BLESSING_FROM_CERES },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "blessing_neptune",          .value = MESSAGE_BLESSING_FROM_NEPTUNE,                    .key = TR_PARAMETER_VALUE_MESSAGE_BLESSING_FROM_NEPTUNE },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "blessing_mercury",          .value = MESSAGE_BLESSING_FROM_MERCURY,                    .key = TR_PARAMETER_VALUE_MESSAGE_BLESSING_FROM_MERCURY },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "blessing_mars",             .value = MESSAGE_BLESSING_FROM_MARS,                    .key = TR_PARAMETER_VALUE_MESSAGE_BLESSING_FROM_MARS },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "blessing_venus",            .value = MESSAGE_BLESSING_FROM_VENUS,                    .key = TR_PARAMETER_VALUE_MESSAGE_BLESSING_FROM_VENUS },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "blessing_mercury_2",        .value = MESSAGE_BLESSING_FROM_MERCURY_ALTERNATE,                    .key = TR_PARAMETER_VALUE_MESSAGE_BLESSING_FROM_MERCURY_ALTERNATE },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "blessing_neptune_2",        .value = MESSAGE_BLESSING_FROM_NEPTUNE_ALTERNATE,                    .key = TR_PARAMETER_VALUE_MESSAGE_BLESSING_FROM_NEPTUNE_ALTERNATE },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "blessing_venus_2",          .value = MESSAGE_BLESSING_FROM_VENUS_ALTERNATE,                    .key = TR_PARAMETER_VALUE_MESSAGE_BLESSING_FROM_VENUS_ALTERNATE },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "wrath_of_mars_3",           .value = MESSAGE_WRATH_OF_MARS_NO_NATIVES,                    .key = TR_PARAMETER_VALUE_MESSAGE_WRATH_OF_MARS_NO_NATIVES },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "gods_wrathful",             .value = MESSAGE_GODS_WRATHFUL,                    .key = TR_PARAMETER_VALUE_MESSAGE_GODS_WRATHFUL },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "distant_battle_lost_no_troops",   .value = MESSAGE_DISTANT_BATTLE_LOST_NO_TROOPS,                    .key = TR_PARAMETER_VALUE_MESSAGE_DISTANT_BATTLE_LOST_NO_TROOPS },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "distant_battle_lost_too_late",    .value = MESSAGE_DISTANT_BATTLE_LOST_TOO_LATE,                    .key = TR_PARAMETER_VALUE_MESSAGE_DISTANT_BATTLE_LOST_TOO_LATE },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "distant_battle_lost_too_weak",    .value = MESSAGE_DISTANT_BATTLE_LOST_TOO_WEAK,                    .key = TR_PARAMETER_VALUE_MESSAGE_DISTANT_BATTLE_LOST_TOO_WEAK },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "distant_battle_won",              .value = MESSAGE_DISTANT_BATTLE_WON,                    .key = TR_PARAMETER_VALUE_MESSAGE_DISTANT_BATTLE_WON },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "distant_battle_city_retaken",     .value = MESSAGE_DISTANT_BATTLE_CITY_RETAKEN,                    .key = TR_PARAMETER_VALUE_MESSAGE_DISTANT_BATTLE_CITY_RETAKEN },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "health_illness",            .value = MESSAGE_HEALTH_ILLNESS,                    .key = TR_PARAMETER_VALUE_MESSAGE_HEALTH_ILLNESS },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "health_disease",            .value = MESSAGE_HEALTH_DISEASE,                    .key = TR_PARAMETER_VALUE_MESSAGE_HEALTH_DISEASE },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "health_pestilence",         .value = MESSAGE_HEALTH_PESTILENCE,                    .key = TR_PARAMETER_VALUE_MESSAGE_HEALTH_PESTILENCE },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "caesar_respect_1",          .value = MESSAGE_CAESAR_RESPECT_1,                    .key = TR_PARAMETER_VALUE_MESSAGE_CAESAR_RESPECT_1 },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "caesar_respect_2",          .value = MESSAGE_CAESAR_RESPECT_2,                    .key = TR_PARAMETER_VALUE_MESSAGE_CAESAR_RESPECT_2 },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "caesar_respect_3",          .value = MESSAGE_CAESAR_RESPECT_3,                    .key = TR_PARAMETER_VALUE_MESSAGE_CAESAR_RESPECT_3 },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "emigration",                .value = MESSAGE_EMIGRATION,                    .key = TR_PARAMETER_VALUE_MESSAGE_EMIGRATION },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "fired",                     .value = MESSAGE_FIRED,                    .key = TR_PARAMETER_VALUE_MESSAGE_FIRED },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "soldiers_starving",         .value = MESSAGE_SOLDIERS_STARVING,                    .key = TR_PARAMETER_VALUE_MESSAGE_SOLDIERS_STARVING },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "caesar_angry",              .value = MESSAGE_CAESAR_ANGER,                    .key = TR_PARAMETER_VALUE_MESSAGE_CAESAR_ANGER },
    {.type = PARAMETER_TYPE_STANDARD_MESSAGE,            .text = "enemies_leaving",           .value = MESSAGE_ENEMIES_LEAVING,                    .key = TR_PARAMETER_VALUE_MESSAGE_ENEMIES_LEAVING },
};

#define SPECIAL_ATTRIBUTE_MAPPINGS_STANDARD_MESSAGE_SIZE (sizeof(special_attribute_mappings_standard_message) / sizeof(special_attribute_mapping_t))

static special_attribute_mapping_t special_attribute_mappings_media_type[] = {
    {.type = PARAMETER_TYPE_MEDIA_TYPE,                  .text = "sound",                     .value = 1,          .key = TR_PARAMETER_VALUE_MEDIA_TYPE_SOUND },
    {.type = PARAMETER_TYPE_MEDIA_TYPE,                  .text = "video",                     .value = 2,          .key = TR_PARAMETER_VALUE_MEDIA_TYPE_VIDEO },
    {.type = PARAMETER_TYPE_MEDIA_TYPE,                  .text = "speech",                    .value = 3,          .key = TR_PARAMETER_VALUE_MEDIA_TYPE_SPEECH },
    {.type = PARAMETER_TYPE_MEDIA_TYPE,                  .text = "background_image",          .value = 4,          .key = TR_PARAMETER_VALUE_MEDIA_TYPE_BACKGROUND_IMAGE },
};

#define SPECIAL_ATTRIBUTE_MAPPINGS_MEDIA_TYPE_SIZE (sizeof(special_attribute_mappings_media_type) / sizeof(special_attribute_mapping_t))

static special_attribute_mapping_t special_attribute_mappings_rating_type[] = {
    {.type = PARAMETER_TYPE_RATING_TYPE,                .text = "peace",                      .value = SELECTED_RATING_PEACE,          .key = TR_PARAMETER_VALUE_RATING_TYPE_PEACE },
    {.type = PARAMETER_TYPE_RATING_TYPE,                .text = "prosperity",                 .value = SELECTED_RATING_PROSPERITY,     .key = TR_PARAMETER_VALUE_RATING_TYPE_PROSPERITY },
};

#define SPECIAL_ATTRIBUTE_MAPPINGS_RATING_TYPE_SIZE (sizeof(special_attribute_mappings_rating_type) / sizeof(special_attribute_mapping_t))

static special_attribute_mapping_t special_attribute_mappings_storage_type[] = {
    {.type = PARAMETER_TYPE_STORAGE_TYPE,                .text = "all",                    .value = STORAGE_TYPE_ALL,               .key = TR_PARAMETER_VALUE_STORAGE_TYPE_ALL },
    {.type = PARAMETER_TYPE_STORAGE_TYPE,                .text = "granaries",              .value = STORAGE_TYPE_GRANARIES,         .key = TR_PARAMETER_VALUE_STORAGE_TYPE_GRANARIES },
    {.type = PARAMETER_TYPE_STORAGE_TYPE,                .text = "warehouses",             .value = STORAGE_TYPE_WAREHOUSES,        .key = TR_PARAMETER_VALUE_STORAGE_TYPE_WAREHOUSES },
};

#define SPECIAL_ATTRIBUTE_MAPPINGS_STORAGE_TYPE_SIZE (sizeof(special_attribute_mappings_storage_type) / sizeof(special_attribute_mapping_t))

static special_attribute_mapping_t special_attribute_mappings_attack_type[] = {
    {.type = PARAMETER_TYPE_INVASION_TYPE,                .text = "enemy_army",          .value = INVASION_TYPE_ENEMY_ARMY,          .key = TR_PARAMETER_VALUE_INVASION_TYPE_ENEMY_ARMY },
    {.type = PARAMETER_TYPE_INVASION_TYPE,                .text = "caesar",              .value = INVASION_TYPE_CAESAR,              .key = TR_PARAMETER_VALUE_INVASION_TYPE_CAESAR },
    {.type = PARAMETER_TYPE_INVASION_TYPE,                .text = "natives",             .value = INVASION_TYPE_LOCAL_UPRISING,      .key = TR_PARAMETER_VALUE_INVASION_TYPE_NATIVES },
    {.type = PARAMETER_TYPE_INVASION_TYPE,                .text = "mars",                .value = INVASION_TYPE_MARS_NATIVES,        .key = TR_PARAMETER_VALUE_INVASION_TYPE_MARS_NATIVES },
};

#define SPECIAL_ATTRIBUTE_MAPPINGS_INVASION_TYPE_SIZE (sizeof(special_attribute_mappings_attack_type) / sizeof(special_attribute_mapping_t))

static special_attribute_mapping_t special_attribute_mappings_target_type[] = {
    {.type = PARAMETER_TYPE_TARGET_TYPE,                .text = "food_chain",          .value = FORMATION_ATTACK_FOOD_CHAIN,      .key = TR_PARAMETER_VALUE_FORMATION_ATTACK_FOOD_CHAIN },
    {.type = PARAMETER_TYPE_TARGET_TYPE,                .text = "gold_stores",         .value = FORMATION_ATTACK_GOLD_STORES,     .key = TR_PARAMETER_VALUE_FORMATION_ATTACK_GOLD_STORES },
    {.type = PARAMETER_TYPE_TARGET_TYPE,                .text = "natives",             .value = FORMATION_ATTACK_BEST_BUILDINGS,  .key = TR_PARAMETER_VALUE_FORMATION_ATTACK_BEST_BUILDINGS },
    {.type = PARAMETER_TYPE_TARGET_TYPE,                .text = "troops",              .value = FORMATION_ATTACK_TROOPS,          .key = TR_PARAMETER_VALUE_FORMATION_ATTACK_TROOPS },
    {.type = PARAMETER_TYPE_TARGET_TYPE,                .text = "random",              .value = FORMATION_ATTACK_RANDOM,          .key = TR_PARAMETER_VALUE_FORMATION_ATTACK_RANDOM },
};

#define SPECIAL_ATTRIBUTE_MAPPINGS_TARGET_TYPE_SIZE (sizeof(special_attribute_mappings_target_type) / sizeof(special_attribute_mapping_t))

static special_attribute_mapping_t special_attribute_mappings_enemy_type[] = {
    {.type = PARAMETER_TYPE_ENEMY_TYPE,                .text = "undefined",       .value = ENEMY_UNDEFINED,         .key = TR_PARAMETER_VALUE_ENEMY_UNDEFINED },
    /* TODO: Once maps no longer override army types, then re-enable this list so user can pick what they want.
        { .type = PARAMETER_TYPE_ENEMY_TYPE,                .text = "barbarian",       .value = ENEMY_0_BARBARIAN,       .key = TR_PARAMETER_VALUE_ENEMY_0_BARBARIAN },
        { .type = PARAMETER_TYPE_ENEMY_TYPE,                .text = "numidian",        .value = ENEMY_1_NUMIDIAN,        .key = TR_PARAMETER_VALUE_ENEMY_1_NUMIDIAN },
        { .type = PARAMETER_TYPE_ENEMY_TYPE,                .text = "gaul",            .value = ENEMY_2_GAUL,            .key = TR_PARAMETER_VALUE_ENEMY_2_GAUL },
        { .type = PARAMETER_TYPE_ENEMY_TYPE,                .text = "celt",            .value = ENEMY_3_CELT,            .key = TR_PARAMETER_VALUE_ENEMY_3_CELT },
        { .type = PARAMETER_TYPE_ENEMY_TYPE,                .text = "goth",            .value = ENEMY_4_GOTH,            .key = TR_PARAMETER_VALUE_ENEMY_4_GOTH },
        { .type = PARAMETER_TYPE_ENEMY_TYPE,                .text = "perganum",        .value = ENEMY_5_PERGAMUM,        .key = TR_PARAMETER_VALUE_ENEMY_5_PERGAMUM },
        { .type = PARAMETER_TYPE_ENEMY_TYPE,                .text = "seleucid",        .value = ENEMY_6_SELEUCID,        .key = TR_PARAMETER_VALUE_ENEMY_6_SELEUCID },
        { .type = PARAMETER_TYPE_ENEMY_TYPE,                .text = "etruscan",        .value = ENEMY_7_ETRUSCAN,        .key = TR_PARAMETER_VALUE_ENEMY_7_ETRUSCAN },
        { .type = PARAMETER_TYPE_ENEMY_TYPE,                .text = "greek",           .value = ENEMY_8_GREEK,           .key = TR_PARAMETER_VALUE_ENEMY_8_GREEK },
        { .type = PARAMETER_TYPE_ENEMY_TYPE,                .text = "egyptian",        .value = ENEMY_9_EGYPTIAN,        .key = TR_PARAMETER_VALUE_ENEMY_9_EGYPTIAN },
        { .type = PARAMETER_TYPE_ENEMY_TYPE,                .text = "carthaginian",    .value = ENEMY_10_CARTHAGINIAN,   .key = TR_PARAMETER_VALUE_ENEMY_10_CARTHAGINIAN },
        { .type = PARAMETER_TYPE_ENEMY_TYPE,                .text = "caesar",          .value = ENEMY_11_CAESAR,         .key = TR_PARAMETER_VALUE_ENEMY_11_CAESAR },
    */
};

#define SPECIAL_ATTRIBUTE_MAPPINGS_ENEMY_TYPE_SIZE (sizeof(special_attribute_mappings_enemy_type) / sizeof(special_attribute_mapping_t))

static special_attribute_mapping_t special_attribute_mappings_god[] =
{
    {.type = PARAMETER_TYPE_GOD,                .text = "Ceres",          .value = GOD_CERES,         .key = TR_PARAMETER_VALUE_GOD_CERES },
    {.type = PARAMETER_TYPE_GOD,                .text = "Mars",           .value = GOD_MARS,          .key = TR_PARAMETER_VALUE_GOD_MARS },
    {.type = PARAMETER_TYPE_GOD,                .text = "Mercury",        .value = GOD_MERCURY,       .key = TR_PARAMETER_VALUE_GOD_MERCURY },
    {.type = PARAMETER_TYPE_GOD,                .text = "Neptune",        .value = GOD_NEPTUNE,       .key = TR_PARAMETER_VALUE_GOD_NEPTUNE },
    {.type = PARAMETER_TYPE_GOD,                .text = "Venus",          .value = GOD_VENUS,         .key = TR_PARAMETER_VALUE_GOD_VENUS },
    {.type = PARAMETER_TYPE_GOD,                .text = "All",            .value = GOD_ALL,           .key = TR_CITY_PROPERTY_ALL },

};

#define SPECIAL_ATTRIBUTE_MAPPINGS_GOD_SIZE (sizeof(special_attribute_mappings_god) / sizeof(special_attribute_mapping_t))

static special_attribute_mapping_t special_attribute_mappings_climate[] =
{
    {.type = PARAMETER_TYPE_CLIMATE,            .text = "Central",        .value = CLIMATE_CENTRAL,   .key = TR_PARAMETER_VALUE_CLIMATE_CENTRAL },
    {.type = PARAMETER_TYPE_CLIMATE,            .text = "Northern",       .value = CLIMATE_NORTHERN,  .key = TR_PARAMETER_VALUE_CLIMATE_NORTHERN },
    {.type = PARAMETER_TYPE_CLIMATE,            .text = "Desert",         .value = CLIMATE_DESERT,    .key = TR_PARAMETER_VALUE_CLIMATE_DESERT },
};

#define SPECIAL_ATTRIBUTE_MAPPINGS_CLIMATE_SIZE (sizeof(special_attribute_mappings_climate) / sizeof(special_attribute_mapping_t))

static special_attribute_mapping_t special_attribute_mappings_terrain[] =
{
    {.type = PARAMETER_TYPE_TERRAIN,            .text = "Water",            .value = TERRAIN_WATER,   .key = TR_PARAMETER_TERRAIN_WATER },
    {.type = PARAMETER_TYPE_TERRAIN,            .text = "Rock",             .value = TERRAIN_ROCK,    .key = TR_PARAMETER_TERRAIN_ROCK },
    {.type = PARAMETER_TYPE_TERRAIN,            .text = "Fertile Ground",   .value = TERRAIN_MEADOW,  .key = TR_PARAMETER_TERRAIN_MEADOW },
    {.type = PARAMETER_TYPE_TERRAIN,            .text = "Tree",             .value = TERRAIN_TREE,    .key = TR_PARAMETER_TERRAIN_TREE },
    {.type = PARAMETER_TYPE_TERRAIN,            .text = "Shrub",            .value = TERRAIN_SHRUB,   .key = TR_PARAMETER_TERRAIN_SHRUB },
};

#define SPECIAL_ATTRIBUTE_MAPPINGS_TERRAIN_SIZE (sizeof(special_attribute_mappings_terrain) / sizeof(special_attribute_mapping_t))

special_attribute_mapping_t special_attribute_mappings_data_type[] =
{
    {.type = PARAMETER_TYPE_DATA_TYPE,          .text = "cost",                     .value = MODEL_COST,                    .key = TR_PARAMETER_COST },
    {.type = PARAMETER_TYPE_DATA_TYPE,          .text = "desirability_value",       .value = MODEL_DESIRABILITY_VALUE,      .key = TR_PARAMETER_DESIRABILITY_VALUE },
    {.type = PARAMETER_TYPE_DATA_TYPE,          .text = "desirability_step",        .value = MODEL_DESIRABILITY_STEP,       .key = TR_PARAMETER_DESIRABILITY_STEP },
    {.type = PARAMETER_TYPE_DATA_TYPE,          .text = "desirability_step_size",   .value = MODEL_DESIRABILITY_STEP_SIZE,  .key = TR_PARAMETER_DESIRABILITY_STEP_SIZE },
    {.type = PARAMETER_TYPE_DATA_TYPE,          .text = "desirability_range",       .value = MODEL_DESIRABILITY_RANGE,      .key = TR_PARAMETER_DESIRABILITY_RANGE },
    {.type = PARAMETER_TYPE_DATA_TYPE,          .text = "laborers",                 .value = MODEL_LABORERS,                .key = TR_PARAMETER_LABORERS },
};

#define SPECIAL_ATTRIBUTE_MAPPINGS_DATA_TYPE_SIZE (sizeof(special_attribute_mappings_data_type) / sizeof(special_attribute_mapping_t))

special_attribute_mapping_t special_attribute_mappings_house_data_type[] =
{
    {.type = PARAMETER_TYPE_HOUSE_DATA_TYPE,          .text = "devolve_desirability",   .value = MODEL_DEVOLVE_DESIRABILITY,    .key = TR_PARAMETER_DEVOLVE_DESIRABILITY },
    {.type = PARAMETER_TYPE_HOUSE_DATA_TYPE,          .text = "evolve_desirability",    .value = MODEL_EVOLVE_DESIRABILITY,     .key = TR_PARAMETER_EVOLVE_DESIRABILITY },
    {.type = PARAMETER_TYPE_HOUSE_DATA_TYPE,          .text = "entertainment",          .value = MODEL_ENTERTAINMENT,           .key = TR_PARAMETER_ENTERTAINMENT },
    {.type = PARAMETER_TYPE_HOUSE_DATA_TYPE,          .text = "water",                  .value = MODEL_WATER,                   .key = TR_PARAMETER_WATER },
    {.type = PARAMETER_TYPE_HOUSE_DATA_TYPE,          .text = "religion",               .value = MODEL_RELIGION,                .key = TR_PARAMETER_RELIGION },
    {.type = PARAMETER_TYPE_HOUSE_DATA_TYPE,          .text = "education",              .value = MODEL_EDUCATION,               .key = TR_PARAMETER_EDUCATION },
    {.type = PARAMETER_TYPE_HOUSE_DATA_TYPE,          .text = "barber",                 .value = MODEL_BARBER,                  .key = TR_PARAMETER_BARBER },
    {.type = PARAMETER_TYPE_HOUSE_DATA_TYPE,          .text = "bathhouse",              .value = MODEL_BATHHOUSE,               .key = TR_PARAMETER_BATHHOUSE },
    {.type = PARAMETER_TYPE_HOUSE_DATA_TYPE,          .text = "health",                 .value = MODEL_HEALTH,                  .key = TR_PARAMETER_HEALTH },
    {.type = PARAMETER_TYPE_HOUSE_DATA_TYPE,          .text = "food_types",             .value = MODEL_FOOD_TYPES,              .key = TR_PARAMETER_FOOD_TYPES },
    {.type = PARAMETER_TYPE_HOUSE_DATA_TYPE,          .text = "pottery",                .value = MODEL_POTTERY,                 .key = TR_PARAMETER_POTTERY },
    {.type = PARAMETER_TYPE_HOUSE_DATA_TYPE,          .text = "oil",                    .value = MODEL_OIL,                     .key = TR_PARAMETER_OIL },
    {.type = PARAMETER_TYPE_HOUSE_DATA_TYPE,          .text = "furniture",              .value = MODEL_FURNITURE,               .key = TR_PARAMETER_FURNITURE },
    {.type = PARAMETER_TYPE_HOUSE_DATA_TYPE,          .text = "wine",                   .value = MODEL_WINE,                    .key = TR_PARAMETER_WINE },
    {.type = PARAMETER_TYPE_HOUSE_DATA_TYPE,          .text = "prosperity",             .value = MODEL_PROSPERITY,              .key = TR_PARAMETER_PROSPERITY },
    {.type = PARAMETER_TYPE_HOUSE_DATA_TYPE,          .text = "max_people",             .value = MODEL_MAX_PEOPLE,              .key = TR_PARAMETER_MAX_PEOPLE },
    {.type = PARAMETER_TYPE_HOUSE_DATA_TYPE,          .text = "tax_multiplier",         .value = MODEL_TAX_MULTIPLIER,          .key = TR_PARAMETER_TAX_MULTIPLIER },
};

#define SPECIAL_ATTRIBUTE_MAPPINGS_HOUSE_DATA_TYPE_SIZE (sizeof(special_attribute_mappings_house_data_type) / sizeof(special_attribute_mapping_t))

static special_attribute_mapping_t special_attribute_mappings_percentage[] = {
    {.type = PARAMETER_TYPE_PERCENTAGE, .text = "Percentage", .value = 0, .key = TR_PARAMETER_PERCENTAGE },
    {.type = PARAMETER_TYPE_PERCENTAGE, .text = "Absolute",   .value = 1, .key = TR_PARAMETER_ABSOLUTE },
};

#define SPECIAL_ATTRIBUTE_MAPPINGS_PERCENTAGE_SIZE (sizeof(special_attribute_mappings_percentage) / sizeof(special_attribute_mapping_t))

static special_attribute_mapping_t special_attribute_mappings_housing[] = {
    // Individual housing types
    {.type = PARAMETER_TYPE_HOUSING_TYPE, .text = "small_tent",       .value = BUILDING_HOUSE_SMALL_TENT,     .key = TR_BUILDING_HOUSE_SMALL_TENT },
    {.type = PARAMETER_TYPE_HOUSING_TYPE, .text = "large_tent",       .value = BUILDING_HOUSE_LARGE_TENT,     .key = TR_BUILDING_HOUSE_LARGE_TENT },
    {.type = PARAMETER_TYPE_HOUSING_TYPE, .text = "small_shack",      .value = BUILDING_HOUSE_SMALL_SHACK,    .key = TR_BUILDING_HOUSE_SMALL_SHACK },
    {.type = PARAMETER_TYPE_HOUSING_TYPE, .text = "large_shack",      .value = BUILDING_HOUSE_LARGE_SHACK,    .key = TR_BUILDING_HOUSE_LARGE_SHACK },
    {.type = PARAMETER_TYPE_HOUSING_TYPE, .text = "small_hovel",      .value = BUILDING_HOUSE_SMALL_HOVEL,    .key = TR_BUILDING_HOUSE_SMALL_HOVEL },
    {.type = PARAMETER_TYPE_HOUSING_TYPE, .text = "large_hovel",      .value = BUILDING_HOUSE_LARGE_HOVEL,    .key = TR_BUILDING_HOUSE_LARGE_HOVEL },
    {.type = PARAMETER_TYPE_HOUSING_TYPE, .text = "small_casa",       .value = BUILDING_HOUSE_SMALL_CASA,     .key = TR_BUILDING_HOUSE_SMALL_CASA },
    {.type = PARAMETER_TYPE_HOUSING_TYPE, .text = "large_casa",       .value = BUILDING_HOUSE_LARGE_CASA,     .key = TR_BUILDING_HOUSE_LARGE_CASA },
    {.type = PARAMETER_TYPE_HOUSING_TYPE, .text = "small_insula",     .value = BUILDING_HOUSE_SMALL_INSULA,   .key = TR_BUILDING_HOUSE_SMALL_INSULA },
    {.type = PARAMETER_TYPE_HOUSING_TYPE, .text = "medium_insula",    .value = BUILDING_HOUSE_MEDIUM_INSULA,  .key = TR_BUILDING_HOUSE_MEDIUM_INSULA },
    {.type = PARAMETER_TYPE_HOUSING_TYPE, .text = "large_insula",     .value = BUILDING_HOUSE_LARGE_INSULA,   .key = TR_BUILDING_HOUSE_LARGE_INSULA },
    {.type = PARAMETER_TYPE_HOUSING_TYPE, .text = "grand_insula",     .value = BUILDING_HOUSE_GRAND_INSULA,   .key = TR_BUILDING_HOUSE_GRAND_INSULA },
    {.type = PARAMETER_TYPE_HOUSING_TYPE, .text = "small_villa",      .value = BUILDING_HOUSE_SMALL_VILLA,    .key = TR_BUILDING_HOUSE_SMALL_VILLA },
    {.type = PARAMETER_TYPE_HOUSING_TYPE, .text = "medium_villa",     .value = BUILDING_HOUSE_MEDIUM_VILLA,   .key = TR_BUILDING_HOUSE_MEDIUM_VILLA },
    {.type = PARAMETER_TYPE_HOUSING_TYPE, .text = "large_villa",      .value = BUILDING_HOUSE_LARGE_VILLA,    .key = TR_BUILDING_HOUSE_LARGE_VILLA },
    {.type = PARAMETER_TYPE_HOUSING_TYPE, .text = "grand_villa",      .value = BUILDING_HOUSE_GRAND_VILLA,    .key = TR_BUILDING_HOUSE_GRAND_VILLA },
    {.type = PARAMETER_TYPE_HOUSING_TYPE, .text = "small_palace",     .value = BUILDING_HOUSE_SMALL_PALACE,   .key = TR_BUILDING_HOUSE_SMALL_PALACE },
    {.type = PARAMETER_TYPE_HOUSING_TYPE, .text = "medium_palace",    .value = BUILDING_HOUSE_MEDIUM_PALACE,  .key = TR_BUILDING_HOUSE_MEDIUM_PALACE },
    {.type = PARAMETER_TYPE_HOUSING_TYPE, .text = "large_palace",     .value = BUILDING_HOUSE_LARGE_PALACE,   .key = TR_BUILDING_HOUSE_LARGE_PALACE },
    {.type = PARAMETER_TYPE_HOUSING_TYPE, .text = "luxury_palace",    .value = BUILDING_HOUSE_LUXURY_PALACE,  .key = TR_BUILDING_HOUSE_LUXURY_PALACE },


    // Housing groups (using overlay enum values for groups)
    {.type = PARAMETER_TYPE_HOUSING_TYPE, .text = "group_tents",      .value = HOUSE_GROUP_TENT,     .key = TR_OVERLAY_HOUSING_TENTS },
    {.type = PARAMETER_TYPE_HOUSING_TYPE, .text = "group_shacks",     .value = HOUSE_GROUP_SHACK,    .key = TR_OVERLAY_HOUSING_SHACKS },
    {.type = PARAMETER_TYPE_HOUSING_TYPE, .text = "group_hovels",     .value = HOUSE_GROUP_HOVEL,    .key = TR_OVERLAY_HOUSING_HOVELS },
    {.type = PARAMETER_TYPE_HOUSING_TYPE, .text = "group_casae",      .value = HOUSE_GROUP_CASA,     .key = TR_OVERLAY_HOUSING_CASAS },
    {.type = PARAMETER_TYPE_HOUSING_TYPE, .text = "group_insulae",    .value = HOUSE_GROUP_INSULA,   .key = TR_OVERLAY_HOUSE_INSULAS },
    {.type = PARAMETER_TYPE_HOUSING_TYPE, .text = "group_villas",     .value = HOUSE_GROUP_VILLA,    .key = TR_OVERLAY_HOUSE_VILLAS },
    {.type = PARAMETER_TYPE_HOUSING_TYPE, .text = "group_palaces",    .value = HOUSE_GROUP_PALACE,   .key = TR_OVERLAY_HOUSE_PALACES },
};

#define SPECIAL_ATTRIBUTE_MAPPINGS_HOUSING_SIZE (sizeof(special_attribute_mappings_housing) / sizeof(special_attribute_mapping_t))

static special_attribute_mapping_t special_attribute_mappings_age[] = {
    // Decenniums (10-year age groups)
    {.type = PARAMETER_TYPE_AGE_GROUP, .text = "decennium_0",  .value = 0,  .key = TR_DECENNIUM_0 }, // Ages 0-9
    {.type = PARAMETER_TYPE_AGE_GROUP, .text = "decennium_1",  .value = 1,  .key = TR_DECENNIUM_1 }, // Ages 10-19
    {.type = PARAMETER_TYPE_AGE_GROUP, .text = "decennium_2",  .value = 2,  .key = TR_DECENNIUM_2 }, // Ages 20-29
    {.type = PARAMETER_TYPE_AGE_GROUP, .text = "decennium_3",  .value = 3,  .key = TR_DECENNIUM_3 }, // Ages 30-39
    {.type = PARAMETER_TYPE_AGE_GROUP, .text = "decennium_4",  .value = 4,  .key = TR_DECENNIUM_4 }, // Ages 40-49
    {.type = PARAMETER_TYPE_AGE_GROUP, .text = "decennium_5",  .value = 5,  .key = TR_DECENNIUM_5 }, // Ages 50-59
    {.type = PARAMETER_TYPE_AGE_GROUP, .text = "decennium_6",  .value = 6,  .key = TR_DECENNIUM_6 }, // Ages 60-69
    {.type = PARAMETER_TYPE_AGE_GROUP, .text = "decennium_7",  .value = 7,  .key = TR_DECENNIUM_7 }, // Ages 70-79
    {.type = PARAMETER_TYPE_AGE_GROUP, .text = "decennium_8",  .value = 8,  .key = TR_DECENNIUM_8 }, // Ages 80-89
    {.type = PARAMETER_TYPE_AGE_GROUP, .text = "decennium_9",  .value = 9,  .key = TR_DECENNIUM_9 }, // Ages 90-99

    // Age groups
    {.type = PARAMETER_TYPE_AGE_GROUP, .text = "school_age",   .value = 10, .key = TR_PLACEHOLDER }, // Ages 0-14
    {.type = PARAMETER_TYPE_AGE_GROUP, .text = "academy_age",  .value = 11, .key = TR_PLACEHOLDER }, // Ages 14-21
    {.type = PARAMETER_TYPE_AGE_GROUP, .text = "working_age",  .value = 12, .key = TR_PLACEHOLDER }, // Ages 20-49/59 (depends on config)
    {.type = PARAMETER_TYPE_AGE_GROUP, .text = "retired",      .value = 13, .key = TR_PLACEHOLDER }, // Ages 50+/60+ (depends on config)
};

#define SPECIAL_ATTRIBUTE_MAPPINGS_AGE_SIZE (sizeof(special_attribute_mappings_age) / sizeof(special_attribute_mapping_t))

// like condition, but not condition
static special_attribute_mapping_t special_attribute_mappings_city_property[] = {
    {.type = PARAMETER_TYPE_CITY_PROPERTY, .text = "difficulty",              .value = CITY_PROPERTY_DIFFICULTY,              .key = TR_CITY_PROPERTY_DIFFICULTY },
    {.type = PARAMETER_TYPE_CITY_PROPERTY, .text = "money",                   .value = CITY_PROPERTY_MONEY,                   .key = TR_CITY_PROPERTY_MONEY },
    {.type = PARAMETER_TYPE_CITY_PROPERTY, .text = "population",              .value = CITY_PROPERTY_POPULATION,              .key = TR_CITY_PROPERTY_POPULATION },
    {.type = PARAMETER_TYPE_CITY_PROPERTY, .text = "savings",                 .value = CITY_PROPERTY_SAVINGS,                 .key = TR_CITY_PROPERTY_SAVINGS },
    {.type = PARAMETER_TYPE_CITY_PROPERTY, .text = "year_finance_balance",    .value = CITY_PROPERTY_YEAR_FINANCE_BALANCE,    .key = TR_CITY_PROPERTY_YEAR_FINANCE_BALANCE },
    {.type = PARAMETER_TYPE_CITY_PROPERTY, .text = "resource_stock",          .value = CITY_PROPERTY_RESOURCE_STOCK,          .key = TR_CITY_PROPERTY_RESOURCE_STOCK },
    {.type = PARAMETER_TYPE_CITY_PROPERTY, .text = "stats_favor",             .value = CITY_PROPERTY_STATS_FAVOR,             .key = TR_CITY_PROPERTY_STATS_FAVOR },
    {.type = PARAMETER_TYPE_CITY_PROPERTY, .text = "stats_prosperity",        .value = CITY_PROPERTY_STATS_PROSPERITY,        .key = TR_CITY_PROPERTY_STATS_PROSPERITY },
    {.type = PARAMETER_TYPE_CITY_PROPERTY, .text = "stats_culture",           .value = CITY_PROPERTY_STATS_CULTURE,           .key = TR_CITY_PROPERTY_STATS_CULTURE },
    {.type = PARAMETER_TYPE_CITY_PROPERTY, .text = "stats_peace",             .value = CITY_PROPERTY_STATS_PEACE,             .key = TR_CITY_PROPERTY_STATS_PEACE },
    {.type = PARAMETER_TYPE_CITY_PROPERTY, .text = "stats_city_health",       .value = CITY_PROPERTY_STATS_CITY_HEALTH,       .key = TR_CITY_PROPERTY_STATS_CITY_HEALTH },
    {.type = PARAMETER_TYPE_CITY_PROPERTY, .text = "service coverage",        .value = CITY_PROPERTY_SERVICE_COVERAGE,        .key = TR_CITY_PROPERTY_SERVICE_COVERAGE },
    {.type = PARAMETER_TYPE_CITY_PROPERTY, .text = "pops_unemployment",       .value = CITY_PROPERTY_POPS_UNEMPLOYMENT,       .key = TR_CITY_PROPERTY_POPS_UNEMPLOYMENT },
    {.type = PARAMETER_TYPE_CITY_PROPERTY, .text = "pops_housing_type",       .value = CITY_PROPERTY_POPS_HOUSING_TYPE,       .key = TR_CITY_PROPERTY_POPS_HOUSING_TYPE },
    {.type = PARAMETER_TYPE_CITY_PROPERTY, .text = "pops_age",                .value = CITY_PROPERTY_POPS_BY_AGE,             .key = TR_CITY_PROPERTY_POPS_BY_AGE },
    {.type = PARAMETER_TYPE_CITY_PROPERTY, .text = "rome_wages",              .value = CITY_PROPERTY_ROME_WAGES,              .key = TR_CITY_PROPERTY_ROME_WAGES },
    {.type = PARAMETER_TYPE_CITY_PROPERTY, .text = "city_wages",              .value = CITY_PROPERTY_CITY_WAGES,              .key = TR_CITY_PROPERTY_CITY_WAGES },
    {.type = PARAMETER_TYPE_CITY_PROPERTY, .text = "building_count",          .value = CITY_PROPERTY_BUILDING_COUNT,          .key = TR_CITY_PROPERTY_BUILDING_COUNT },
    {.type = PARAMETER_TYPE_CITY_PROPERTY, .text = "troops_count_player",     .value = CITY_PROPERTY_TROOPS_COUNT_PLAYER,     .key = TR_CITY_PROPERTY_TROOPS_COUNT_PLAYER },
    {.type = PARAMETER_TYPE_CITY_PROPERTY, .text = "troops_count_enemy",      .value = CITY_PROPERTY_TROOPS_COUNT_ENEMY,      .key = TR_CITY_PROPERTY_TROOPS_COUNT_ENEMY },
    {.type = PARAMETER_TYPE_CITY_PROPERTY, .text = "terrain_count_tiles",     .value = CITY_PROPERTY_TERRAIN_COUNT_TILES,     .key = TR_CITY_PROPERTY_TERRAIN_COUNT_TILES },
    {.type = PARAMETER_TYPE_CITY_PROPERTY, .text = "quota_fill",       .value = CITY_PROPERTY_QUOTA_FILL,              .key = TR_CITY_PROPERTY_QUOTA_FILL },

};

#define SPECIAL_ATTRIBUTE_MAPPINGS_CITY_PROPERTY_SIZE (sizeof(special_attribute_mappings_city_property) / sizeof(special_attribute_mapping_t))


static special_attribute_mapping_t special_attribute_mappings_troops_class[] = {
    {.type = PARAMETER_TYPE_ENEMY_CLASS, .text = "all",            .value = ENEMY_CLASS_ALL,            .key = TR_CITY_PROPERTY_ALL },
    {.type = PARAMETER_TYPE_ENEMY_CLASS, .text = "melee",          .value = ENEMY_CLASS_MELEE,          .key = TR_CITY_PROPERTY_TROOPS_MELEE },
    {.type = PARAMETER_TYPE_ENEMY_CLASS, .text = "ranged",         .value = ENEMY_CLASS_RANGED,         .key = TR_CITY_PROPERTY_TROOPS_RANGED },
    {.type = PARAMETER_TYPE_ENEMY_CLASS, .text = "cavalry",        .value = ENEMY_CLASS_MOUNTED,        .key = TR_CITY_PROPERTY_TROOPS_MOUNTED },
};
#define SPECIAL_ATTRIBUTE_MAPPINGS_TROOPS_CLASS_SIZE (sizeof(special_attribute_mappings_troops_class) / sizeof(special_attribute_mapping_t))

static special_attribute_mapping_t special_attribute_mappings_player_troops[] = {
    {.type = PARAMETER_TYPE_PLAYER_TROOPS, .text = "all",            .value = FIGURE_FORT_STANDARD,                          .key = TR_CITY_PROPERTY_ALL},
    {.type = PARAMETER_TYPE_PLAYER_TROOPS, .text = "sword",          .value = FIGURE_FORT_INFANTRY ,      .key = TR_BUILDING_FORT_AUXILIA_INFANTRY },
    {.type = PARAMETER_TYPE_PLAYER_TROOPS, .text = "bow",            .value = FIGURE_FORT_ARCHER,       .key = TR_BUILDING_FORT_ARCHERS },
    {.type = PARAMETER_TYPE_PLAYER_TROOPS, .text = "cavalry",        .value = FIGURE_FORT_MOUNTED,      .key = TR_BUILDING_FORT_MOUNTED},
    {.type = PARAMETER_TYPE_PLAYER_TROOPS, .text = "legionary",      .value = FIGURE_FORT_LEGIONARY,       .key = TR_BUILDING_FORT_LEGIONARIES },
    {.type = PARAMETER_TYPE_PLAYER_TROOPS, .text = "javelin",        .value = FIGURE_FORT_JAVELIN,         .key = TR_BUILDING_FORT_JAVELIN },
};

#define SPECIAL_ATTRIBUTE_MAPPINGS_PLAYER_TROOPS_SIZE (sizeof(special_attribute_mappings_player_troops) / sizeof(special_attribute_mapping_t))

static special_attribute_mapping_t special_attribute_mappings_coverage_buildings[] = {
    {.type = PARAMETER_TYPE_COVERAGE_BUILDINGS,     .text = "theatres",      .value = BUILDING_THEATER,              .key = TR_CITY_PROPERTY_COVERAGE_THEATRE },
    {.type = PARAMETER_TYPE_COVERAGE_BUILDINGS,     .text = "arenas",        .value = BUILDING_ARENA,                .key = TR_CITY_PROPERTY_COVERAGE_ARENA },
    {.type = PARAMETER_TYPE_COVERAGE_BUILDINGS,     .text = "amphitheatres", .value = BUILDING_AMPHITHEATER,         .key = TR_CITY_PROPERTY_COVERAGE_AMPHITHEATRE },
    {.type = PARAMETER_TYPE_COVERAGE_BUILDINGS,     .text = "taverns",       .value = BUILDING_TAVERN,               .key = TR_CITY_PROPERTY_COVERAGE_TAVERN },
    {.type = PARAMETER_TYPE_COVERAGE_BUILDINGS,     .text = "colosseum",     .value = BUILDING_COLOSSEUM,            .key = TR_CITY_PROPERTY_COVERAGE_COLOSSEUM },
    {.type = PARAMETER_TYPE_COVERAGE_BUILDINGS,     .text = "hippodrome",    .value = BUILDING_HIPPODROME,           .key = TR_CITY_PROPERTY_COVERAGE_HIPPODROME },
    {.type = PARAMETER_TYPE_COVERAGE_BUILDINGS,     .text = "doctors",       .value = BUILDING_DOCTOR,               .key = TR_CITY_PROPERTY_COVERAGE_DOCTOR },
    {.type = PARAMETER_TYPE_COVERAGE_BUILDINGS,     .text = "barbers",       .value = BUILDING_BARBER,               .key = TR_CITY_PROPERTY_COVERAGE_BARBER },
    {.type = PARAMETER_TYPE_COVERAGE_BUILDINGS,     .text = "hospitals",     .value = BUILDING_HOSPITAL,             .key = TR_CITY_PROPERTY_COVERAGE_HOSPITAL },
    {.type = PARAMETER_TYPE_COVERAGE_BUILDINGS,     .text = "baths",         .value = BUILDING_BATHHOUSE,            .key = TR_CITY_PROPERTY_COVERAGE_BATHS },
    {.type = PARAMETER_TYPE_COVERAGE_BUILDINGS,     .text = "schools",       .value = BUILDING_SCHOOL,               .key = TR_CITY_PROPERTY_COVERAGE_SCHOOL },
    {.type = PARAMETER_TYPE_COVERAGE_BUILDINGS,     .text = "libraries",     .value = BUILDING_LIBRARY,              .key = TR_CITY_PROPERTY_COVERAGE_LIBRARY },
    {.type = PARAMETER_TYPE_COVERAGE_BUILDINGS,     .text = "academies",     .value = BUILDING_ACADEMY,              .key = TR_CITY_PROPERTY_COVERAGE_ACADEMY },
};
#define SPECIAL_ATTRIBUTE_MAPPINGS_COVERAGE_BUILDINGS_SIZE (sizeof(special_attribute_mappings_coverage_buildings) / sizeof(special_attribute_mapping_t))

static special_attribute_mapping_t special_attribute_mappings_rank[] = {
    {.type = PARAMETER_TYPE_RANK, .text = "citizen",        .value = 0,  .key = TR_RANK_CITIZEN },
    {.type = PARAMETER_TYPE_RANK, .text = "clerk",          .value = 1,  .key = TR_RANK_CLERK },
    {.type = PARAMETER_TYPE_RANK, .text = "engineer",       .value = 2,  .key = TR_RANK_ENGINEER },
    {.type = PARAMETER_TYPE_RANK, .text = "architect",      .value = 3,  .key = TR_RANK_ARCHITECT },
    {.type = PARAMETER_TYPE_RANK, .text = "quaestor",       .value = 4,  .key = TR_RANK_QUAESTOR },
    {.type = PARAMETER_TYPE_RANK, .text = "procurator",     .value = 5,  .key = TR_RANK_PROCURATOR },
    {.type = PARAMETER_TYPE_RANK, .text = "aedile",         .value = 6,  .key = TR_RANK_AEDILE },
    {.type = PARAMETER_TYPE_RANK, .text = "praetor",        .value = 7,  .key = TR_RANK_PRAETOR },
    {.type = PARAMETER_TYPE_RANK, .text = "consul",         .value = 8,  .key = TR_RANK_CONSUL },
    {.type = PARAMETER_TYPE_RANK, .text = "proconsul",      .value = 9,  .key = TR_RANK_PROCONSUL },
    {.type = PARAMETER_TYPE_RANK, .text = "caesar",         .value = 10, .key = TR_RANK_CAESAR },
    {.type = PARAMETER_TYPE_RANK, .text = "promotion",      .value = 11, .key = TR_RANK_PROMOTE },
    {.type = PARAMETER_TYPE_RANK, .text = "demotion",       .value = 12, .key = TR_RANK_DEMOTE}
};
#define SPECIAL_ATTRIBUTE_MAPPINGS_RANK_SIZE (sizeof(special_attribute_mappings_rank) / sizeof(special_attribute_mapping_t))

static void generate_building_type_mappings(void)
{
    if (special_attribute_mappings_building_type_size > 0) {
        return;
    }
    for (building_type type = BUILDING_NONE; type < BUILDING_TYPE_MAX; type++) {
        const building_properties *props = building_properties_for_type(type);
        if (!props->event_data.attr || props->event_data.cannot_count) {
            continue;
        }
        special_attribute_mapping_t *mapping = &special_attribute_mappings_buildings[special_attribute_mappings_building_type_size];
        mapping->type = PARAMETER_TYPE_BUILDING;
        mapping->text = props->event_data.attr;
        mapping->value = type;
        mapping->key = props->event_data.key ? props->event_data.key : TR_PARAMETER_VALUE_DYNAMIC_RESOLVE;
        special_attribute_mappings_building_type_size++;
    }
}

static void generate_model_mappings(void)
{
    if (special_attribute_mappings_model_buildings_size > 0) {
        return;
    }
    for (building_type type = BUILDING_NONE; type < BUILDING_TYPE_MAX; type++) {
        const building_properties *props = building_properties_for_type(type);
        if ((!props->size || !props->event_data.attr) && type != BUILDING_CLEAR_LAND && type != BUILDING_REPAIR_LAND ||
            (type == BUILDING_GRAND_GARDEN || type == BUILDING_DOLPHIN_FOUNTAIN)) {
            continue;
        }
        special_attribute_mapping_t *mapping = &special_attribute_mappings_model_buildings[special_attribute_mappings_model_buildings_size];
        mapping->type = PARAMETER_TYPE_MODEL;
        mapping->text = props->event_data.attr;
        mapping->value = type;
        mapping->key = props->event_data.key ? props->event_data.key : TR_PARAMETER_VALUE_DYNAMIC_RESOLVE;
        special_attribute_mappings_model_buildings_size++;
    }
}

static void generate_submenu_mappings(build_menu_group menu)
{
    unsigned int menu_items = building_menu_count_all_items(menu);
    for (unsigned int i = 0; i < menu_items; i++) {
        building_type type = building_menu_type(menu, i);
        build_menu_group submenu = building_menu_get_submenu_for_type(type);
        if (submenu) {
            if (submenu == menu) {
                continue;
            }
            generate_submenu_mappings(submenu);
        } else {
            const building_properties *props = building_properties_for_type(type);
            if (!props->event_data.attr) {
                continue;
            }
            special_attribute_mapping_t *mapping = &special_attribute_mappings_allowed_buildings[special_attribute_mappings_allowed_buildings_size];
            mapping->type = PARAMETER_TYPE_ALLOWED_BUILDING;
            mapping->text = props->event_data.attr;
            mapping->value = type;
            mapping->key = props->event_data.key ? props->event_data.key : TR_PARAMETER_VALUE_DYNAMIC_RESOLVE;
            special_attribute_mappings_allowed_buildings_size++;
        }
    }
}

static void generate_allowed_buildings_mappings(void)
{
    if (special_attribute_mappings_allowed_buildings_size > 0) {
        return;
    }
    for (build_menu_group group = 0; group < BUILD_MENU_MAX; group++) {
        // Top level function: main menus only
        if (building_menu_is_submenu(group)) {
            break;
        }
        generate_submenu_mappings(group);
    }
}

special_attribute_mapping_t *scenario_events_parameter_data_get_attribute_mapping(parameter_type type, int index)
{
    switch (type) {
        case PARAMETER_TYPE_BOOLEAN:
            return &special_attribute_mappings_boolean[index];
        case PARAMETER_TYPE_INVASION_TYPE:
            return &special_attribute_mappings_attack_type[index];
        case PARAMETER_TYPE_CHECK:
            return &special_attribute_mappings_check[index];
        case PARAMETER_TYPE_DIFFICULTY:
            return &special_attribute_mappings_difficulty[index];
        case PARAMETER_TYPE_ENEMY_TYPE:
            return &special_attribute_mappings_enemy_type[index];
        case PARAMETER_TYPE_POP_CLASS:
            return &special_attribute_mappings_pop_class[index];
        case PARAMETER_TYPE_BUILDING:
        case PARAMETER_TYPE_BUILDING_COUNTING:
            generate_building_type_mappings();
            return &special_attribute_mappings_buildings[index];
        case PARAMETER_TYPE_ALLOWED_BUILDING:
            generate_allowed_buildings_mappings();
            return &special_attribute_mappings_allowed_buildings[index];
        case PARAMETER_TYPE_STANDARD_MESSAGE:
            return &special_attribute_mappings_standard_message[index];
        case PARAMETER_TYPE_MEDIA_TYPE:
            return &special_attribute_mappings_media_type[index];
        case PARAMETER_TYPE_RATING_TYPE:
            return &special_attribute_mappings_rating_type[index];
        case PARAMETER_TYPE_STORAGE_TYPE:
            return &special_attribute_mappings_storage_type[index];
        case PARAMETER_TYPE_TARGET_TYPE:
            return &special_attribute_mappings_target_type[index];
        case PARAMETER_TYPE_GOD:
            return &special_attribute_mappings_god[index];
        case PARAMETER_TYPE_CLIMATE:
            return &special_attribute_mappings_climate[index];
        case PARAMETER_TYPE_TERRAIN:
            return &special_attribute_mappings_terrain[index];
        case PARAMETER_TYPE_DATA_TYPE:
            return &special_attribute_mappings_data_type[index];
        case PARAMETER_TYPE_HOUSE_DATA_TYPE:
            return &special_attribute_mappings_house_data_type[index];
        case PARAMETER_TYPE_MODEL:
            generate_model_mappings();
            return &special_attribute_mappings_model_buildings[index];
        case PARAMETER_TYPE_HOUSING_TYPE:
            return &special_attribute_mappings_housing[index];
        case PARAMETER_TYPE_AGE_GROUP:
            return &special_attribute_mappings_age[index];
        case PARAMETER_TYPE_CITY_PROPERTY:
            return &special_attribute_mappings_city_property[index];
        case PARAMETER_TYPE_PERCENTAGE:
            return &special_attribute_mappings_percentage[index];
        case PARAMETER_TYPE_ENEMY_CLASS:
            return &special_attribute_mappings_troops_class[index];
        case PARAMETER_TYPE_PLAYER_TROOPS:
            return &special_attribute_mappings_player_troops[index];
        case PARAMETER_TYPE_COVERAGE_BUILDINGS:
            return &special_attribute_mappings_coverage_buildings[index];
        case PARAMETER_TYPE_RANK:
            return &special_attribute_mappings_rank[index];
        default:
            return 0;
    }
}

int scenario_events_parameter_data_get_mappings_size(parameter_type type)
{
    switch (type) {
        case PARAMETER_TYPE_BOOLEAN:
            return SPECIAL_ATTRIBUTE_MAPPINGS_BOOLEAN_SIZE;
        case PARAMETER_TYPE_INVASION_TYPE:
            return SPECIAL_ATTRIBUTE_MAPPINGS_INVASION_TYPE_SIZE;
        case PARAMETER_TYPE_CHECK:
            return SPECIAL_ATTRIBUTE_MAPPINGS_CHECK_SIZE;
        case PARAMETER_TYPE_DIFFICULTY:
            return SPECIAL_ATTRIBUTE_MAPPINGS_CHECK_DIFFICULTY;
        case PARAMETER_TYPE_ENEMY_TYPE:
            return SPECIAL_ATTRIBUTE_MAPPINGS_ENEMY_TYPE_SIZE;
        case PARAMETER_TYPE_POP_CLASS:
            return SPECIAL_ATTRIBUTE_MAPPINGS_POP_CLASS_SIZE;
        case PARAMETER_TYPE_BUILDING:
        case PARAMETER_TYPE_BUILDING_COUNTING:
            generate_building_type_mappings();
            return special_attribute_mappings_building_type_size;
        case PARAMETER_TYPE_ALLOWED_BUILDING:
            generate_allowed_buildings_mappings();
            return special_attribute_mappings_allowed_buildings_size;
        case PARAMETER_TYPE_STANDARD_MESSAGE:
            return SPECIAL_ATTRIBUTE_MAPPINGS_STANDARD_MESSAGE_SIZE;
        case PARAMETER_TYPE_MEDIA_TYPE:
            return SPECIAL_ATTRIBUTE_MAPPINGS_MEDIA_TYPE_SIZE;
        case PARAMETER_TYPE_RATING_TYPE:
            return SPECIAL_ATTRIBUTE_MAPPINGS_RATING_TYPE_SIZE;
        case PARAMETER_TYPE_STORAGE_TYPE:
            return SPECIAL_ATTRIBUTE_MAPPINGS_STORAGE_TYPE_SIZE;
        case PARAMETER_TYPE_TARGET_TYPE:
            return SPECIAL_ATTRIBUTE_MAPPINGS_TARGET_TYPE_SIZE;
        case PARAMETER_TYPE_GOD:
            return SPECIAL_ATTRIBUTE_MAPPINGS_GOD_SIZE;
        case PARAMETER_TYPE_CLIMATE:
            return SPECIAL_ATTRIBUTE_MAPPINGS_CLIMATE_SIZE;
        case PARAMETER_TYPE_TERRAIN:
            return SPECIAL_ATTRIBUTE_MAPPINGS_TERRAIN_SIZE;
        case PARAMETER_TYPE_DATA_TYPE:
            return SPECIAL_ATTRIBUTE_MAPPINGS_DATA_TYPE_SIZE;
        case PARAMETER_TYPE_HOUSE_DATA_TYPE:
            return SPECIAL_ATTRIBUTE_MAPPINGS_HOUSE_DATA_TYPE_SIZE;
        case PARAMETER_TYPE_MODEL:
            generate_model_mappings();
            return special_attribute_mappings_model_buildings_size;
        case PARAMETER_TYPE_HOUSING_TYPE:
            return SPECIAL_ATTRIBUTE_MAPPINGS_HOUSING_SIZE;
        case PARAMETER_TYPE_AGE_GROUP:
            return SPECIAL_ATTRIBUTE_MAPPINGS_AGE_SIZE;
        case PARAMETER_TYPE_CITY_PROPERTY:
            return SPECIAL_ATTRIBUTE_MAPPINGS_CITY_PROPERTY_SIZE;
        case PARAMETER_TYPE_PERCENTAGE:
            return SPECIAL_ATTRIBUTE_MAPPINGS_PERCENTAGE_SIZE;
        case PARAMETER_TYPE_ENEMY_CLASS:
            return SPECIAL_ATTRIBUTE_MAPPINGS_TROOPS_CLASS_SIZE;
        case PARAMETER_TYPE_PLAYER_TROOPS:
            return SPECIAL_ATTRIBUTE_MAPPINGS_PLAYER_TROOPS_SIZE;
        case PARAMETER_TYPE_COVERAGE_BUILDINGS:
            return SPECIAL_ATTRIBUTE_MAPPINGS_COVERAGE_BUILDINGS_SIZE;
        case PARAMETER_TYPE_RANK:
            return SPECIAL_ATTRIBUTE_MAPPINGS_RANK_SIZE;
        default:
            return 0;
    }
}

special_attribute_mapping_t *scenario_events_parameter_data_get_attribute_mapping_by_value(parameter_type type, int target)
{
    int array_size = scenario_events_parameter_data_get_mappings_size(type);
    for (int i = 0; i < array_size; i++) {
        special_attribute_mapping_t *current = scenario_events_parameter_data_get_attribute_mapping(type, i);
        if (target == current->value) {
            return current;
        }
    }
    return 0;
}

special_attribute_mapping_t *scenario_events_parameter_data_get_attribute_mapping_by_text(parameter_type type, const char *value)
{
    if (!value) {
        return 0;
    }

    int array_size = scenario_events_parameter_data_get_mappings_size(type);
    for (int i = 0; i < array_size; i++) {
        special_attribute_mapping_t *current = scenario_events_parameter_data_get_attribute_mapping(type, i);
        if (xml_parser_compare_multiple(current->text, value)) {
            return current;
        }
    }
    return 0;
}

int scenario_events_parameter_data_get_default_value_for_parameter(xml_data_attribute_t *attribute_data)
{
    switch (attribute_data->type) {
        case PARAMETER_TYPE_NUMBER:
        case PARAMETER_TYPE_GRID_SLICE:
            if (attribute_data->min_limit > 0) {
                return attribute_data->min_limit;
            } else {
                if (attribute_data->max_limit < 0) {
                    return attribute_data->max_limit;
                }
                return 0;
            }
        case PARAMETER_TYPE_INVASION_TYPE:
            return INVASION_TYPE_ENEMY_ARMY;
        case PARAMETER_TYPE_CHECK:
            return COMPARISON_TYPE_EQUAL_OR_MORE;
        case PARAMETER_TYPE_DIFFICULTY:
            return DIFFICULTY_NORMAL;
        case PARAMETER_TYPE_ENEMY_TYPE:
            return ENEMY_UNDEFINED;
        case PARAMETER_TYPE_RESOURCE:
            return RESOURCE_WHEAT;
        case PARAMETER_TYPE_POP_CLASS:
            return POP_CLASS_ALL;
        case PARAMETER_TYPE_BUILDING:
        case PARAMETER_TYPE_ALLOWED_BUILDING:
        case PARAMETER_TYPE_BUILDING_COUNTING:
        case PARAMETER_TYPE_MODEL:
            return BUILDING_WELL;
        case PARAMETER_TYPE_STANDARD_MESSAGE:
            return MESSAGE_CAESAR_WRATH;
        case PARAMETER_TYPE_RATING_TYPE:
            return SELECTED_RATING_PEACE;
        case PARAMETER_TYPE_STORAGE_TYPE:
            return STORAGE_TYPE_ALL;
        case PARAMETER_TYPE_TARGET_TYPE:
            return FORMATION_ATTACK_BEST_BUILDINGS;
        case PARAMETER_TYPE_GOD:
            return GOD_CERES;
        case PARAMETER_TYPE_CLIMATE:
            return CLIMATE_CENTRAL;
        case PARAMETER_TYPE_TERRAIN:
            return TERRAIN_WATER;
        case PARAMETER_TYPE_DATA_TYPE:
            return MODEL_COST;
        case PARAMETER_TYPE_HOUSE_DATA_TYPE:
            return MODEL_EVOLVE_DESIRABILITY;
        case PARAMETER_TYPE_HOUSING_TYPE:
            return BUILDING_HOUSE_SMALL_TENT;
        case PARAMETER_TYPE_CITY_PROPERTY:
            return CITY_PROPERTY_DIFFICULTY;
        case PARAMETER_TYPE_ENEMY_CLASS:
            return ENEMY_CLASS_ALL;
        case PARAMETER_TYPE_PLAYER_TROOPS:
            return FIGURE_FORT_STANDARD;
        case PARAMETER_TYPE_COVERAGE_BUILDINGS:
            return BUILDING_THEATER;
        case PARAMETER_TYPE_ROUTE_RESOURCE:
            // Encode default route_id=1 with RESOURCE_ALL_BUYS (RESOURCE_MAX + 1)
            return window_editor_select_city_trade_route_encode_route_resource(1, RESOURCE_MAX + 1);
        case PARAMETER_TYPE_ROUTE:
            return 1; // there should be at least one route
        default:
            return 0;
    }
}

parameter_type scenario_events_parameter_data_resolve_flexible_type(const scenario_action_t *action, int param_number)
{
    // Only ACTION_TYPE_CUSTOM_VARIABLE_CITY_PROPERTY uses flexible parameters
    if (action->type != ACTION_TYPE_CUSTOM_VARIABLE_CITY_PROPERTY || param_number < 3 || param_number > 5) {
        return PARAMETER_TYPE_UNDEFINED;
    }

    // Get the city property from parameter2
    city_property_t city_property = action->parameter2;
    city_property_info_t info = city_property_get_param_info(city_property);

    // Map param_number to the array index (3->0, 4->1, 5->2)
    int param_index = param_number - 3;
    if (param_index >= info.count) {    // Check if this parameter is needed for this city property
        return PARAMETER_TYPE_UNDEFINED;
    }
    return info.param_types[param_index];
}

static const uint8_t *get_allowed_building_name(building_type type)
{
    if (type == BUILDING_HOUSE_VACANT_LOT) {
        return lang_get_string(68, 20);
    }
    if (type == BUILDING_CLEAR_LAND) {
        return lang_get_string(CUSTOM_TRANSLATION, TR_BUILDING_LAND_CLEAR);
    }
    if (type == BUILDING_REPAIR_LAND) {
        return lang_get_string(CUSTOM_TRANSLATION, TR_BUILDING_LAND_REPAIR);
    }
    return lang_get_building_type_string(type);
}

const uint8_t *scenario_events_parameter_data_get_display_string(special_attribute_mapping_t *entry)
{
    switch (entry->type) {
        case PARAMETER_TYPE_BUILDING:
        case PARAMETER_TYPE_BUILDING_COUNTING:
        case PARAMETER_TYPE_MODEL:
            if (entry->key == TR_PARAMETER_VALUE_DYNAMIC_RESOLVE) {
                return lang_get_building_type_string(entry->value);
            } else {
                return translation_for(entry->key);
            }
            break;
        case PARAMETER_TYPE_ALLOWED_BUILDING:
            if (entry->key == TR_PARAMETER_VALUE_DYNAMIC_RESOLVE) {
                return get_allowed_building_name(entry->value);
            } else {
                return translation_for(entry->key);
            }
            break;
        case PARAMETER_TYPE_GRID_SLICE:
        {
            static uint8_t buffer[16];
            string_from_int(buffer, entry->value, 0);
            return (const uint8_t *) buffer;
        }
        default:
            return translation_for(entry->key);
    }
}

static uint8_t *string_from_year(uint8_t *dst, int year, int *maxlength)
{
    uint8_t *cursor = dst;
    if (year >= 0) {
        int use_year_ad = locale_year_before_ad();
        if (use_year_ad) {
            cursor += string_from_int(cursor, year, 0);
            *cursor = ' ';
            cursor++;
            cursor = string_copy(lang_get_string(20, 1), cursor, *maxlength - 10);
        } else {
            cursor = string_copy(lang_get_string(20, 1), cursor, *maxlength - 10);
            *cursor = ' ';
            cursor++;
            cursor += string_from_int(cursor, year, 0);
        }
    } else {
        cursor += string_from_int(cursor, -year, 0);
        *cursor = ' ';
        cursor++;
        cursor = string_copy(lang_get_string(20, 0), cursor, *maxlength - 10);
    }
    int total_chars = (int) (cursor - dst);
    *maxlength -= total_chars;
    return cursor;
}

static uint8_t *translation_for_request_value(int value, uint8_t *result_text, int *maxlength)
{
    if (value < 0 || value >= (int) scenario_request_count_total()) {
        return string_copy(translation_for(TR_PARAMETER_VALUE_NONE), result_text, *maxlength);
    }
    const scenario_request *request = scenario_request_get(value);
    if (!request || request->resource == RESOURCE_NONE) {
        return string_copy(translation_for(TR_PARAMETER_VALUE_NONE), result_text, *maxlength);
    }
    uint8_t *cursor = result_text;
    cursor = string_from_year(cursor, scenario_property_start_year() + request->year, maxlength);
    cursor = string_copy(string_from_ascii(", "), cursor, *maxlength);
    *maxlength -= 2;
    int numbers = string_from_int(cursor, request->amount.min, 0);
    *maxlength -= numbers;
    cursor += numbers;
    if (request->amount.min < request->amount.max) {
        cursor = string_copy(string_from_ascii("-"), cursor, *maxlength);
        numbers = string_from_int(cursor, request->amount.max, 0);
        *maxlength -= numbers;
        cursor += numbers;
    }
    cursor = string_copy(string_from_ascii(" "), cursor, *maxlength);
    *maxlength -= 1;
    cursor = string_copy(resource_get_data(request->resource)->text, cursor, *maxlength);

    return cursor;
}

void scenario_events_parameter_data_get_display_string_for_value(parameter_type type, int value,
    uint8_t *result_text, int maxlength)
{
    switch (type) {
        case PARAMETER_TYPE_NUMBER:
        case PARAMETER_TYPE_MIN_MAX_NUMBER:
        case PARAMETER_TYPE_GRID_SLICE:
            string_from_int(result_text, value, 0);
            return;
        case PARAMETER_TYPE_CUSTOM_VARIABLE:
        {
            if (scenario_custom_variable_exists(value)) {
                const uint8_t *text = scenario_custom_variable_get_name(value);
                if (text) {
                    result_text = string_copy(text, result_text, maxlength);
                }
            }
            return;
        }
        case PARAMETER_TYPE_REQUEST:
        {
            translation_for_request_value(value, result_text, &maxlength);
            return;
        }
        case PARAMETER_TYPE_CUSTOM_MESSAGE:
        {
            custom_message_t *message = custom_messages_get(value);
            if (message) {
                if (message->linked_uid) {
                    const uint8_t *text = message->linked_uid->text;
                    result_text = string_copy(text, result_text, maxlength);
                }
            }
            return;
        }
        case PARAMETER_TYPE_ROUTE:
        {
            int city_id = empire_city_get_for_trade_route(value);
            if (city_id) {
                empire_city *city = empire_city_get(city_id);
                const uint8_t *text = empire_city_get_name(city);
                result_text = string_copy(text, result_text, maxlength);
            }
            return;
        }
        case PARAMETER_TYPE_FUTURE_CITY:
        {
            empire_city *city = empire_city_get(value);
            if (city) {
                const uint8_t *text = empire_city_get_name(city);
                result_text = string_copy(text, result_text, maxlength);
            }
            return;
        }
        case PARAMETER_TYPE_RESOURCE:
        {
            const uint8_t *text = resource_get_data(value)->text;
            result_text = string_copy(text, result_text, maxlength);
            return;
        }
        case PARAMETER_TYPE_FORMULA:
        {
            const uint8_t *formula_string = scenario_formula_get_string(value);
            if (formula_string) {
                result_text = string_copy(formula_string, result_text, maxlength);
            }
            return;
        }
        case PARAMETER_TYPE_ROUTE_RESOURCE:
        {
            if (RESOURCE_MIN < value && value < RESOURCE_MAX) {
                result_text = string_copy(translation_for(TR_PARAMETER_VALUE_NONE), result_text, maxlength);
                return;
            }
            const uint8_t *text = window_editor_select_city_trade_route_show_get_selected_name(value);
            result_text = string_copy(text, result_text, maxlength);
            return;
        }
        default:
        {
            special_attribute_mapping_t *attribute = scenario_events_parameter_data_get_attribute_mapping_by_value(type, value);
            if (attribute) {
                const uint8_t *text = scenario_events_parameter_data_get_display_string(attribute);
                result_text = string_copy(text, result_text, maxlength);
            }
            return;
        }
    }
}

static uint8_t *append_text(const uint8_t *text_to_append, uint8_t *result_text, int *maxlength)
{
    int text_length = string_length(text_to_append);
    result_text = string_copy(text_to_append, result_text, *maxlength);
    *maxlength -= text_length;
    return result_text;
}

static uint8_t *translation_for_set_or_add_text(int parameter, uint8_t *result_text, int *maxlength)
{
    result_text = append_text(string_from_ascii(" "), result_text, maxlength);
    if (parameter) {
        result_text = append_text(translation_for(TR_PARAMETER_DISPLAY_SET_TO), result_text, maxlength);
    } else {
        result_text = append_text(translation_for(TR_PARAMETER_DISPLAY_ADD_TO), result_text, maxlength);
    }
    return result_text;
}

static uint8_t *translation_for_min_max_values(int min, int max, uint8_t *result_text, int *maxlength)
{
    result_text = append_text(string_from_ascii(" "), result_text, maxlength);
    result_text = append_text(translation_for(TR_PARAMETER_DISPLAY_BETWEEN), result_text, maxlength);
    result_text = append_text(string_from_ascii(" "), result_text, maxlength);

    int number_length = string_from_int(result_text, min, 0);
    result_text += number_length;
    *maxlength -= number_length;

    result_text = append_text(string_from_ascii(".."), result_text, maxlength);

    number_length = string_from_int(result_text, max, 0);
    result_text += number_length;
    *maxlength -= number_length;

    return result_text;
}

static uint8_t *translation_for_number_value(int value, uint8_t *result_text, int *maxlength)
{
    result_text = append_text(string_from_ascii(" "), result_text, maxlength);

    int number_length = string_from_int(result_text, value, 0);
    result_text += number_length;
    *maxlength -= number_length;

    return result_text;
}

static uint8_t *translation_for_boolean_text(int value, translation_key true_key, translation_key false_key, uint8_t *result_text, int *maxlength)
{
    result_text = append_text(string_from_ascii(" "), result_text, maxlength);
    if (value) {
        result_text = append_text(translation_for(true_key), result_text, maxlength);
    } else {
        result_text = append_text(translation_for(false_key), result_text, maxlength);
    }

    return result_text;
}

static uint8_t *translation_for_formula_index(int index, uint8_t *result_text, int *maxlength)
{
    result_text = append_text(string_from_ascii(" "), result_text, maxlength);

    const uint8_t *text = scenario_formula_get_string(index);
    if (text) {
        result_text = append_text(text, result_text, maxlength);
    } else {
        result_text = append_text(string_from_ascii("???"), result_text, maxlength);
    }

    return result_text;
}

static uint8_t *translation_for_grid_offset(int value, uint8_t *result_text, int *maxlength)
{
    result_text = append_text(string_from_ascii(" "), result_text, maxlength);

    int number_length = string_from_int(result_text, value, 0);
    result_text += number_length;
    *maxlength -= number_length;

    return result_text;
}

static uint8_t *translation_for_attr_mapping_text(parameter_type type, int value, uint8_t *result_text, int *maxlength)
{
    result_text = append_text(string_from_ascii(" "), result_text, maxlength);
    special_attribute_mapping_t *attr_mapping = scenario_events_parameter_data_get_attribute_mapping_by_value(type, value);

    result_text = append_text(translation_for(attr_mapping->key), result_text, maxlength);
    return result_text;
}

static uint8_t *translation_for_type_lookup_by_value(parameter_type type, int value, uint8_t *result_text, int *maxlength)
{
    result_text = append_text(string_from_ascii(" "), result_text, maxlength);

    uint8_t text[50];
    memset(text, 0, 50);
    scenario_events_parameter_data_get_display_string_for_value(type, value, text, 50);
    result_text = append_text(text, result_text, maxlength);

    return result_text;
}

void scenario_events_parameter_data_get_display_string_for_action(const scenario_action_t *action, uint8_t *result_text,
    int maxlength)
{
    scenario_action_data_t *xml_info = scenario_events_parameter_data_get_actions_xml_attributes(action->type);
    result_text = append_text(translation_for(xml_info->xml_attr.key), result_text, &maxlength);
    switch (action->type) {
        case ACTION_TYPE_ADJUST_CITY_HEALTH:
        case ACTION_TYPE_ADJUST_ROME_WAGES:
        {
            result_text = translation_for_set_or_add_text(action->parameter2, result_text, &maxlength);
            result_text = translation_for_formula_index(action->parameter1, result_text, &maxlength);
            return;
        }
        case ACTION_TYPE_ADJUST_FAVOR:
        {
            result_text = translation_for_formula_index(action->parameter1, result_text, &maxlength);
            return;
        }
        case ACTION_TYPE_ADJUST_MONEY:
        case ACTION_TYPE_ADJUST_SAVINGS:
        {
            result_text = translation_for_formula_index(action->parameter1, result_text, &maxlength);
            return;
        }
        case ACTION_TYPE_BUILDING_FORCE_COLLAPSE:
        {
            result_text = append_text(string_from_ascii(" "), result_text, &maxlength);
            result_text = append_text(translation_for(TR_PARAMETER_GRID_OFFSET_CORNER1), result_text, &maxlength);
            result_text = translation_for_grid_offset(action->parameter1, result_text, &maxlength);
            result_text = append_text(string_from_ascii(" "), result_text, &maxlength);
            result_text = append_text(translation_for(TR_PARAMETER_GRID_OFFSET_CORNER2), result_text, &maxlength);
            result_text = translation_for_grid_offset(action->parameter2, result_text, &maxlength);
            if (action->parameter4) {
                result_text = append_text(string_from_ascii(" "), result_text, &maxlength);
                result_text = append_text(translation_for(TR_PARAMETER_DISPLAY_DESTROY_ALL_TYPES), result_text, &maxlength);
            } else {
                result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_BUILDING, action->parameter3, result_text, &maxlength);
            }
            return;
        }
        case ACTION_TYPE_CHANGE_ALLOWED_BUILDINGS:
        {
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_ALLOWED_BUILDING, action->parameter1, result_text, &maxlength);
            result_text = translation_for_boolean_text(action->parameter2, TR_PARAMETER_DISPLAY_ALLOWED, TR_PARAMETER_DISPLAY_DISALLOWED, result_text, &maxlength);
            return;
        }
        case ACTION_TYPE_CHANGE_CITY_RATING:
        {
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_RATING_TYPE, action->parameter1, result_text, &maxlength);
            result_text = translation_for_set_or_add_text(action->parameter3, result_text, &maxlength);
            result_text = translation_for_formula_index(action->parameter2, result_text, &maxlength);
            return;
        }
        case ACTION_TYPE_CHANGE_CUSTOM_VARIABLE:
        {
            result_text = append_text(string_from_ascii(" "), result_text, &maxlength);

            if (scenario_custom_variable_exists(action->parameter1) &&
                scenario_custom_variable_get_name(action->parameter1)) {
                result_text = append_text(scenario_custom_variable_get_name(action->parameter1),
                    result_text, &maxlength);
            } else {
                result_text = append_text(string_from_ascii("???"), result_text, &maxlength);
            }

            result_text = translation_for_set_or_add_text(action->parameter3, result_text, &maxlength);
            result_text = translation_for_formula_index(action->parameter2, result_text, &maxlength);
            return;
        }
        case ACTION_TYPE_CHANGE_CUSTOM_VARIABLE_VISIBILITY:
        {
            result_text = append_text(string_from_ascii(" "), result_text, &maxlength);

            if (scenario_custom_variable_exists(action->parameter1) &&
                scenario_custom_variable_get_name(action->parameter1)) {
                result_text = append_text(scenario_custom_variable_get_name(action->parameter1),
                    result_text, &maxlength);
            } else {
                result_text = append_text(string_from_ascii("???"), result_text, &maxlength);
            }

            result_text = translation_for_boolean_text(action->parameter2, TR_PARAMETER_VALUE_BOOLEAN_TRUE, TR_PARAMETER_VALUE_BOOLEAN_FALSE, result_text, &maxlength);
            return;
        }
        case ACTION_TYPE_CHANGE_RESOURCE_PRODUCED:
        {
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_RESOURCE, action->parameter1, result_text, &maxlength);
            result_text = translation_for_boolean_text(action->parameter2, TR_PARAMETER_DISPLAY_ALLOWED, TR_PARAMETER_DISPLAY_DISALLOWED, result_text, &maxlength);
            return;
        }
        case ACTION_TYPE_CHANGE_RESOURCE_STOCKPILES:
        {
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_STORAGE_TYPE, action->parameter3, result_text, &maxlength);
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_RESOURCE, action->parameter1, result_text, &maxlength);
            result_text = translation_for_formula_index(action->parameter2, result_text, &maxlength);
            result_text = translation_for_boolean_text(action->parameter4, TR_PARAMETER_DISPLAY_RESPECT_SETTINGS, TR_PARAMETER_DISPLAY_IGNORE_SETTINGS, result_text, &maxlength);
            return;
        }
        case ACTION_TYPE_EMPIRE_MAP_CONVERT_FUTURE_TRADE_CITY:
        {
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_FUTURE_CITY, action->parameter1, result_text, &maxlength);
            result_text = translation_for_boolean_text(action->parameter2, TR_PARAMETER_DISPLAY_SHOW_MESSAGE, TR_PARAMETER_DISPLAY_DO_NOT_SHOW_MESSAGE, result_text, &maxlength);
            return;
        }
        case ACTION_TYPE_GLADIATOR_REVOLT:
        {
            return;
        }
        case ACTION_TYPE_INVASION_IMMEDIATE:
        {
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_INVASION_TYPE, action->parameter1, result_text, &maxlength);
            result_text = append_text(string_from_ascii(" "), result_text, &maxlength);
            result_text = append_text(translation_for(TR_PARAMETER_TYPE_INVASION_SIZE), result_text, &maxlength);
            result_text = translation_for_formula_index(action->parameter2, result_text, &maxlength);
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_ENEMY_TYPE, action->parameter5, result_text, &maxlength);
            result_text = append_text(string_from_ascii(" "), result_text, &maxlength);
            result_text = append_text(translation_for(TR_PARAMETER_TYPE_INVASION_POINT), result_text, &maxlength);
            result_text = translation_for_formula_index(action->parameter3, result_text, &maxlength);
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_TARGET_TYPE, action->parameter4, result_text, &maxlength);
            return;
        }
        case ACTION_TYPE_REQUEST_IMMEDIATELY_START:
        case ACTION_TYPE_TAX_RATE_SET:
        {
            result_text = translation_for_formula_index(action->parameter1, result_text, &maxlength);
            return;
        }
        case ACTION_TYPE_TRADE_PROBLEM_LAND:
        case ACTION_TYPE_TRADE_PROBLEM_SEA:
        {
            result_text = translation_for_formula_index(action->parameter1, result_text, &maxlength);
            result_text = append_text(string_from_ascii(" "), result_text, &maxlength);
            result_text = append_text(translation_for(TR_PARAMETER_DISPLAY_DAYS), result_text, &maxlength);
            return;
        }
        case ACTION_TYPE_SEND_STANDARD_MESSAGE:
        {
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_STANDARD_MESSAGE, action->parameter1, result_text, &maxlength);
            return;
        }
        case ACTION_TYPE_TRADE_ADJUST_PRICE:
        {
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_RESOURCE, action->parameter1, result_text, &maxlength);
            result_text = translation_for_formula_index(action->parameter2, result_text, &maxlength);
            result_text = translation_for_boolean_text(action->parameter3, TR_PARAMETER_DISPLAY_SHOW_MESSAGE, TR_PARAMETER_DISPLAY_DO_NOT_SHOW_MESSAGE, result_text, &maxlength);
            return;
        }
        case ACTION_TYPE_TRADE_ADJUST_ROUTE_AMOUNT:
        {
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_ROUTE, action->parameter1, result_text, &maxlength);
            result_text = translation_for_formula_index(action->parameter3, result_text, &maxlength);
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_RESOURCE, action->parameter2, result_text, &maxlength);
            result_text = translation_for_boolean_text(action->parameter4, TR_PARAMETER_DISPLAY_SHOW_MESSAGE, TR_PARAMETER_DISPLAY_DO_NOT_SHOW_MESSAGE, result_text, &maxlength);
            return;
        }
        case ACTION_TYPE_TRADE_ROUTE_ADD_NEW_RESOURCE:
        {
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_ROUTE, action->parameter1, result_text, &maxlength);
            result_text = translation_for_boolean_text(action->parameter4, TR_PARAMETER_DISPLAY_ADD_AS_BUYING, TR_PARAMETER_DISPLAY_ADD_AS_SELLING, result_text, &maxlength);
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_RESOURCE, action->parameter2, result_text, &maxlength);
            result_text = translation_for_formula_index(action->parameter3, result_text, &maxlength);
            result_text = translation_for_boolean_text(action->parameter5, TR_PARAMETER_DISPLAY_SHOW_MESSAGE, TR_PARAMETER_DISPLAY_DO_NOT_SHOW_MESSAGE, result_text, &maxlength);
            return;
        }
        case ACTION_TYPE_TRADE_ADJUST_ROUTE_OPEN_PRICE:
        {
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_ROUTE, action->parameter1, result_text, &maxlength);
            result_text = translation_for_set_or_add_text(action->parameter3, result_text, &maxlength);
            result_text = translation_for_formula_index(action->parameter2, result_text, &maxlength);
            result_text = translation_for_boolean_text(action->parameter4, TR_PARAMETER_DISPLAY_SHOW_MESSAGE, TR_PARAMETER_DISPLAY_DO_NOT_SHOW_MESSAGE, result_text, &maxlength);
            return;
        }
        case ACTION_TYPE_TRADE_ROUTE_SET_OPEN:
        {
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_ROUTE, action->parameter1, result_text, &maxlength);
            result_text = translation_for_boolean_text(action->parameter2, TR_PARAMETER_DISPLAY_APPLY_COST, TR_PARAMETER_DISPLAY_NO_COST, result_text, &maxlength);
            return;
        }
        case ACTION_TYPE_TRADE_SET_PRICE:
        {
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_RESOURCE, action->parameter1, result_text, &maxlength);
            result_text = translation_for_boolean_text(action->parameter3, TR_PARAMETER_DISPLAY_BUY_PRICE, TR_PARAMETER_DISPLAY_SELL_PRICE, result_text, &maxlength);
            result_text = translation_for_formula_index(action->parameter2, result_text, &maxlength);
            result_text = translation_for_boolean_text(action->parameter4, TR_PARAMETER_DISPLAY_SHOW_MESSAGE, TR_PARAMETER_DISPLAY_DO_NOT_SHOW_MESSAGE, result_text, &maxlength);
            return;
        }
        case ACTION_TYPE_TRADE_SET_BUY_PRICE_ONLY:
        case ACTION_TYPE_TRADE_SET_SELL_PRICE_ONLY:
        {
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_RESOURCE, action->parameter1, result_text, &maxlength);
            result_text = translation_for_formula_index(action->parameter2, result_text, &maxlength);
            return;
        }
        case ACTION_TYPE_SHOW_CUSTOM_MESSAGE:
        {
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_CUSTOM_MESSAGE, action->parameter1, result_text, &maxlength);
            return;
        }
        case ACTION_TYPE_CAUSE_BLESSING:
        case ACTION_TYPE_CAUSE_MINOR_CURSE:
        case ACTION_TYPE_CAUSE_MAJOR_CURSE:
        {
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_GOD, action->parameter1, result_text, &maxlength);
            return;
        }
        case ACTION_TYPE_CHANGE_CLIMATE:
        {
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_CLIMATE, action->parameter1, result_text, &maxlength);
            return;
        }
        case ACTION_TYPE_CHANGE_TERRAIN:
        {
            result_text = append_text(string_from_ascii(" "), result_text, &maxlength);
            result_text = append_text(translation_for(TR_PARAMETER_GRID_OFFSET_CORNER1), result_text, &maxlength);
            result_text = translation_for_grid_offset(action->parameter1, result_text, &maxlength);
            result_text = append_text(string_from_ascii(" "), result_text, &maxlength);
            result_text = append_text(translation_for(TR_PARAMETER_GRID_OFFSET_CORNER2), result_text, &maxlength);
            result_text = translation_for_grid_offset(action->parameter2, result_text, &maxlength);
            if (action->parameter4) {
                result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_TERRAIN, action->parameter3, result_text, &maxlength);
                result_text = append_text(string_from_ascii(" "), result_text, &maxlength);
                result_text = append_text(translation_for(TR_PARAMETER_ADD), result_text, &maxlength);
            } else {
                result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_TERRAIN, action->parameter3, result_text, &maxlength);
                result_text = append_text(string_from_ascii(" "), result_text, &maxlength);
                result_text = append_text(translation_for(TR_EDITOR_DELETE), result_text, &maxlength);
            }
            return;
        }
        case ACTION_TYPE_CHANGE_MODEL_DATA:
        {
            result_text = append_text(string_from_ascii(": "), result_text, &maxlength);
            result_text = append_text(translation_for(action->parameter4 ? TR_PARAMETER_SET : TR_PARAMETER_CHANGE), result_text, &maxlength);
            result_text = append_text(string_from_ascii(" "), result_text, &maxlength);
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_DATA_TYPE, action->parameter2, result_text, &maxlength);
            result_text = append_text(string_from_ascii(" "), result_text, &maxlength);
            result_text = append_text(translation_for(TR_PARAMETER_OF), result_text, &maxlength);
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_MODEL, action->parameter1, result_text, &maxlength);
            result_text = append_text(string_from_ascii(" "), result_text, &maxlength);
            result_text = append_text(translation_for(action->parameter4 ? TR_PARAMETER_TO : TR_PARAMETER_BY), result_text, &maxlength);
            result_text = translation_for_formula_index(action->parameter3, result_text, &maxlength);
            return;
        }
        case ACTION_TYPE_CUSTOM_VARIABLE_FORMULA:
        {
            result_text = append_text(string_from_ascii(" "), result_text, &maxlength);
            if (scenario_custom_variable_exists(action->parameter1) &&
                scenario_custom_variable_get_name(action->parameter1)) {
                result_text = append_text(scenario_custom_variable_get_name(action->parameter1),
                    result_text, &maxlength);
            } else {
                result_text = append_text(string_from_ascii("???"), result_text, &maxlength);
            }
            result_text = append_text(string_from_ascii(" = "), result_text, &maxlength);
            // Get the formula string from the formula array
            if (action->parameter2 > 0) {
                const uint8_t *formula_str = scenario_formula_get_string(action->parameter2);
                if (formula_str) {
                    result_text = append_text(formula_str, result_text, &maxlength);
                } else {
                    result_text = append_text(string_from_ascii("???"), result_text, &maxlength);
                }
            } else {
                result_text = append_text(string_from_ascii("0"), result_text, &maxlength);
            }
            return;
        }
        case ACTION_TYPE_CUSTOM_VARIABLE_CITY_PROPERTY:
        {
            result_text = append_text(string_from_ascii(" "), result_text, &maxlength);
            if (scenario_custom_variable_exists(action->parameter1) &&
                scenario_custom_variable_get_name(action->parameter1)) {
                result_text = append_text(scenario_custom_variable_get_name(action->parameter1),
                    result_text, &maxlength);
            } else {
                result_text = append_text(string_from_ascii("???"), result_text, &maxlength);
            }
            result_text = append_text(string_from_ascii(" = "), result_text, &maxlength);
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_CITY_PROPERTY, action->parameter2, result_text, &maxlength);
            return;
        }
        case ACTION_TYPE_GOD_SENTIMENT_CHANGE:
        {
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_GOD, action->parameter1, result_text, &maxlength);
            result_text = translation_for_set_or_add_text(action->parameter3, result_text, &maxlength);
            // Get the formula string if parameter2 is a formula index
            if (action->parameter2 > 0) {
                const uint8_t *formula_str = scenario_formula_get_string(action->parameter2);
                if (formula_str) {
                    result_text = append_text(formula_str, result_text, &maxlength);
                } else {
                    result_text = translation_for_formula_index(action->parameter2, result_text, &maxlength);
                }
            } else {
                result_text = translation_for_formula_index(action->parameter2, result_text, &maxlength);
            }
            return;
        }
        case ACTION_TYPE_POP_SENTIMENT_CHANGE:
        {
            result_text = translation_for_set_or_add_text(action->parameter2, result_text, &maxlength);
            // Get the formula string if parameter1 is a formula index
            if (action->parameter1 > 0) {
                const uint8_t *formula_str = scenario_formula_get_string(action->parameter1);
                if (formula_str) {
                    result_text = append_text(formula_str, result_text, &maxlength);
                } else {
                    result_text = translation_for_formula_index(action->parameter1, result_text, &maxlength);
                }
            } else {
                result_text = translation_for_formula_index(action->parameter1, result_text, &maxlength);
            }
            return;
        }
        case ACTION_TYPE_WIN:
        case ACTION_TYPE_LOSE:
        {
            // No parameters to display
            return;
        }
        case ACTION_TYPE_CHANGE_RANK:
        {
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_RANK, action->parameter1, result_text, &maxlength);
            return;
        }
        case ACTION_TYPE_CHANGE_PRODUCTION_RATE:
        {
            result_text = append_text(string_from_ascii(" "), result_text, &maxlength);
            result_text = append_text(translation_for(TR_PARAMETER_OF), result_text, &maxlength);
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_RESOURCE, action->parameter1, result_text, &maxlength);
            result_text = append_text(string_from_ascii(" "), result_text, &maxlength);
            result_text = append_text(translation_for(action->parameter3 ? TR_PARAMETER_BY : TR_PARAMETER_TO), result_text, &maxlength);
            result_text = append_text(string_from_ascii(" "), result_text, &maxlength);
            result_text = translation_for_formula_index(action->parameter2, result_text, &maxlength);
            return;
        }
        case ACTION_TYPE_CHANGE_HOUSE_MODEL_DATA:
        {
            result_text = append_text(string_from_ascii(": "), result_text, &maxlength);
            result_text = append_text(translation_for(action->parameter4 ? TR_PARAMETER_SET : TR_PARAMETER_CHANGE), result_text, &maxlength);
            result_text = append_text(string_from_ascii(" "), result_text, &maxlength);
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_HOUSE_DATA_TYPE, action->parameter2, result_text, &maxlength);
            result_text = append_text(string_from_ascii(" "), result_text, &maxlength);
            result_text = append_text(translation_for(TR_PARAMETER_OF), result_text, &maxlength);
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_MODEL, action->parameter1, result_text, &maxlength);
            result_text = append_text(string_from_ascii(" "), result_text, &maxlength);
            result_text = append_text(translation_for(action->parameter4 ? TR_PARAMETER_TO : TR_PARAMETER_BY), result_text, &maxlength);
            result_text = translation_for_formula_index(action->parameter3, result_text, &maxlength);
            return;
        }
        default:
        {
            result_text = append_text(string_from_ascii(" UNHANDLED ACTION TYPE!"), result_text, &maxlength);
            return;
        }
    }
}
void scenario_events_parameter_data_get_display_string_for_condition(const scenario_condition_t *condition,
    uint8_t *result_text, int maxlength)
{
    scenario_condition_data_t *xml_info = scenario_events_parameter_data_get_conditions_xml_attributes(condition->type);
    result_text = append_text(translation_for(xml_info->xml_attr.key), result_text, &maxlength);

    switch (condition->type) {
        case CONDITION_TYPE_BUILDING_COUNT_ACTIVE:
        case CONDITION_TYPE_BUILDING_COUNT_ANY:
        {
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_BUILDING_COUNTING, condition->parameter3, result_text, &maxlength);
            result_text = translation_for_attr_mapping_text(xml_info->xml_parm1.type, condition->parameter1, result_text, &maxlength);
            result_text = translation_for_formula_index(condition->parameter2, result_text, &maxlength);
            return;
        }
        case CONDITION_TYPE_CITY_POPULATION:
        {
            result_text = translation_for_attr_mapping_text(xml_info->xml_parm3.type, condition->parameter3, result_text, &maxlength);
            result_text = translation_for_attr_mapping_text(xml_info->xml_parm1.type, condition->parameter1, result_text, &maxlength);
            result_text = translation_for_formula_index(condition->parameter2, result_text, &maxlength);
            return;
        }
        case CONDITION_TYPE_COUNT_OWN_TROOPS:
        {
            result_text = translation_for_boolean_text(condition->parameter3, TR_PARAMETER_DISPLAY_IN_CITY, TR_PARAMETER_DISPLAY_ANYWHERE, result_text, &maxlength);
            result_text = translation_for_attr_mapping_text(xml_info->xml_parm1.type, condition->parameter1, result_text, &maxlength);
            result_text = translation_for_formula_index(condition->parameter2, result_text, &maxlength);
            return;
        }
        case CONDITION_TYPE_CUSTOM_VARIABLE_CHECK:
        {
            result_text = append_text(string_from_ascii(" "), result_text, &maxlength);

            if (scenario_custom_variable_exists(condition->parameter1) &&
                scenario_custom_variable_get_name(condition->parameter1)) {
                result_text = append_text(scenario_custom_variable_get_name(condition->parameter1), result_text, &maxlength);
            } else {
                result_text = append_text(string_from_ascii("???"), result_text, &maxlength);
            }

            result_text = translation_for_attr_mapping_text(xml_info->xml_parm2.type, condition->parameter2, result_text, &maxlength);
            result_text = translation_for_formula_index(condition->parameter3, result_text, &maxlength);
            return;
        }
        case CONDITION_TYPE_DIFFICULTY:
        {
            result_text = translation_for_attr_mapping_text(xml_info->xml_parm1.type, condition->parameter1, result_text, &maxlength);
            result_text = translation_for_attr_mapping_text(xml_info->xml_parm2.type, condition->parameter2, result_text, &maxlength);
            return;
        }
        case CONDITION_TYPE_MONEY:
        case CONDITION_TYPE_SAVINGS:
        case CONDITION_TYPE_STATS_FAVOR:
        case CONDITION_TYPE_STATS_PROSPERITY:
        case CONDITION_TYPE_STATS_CULTURE:
        case CONDITION_TYPE_STATS_PEACE:
        case CONDITION_TYPE_ROME_WAGES:
        case CONDITION_TYPE_TAX_RATE:
        case CONDITION_TYPE_STATS_CITY_HEALTH:
        {
            result_text = translation_for_attr_mapping_text(xml_info->xml_parm1.type, condition->parameter1, result_text, &maxlength);
            result_text = translation_for_formula_index(condition->parameter2, result_text, &maxlength);
            return;
        }
        case CONDITION_TYPE_POPS_UNEMPLOYMENT:
        {
            result_text = translation_for_boolean_text(condition->parameter1, TR_PARAMETER_DISPLAY_PERCENTAGE, TR_PARAMETER_DISPLAY_FLAT_NUMBER, result_text, &maxlength);
            result_text = translation_for_attr_mapping_text(xml_info->xml_parm2.type, condition->parameter2, result_text, &maxlength);
            result_text = translation_for_formula_index(condition->parameter3, result_text, &maxlength);
            return;
        }
        case CONDITION_TYPE_REQUEST_IS_ONGOING:
        {
            result_text = append_text(string_from_ascii(" "), result_text, &maxlength);
            result_text = translation_for_request_value(condition->parameter1, result_text, &maxlength);
            result_text = translation_for_boolean_text(condition->parameter2, TR_PARAMETER_DISPLAY_ONGOING, TR_PARAMETER_DISPLAY_NOT_ONGOING, result_text, &maxlength);
            return;
        }
        case CONDITION_TYPE_BUILDING_COUNT_AREA:
        case CONDITION_TYPE_TERRAIN_IN_AREA:
        {
            result_text = append_text(string_from_ascii(" "), result_text, &maxlength);
            result_text = append_text(translation_for(TR_PARAMETER_GRID_OFFSET), result_text, &maxlength);
            result_text = translation_for_grid_offset(condition->parameter1, result_text, &maxlength);
            result_text = append_text(string_from_ascii("-"), result_text, &maxlength);
            result_text = append_text(translation_for(TR_PARAMETER_GRID_OFFSET), result_text, &maxlength);
            result_text = translation_for_grid_offset(condition->parameter2, result_text, &maxlength);
            int param_type = condition->type == CONDITION_TYPE_BUILDING_COUNT_AREA ? PARAMETER_TYPE_BUILDING : PARAMETER_TYPE_TERRAIN;
            result_text = translation_for_type_lookup_by_value(param_type, condition->parameter3, result_text, &maxlength);
            result_text = translation_for_attr_mapping_text(xml_info->xml_parm4.type, condition->parameter4, result_text, &maxlength);
            result_text = translation_for_formula_index(condition->parameter5, result_text, &maxlength);
            return;
        }
        case CONDITION_TYPE_RESOURCE_STORAGE_AVAILABLE:
        {
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_STORAGE_TYPE, condition->parameter4, result_text, &maxlength);
            result_text = translation_for_boolean_text(condition->parameter5, TR_PARAMETER_DISPLAY_RESPECT_SETTINGS, TR_PARAMETER_DISPLAY_IGNORE_SETTINGS, result_text, &maxlength);
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_RESOURCE, condition->parameter1, result_text, &maxlength);
            result_text = translation_for_attr_mapping_text(xml_info->xml_parm2.type, condition->parameter2, result_text, &maxlength);
            result_text = translation_for_formula_index(condition->parameter3, result_text, &maxlength);
            return;
        }
        case CONDITION_TYPE_RESOURCE_STORED_COUNT:
        {
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_STORAGE_TYPE, condition->parameter4, result_text, &maxlength);
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_RESOURCE, condition->parameter1, result_text, &maxlength);
            result_text = translation_for_attr_mapping_text(xml_info->xml_parm2.type, condition->parameter2, result_text, &maxlength);
            result_text = translation_for_formula_index(condition->parameter3, result_text, &maxlength);
            return;
        }
        case CONDITION_TYPE_TIME_PASSED:
        {
            result_text = translation_for_attr_mapping_text(xml_info->xml_parm1.type, condition->parameter1, result_text, &maxlength);
            result_text = translation_for_min_max_values(condition->parameter2, condition->parameter3, result_text, &maxlength);
            return;
        }
        case CONDITION_TYPE_TRADE_ROUTE_OPEN:
        {
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_ROUTE, condition->parameter1, result_text, &maxlength);
            result_text = translation_for_boolean_text(condition->parameter2, TR_PARAMETER_DISPLAY_ROUTE_OPEN, TR_PARAMETER_DISPLAY_ROUTE_CLOSED, result_text, &maxlength);
            return;
        }
        case CONDITION_TYPE_TRADE_ROUTE_PRICE:
        {
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_ROUTE, condition->parameter1, result_text, &maxlength);
            result_text = translation_for_attr_mapping_text(xml_info->xml_parm2.type, condition->parameter2, result_text, &maxlength);
            result_text = translation_for_formula_index(condition->parameter3, result_text, &maxlength);
            return;
        }
        case CONDITION_TYPE_TRADE_SELL_PRICE:
        {
            result_text = translation_for_type_lookup_by_value(PARAMETER_TYPE_RESOURCE, condition->parameter1, result_text, &maxlength);
            result_text = translation_for_attr_mapping_text(xml_info->xml_parm2.type, condition->parameter2, result_text, &maxlength);
            result_text = translation_for_formula_index(condition->parameter3, result_text, &maxlength);
            return;
        }
        case CONDITION_TYPE_CHECK_FORMULA:
        {
            result_text = append_text(string_from_ascii(" "), result_text, &maxlength);
            result_text = translation_for_formula_index(condition->parameter1, result_text, &maxlength);
            result_text = translation_for_attr_mapping_text(xml_info->xml_parm2.type, condition->parameter2, result_text, &maxlength);
            result_text = translation_for_formula_index(condition->parameter3, result_text, &maxlength);
            return;
        }

        default:
        {
            result_text = append_text(string_from_ascii(" UNHANDLED CONDITION TYPE!"), result_text, &maxlength);
            return;
        }
    }
}

#include "game/tick.h"

extern "C" {
#include "building/connectable.h"
#include "building/count.h"
#include "building/dock.h"
#include "building/entertainment.h"
#include "building/figure.h"
#include "building/government.h"
#include "building/granary.h"
#include "building/house_evolution.h"
#include "building/house_population.h"
#include "building/house_service.h"
#include "building/industry.h"
#include "building/lighthouse.h"
#include "building/maintenance.h"
#include "building/warehouse.h"
#include "city/buildings.h"
#include "city/culture.h"
#include "city/emperor.h"
#include "city/festival.h"
#include "city/finance.h"
#include "city/games.h"
#include "city/gods.h"
#include "city/health.h"
#include "city/labor.h"
#include "city/message.h"
#include "city/migration.h"
#include "city/population.h"
#include "city/ratings.h"
#include "city/resource.h"
#include "city/sentiment.h"
#include "city/trade.h"
#include "city/victory.h"
#include "core/config.h"
#include "core/dir.h"
#include "core/random.h"
#include "editor/editor.h"
#include "empire/city.h"
#include "figure/formation.h"
#include "figuretype/crime.h"
#include "game/file.h"
#include "game/settings.h"
#include "game/time.h"
#include "game/tutorial.h"
#include "game/undo.h"
#include "graphics/weather.h"
#include "map/desirability.h"
#include "map/natives.h"
#include "map/road_network.h"
#include "map/routing_terrain.h"
#include "map/tiles.h"
#include "map/water_supply.h"
#include "scenario/demand_change.h"
#include "scenario/distant_battle.h"
#include "scenario/earthquake.h"
#include "scenario/emperor_change.h"
#include "scenario/empire.h"
#include "scenario/event/controller.h"
#include "scenario/gladiator_revolt.h"
#include "scenario/invasion.h"
#include "scenario/price_change.h"
#include "scenario/random_event.h"
#include "scenario/request.h"
#include "sound/music.h"
#include "widget/minimap.h"
}

static void advance_year(void)
{
    game_undo_disable();
    game_time_advance_year();
    scenario_empire_process_expansion();
    city_population_request_yearly_update();
    city_finance_handle_year_change();
    empire_city_reset_yearly_trade_amounts();
    building_maintenance_update_fire_direction();
    city_ratings_update(1, 0);
}

static void advance_month(void)
{
    city_migration_reset_newcomers();
    city_health_update();
    scenario_random_event_process();
    city_finance_handle_month_change();
    city_resource_consume_food();
    scenario_distant_battle_process();
    scenario_invasion_process();
    scenario_request_process();
    scenario_demand_change_process();
    scenario_price_change_process();
    city_victory_update_months_to_govern();
    formation_update_monthly_morale_at_rest();
    city_message_decrease_delays();
    city_sentiment_decrement_blessing_boost();
    building_industry_advance_stats();
    building_industry_start_strikes();
    building_trim();

    building_connectable_update_connections();
    map_tiles_update_all_roads();
    map_tiles_update_all_highways();
    map_tiles_update_all_water();
    map_routing_update_land_citizen();
    city_message_sort_and_compact();

    if (game_time_advance_month()) {
        advance_year();
    } else {
        city_ratings_update(0, 1);
    }

    city_population_record_monthly();
    city_festival_update();
    city_games_decrement_month_counts();
    city_gods_update_blessings();
    tutorial_on_month_tick();
    if (setting_monthly_autosave()) {
        game_file_write_saved_game(dir_append_location("autosave.svv", PATH_LOCATION_SAVEGAME));
    }

    city_weather_update(game_time_month());
}

static void advance_day(void)
{
    if (game_time_advance_day()) {
        advance_month();
    }

    if (game_time_day() == 0 || game_time_day() == 8) {
        city_sentiment_update();
    }
    if (game_time_day() == 0 || game_time_day() == 7) {
        building_lighthouse_consume_timber();
    }
    tutorial_on_day_tick();
    if (config_get(CONFIG_GP_CH_YEARLY_AUTOSAVE) && game_time_is_last_day_of_year()) {
        game_file_make_yearly_autosave();
    }
    scenario_events_progress_paused(1);
    scenario_events_process_all();
}

static void advance_tick(void)
{
    const int current_tick = game_time_tick();
    if (current_tick == game_time_scale_legacy_day_tick_index(1)) { city_gods_calculate_moods(1); }
    if (current_tick == game_time_scale_legacy_day_tick_index(2)) { sound_music_update(0); }
    if (current_tick == game_time_scale_legacy_day_tick_index(3)) { widget_minimap_invalidate(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(4)) { city_emperor_update(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(5)) { formation_update_all(0); }
    if (current_tick == game_time_scale_legacy_day_tick_index(6)) { map_natives_check_land(1); }
    if (current_tick == game_time_scale_legacy_day_tick_index(7)) { map_road_network_update(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(8)) { building_granaries_calculate_stocks(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(9)) { city_buildings_update_plague(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(12)) { house_service_decay_houses_covered(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(16)) { city_resource_calculate_warehouse_stocks(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(17)) { city_resource_calculate_food_stocks_and_supply_wheat(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(19)) { building_dock_update_open_water_access(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(20)) { building_industry_update_production(1); }
    if (current_tick == game_time_scale_legacy_day_tick_index(21)) { building_maintenance_check_rome_access(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(22)) { house_population_update_room(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(23)) { house_population_update_migration(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(24)) { house_population_evict_overcrowded(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(25)) { city_labor_update(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(27)) { map_water_supply_update_reservoir_fountain(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(28)) { map_water_supply_update_buildings(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(29)) { formation_update_all(1); }
    if (current_tick == game_time_scale_legacy_day_tick_index(30)) { widget_minimap_invalidate(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(31)) { building_figure_generate(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(32)) { city_trade_update(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(33)) { building_entertainment_run_shows(); city_culture_update_coverage(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(34)) { building_government_distribute_treasury(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(35)) { house_service_decay_culture(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(36)) { house_service_calculate_culture_aggregates(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(37)) { map_desirability_update(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(38)) { building_update_desirability(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(39)) { building_house_process_evolve_and_consume_goods(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(40)) { building_update_state(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(42)) { city_finance_spawn_tourist(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(43)) { building_maintenance_update_burning_ruins(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(44)) { building_maintenance_check_fire_collapse(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(45)) { figure_generate_criminals(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(46)) { building_industry_update_production(0); }
    if (current_tick == game_time_scale_legacy_day_tick_index(47)) { city_games_decrement_duration(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(48)) { house_service_decay_tax_collector(); }
    if (current_tick == game_time_scale_legacy_day_tick_index(49)) { city_culture_calculate(); }
    if (game_time_advance_tick()) {
        advance_day();
    }
}

void game_tick_run(void)
{
    if (editor_is_active()) {
        random_generate_next(); // update random to randomize native huts
        figure_action_handle(); // just update the flag figures
        return;
    }
    random_generate_next();
    game_undo_reduce_time_available();
    advance_tick();
    figure_action_handle();
    scenario_earthquake_process();
    scenario_gladiator_revolt_process();
    scenario_emperor_change_process();
    city_victory_check();
}

void game_tick_cheat_year(void)
{
    advance_year();
}

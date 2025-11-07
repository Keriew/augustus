#include "config.h"

#include "core/dir.h"
#include "core/file.h"
#include "core/log.h"

#include <stdio.h>
#include <string.h>

#define MAX_LINE 100

static const char *INI_FILENAME = "augustus.ini";
static int needs_user_directory_setup;

// Keep this in the same order as the config_keys in config.h
static const char *ini_keys[] = {
    [CONFIG_GENERAL_ENABLE_AUDIO] = "enable_audio",
    [CONFIG_GENERAL_MASTER_VOLUME] = "master_volume",
    [CONFIG_GENERAL_ENABLE_MUSIC_RANDOMISE] = "enable_music_randomise",
    [CONFIG_GENERAL_ENABLE_VIDEO_SOUND] = "enable_audio_in_videos",
    [CONFIG_GENERAL_VIDEO_VOLUME] = "video_volume",
    [CONFIG_GENERAL_NEXT_AUTOSAVE_SLOT] = "next_autosave_slot",
    [CONFIG_GENERAL_HAS_SET_USER_DIRECTORIES] = "has_set_user_directories",
    [CONFIG_GP_FIX_IMMIGRATION_BUG] = "gameplay_fix_immigration",
    [CONFIG_GP_FIX_100_YEAR_GHOSTS] = "gameplay_fix_100y_ghosts",
    [CONFIG_SCREEN_DISPLAY_SCALE] = "screen_display_scale",
    [CONFIG_SCREEN_CURSOR_SCALE] = "screen_cursor_scale",
    [CONFIG_SCREEN_COLOR_CURSORS] = "screen_color_cursors",
    [CONFIG_UI_SIDEBAR_INFO] = "ui_sidebar_info",
    [CONFIG_UI_SHOW_INTRO_VIDEO] = "ui_show_intro_video",
    [CONFIG_UI_SMOOTH_SCROLLING] = "ui_smooth_scrolling",
    [CONFIG_UI_DISABLE_MOUSE_EDGE_SCROLLING] = "ui_disable_mouse_edge_scrolling",
    [CONFIG_UI_VISUAL_FEEDBACK_ON_DELETE] = "ui_visual_feedback_on_delete",
    [CONFIG_UI_ALLOW_CYCLING_TEMPLES] = "ui_allow_cycling_temples",
    [CONFIG_UI_SHOW_WATER_STRUCTURE_RANGE] = "ui_show_water_structure_range",
    [CONFIG_UI_SHOW_WATER_STRUCTURE_RANGE_HOUSES] = "ui_show_water_structure_range_houses",
    [CONFIG_UI_SHOW_MARKET_RANGE] = "ui_show_market_range",
    [CONFIG_UI_SHOW_CONSTRUCTION_SIZE] = "ui_show_construction_size",
    [CONFIG_UI_HIGHLIGHT_LEGIONS] = "ui_highlight_legions",
    [CONFIG_UI_SHOW_MILITARY_SIDEBAR] = "ui_show_military_sidebar",
    [CONFIG_UI_DISABLE_RIGHT_CLICK_MAP_DRAG] = "ui_disable_map_drag",
    [CONFIG_UI_SHOW_MAX_PROSPERITY] = "ui_show_max_prosperity",
    [CONFIG_UI_DIGIT_SEPARATOR] = "ui_digit_separator",
    [CONFIG_UI_INVERSE_MAP_DRAG] = "ui_inverse_map_drag",
    [CONFIG_UI_MESSAGE_ALERTS] = "ui_message_alerts",
    [CONFIG_UI_SHOW_GRID] = "ui_show_grid",
    [CONFIG_UI_SHOW_PARTIAL_GRID_AROUND_CONSTRUCTION] = "ui_show_partial_grid_around_construction",
    [CONFIG_UI_ALWAYS_SHOW_ROTATION_BUTTONS] = "ui_always_show_rotation_buttons",
    [CONFIG_UI_SHOW_ROAMING_PATH] = "ui_show_roaming_path",
    [CONFIG_UI_DRAW_CLOUD_SHADOWS] = "ui_draw_cloud_shadows",
    [CONFIG_UI_DISPLAY_FPS] = "ui_display_fps",
    [CONFIG_UI_ASK_CONFIRMATION_ON_FILE_OVERWRITE] = "ui_ask_confirmation_on_file_overwrite",
    [CONFIG_GP_CH_MAX_GRAND_TEMPLES] = "gameplay_change_max_grand_temples",
    [CONFIG_GP_CH_JEALOUS_GODS] = "gameplay_change_jealous_gods",
    [CONFIG_GP_CH_GLOBAL_LABOUR] = "gameplay_change_global_labour",
    [CONFIG_GP_CH_RETIRE_AT_60] = "gameplay_change_retire_at_60",
    [CONFIG_GP_CH_FIXED_WORKERS] = "gameplay_change_fixed_workers",
    [CONFIG_GP_CH_WOLVES_BLOCK] = "gameplay_wolves_block",
    [CONFIG_GP_CH_NO_SUPPLIER_DISTRIBUTION] = "gameplay_buyers_dont_distribute",
    [CONFIG_GP_CH_GETTING_GRANARIES_GO_OFFROAD] = "gameplay_change_getting_granaries_go_offroad",
    [CONFIG_GP_CH_GRANARIES_GET_DOUBLE] = "gameplay_change_granaries_get_double",
    [CONFIG_GP_CH_ALLOW_EXPORTING_FROM_GRANARIES] = "gameplay_change_allow_exporting_from_granaries",
    [CONFIG_GP_CH_TOWER_SENTRIES_GO_OFFROAD] = "gameplay_change_tower_sentries_go_offroad",
    [CONFIG_GP_CH_FARMS_DELIVER_CLOSE] = "gameplay_change_farms_deliver_close",
    [CONFIG_GP_CH_DELIVER_ONLY_TO_ACCEPTING_GRANARIES] = "gameplay_change_only_deliver_to_accepting_granaries",
    [CONFIG_GP_CH_ALL_HOUSES_MERGE] = "gameplay_change_all_houses_merge",
    [CONFIG_GP_CH_RANDOM_COLLAPSES_TAKE_MONEY] = "gameplay_change_random_mine_or_pit_collapses_take_money",
    [CONFIG_GP_CH_MULTIPLE_BARRACKS] = "gameplay_change_multiple_barracks",
    [CONFIG_GP_CH_WAREHOUSES_DONT_ACCEPT] = "gameplay_change_warehouses_dont_accept",
    [CONFIG_GP_CH_MARKETS_DONT_ACCEPT] = "gameplay_change_markets_dont_accept",
    [CONFIG_GP_CH_WAREHOUSES_GRANARIES_OVER_ROAD_PLACEMENT] = "gameplay_change_warehouses_granaries_over_road_placement",
    [CONFIG_GP_CH_HOUSES_DONT_EXPAND_INTO_GARDENS] = "gameplay_change_houses_dont_expand_into_gardens",
    [CONFIG_GP_CH_MONUMENTS_BOOST_CULTURE_RATING] = "gameplay_change_monuments_boost_culture_rating",
    [CONFIG_GP_CH_DISABLE_INFINITE_WOLVES_SPAWNING] = "gameplay_change_disable_infinite_wolves_spawning",
    [CONFIG_GP_CH_ROAMERS_DONT_SKIP_CORNERS] = "gameplay_change_romers_dont_skip_corners",
    [CONFIG_GP_CH_YEARLY_AUTOSAVE] = "gameplay_change_yearly_autosave",
    [CONFIG_GP_CH_AUTO_KILL_ANIMALS] = "gameplay_change_auto_kill_animals",
    [CONFIG_GP_CH_GATES_DEFAULT_TO_PASS_ALL_WALKERS] = "gameplay_change_nonmilitary_gates_allow_walkers",
    [CONFIG_GP_CH_MAX_AUTOSAVE_SLOTS] = "gameplay_change_max_autosave_slots",
    [CONFIG_UI_SHOW_SPEEDRUN_INFO] = "ui_show_speedrun_info",
    [CONFIG_UI_SHOW_DESIRABILITY_RANGE] = "ui_show_desirability_range",
    [CONFIG_UI_SHOW_DESIRABILITY_RANGE_ALL] = "ui_show_desirability_range_all",
    [CONFIG_UI_DRAW_ASCLEPIUS] = "ui_draw_asclepius",
    [CONFIG_UI_HIGHLIGHT_SELECTED_BUILDING] = "ui_highlight_selected_building",
    [CONFIG_GP_CARAVANS_MOVE_OFF_ROAD] = "gameplay_change_caravans_move_off_road",
    [CONFIG_UI_DRAW_WEATHER] = "ui_draw_weather",
    [CONFIG_GP_STORAGE_INCREMENT_4] = "gameplay_change_storage_step_4",
    [CONFIG_UI_MOVE_SAVINGS_TO_RIGHT] = "ui_move_savings_to_right",
    [CONFIG_GP_CH_PATRICIAN_DEVOLUTION_FIX] = "gameplay_patrician_devolution_fix",
    [CONFIG_WT_SNOW_INTENSITY] = "weather_snow_intensity",
    [CONFIG_WT_RAIN_INTENSITY] = "weather_rain_intensity",
    [CONFIG_WT_SANDSTORM_INTENSITY] = "weather_sandstorm_intensity",
    [CONFIG_WT_RAIN_SPEED] = "weather_rain_speed",
    [CONFIG_WT_RAIN_LENGTH] = "weather_rain_length",
    [CONFIG_WT_SNOW_SPEED] = "weather_snow_speed",
    [CONFIG_WT_SANDSTORM_SPEED] = "weather_sandstorm_speed",
    [CONFIG_UI_EMPIRE_SIDEBAR_WIDTH] = "ui_empire_sidebar_width",
    [CONFIG_GP_CH_DEFAULT_GAME_SPEED] = "gameplay_change_default_game_speed",
    [CONFIG_UI_SHOW_CUSTOM_VARIABLES] = "ui_show_custom_variables",
    [CONFIG_GP_CH_ENABLE_GETTING_WHILE_STOCKPILED] = "gameplay_change_stockpiled_getting",
    [CONFIG_UI_PAVED_ROADS_NEAR_GRANNARIES] = "ui_paved_roads_near_grannaries",
    [CONFIG_UI_ANIMATE_TRADE_ROUTES] = "ui_animate_trade_routes",
    [CONFIG_UI_MOVE_LEGION_SOUND_SWAP] = "ui_move_legion_sound_swap",
    [CONFIG_GP_CH_CART_DEPOT_ADVANCED] = "gameplay_change_cart_depot_advanced_settings",
    [CONFIG_UI_CART_DEPOT_TOOLTIP_STYLE] = "ui_cart_depot_tooltip_style",
    [CONFIG_DEBUG_START_WITH_TOOLTIP] = "debug_start_with_tooltip",
    [CONFIG_UI_CLEAR_WARNINGS_RIGHTCLICK] = "ui_clear_warnings_rightclick",
    [CONFIG_GP_CH_STORAGE_REQUESTS_RESPECT_MAINTAIN] = "gp_ch_storage_requests_respect_maintain",
    [CONFIG_GP_CH_MARKET_RANGE] = "gameplay_market_range",
    [CONFIG_UI_CV_BUILD_MENU_ICONS] = "ui_cv_build_menu_icons",
    [CONFIG_UI_WT_ENABLE_SNOW_CENTRAL] = "ui_wt_enable_snow_central",
    [CONFIG_UI_CV_CURSOR_SHADOW] = "ui_cv_cursor_shadow",
    [CONFIG_GENERAL_UNLOCK_MOUSE] = "general_unlock_mouse",
    [CONFIG_GP_CH_HOUSING_PRE_MERGE_VACANT_LOTS] = "gp_ch_housing_pre_merge_vacant_lots",
    [CONFIG_UI_BUILD_SHOW_RESERVOIR_RANGES] = "ui_build_show_reservoir_ranges",
};

static const char *ini_string_keys[] = {
    "ui_language_dir",
};

static int values[CONFIG_MAX_ENTRIES];
static char string_values[CONFIG_STRING_MAX_ENTRIES][CONFIG_STRING_VALUE_MAX];

static int default_values[CONFIG_MAX_ENTRIES] = {
    [CONFIG_GENERAL_ENABLE_AUDIO] = 1,
    [CONFIG_GENERAL_MASTER_VOLUME] = 100,
    [CONFIG_GENERAL_ENABLE_MUSIC_RANDOMISE] = 0,
    [CONFIG_GENERAL_ENABLE_VIDEO_SOUND] = 1,
    [CONFIG_GENERAL_VIDEO_VOLUME] = 100,
    [CONFIG_GENERAL_HAS_SET_USER_DIRECTORIES] = 1,
    [CONFIG_UI_SIDEBAR_INFO] = 1,
    [CONFIG_UI_SMOOTH_SCROLLING] = 1,
    [CONFIG_UI_SHOW_WATER_STRUCTURE_RANGE] = 1,
    [CONFIG_UI_SHOW_CONSTRUCTION_SIZE] = 1,
    [CONFIG_UI_HIGHLIGHT_LEGIONS] = 1,
    [CONFIG_UI_ASK_CONFIRMATION_ON_FILE_OVERWRITE] = 1,
    [CONFIG_SCREEN_DISPLAY_SCALE] = 100,
    [CONFIG_SCREEN_CURSOR_SCALE] = 100,
    [CONFIG_GP_CH_MAX_GRAND_TEMPLES] = 2,
    [CONFIG_UI_SHOW_DESIRABILITY_RANGE] = 0,
    [CONFIG_UI_SHOW_DESIRABILITY_RANGE_ALL] = 0,
    [CONFIG_UI_HIGHLIGHT_SELECTED_BUILDING] = 1,
    [CONFIG_GP_CH_MAX_AUTOSAVE_SLOTS] = 10,
    [CONFIG_GENERAL_NEXT_AUTOSAVE_SLOT] = 0,
    [CONFIG_GP_CARAVANS_MOVE_OFF_ROAD] = 0,
    [CONFIG_UI_DRAW_WEATHER] = 0,
    [CONFIG_GP_STORAGE_INCREMENT_4] = 0,
    [CONFIG_UI_MOVE_SAVINGS_TO_RIGHT] = 0,
    [CONFIG_GP_CH_PATRICIAN_DEVOLUTION_FIX] = 1,
    [CONFIG_WT_SNOW_INTENSITY] = 20,
    [CONFIG_WT_RAIN_INTENSITY] = 20,
    [CONFIG_WT_SANDSTORM_INTENSITY] = 40,
    [CONFIG_WT_RAIN_SPEED] = 4,
    [CONFIG_WT_RAIN_LENGTH] = 10,
    [CONFIG_WT_SNOW_SPEED] = 1,
    [CONFIG_WT_SANDSTORM_SPEED] = 2,
    [CONFIG_UI_EMPIRE_SIDEBAR_WIDTH] = 25,
    [CONFIG_GP_CH_DEFAULT_GAME_SPEED] = 7, //0-based index, 7 points to 80%
    [CONFIG_UI_SHOW_CUSTOM_VARIABLES] = 1,
    [CONFIG_GP_CH_ENABLE_GETTING_WHILE_STOCKPILED] = 0,
    [CONFIG_UI_PAVED_ROADS_NEAR_GRANNARIES] = 1,
    [CONFIG_UI_ANIMATE_TRADE_ROUTES] = 1,
    [CONFIG_UI_MOVE_LEGION_SOUND_SWAP] = 0,
    [CONFIG_GP_CH_CART_DEPOT_ADVANCED] = 1,
    [CONFIG_UI_CART_DEPOT_TOOLTIP_STYLE] = 1,
    [CONFIG_DEBUG_START_WITH_TOOLTIP] = 0,
    [CONFIG_UI_CLEAR_WARNINGS_RIGHTCLICK] = 1,
    [CONFIG_GP_CH_STORAGE_REQUESTS_RESPECT_MAINTAIN] = 1,
    [CONFIG_GP_CH_MARKET_RANGE] = 1,

    [CONFIG_UI_CV_BUILD_MENU_ICONS] = 1,
    [CONFIG_UI_WT_ENABLE_SNOW_CENTRAL] = 0,
    [CONFIG_UI_CV_CURSOR_SHADOW] = 1,
    [CONFIG_GENERAL_UNLOCK_MOUSE] = 1,
    [CONFIG_GP_CH_HOUSING_PRE_MERGE_VACANT_LOTS] = 1,
    [CONFIG_UI_BUILD_SHOW_RESERVOIR_RANGES] = 1,
};

static const char default_string_values[CONFIG_STRING_MAX_ENTRIES][CONFIG_STRING_VALUE_MAX] = { 0 };

int config_get(config_key key)
{
    return values[key];
}

void config_set(config_key key, int value)
{
    values[key] = value;
}

const char *config_get_string(config_string_key key)
{
    return string_values[key];
}

void config_set_string(config_string_key key, const char *value)
{
    if (!value) {
        string_values[key][0] = 0;
    } else {
        snprintf(string_values[key], CONFIG_STRING_VALUE_MAX, "%s", value);
    }
}

int config_get_default_value(config_key key)
{
    return default_values[key];
}

const char *config_get_default_string_value(config_string_key key)
{
    return default_string_values[key];
}

static void set_defaults(void)
{
    for (int i = 0; i < CONFIG_MAX_ENTRIES; ++i) {
        values[i] = default_values[i];
    }
    snprintf(string_values[CONFIG_STRING_UI_LANGUAGE_DIR], CONFIG_STRING_VALUE_MAX, "%s",
        default_string_values[CONFIG_STRING_UI_LANGUAGE_DIR]);
}

void config_load(void)
{
    set_defaults();
    needs_user_directory_setup = 1;
    const char *file_name = dir_get_file_at_location(INI_FILENAME, PATH_LOCATION_CONFIG);
    if (!file_name) {
        return;
    }
    FILE *fp = file_open(file_name, "rt");
    if (!fp) {
        return;
    }
    // Override default, if value is the same at end, then we never setup the directories
    needs_user_directory_setup = 0;
    values[CONFIG_GENERAL_HAS_SET_USER_DIRECTORIES] = -1;

    char line_buffer[MAX_LINE];
    char *line;
    while ((line = fgets(line_buffer, MAX_LINE, fp)) != 0) {
        // Remove newline from string
        size_t size = strlen(line);
        while (size > 0 && (line[size - 1] == '\n' || line[size - 1] == '\r')) {
            line[--size] = 0;
        }
        char *equals = strchr(line, '=');
        if (equals) {
            *equals = 0;
            for (int i = 0; i < CONFIG_MAX_ENTRIES; i++) {
                if (strcmp(ini_keys[i], line) == 0) {
                    int value = atoi(&equals[1]);
                    log_info("Config key", ini_keys[i], value);
                    values[i] = value;
                    break;
                }
            }
            for (int i = 0; i < CONFIG_STRING_MAX_ENTRIES; i++) {
                if (strcmp(ini_string_keys[i], line) == 0) {
                    const char *value = &equals[1];
                    log_info("Config key", ini_string_keys[i], 0);
                    log_info("Config value", value, 0);
                    snprintf(string_values[i], CONFIG_STRING_VALUE_MAX, "%s", value);
                    break;
                }
            }
        }
    }
    file_close(fp);
    if (values[CONFIG_GENERAL_HAS_SET_USER_DIRECTORIES] == -1) {
        values[CONFIG_GENERAL_HAS_SET_USER_DIRECTORIES] = default_values[CONFIG_GENERAL_HAS_SET_USER_DIRECTORIES];
        needs_user_directory_setup = 1;
    }
}

int config_must_configure_user_directory(void)
{
    return needs_user_directory_setup;
}

void config_save(void)
{
    const char *file_name = dir_append_location(INI_FILENAME, PATH_LOCATION_CONFIG);
    if (!file_name) {
        return;
    }
    FILE *fp = file_open(file_name, "wt");
    if (!fp) {
        log_error("Unable to write configuration file", INI_FILENAME, 0);
        return;
    }
    for (int i = 0; i < CONFIG_MAX_ENTRIES; i++) {
        fprintf(fp, "%s=%d\n", ini_keys[i], values[i]);
    }
    for (int i = 0; i < CONFIG_STRING_MAX_ENTRIES; i++) {
        fprintf(fp, "%s=%s\n", ini_string_keys[i], string_values[i]);
    }
    file_close(fp);
    needs_user_directory_setup = 0;
}

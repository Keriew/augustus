#include "config.h"

#include "core/dir.h"
#include "core/file.h"
#include "core/log.h"

#include <stdio.h>
#include <string.h>

#define MAX_LINE 100

static const char *PRIMARY_INI_FILENAME = "Vespasian.ini";
static const char *LEGACY_INI_FILENAME = "augustus.ini";
static int needs_user_directory_setup;

// Keep this in the same order as the config_keys in config.h
static const char *ini_keys[] = {
    "enable_audio",
    "master_volume",
    "enable_music_randomise",
    "enable_audio_in_videos",
    "video_volume",
    "next_autosave_slot",
    "has_set_user_directories",
    "gameplay_fix_immigration",
    "gameplay_fix_100y_ghosts",
    "screen_display_scale",
    "screen_cursor_scale",
    "screen_color_cursors",
    "ui_sidebar_info",
    "ui_show_intro_video",
    "ui_smooth_scrolling",
    "ui_disable_mouse_edge_scrolling",
    "ui_visual_feedback_on_delete",
    "ui_allow_cycling_temples",
    "ui_show_water_structure_range",
    "ui_show_water_structure_range_houses",
    "ui_show_market_range",
    "ui_show_construction_size",
    "ui_highlight_legions",
    "ui_show_military_sidebar",
    "ui_disable_map_drag",
    "ui_show_max_prosperity",
    "ui_digit_separator",
    "ui_inverse_map_drag",
    "ui_message_alerts",
    "ui_show_grid",
    "ui_show_partial_grid_around_construction",
    "ui_always_show_rotation_buttons",
    "ui_show_roaming_path",
    "ui_draw_cloud_shadows",
    "ui_display_fps",
    "ui_ask_confirmation_on_file_overwrite",
    "gameplay_change_max_grand_temples",
    "gameplay_change_jealous_gods",
    "gameplay_change_global_labour",
    "gameplay_change_retire_at_60",
    "gameplay_change_fixed_workers",
    "gameplay_wolves_block",
    "gameplay_buyers_dont_distribute",
    "gameplay_change_getting_granaries_go_offroad",
    "gameplay_change_granaries_get_double",
    "gameplay_change_allow_exporting_from_granaries",
    "gameplay_change_tower_sentries_go_offroad",
    "gameplay_change_farms_deliver_close",
    "gameplay_change_only_deliver_to_accepting_granaries",
    "gameplay_change_all_houses_merge",
    "gameplay_change_random_mine_or_pit_collapses_take_money",
    "gameplay_change_multiple_barracks",
    "gameplay_change_warehouses_dont_accept",
    "gameplay_change_markets_dont_accept",
    "gameplay_change_warehouses_granaries_over_road_placement",
    "gameplay_change_houses_dont_expand_into_gardens",
    "gameplay_change_monuments_boost_culture_rating",
    "gameplay_change_disable_infinite_wolves_spawning",
    "gameplay_change_romers_dont_skip_corners",
    "gameplay_change_yearly_autosave",
    "gameplay_change_auto_kill_animals",
    "gameplay_change_nonmilitary_gates_allow_walkers",
    "gameplay_change_max_autosave_slots",
    "ui_show_speedrun_info",
    "ui_show_desirability_range",
    "ui_show_desirability_range_all",
    "ui_draw_asclepius",
    "ui_highlight_selected_building",
    "gameplay_change_caravans_move_off_road",
    "ui_draw_weather",
    "gameplay_change_storage_step_4",
    "ui_move_savings_to_right",
    "gameplay_patrician_devolution_fix",
    "weather_snow_intensity",
    "weather_rain_intensity",
    "weather_sandstorm_intensity",
    "weather_rain_speed",
    "weather_rain_length",
    "weather_snow_speed",
    "weather_sandstorm_speed",
    "ui_empire_sidebar_width",
    "gameplay_change_default_game_speed",
    "ui_show_custom_variables",
    "gameplay_change_stockpiled_getting",
    "ui_paved_roads_near_grannaries",
    "ui_animate_trade_routes",
    "ui_move_legion_sound_swap",
    "gameplay_change_cart_depot_advanced_settings",
    "ui_cart_depot_tooltip_style",
    "debug_start_with_tooltip",
    "ui_clear_warnings_rightclick",
    "gp_ch_storage_requests_respect_maintain",
    "gameplay_market_range",
    "ui_cv_build_menu_icons",
    "ui_wt_enable_snow_central",
    "ui_cv_cursor_shadow",
    "general_unlock_mouse",
    "gp_ch_housing_pre_merge_vacant_lots",
    "ui_build_show_reservoir_ranges",
    "ui_empire_smart_border_placement",
    "always_be_able_to_destroy_bridges",
    "ui_enable_build_menu_shortcuts",
    "ui_empire_must_click_to_delete",
    "ui_empire_confirm_deletion",
    "ui_wt_preview_rain",
    "ui_wt_snow_preview",
    "ui_wt_sandstorm_preview",
    "ui_wt_preview_heavy_rain",
    "ui_wt_sandstorm_size",
    "ui_wt_snowflake_size",
    "ui_wt_weather_duration",
    "ui_scale_filter"
};

static const char *ini_string_keys[] = {
    "ui_language_dir",
};

static int values[CONFIG_MAX_ENTRIES];
static char string_values[CONFIG_STRING_MAX_ENTRIES][CONFIG_STRING_VALUE_MAX];

static int default_values[CONFIG_MAX_ENTRIES] = {
    1,
    100,
    0,
    1,
    100,
    0,
    1,
    0,
    0,
    100,
    100,
    0,
    1,
    0,
    1,
    0,
    0,
    0,
    1,
    0,
    0,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    2,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    10,
    0,
    0,
    0,
    0,
    1,
    0,
    1,
    0,
    0,
    1,
    30,
    60,
    10,
    2,
    2,
    1,
    0,
    25,
    7, // 0-based index, 7 points to 80%
    1,
    0,
    1,
    1,
    0,
    1,
    1,
    0,
    1,
    1,
    1,
    1,
    0,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    2,
    1,
    CONFIG_UI_SCALE_FILTER_AUTO
};

static const char default_string_values[CONFIG_STRING_MAX_ENTRIES][CONFIG_STRING_VALUE_MAX] = { 0 };

static const char *config_get_load_file_name(void)
{
    const char *file_name = dir_get_file_at_location(PRIMARY_INI_FILENAME, PATH_LOCATION_CONFIG);
    if (file_name) {
        return file_name;
    }
    return dir_get_file_at_location(LEGACY_INI_FILENAME, PATH_LOCATION_CONFIG);
}

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
    const char *file_name = config_get_load_file_name();
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
    const char *file_name = dir_append_location(PRIMARY_INI_FILENAME, PATH_LOCATION_CONFIG);
    if (!file_name) {
        return;
    }
    FILE *fp = file_open(file_name, "wt");
    if (!fp) {
        log_error("Unable to write configuration file", PRIMARY_INI_FILENAME, 0);
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

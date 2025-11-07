#include "config.h"

#include "core/calc.h"
#include "core/config.h"
#include "core/dir.h"
#include "core/image_group.h"
#include "core/lang.h"
#include "core/log.h"
#include "core/string.h"
#include "game/game.h"
#include "game/settings.h"
#include "game/system.h"
#include "game/speed.h"
#include "graphics/button.h"
#include "graphics/color.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/list_box.h"
#include "graphics/image.h"
#include "graphics/panel.h"
#include "graphics/screen.h"
#include "graphics/scrollbar.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "platform/screen.h"
#include "sound/city.h"
#include "sound/device.h"
#include "sound/effect.h"
#include "sound/music.h"
#include "sound/speech.h"
#include "translation/translation.h"
#include "window/hotkey_config.h"
#include "window/main_menu.h"
#include "window/plain_message_dialog.h"
#include "window/select_list.h"
#include "window/text_input.h"
#include "window/user_path_setup.h"

#include <stdio.h>
#include <string.h>
#include <SDL_mouse.h>

#define MAX_LANGUAGE_DIRS 20
#define MAX_WIDGETS       64
#define NUM_VISIBLE_FALLBACK 13  //  keeps slider track length close to old feel
#define ITEM_Y_OFFSET  100
#define ITEM_BASE_H     24
#define CHECKBOX_CHECK_SIZE 20
#define CHECKBOX_MARGIN 5
#define PLAYER_NAME_LENGTH 32
//  Left list (category)
#define LIST_BOX_SHIFT   180
#define LIST_BOX_WIDTH   180
#define LIST_BOX_HEIGHT  310
#define LIST_BOX_X        20
#define LIST_BOX_Y       100
#define LIST_BOX_ITEM_H   24
//  List (content) rect
#define LIST_Y       75
#define LIST_HEIGHT 355
#define LIST_BOTTOM (LIST_Y + LIST_HEIGHT)
//  Slider visuals
#define NUMERICAL_RANGE_X        20
#define NUMERICAL_SLIDER_PADDING  2
#define NUMERICAL_DOT_SIZE       20
// bottom buttons
#define NUM_BOTTOM_BUTTONS 5

enum {
    TYPE_NONE,
    TYPE_SPACE,
    TYPE_HEADER,
    TYPE_CHECKBOX,
    TYPE_SELECT, // dropdown or text input
    TYPE_NUMERICAL_DESC, //  label line for a slider
    TYPE_NUMERICAL_RANGE
};

enum {
    SELECT_LANGUAGE,
    SELECT_PLAYER_NAME,
    SELECT_USER_DIRECTORY
};

enum {
    RANGE_GAME_SPEED,
    RANGE_RESOLUTION,
    RANGE_DISPLAY_SCALE,
    RANGE_CURSOR_SCALE,
    RANGE_MASTER_VOLUME,
    RANGE_MUSIC_VOLUME,
    RANGE_SPEECH_VOLUME,
    RANGE_SOUND_EFFECTS_VOLUME,
    RANGE_CITY_SOUNDS_VOLUME,
    RANGE_VIDEO_VOLUME,
    RANGE_SCROLL_SPEED,
    RANGE_DIFFICULTY,
    RANGE_MAX_GRAND_TEMPLES,
    RANGE_MAX_AUTOSAVE_SLOTS,
    RANGE_DEFAULT_GAME_SPEED,
};

enum {
    CONFIG_ORIGINAL_GAME_SPEED = CONFIG_MAX_ENTRIES,
    CONFIG_ORIGINAL_FULLSCREEN,
    CONFIG_ORIGINAL_WINDOWED_RESOLUTION,
    CONFIG_ORIGINAL_ENABLE_MUSIC,
    CONFIG_ORIGINAL_MUSIC_VOLUME,
    CONFIG_ORIGINAL_ENABLE_SPEECH,
    CONFIG_ORIGINAL_SPEECH_VOLUME,
    CONFIG_ORIGINAL_ENABLE_SOUND_EFFECTS,
    CONFIG_ORIGINAL_SOUND_EFFECTS_VOLUME,
    CONFIG_ORIGINAL_ENABLE_CITY_SOUNDS,
    CONFIG_ORIGINAL_CITY_SOUNDS_VOLUME,
    CONFIG_ORIGINAL_SCROLL_SPEED,
    CONFIG_ORIGINAL_DIFFICULTY,
    CONFIG_ORIGINAL_GODS_EFFECTS,
    CONFIG_MAX_ALL
};

enum {
    CONFIG_STRING_ORIGINAL_PLAYER_NAME = CONFIG_STRING_MAX_ENTRIES,
    CONFIG_STRING_MAX_ALL
};

//  Widget main struct - use this for all widgets to ensure unified treatment, while allowing multiline checkboxes,
//  variable height and width

typedef struct {
    int type;
    int subtype;
    translation_key description;          //  label / header text key
    const uint8_t *(*get_display_text)(void);
    int y_offset; //  kept for compatibility 
    int enabled; //  runtime on/off
    int height;
    int margin_top;  //  extra spacing before (can be used instead of TYPE_SPACE)
} config_widget;

static const uint8_t *display_text_language(void);
static const uint8_t *display_text_user_directory(void);
static const uint8_t *display_text_player_name(void);
static const uint8_t *display_text_game_speed(void);
static const uint8_t *display_text_resolution(void);
static const uint8_t *display_text_display_scale(void);
static const uint8_t *display_text_cursor_scale(void);
static const uint8_t *display_text_master_volume(void);
static const uint8_t *display_text_music_volume(void);
static const uint8_t *display_text_speech_volume(void);
static const uint8_t *display_text_sound_effects_volume(void);
static const uint8_t *display_text_city_sounds_volume(void);
static const uint8_t *display_text_video_volume(void);
static const uint8_t *display_text_scroll_speed(void);
static const uint8_t *display_text_difficulty(void);
static const uint8_t *display_text_max_grand_temples(void);
static const uint8_t *display_text_autosave_slots(void);
static const uint8_t *display_text_default_game_speed(void);

// page-related helpers
static int get_widget_count_for(unsigned int page);
static void set_page(unsigned int page);
static int page_is_category(unsigned int page);
//input helpers
static void on_scroll(void);

//---------------------------------------------------------------------
// ---------- WIDGET PLACEMENT IN PAGES IN ORDER ----------------------
//---------------------------------------------------------------------

// ---------- General ----------------------
static config_widget page_general[] = {
    {TYPE_SELECT, SELECT_USER_DIRECTORY, TR_USER_DIRETORIES_WINDOW_USER_PATH, display_text_user_directory, 0, 1, ITEM_BASE_H, 8},
    {TYPE_SELECT, SELECT_LANGUAGE, TR_CONFIG_LANGUAGE_LABEL, display_text_language, 0, 1, ITEM_BASE_H, 8},
    {TYPE_SELECT, SELECT_PLAYER_NAME, TR_CONFIG_DEFAULT_PLAYER_NAME, display_text_player_name, 0, 1, ITEM_BASE_H, 8},

    {TYPE_NUMERICAL_DESC, RANGE_GAME_SPEED, TR_CONFIG_GAME_SPEED, NULL, 0, 1, ITEM_BASE_H, 10},
    {TYPE_NUMERICAL_RANGE, RANGE_GAME_SPEED, 0, display_text_game_speed, 0, 1, ITEM_BASE_H, 2},

    {TYPE_NUMERICAL_DESC, RANGE_DEFAULT_GAME_SPEED, TR_CONFIG_DEFAULT_GAME_SPEED, NULL, 0, 1, ITEM_BASE_H, 10},
    {TYPE_NUMERICAL_RANGE, RANGE_DEFAULT_GAME_SPEED, 0, display_text_default_game_speed, 0, 1, ITEM_BASE_H, 2},

    {TYPE_NUMERICAL_DESC, RANGE_MAX_AUTOSAVE_SLOTS, TR_CONFIG_MAX_AUTOSAVE_SLOTS, NULL, 0, 1, ITEM_BASE_H, 10},
    {TYPE_NUMERICAL_RANGE, RANGE_MAX_AUTOSAVE_SLOTS, 0, display_text_autosave_slots, 0, 1, ITEM_BASE_H, 2},
    {TYPE_CHECKBOX, CONFIG_GP_CH_YEARLY_AUTOSAVE, TR_BUTTON_YEARLY_AUTOSAVE_ON, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},

    {TYPE_HEADER, 0, TR_CONFIG_VIDEO, NULL, 0, 1, ITEM_BASE_H, 14},
    {TYPE_CHECKBOX, CONFIG_ORIGINAL_FULLSCREEN, TR_CONFIG_FULLSCREEN, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
    {TYPE_NUMERICAL_DESC, RANGE_RESOLUTION, TR_CONFIG_WINDOWED_RESOLUTION, NULL, 0, 1, ITEM_BASE_H, 10},
    {TYPE_NUMERICAL_RANGE, RANGE_RESOLUTION, 0, display_text_resolution, 0, 1, ITEM_BASE_H, 2},
    {TYPE_NUMERICAL_DESC, RANGE_DISPLAY_SCALE, TR_CONFIG_DISPLAY_SCALE, NULL, 0, 1, ITEM_BASE_H, 10},
    {TYPE_NUMERICAL_RANGE, RANGE_DISPLAY_SCALE, 0, display_text_display_scale, 0, 1, ITEM_BASE_H, 2},
    {TYPE_NUMERICAL_DESC, RANGE_CURSOR_SCALE, TR_CONFIG_CURSOR_SCALE, NULL, 0, 1, ITEM_BASE_H, 10},
    {TYPE_NUMERICAL_RANGE, RANGE_CURSOR_SCALE, 0, display_text_cursor_scale, 0, 1, ITEM_BASE_H, 2},
    {TYPE_CHECKBOX, CONFIG_SCREEN_COLOR_CURSORS, TR_CONFIG_USE_COLOR_CURSORS, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
    {TYPE_CHECKBOX, CONFIG_GENERAL_UNLOCK_MOUSE, TR_CONFIG_GENERAL_UNLOCK_MOUSE, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},

    {TYPE_HEADER, 0, TR_CONFIG_AUDIO, NULL, 0, 1, ITEM_BASE_H, 14},
    {TYPE_CHECKBOX, CONFIG_GENERAL_ENABLE_AUDIO, TR_CONFIG_ENABLE_AUDIO, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
    {TYPE_NUMERICAL_RANGE, RANGE_MASTER_VOLUME, 0, display_text_master_volume, 0, 1, ITEM_BASE_H, 2},
    {TYPE_CHECKBOX, CONFIG_ORIGINAL_ENABLE_MUSIC, TR_CONFIG_MUSIC, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
    {TYPE_CHECKBOX, CONFIG_GENERAL_ENABLE_MUSIC_RANDOMISE, TR_CONFIG_RANDOMISE_MUSIC, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
    {TYPE_NUMERICAL_RANGE, RANGE_MUSIC_VOLUME, 0, display_text_music_volume, 0, 1, ITEM_BASE_H, 2},
    {TYPE_CHECKBOX, CONFIG_ORIGINAL_ENABLE_SPEECH, TR_CONFIG_SPEECH, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
    {TYPE_NUMERICAL_RANGE, RANGE_SPEECH_VOLUME, 0, display_text_speech_volume, 0, 1, ITEM_BASE_H, 2},
    {TYPE_CHECKBOX, CONFIG_ORIGINAL_ENABLE_SOUND_EFFECTS, TR_CONFIG_EFFECTS, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
    {TYPE_CHECKBOX, CONFIG_UI_MOVE_LEGION_SOUND_SWAP, TR_CONFIG_MOVE_LEGION_SOUND_SWAP, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
    {TYPE_NUMERICAL_RANGE, RANGE_SOUND_EFFECTS_VOLUME, 0, display_text_sound_effects_volume, 0, 1, ITEM_BASE_H, 2},
    {TYPE_CHECKBOX, CONFIG_ORIGINAL_ENABLE_CITY_SOUNDS, TR_CONFIG_CITY_SOUNDS, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
    {TYPE_NUMERICAL_RANGE, RANGE_CITY_SOUNDS_VOLUME, 0, display_text_city_sounds_volume, 0, 1, ITEM_BASE_H, 2},
    {TYPE_CHECKBOX, CONFIG_GENERAL_ENABLE_VIDEO_SOUND, TR_CONFIG_VIDEO_SOUND, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
    {TYPE_NUMERICAL_RANGE, RANGE_VIDEO_VOLUME, 0, display_text_video_volume, 0, 1, ITEM_BASE_H, 2},
    {TYPE_NONE}
};


// ---------- UI ----------------------
static config_widget ui_widgets_by_category[CATEGORY_UI_COUNT][MAX_WIDGETS] = {
    // General
    {
        {TYPE_CHECKBOX, CONFIG_UI_SHOW_INTRO_VIDEO, TR_CONFIG_SHOW_INTRO_VIDEO, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_SIDEBAR_INFO, TR_CONFIG_SIDEBAR_INFO, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_ASK_CONFIRMATION_ON_FILE_OVERWRITE, TR_CONFIG_ASK_CONFIRMATION_ON_FILE_OVERWRITE, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_SHOW_MAX_PROSPERITY, TR_CONFIG_SHOW_MAX_POSSIBLE_PROSPERITY, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_DIGIT_SEPARATOR, TR_CONFIG_DIGIT_SEPARATOR, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_MESSAGE_ALERTS, TR_CONFIG_UI_MESSAGE_ALERTS, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_NONE}
    },
    // Scrolling
    {
        {TYPE_NUMERICAL_DESC, RANGE_SCROLL_SPEED, TR_CONFIG_SCROLL_SPEED, NULL, 0, 1, ITEM_BASE_H, 10},
        {TYPE_NUMERICAL_RANGE, RANGE_SCROLL_SPEED, 0, display_text_scroll_speed, 0, 1, ITEM_BASE_H, 2},
        {TYPE_CHECKBOX, CONFIG_UI_SMOOTH_SCROLLING, TR_CONFIG_SMOOTH_SCROLLING, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_DISABLE_MOUSE_EDGE_SCROLLING, TR_CONFIG_DISABLE_MOUSE_EDGE_SCROLLING, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_DISABLE_RIGHT_CLICK_MAP_DRAG, TR_CONFIG_DISABLE_RIGHT_CLICK_MAP_DRAG, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_INVERSE_MAP_DRAG, TR_CONFIG_UI_INVERSE_MAP_DRAG, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_ANIMATE_TRADE_ROUTES, TR_CONFIG_UI_ANIMATE_TRADE_ROUTES, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_NONE}
    },
    // Building
    {
        {TYPE_CHECKBOX, CONFIG_UI_SHOW_MARKET_RANGE, TR_CONFIG_SHOW_MARKET_RANGE, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_SHOW_CONSTRUCTION_SIZE, TR_CONFIG_SHOW_CONSTRUCTION_SIZE, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_ALWAYS_SHOW_ROTATION_BUTTONS, TR_CONFIG_UI_ALWAYS_SHOW_ROTATION_BUTTONS, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_SHOW_GRID, TR_CONFIG_UI_SHOW_GRID, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_SHOW_PARTIAL_GRID_AROUND_CONSTRUCTION, TR_CONFIG_UI_SHOW_PARTIAL_GRID_AROUND_CONSTRUCTION, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_SHOW_ROAMING_PATH, TR_CONFIG_SHOW_ROAMING_PATH, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_SHOW_DESIRABILITY_RANGE, TR_CONFIG_SHOW_DESIRABILITY_RANGE, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_SHOW_DESIRABILITY_RANGE_ALL, TR_CONFIG_SHOW_DESIRABILITY_RANGE_ALL, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_SHOW_WATER_STRUCTURE_RANGE, TR_CONFIG_SHOW_WATER_STRUCTURE_RANGE, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_SHOW_WATER_STRUCTURE_RANGE_HOUSES, TR_CONFIG_SHOW_WATER_STRUCTURE_RANGE_HOUSES, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_PAVED_ROADS_NEAR_GRANNARIES, TR_CONFIG_ENABLE_PAVED_ROADS_NEAR_GRANARIES, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_BUILD_SHOW_RESERVOIR_RANGES, TR_CONFIG_UI_BUILD_SHOW_RESERVOIR_RANGES, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_NONE}
    },
    // CityView
    {
        {TYPE_CHECKBOX, CONFIG_UI_HIGHLIGHT_LEGIONS, TR_CONFIG_HIGHLIGHT_LEGIONS, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_SHOW_MILITARY_SIDEBAR, TR_CONFIG_SHOW_MILITARY_SIDEBAR, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_HIGHLIGHT_SELECTED_BUILDING, TR_CONFIG_HIGHLIGHT_SELECTED_BUILDING, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_MOVE_SAVINGS_TO_RIGHT, TR_CONFIG_MOVE_SAVINGS_TO_THE_RIGHT, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_DRAW_ASCLEPIUS, TR_CONFIG_DRAW_ASCLEPIUS, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_SHOW_CUSTOM_VARIABLES, TR_CONFIG_SHOW_CUSTOM_VARIABLES, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_CLEAR_WARNINGS_RIGHTCLICK, TR_CONFIG_CLEAR_WARNINGS_RIGHTCLICK, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_CV_BUILD_MENU_ICONS, TR_CONFIG_UI_CV_BUILD_MENU_ICONS, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_CV_CURSOR_SHADOW, TR_CONFIG_UI_CV_CURSOR_SHADOW, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_NONE}
    },
    // Weather
    {
        {TYPE_CHECKBOX, CONFIG_UI_DRAW_CLOUD_SHADOWS, TR_CONFIG_DRAW_CLOUD_SHADOWS, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_DRAW_WEATHER, TR_CONFIG_DRAW_WEATHER, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_UI_WT_ENABLE_SNOW_CENTRAL, TR_CONFIG_UI_WT_ENABLE_SNOW_CENTRAL, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_NONE}
    }
};


// ---------- Difficulty (GAMEPLAY) ----------------------
static config_widget page_difficulty[] = {
    {TYPE_NUMERICAL_DESC, RANGE_DIFFICULTY, TR_CONFIG_DIFFICULTY, NULL, 0, 1, ITEM_BASE_H, 0},
    {TYPE_NUMERICAL_RANGE, RANGE_DIFFICULTY, 0, display_text_difficulty, 0, 1, ITEM_BASE_H, 2},
    {TYPE_CHECKBOX, CONFIG_ORIGINAL_GODS_EFFECTS, TR_CONFIG_GODS_EFFECTS, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
    {TYPE_CHECKBOX, CONFIG_GP_CH_JEALOUS_GODS, TR_CONFIG_JEALOUS_GODS, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
    {TYPE_CHECKBOX, CONFIG_GP_CH_GLOBAL_LABOUR, TR_CONFIG_GLOBAL_LABOUR, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
    {TYPE_CHECKBOX, CONFIG_GP_CH_RETIRE_AT_60, TR_CONFIG_RETIRE_AT_60, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
    {TYPE_CHECKBOX, CONFIG_GP_CH_FIXED_WORKERS, TR_CONFIG_FIXED_WORKERS, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
    {TYPE_CHECKBOX, CONFIG_GP_CH_WOLVES_BLOCK, TR_CONFIG_WOLVES_BLOCK, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
    {TYPE_CHECKBOX, CONFIG_GP_CH_AUTO_KILL_ANIMALS, TR_CONFIG_AUTO_KILL_ANIMALS, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
    {TYPE_CHECKBOX, CONFIG_GP_CH_MULTIPLE_BARRACKS, TR_CONFIG_MULTIPLE_BARRACKS, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
    {TYPE_CHECKBOX, CONFIG_GP_CH_RANDOM_COLLAPSES_TAKE_MONEY, TR_CONFIG_RANDOM_COLLAPSES_TAKE_MONEY, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
    {TYPE_CHECKBOX, CONFIG_GP_CH_DISABLE_INFINITE_WOLVES_SPAWNING, TR_CONFIG_GP_CH_DISABLE_INFINITE_WOLVES_SPAWNING, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
    {TYPE_NUMERICAL_DESC, RANGE_MAX_GRAND_TEMPLES, TR_CONFIG_MAX_GRAND_TEMPLES, NULL, 0, 1, ITEM_BASE_H, 10},
    {TYPE_NUMERICAL_RANGE, RANGE_MAX_GRAND_TEMPLES, 0, display_text_max_grand_temples, 0, 1, ITEM_BASE_H, 2},
    {TYPE_SPACE, 0, 0, NULL, 0, 1, ITEM_BASE_H, 0},
    {TYPE_SPACE, 0, 0, NULL, 0, 1, ITEM_BASE_H, 0}, //two spaces to visually match the padding to the general settings
    {TYPE_NONE}
};

// ---------- City Management ----------------------
static config_widget city_mgmt_widgets_by_category[CATEGORY_CITY_COUNT][MAX_WIDGETS] = {
    // Storage
    {
        {TYPE_CHECKBOX, CONFIG_GP_CH_NO_SUPPLIER_DISTRIBUTION, TR_CONFIG_NO_SUPPLIER_DISTRIBUTION, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_GP_CH_GRANARIES_GET_DOUBLE, TR_CONFIG_GRANARIES_GET_DOUBLE, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_GP_CH_ALLOW_EXPORTING_FROM_GRANARIES, TR_CONFIG_ALLOW_EXPORTING_FROM_GRANARIES, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_GP_CH_FARMS_DELIVER_CLOSE, TR_CONFIG_FARMS_DELIVER_CLOSE, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_GP_CH_WAREHOUSES_DONT_ACCEPT, TR_CONFIG_NOT_ACCEPTING_WAREHOUSES, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_GP_CH_MARKETS_DONT_ACCEPT, TR_CONFIG_NOT_ACCEPTING_MARKETS, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_GP_CH_DELIVER_ONLY_TO_ACCEPTING_GRANARIES, TR_CONFIG_DELIVER_ONLY_TO_ACCEPTING_GRANARIES, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_GP_STORAGE_INCREMENT_4, TR_CONFIG_STORAGE_STEP_4, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_GP_CH_ENABLE_GETTING_WHILE_STOCKPILED, TR_CONFIG_ENABLE_GETTING_WHILE_STOCKPILED, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_GP_CH_CART_DEPOT_ADVANCED, TR_CONFIG_ADVANCED_CART_DEPOT_OPTIONS, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_GP_CH_STORAGE_REQUESTS_RESPECT_MAINTAIN, TR_CONFIG_GP_CH_STORAGE_REQUESTS_RESPECT_MAINTAIN, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_GP_CH_MARKET_RANGE, TR_CONFIG_ENABLE_MARKET_RANGE, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_NONE}
    },
    // Roads
    {
        {TYPE_CHECKBOX, CONFIG_GP_CH_WAREHOUSES_GRANARIES_OVER_ROAD_PLACEMENT, TR_CONFIG_WAREHOUSES_GRANARIES_OVER_ROAD_PLACEMENT, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_GP_CH_ROAMERS_DONT_SKIP_CORNERS, TR_CONFIG_ROAMERS_DONT_SKIP_CORNERS, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_GP_CH_TOWER_SENTRIES_GO_OFFROAD, TR_CONFIG_TOWER_SENTRIES_GO_OFFROAD, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_GP_CARAVANS_MOVE_OFF_ROAD, TR_CONFIG_CARAVANS_MOVE_OFF_ROAD, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_GP_CH_GETTING_GRANARIES_GO_OFFROAD, TR_CONFIG_GETTING_GRANARIES_GO_OFFROAD, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_NONE}
    },
    // Roadblock
    {
        {TYPE_CHECKBOX, CONFIG_GP_CH_GATES_DEFAULT_TO_PASS_ALL_WALKERS, TR_CONFIG_GATES_DEFAULT_TO_PASS_ALL_WALKERS, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_NONE}
    },
    // Housing
    {
        {TYPE_CHECKBOX, CONFIG_GP_CH_ALL_HOUSES_MERGE, TR_CONFIG_ALL_HOUSES_MERGE, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_GP_CH_PATRICIAN_DEVOLUTION_FIX, TR_CONFIG_PATRICIAN_DEVOLUTION_FIX, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_GP_CH_HOUSES_DONT_EXPAND_INTO_GARDENS, TR_CONFIG_HOUSES_DONT_EXPAND_INTO_GARDENS, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_CHECKBOX, CONFIG_GP_CH_HOUSING_PRE_MERGE_VACANT_LOTS, TR_CONFIG_GP_CH_HOUSING_PRE_MERGE_VACANT_LOTS, NULL, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN},
        {TYPE_NONE}
    }
};

//more typedefs:

typedef struct {
    int width;
    int height;
} resolution;

typedef struct {
    int x;
    int width_blocks;
    int min;
    int max;
    int step;
    int *value;
} numerical_range_widget;

typedef struct {
    list_box_type *lb;
    int count;
    int page_id;
    int *selected_ref; //  points into selected_categories
} category_page_properties;

//    Widget ops (measure / draw / input)
typedef struct {
    void (*measure)(const config_widget *, int avail_text_w, int *out_h);
    void (*draw_bg)(const config_widget *, int x, int y, int avail_text_w);
    void (*draw_fg)(const config_widget *, int x, int y, int avail_text_w, int focused);
    int  (*handle_input)(const config_widget *, int x, int y, int avail_text_w, const mouse *m, unsigned *focused);
} widget_ops;

typedef struct {
    int x;
    int text_w;
} content_span;

static scrollbar_type scrollbar = { 580,ITEM_Y_OFFSET,ITEM_BASE_H * NUM_VISIBLE_FALLBACK,
    560,NUM_VISIBLE_FALLBACK,on_scroll, 0, 4
};

static const resolution resolutions[] = {
    {  640,  480 }, {  800,  600 }, { 1024,  768 }, { 1280,  720 },
    { 1280,  800 }, { 1280, 1024 }, { 1360,  768 }, { 1366,  768 },
    { 1440,  900 }, { 1536,  864 }, { 1600,  900 }, { 1680, 1050 },
    { 1920, 1080 }, { 1920, 1200 }, { 2048, 1152 }, { 2560, 1080 },
    { 2560, 1440 }, { 3440, 1440 }, { 3840, 2160 }
};
static resolution available_resolutions[sizeof(resolutions) / sizeof(resolution) + 2];

static const unsigned char page_is_category_helper[CONFIG_PAGES] = {
    // pages with category submenu
    [CONFIG_PAGE_GENERAL] = 0,
    [CONFIG_PAGE_UI_CHANGES] = 1,
    [CONFIG_PAGE_GAMEPLAY_CHANGES] = 0,
    [CONFIG_PAGE_CITY_MANAGEMENT_CHANGES] = 1,
};

// buttons helpers
static void button_language_select(const generic_button *button);
static void button_edit_player_name(const generic_button *button);
static void button_change_user_directory(const generic_button *button);
static generic_button select_buttons[] = {
    {225, 0, 200, 24, button_language_select},
    {225, 0, 200, 24, button_edit_player_name},
    {225, 0, 200, 24, button_change_user_directory},
};

//  Numeric ranges (slider bar geometry + value binding populated in set_range_values()).

static numerical_range_widget ranges[] = {
    { 50, 30,   0,  TOTAL_GAME_SPEEDS - 1,  1, 0},   //  game speed index, needs conversion from count to 0-based index
    { 98, 27,   0,   0,  1, 0},   //  resolution index
    { 50, 30,  50, 500,  5, 0},   //  display scale %
    { 50, 30, 100, 200, 50, 0},   //  cursor scale %
    {130, 25,   0, 100,  1, 0},   //  master volume %
    {130, 25,   0, 100,  1, 0},   //  music %
    {130, 25,   0, 100,  1, 0},   //  speech %
    {130, 25,   0, 100,  1, 0},   //  effects %
    {130, 25,   0, 100,  1, 0},   //  city %
    {130, 25,   0, 100,  1, 0},   //  video %
    { 50, 18,   0, 100, 10, 0},   //  scroll speed %
    {146, 24,   0,   4,  1, 0},   //  difficulty enum index (0..4)
    { 50, 30,   0,   5,  1, 0},   //  max grand temples
    { 50, 30,   1,  20,  1, 0},   //  autosave slots
    { 50, 30,   0,  TOTAL_GAME_SPEEDS - 1,  1, 0},   //  default game speed index

};

//  Bottom buttons & page tabs

static void button_hotkeys(const generic_button *button);
static void button_reset_defaults(const generic_button *button);
static void button_close(const generic_button *button);
static void button_page(const generic_button *button);

static generic_button bottom_buttons[NUM_BOTTOM_BUTTONS] = {
    {  20, 436,  120, 30, button_hotkeys, 0, 0, TR_BUTTON_CONFIGURE_HOTKEYS },
    { 170, 436, 150, 30, button_reset_defaults, 0, 0, TR_BUTTON_RESET_DEFAULTS },
    { 330, 436,  90, 30, button_close, 0, 0, TR_BUTTON_CANCEL },
    { 430, 436,  90, 30, button_close, 0, 1, TR_BUTTON_OK },
    { 530, 436,  90, 30, button_close, 0, 2, TR_OPTION_MENU_APPLY }
};

static generic_button page_buttons[] = {
    { 0, 48, 0, 30, button_page, 0, 0 },
    { 0, 48, 0, 30, button_page, 0, 1 },
    { 0, 48, 0, 30, button_page, 0, 2 },
    { 0, 48, 0, 30, button_page, 0, 3 },
};

static translation_key page_names[CONFIG_PAGES] = {
    TR_CONFIG_HEADER_GENERAL,
    TR_CONFIG_HEADER_UI_CHANGES,
    TR_CONFIG_HEADER_GAMEPLAY_CHANGES,
    TR_CONFIG_HEADER_CITY_MANAGEMENT_CHANGES
};

static const translation_key ui_category_keys[CATEGORY_UI_COUNT] = {
    TR_CONFIG_CATEGORY_GENERAL,
    TR_CONFIG_CATEGORY_UI_MAP,
    TR_CONFIG_CATEGORY_UI_BUILDING,
    TR_CONFIG_CATEGORY_UI_CITY,
    TR_CONFIG_CATEGORY_UI_WEATHER,
};

static const translation_key city_mgmt_category_keys[CATEGORY_CITY_COUNT] = {
    TR_CONFIG_CATEGORY_MANAGEMENT_STORAGE,
    TR_CONFIG_CATEGORY_MANAGEMENT_ROADS,
    TR_CONFIG_CATEGORY_MANAGEMENT_ROADBLOCKS,
    TR_CONFIG_CATEGORY_MANAGEMENT_HOUSING,
};


static struct {
    ui_config_category ui_category;
    city_management_category city_mgmt_category;
} selected_categories = {
    CATEGORY_UI_GENERAL, CATEGORY_CITY_MANAGEMENT_STORAGE
};

static list_box_type ui_list_box;
static list_box_type city_mgmt_list_box;

static void draw_list_box_item(const list_box_item *item);
static void handle_list_box_select(unsigned int index, int is_double_click);
static void list_box_tooltip(const list_box_item *item, tooltip_context *c);
//    Data model & helpers

static struct {
    unsigned int page;
    unsigned int page_focus_button;
    unsigned int bottom_focus_button;
    unsigned int focus_button;
    int show_background_image;
    int has_changes;
    int reload_cursors;
    struct {
        int original_value;
        int new_value;
        int (*change_action)(int key);
    } config_values[CONFIG_MAX_ALL];
    struct {
        char original_value[CONFIG_STRING_VALUE_MAX];
        char new_value[CONFIG_STRING_VALUE_MAX];
        int (*change_action)(int key);
    } config_string_values[CONFIG_STRING_MAX_ALL];
    struct {
        const uint8_t *options[MAX_LANGUAGE_DIRS];
        uint8_t data[MAX_LANGUAGE_DIRS][CONFIG_STRING_VALUE_MAX];
        char    data_utf8[MAX_LANGUAGE_DIRS][CONFIG_STRING_VALUE_MAX];
        int total;
        int selected;
    } language_options;
    uint8_t player_name[PLAYER_NAME_LENGTH];
    uint8_t display_text[32];
    uint8_t volume_text[64];
    uint8_t *volume_offset;
    int active_numerical_range; //  RANGE_* + 1 while dragging, 0 otherwise
    int graphics_behind_tab[CONFIG_PAGES];
    //  layout cache for the current page
    struct {
        const config_widget *rows[MAX_WIDGETS];
        int count;               //  total rows for page (enabled only)
        int visible_from;        //  equals scrollbar.scroll_position
        int visible_to;          //  exclusive end index
        int y[MAX_WIDGETS];      //  y per visible row
        int h[MAX_WIDGETS];      //  h per visible row
        int has_scrollbar;
    } layout;

} data;

//    Generic text helpers
static int one_line_ml_height(font_t font)
{
    int h = font_definition_for(font)->line_height;
    if (h < 11) {
        h = 11;
    }
    return h + 5; //  matches text_draw_multiline() per-line advance

}
static uint8_t *percentage_string(uint8_t *s, int p)
{
    int o = string_from_int(s, p, 0);
    s[o] = '%'; s[o + 1] = 0;
    return s;
}

//    Config get/set action functions (kept, some grouped)

static int config_change_basic(int key)
{
    if (key < CONFIG_MAX_ENTRIES) {
        config_set(key, data.config_values[key].new_value);
    }
    data.config_values[key].original_value = data.config_values[key].new_value;
    return 1;
}
static int config_change_string_basic(int key)
{
    config_set_string(key, data.config_string_values[key].new_value);
    memcpy(data.config_string_values[key].original_value, data.config_string_values[key].new_value,
           CONFIG_STRING_VALUE_MAX);
    return 1;
}

// specialized functions for config options that require more than just value

static int config_change_game_speed(int key)
{
    config_change_basic(key);
    int target_speed = game_speed_get_speed(data.config_values[key].new_value);
    setting_set_game_speed(target_speed);
    return 1;
}

static int config_change_fullscreen(int key)
{
    if (!system_is_fullscreen_only()) {
        system_set_fullscreen(data.config_values[key].new_value);
        if (data.config_values[key].new_value) {
            setting_set_display(1, screen_width(), screen_height());
        }
        config_change_basic(key);
    }
    return 1;
}
static int config_change_display_resolution(int key)
{
    if (!system_is_fullscreen_only()) {
        const resolution *r = &available_resolutions[data.config_values[key].new_value];
        if (!setting_fullscreen()) {
            system_resize(r->width, r->height);
        } else {
            setting_set_display(0, r->width, r->height);
            setting_set_display(1, r->width, r->height);
        }
        config_change_basic(key);
    }
    return 1;
}
static int config_change_display_scale(int key)
{
    data.config_values[key].new_value = system_scale_display(data.config_values[key].new_value);
    return config_change_basic(key);
}
static void restart_cursors(void)
{
    if (data.reload_cursors) {
        system_init_cursors(config_get(CONFIG_SCREEN_CURSOR_SCALE));
        data.reload_cursors = 0;
    }
}
static int config_change_cursors(int key)
{
    config_change_basic(key);
    data.reload_cursors = 1;
    return 1;
}

//  Audio  functions

static int config_enable_audio(int key)
{
    config_change_basic(key);
    if (data.config_values[key].new_value) {
        if (data.show_background_image) {
            sound_music_play_intro();
        } else {
            sound_music_stop();
            sound_music_update(1);
        }
    } else {
        sound_music_stop();
        sound_speech_stop();
    }
    return 1;
}
static int config_set_master_volume(int key)
{
    config_change_basic(key);
    sound_music_set_volume(setting_sound(SOUND_TYPE_MUSIC)->volume);
    sound_speech_set_volume(setting_sound(SOUND_TYPE_SPEECH)->volume);
    sound_effect_set_volume(setting_sound(SOUND_TYPE_EFFECTS)->volume);
    sound_city_set_volume(setting_sound(SOUND_TYPE_CITY)->volume);
    return 1;
}
static int config_enable_music(int key)
{
    config_change_basic(key);
    if (setting_sound_is_enabled(SOUND_TYPE_MUSIC) != data.config_values[key].new_value) {
        setting_toggle_sound_enabled(SOUND_TYPE_MUSIC);
    }

    if (data.config_values[key].new_value) {
        if (data.show_background_image) {
            sound_music_play_intro();
        } else {
            sound_music_stop();
            sound_music_update(1);
        }
    } else {
        sound_music_stop();
    }
    return 1;
}
static int config_enable_music_randomise(int key)
{
    int ok = config_change_basic(key);
    sound_music_update(1);
    return ok;
}
static int config_set_music_volume(int key)
{
    config_change_basic(key);
    setting_set_sound_volume(SOUND_TYPE_MUSIC, data.config_values[key].new_value);
    sound_music_set_volume(setting_sound(SOUND_TYPE_MUSIC)->volume);
    return 1;
}
static int config_enable_speech(int key)
{
    config_change_basic(key);
    if (setting_sound_is_enabled(SOUND_TYPE_SPEECH) != data.config_values[key].new_value) {
        setting_toggle_sound_enabled(SOUND_TYPE_SPEECH);
    }

    if (!data.config_values[key].new_value) {
        sound_speech_stop();
    }
    return 1;
}
static int config_set_speech_volume(int key)
{
    config_change_basic(key);
    setting_set_sound_volume(SOUND_TYPE_SPEECH, data.config_values[key].new_value);
    sound_speech_set_volume(setting_sound(SOUND_TYPE_SPEECH)->volume);
    return 1;
}
static int config_enable_effects(int key)
{
    config_change_basic(key);
    if (setting_sound_is_enabled(SOUND_TYPE_EFFECTS) != data.config_values[key].new_value) {
        setting_toggle_sound_enabled(SOUND_TYPE_EFFECTS);
    }

    return 1;
}
static int config_set_effects_volume(int key)
{
    config_change_basic(key);
    setting_set_sound_volume(SOUND_TYPE_EFFECTS, data.config_values[key].new_value);
    sound_effect_set_volume(setting_sound(SOUND_TYPE_EFFECTS)->volume);
    return 1;
}
static int config_enable_city_sounds(int key)
{
    config_change_basic(key);
    if (setting_sound_is_enabled(SOUND_TYPE_CITY) != data.config_values[key].new_value) {
        setting_toggle_sound_enabled(SOUND_TYPE_CITY);
    }

    return 1;
}
static int config_set_city_sounds_volume(int key)
{
    config_change_basic(key);
    setting_set_sound_volume(SOUND_TYPE_CITY, data.config_values[key].new_value);
    sound_city_set_volume(setting_sound(SOUND_TYPE_CITY)->volume);
    return 1;
}

static int config_mouse_unlock_fullscreen(int key)
{
    config_change_basic(key);
    platform_screen_update_window_grab();
    window_invalidate();
    return 1;
}

//  Scroll

static int config_change_scroll_speed(int key)
{
    config_change_basic(key);
    while (setting_scroll_speed() > data.config_values[key].new_value) {
        setting_decrease_scroll_speed();
    }
    while (setting_scroll_speed() < data.config_values[key].new_value) {
        setting_increase_scroll_speed();
    }
    return 1;
}

// Difficulty

static int config_set_difficulty(int key)
{
    config_change_basic(key);
    while (setting_difficulty() > data.config_values[key].new_value) setting_decrease_difficulty();
    while (setting_difficulty() < data.config_values[key].new_value) setting_increase_difficulty();
    return 1;
}
static int config_enable_gods_effects(int key)
{
    config_change_basic(key);
    if (setting_gods_enabled() != data.config_values[key].new_value) setting_toggle_gods_enabled();
    return 1;
}

//  Strings

static int config_change_string_language(int key)
{
    config_set_string(CONFIG_STRING_UI_LANGUAGE_DIR, data.config_string_values[key].new_value);
    if (!game_reload_language()) {
        config_set_string(CONFIG_STRING_UI_LANGUAGE_DIR, data.config_string_values[key].original_value);
        game_reload_language();
        window_plain_message_dialog_show(TR_INVALID_LANGUAGE_TITLE, TR_INVALID_LANGUAGE_MESSAGE, 1);
        return 0;
    }
    char title[100];
    encoding_to_utf8(lang_get_string(9, 0), title, 100, 0);
    system_change_window_title(title);
    memcpy(data.config_string_values[key].original_value, data.config_string_values[key].new_value,
           CONFIG_STRING_VALUE_MAX);
    string_copy(translation_for(TR_CONFIG_LANGUAGE_DEFAULT), data.language_options.data[0], CONFIG_STRING_VALUE_MAX);
    data.volume_offset = string_copy(translation_for(TR_CONFIG_VOLUME), data.volume_text, 63);
    data.volume_offset = string_copy(string_from_ascii(" "), data.volume_offset,
                                     (int) (data.volume_offset - data.volume_text - 1));
    return 1;
}
static int config_change_string_player_name(int key)
{
    uint8_t player_name[PLAYER_NAME_LENGTH];
    encoding_from_utf8(data.config_string_values[key].new_value, player_name, PLAYER_NAME_LENGTH);
    setting_set_player_name(player_name);
    memcpy(data.config_string_values[key].original_value, data.config_string_values[key].new_value,
           CONFIG_STRING_VALUE_MAX);
    return 1;
}

//Display text functions 

static uint8_t *percent_buf(int key)
{
    return percentage_string(data.display_text, data.config_values[key].new_value);
}
static const uint8_t *display_text_display_scale(void)
{
    return percent_buf(CONFIG_SCREEN_DISPLAY_SCALE);
}
static const uint8_t *display_text_cursor_scale(void)
{
    return percent_buf(CONFIG_SCREEN_CURSOR_SCALE);
}
static const uint8_t *display_text_scroll_speed(void)
{
    return percent_buf(CONFIG_ORIGINAL_SCROLL_SPEED);
}
static const uint8_t *display_text_master_volume(void)
{
    percentage_string(data.volume_offset, data.config_values[CONFIG_GENERAL_MASTER_VOLUME].new_value); return data.volume_text;
}
static const uint8_t *display_text_music_volume(void)
{
    percentage_string(data.volume_offset, data.config_values[CONFIG_ORIGINAL_MUSIC_VOLUME].new_value); return data.volume_text;
}
static const uint8_t *display_text_speech_volume(void)
{
    percentage_string(data.volume_offset, data.config_values[CONFIG_ORIGINAL_SPEECH_VOLUME].new_value); return data.volume_text;
}
static const uint8_t *display_text_sound_effects_volume(void)
{
    percentage_string(data.volume_offset, data.config_values[CONFIG_ORIGINAL_SOUND_EFFECTS_VOLUME].new_value); return data.volume_text;
}
static const uint8_t *display_text_city_sounds_volume(void)
{
    percentage_string(data.volume_offset, data.config_values[CONFIG_ORIGINAL_CITY_SOUNDS_VOLUME].new_value); return data.volume_text;
}
static const uint8_t *display_text_video_volume(void)
{
    percentage_string(data.volume_offset, data.config_values[CONFIG_GENERAL_VIDEO_VOLUME].new_value); return data.volume_text;
}
static const uint8_t *display_text_language(void)
{
    return data.language_options.options[data.language_options.selected];
}
static const uint8_t *display_text_user_directory(void)
{
    return translation_for(TR_USER_DIRECTORIES_WINDOW_TITLE);
}
static const uint8_t *display_text_player_name(void)
{
    return data.player_name;
}
static const uint8_t *display_text_game_speed(void)
{
    return percentage_string(data.display_text, game_speed_get_speed(data.config_values[CONFIG_ORIGINAL_GAME_SPEED].new_value));
}
static const uint8_t *display_text_resolution(void)
{
    uint8_t *str = data.display_text;
    resolution *r = &available_resolutions[data.config_values[CONFIG_ORIGINAL_WINDOWED_RESOLUTION].new_value];
    str += string_from_int(str, r->width, 0);
    str = string_copy(string_from_ascii("x"), str, 5);
    string_from_int(str, r->height, 0);
    return data.display_text;
}
static const uint8_t *display_text_difficulty(void)
{
    return lang_get_string(153, data.config_values[CONFIG_ORIGINAL_DIFFICULTY].new_value + 1);
}
static const uint8_t *display_text_max_grand_temples(void)
{
    string_from_int(data.display_text, data.config_values[CONFIG_GP_CH_MAX_GRAND_TEMPLES].new_value, 0);
    return data.display_text;
}
static const uint8_t *display_text_autosave_slots(void)
{
    string_from_int(data.display_text, data.config_values[CONFIG_GP_CH_MAX_AUTOSAVE_SLOTS].new_value, 0);
    return data.display_text;
}
static const uint8_t *display_text_default_game_speed(void)
{
    return percentage_string(data.display_text, game_speed_get_speed(data.config_values[CONFIG_GP_CH_DEFAULT_GAME_SPEED].new_value));
}

//    Range value binding, custom change-action table, init

static void set_range_values(void)
{
    ranges[RANGE_GAME_SPEED].value = &data.config_values[CONFIG_ORIGINAL_GAME_SPEED].new_value;
    ranges[RANGE_RESOLUTION].value = &data.config_values[CONFIG_ORIGINAL_WINDOWED_RESOLUTION].new_value;
    ranges[RANGE_DISPLAY_SCALE].value = &data.config_values[CONFIG_SCREEN_DISPLAY_SCALE].new_value;
    ranges[RANGE_CURSOR_SCALE].value = &data.config_values[CONFIG_SCREEN_CURSOR_SCALE].new_value;
    ranges[RANGE_MASTER_VOLUME].value = &data.config_values[CONFIG_GENERAL_MASTER_VOLUME].new_value;
    ranges[RANGE_MUSIC_VOLUME].value = &data.config_values[CONFIG_ORIGINAL_MUSIC_VOLUME].new_value;
    ranges[RANGE_SPEECH_VOLUME].value = &data.config_values[CONFIG_ORIGINAL_SPEECH_VOLUME].new_value;
    ranges[RANGE_SOUND_EFFECTS_VOLUME].value = &data.config_values[CONFIG_ORIGINAL_SOUND_EFFECTS_VOLUME].new_value;
    ranges[RANGE_CITY_SOUNDS_VOLUME].value = &data.config_values[CONFIG_ORIGINAL_CITY_SOUNDS_VOLUME].new_value;
    ranges[RANGE_VIDEO_VOLUME].value = &data.config_values[CONFIG_GENERAL_VIDEO_VOLUME].new_value;
    ranges[RANGE_SCROLL_SPEED].value = &data.config_values[CONFIG_ORIGINAL_SCROLL_SPEED].new_value;
    ranges[RANGE_DIFFICULTY].value = &data.config_values[CONFIG_ORIGINAL_DIFFICULTY].new_value;
    ranges[RANGE_MAX_GRAND_TEMPLES].value = &data.config_values[CONFIG_GP_CH_MAX_GRAND_TEMPLES].new_value;
    ranges[RANGE_MAX_AUTOSAVE_SLOTS].value = &data.config_values[CONFIG_GP_CH_MAX_AUTOSAVE_SLOTS].new_value;
    ranges[RANGE_DEFAULT_GAME_SPEED].value = &data.config_values[CONFIG_GP_CH_DEFAULT_GAME_SPEED].new_value;
}
static void set_custom_config_changes(void)
{
    //  default

    for (int i = 0; i < CONFIG_MAX_ALL; ++i) {
        data.config_values[i].change_action = config_change_basic;
    }
    for (int i = 0; i < CONFIG_STRING_MAX_ALL; ++i) {
        data.config_string_values[i].change_action = config_change_string_basic;
    }

    //  strings

    data.config_string_values[CONFIG_STRING_UI_LANGUAGE_DIR].change_action = config_change_string_language;
    data.config_string_values[CONFIG_STRING_ORIGINAL_PLAYER_NAME].change_action = config_change_string_player_name;

    //  display / input

    data.config_values[CONFIG_ORIGINAL_FULLSCREEN].change_action = config_change_fullscreen;
    data.config_values[CONFIG_ORIGINAL_WINDOWED_RESOLUTION].change_action = config_change_display_resolution;
    data.config_values[CONFIG_SCREEN_DISPLAY_SCALE].change_action = config_change_display_scale;
    data.config_values[CONFIG_SCREEN_CURSOR_SCALE].change_action = config_change_cursors;
    data.config_values[CONFIG_SCREEN_COLOR_CURSORS].change_action = config_change_cursors;
    data.config_values[CONFIG_GENERAL_UNLOCK_MOUSE].change_action = config_mouse_unlock_fullscreen;
    data.config_values[CONFIG_ORIGINAL_GAME_SPEED].change_action = config_change_game_speed;
    data.config_values[CONFIG_GP_CH_DEFAULT_GAME_SPEED].change_action = config_change_basic;
    //  audio

    data.config_values[CONFIG_GENERAL_ENABLE_AUDIO].change_action = config_enable_audio;
    data.config_values[CONFIG_GENERAL_MASTER_VOLUME].change_action = config_set_master_volume;
    data.config_values[CONFIG_ORIGINAL_ENABLE_MUSIC].change_action = config_enable_music;
    data.config_values[CONFIG_GENERAL_ENABLE_MUSIC_RANDOMISE].change_action = config_enable_music_randomise;
    data.config_values[CONFIG_ORIGINAL_MUSIC_VOLUME].change_action = config_set_music_volume;
    data.config_values[CONFIG_ORIGINAL_ENABLE_SPEECH].change_action = config_enable_speech;
    data.config_values[CONFIG_ORIGINAL_SPEECH_VOLUME].change_action = config_set_speech_volume;
    data.config_values[CONFIG_ORIGINAL_ENABLE_SOUND_EFFECTS].change_action = config_enable_effects;
    data.config_values[CONFIG_ORIGINAL_SOUND_EFFECTS_VOLUME].change_action = config_set_effects_volume;
    data.config_values[CONFIG_ORIGINAL_ENABLE_CITY_SOUNDS].change_action = config_enable_city_sounds;
    data.config_values[CONFIG_ORIGINAL_CITY_SOUNDS_VOLUME].change_action = config_set_city_sounds_volume;

    //  gameplay

    data.config_values[CONFIG_ORIGINAL_SCROLL_SPEED].change_action = config_change_scroll_speed;
    data.config_values[CONFIG_ORIGINAL_DIFFICULTY].change_action = config_set_difficulty;
    data.config_values[CONFIG_ORIGINAL_GODS_EFFECTS].change_action = config_enable_gods_effects;
}

static void set_player_name_width(void)
{
    int width = text_get_width(data.player_name, FONT_NORMAL_BLACK) + 16;
    if (width < 200) {
        width = 200;
    } else if (width > 322) {
        width = 322;
        text_ellipsize(data.player_name, FONT_NORMAL_BLACK, width - 16);
    }
    select_buttons[SELECT_PLAYER_NAME].width = width;
}

static void fetch_original_config_values(void)
{
    data.config_values[CONFIG_ORIGINAL_GAME_SPEED].original_value = game_speed_get_index(setting_game_speed());
    data.config_values[CONFIG_ORIGINAL_GAME_SPEED].new_value = game_speed_get_index(setting_game_speed());
    data.config_values[CONFIG_ORIGINAL_ENABLE_MUSIC].original_value = setting_sound(SOUND_TYPE_MUSIC)->enabled;
    data.config_values[CONFIG_ORIGINAL_ENABLE_MUSIC].new_value = setting_sound(SOUND_TYPE_MUSIC)->enabled;
    data.config_values[CONFIG_ORIGINAL_MUSIC_VOLUME].original_value = setting_sound(SOUND_TYPE_MUSIC)->volume;
    data.config_values[CONFIG_ORIGINAL_MUSIC_VOLUME].new_value = setting_sound(SOUND_TYPE_MUSIC)->volume;

    data.config_values[CONFIG_ORIGINAL_ENABLE_SPEECH].original_value = setting_sound(SOUND_TYPE_SPEECH)->enabled;
    data.config_values[CONFIG_ORIGINAL_ENABLE_SPEECH].new_value = setting_sound(SOUND_TYPE_SPEECH)->enabled;
    data.config_values[CONFIG_ORIGINAL_SPEECH_VOLUME].original_value = setting_sound(SOUND_TYPE_SPEECH)->volume;
    data.config_values[CONFIG_ORIGINAL_SPEECH_VOLUME].new_value = setting_sound(SOUND_TYPE_SPEECH)->volume;

    data.config_values[CONFIG_ORIGINAL_ENABLE_SOUND_EFFECTS].original_value = setting_sound(SOUND_TYPE_EFFECTS)->enabled;
    data.config_values[CONFIG_ORIGINAL_ENABLE_SOUND_EFFECTS].new_value = setting_sound(SOUND_TYPE_EFFECTS)->enabled;
    data.config_values[CONFIG_ORIGINAL_SOUND_EFFECTS_VOLUME].original_value = setting_sound(SOUND_TYPE_EFFECTS)->volume;
    data.config_values[CONFIG_ORIGINAL_SOUND_EFFECTS_VOLUME].new_value = setting_sound(SOUND_TYPE_EFFECTS)->volume;

    data.config_values[CONFIG_ORIGINAL_ENABLE_CITY_SOUNDS].original_value = setting_sound(SOUND_TYPE_CITY)->enabled;
    data.config_values[CONFIG_ORIGINAL_ENABLE_CITY_SOUNDS].new_value = setting_sound(SOUND_TYPE_CITY)->enabled;
    data.config_values[CONFIG_ORIGINAL_CITY_SOUNDS_VOLUME].original_value = setting_sound(SOUND_TYPE_CITY)->volume;
    data.config_values[CONFIG_ORIGINAL_CITY_SOUNDS_VOLUME].new_value = setting_sound(SOUND_TYPE_CITY)->volume;

    data.config_values[CONFIG_ORIGINAL_SCROLL_SPEED].original_value = setting_scroll_speed();
    data.config_values[CONFIG_ORIGINAL_SCROLL_SPEED].new_value = setting_scroll_speed();

    data.config_values[CONFIG_ORIGINAL_DIFFICULTY].original_value = setting_difficulty();
    data.config_values[CONFIG_ORIGINAL_DIFFICULTY].new_value = setting_difficulty();

    data.config_values[CONFIG_ORIGINAL_GODS_EFFECTS].original_value = setting_gods_enabled();
    data.config_values[CONFIG_ORIGINAL_GODS_EFFECTS].new_value = setting_gods_enabled();

    //  player name

    const uint8_t *player_name = setting_player_name();
    if (!string_length(player_name)) player_name = lang_get_string(9, 5);
    string_copy(player_name, data.player_name, PLAYER_NAME_LENGTH);
    encoding_to_utf8(data.player_name, data.config_string_values[CONFIG_STRING_ORIGINAL_PLAYER_NAME].original_value,
                     PLAYER_NAME_LENGTH, 0);
    snprintf(data.config_string_values[CONFIG_STRING_ORIGINAL_PLAYER_NAME].new_value, CONFIG_STRING_VALUE_MAX, "%s",
             data.config_string_values[CONFIG_STRING_ORIGINAL_PLAYER_NAME].original_value);

    set_player_name_width();
}

static void update_scale(void)
{
    int min_scale = 0, max_scale = 0;
    if (system_can_scale_display(&min_scale, &max_scale)) {
        ranges[RANGE_DISPLAY_SCALE].min = min_scale;
        ranges[RANGE_DISPLAY_SCALE].max = max_scale;
        if (*ranges[RANGE_DISPLAY_SCALE].value > max_scale) {
            *ranges[RANGE_DISPLAY_SCALE].value = max_scale;
        }

    }
}
static void calculate_available_resolutions_and_fullscreen(void)
{
    if (system_is_fullscreen_only()) {
        return;
    }
    memset(available_resolutions, 0, sizeof(available_resolutions));
    int resolution_index = 0;
    int display_resolution_index = -1;
    int current_resolution_index = -1;

    resolution max;
    system_get_max_resolution(&max.width, &max.height);

    static int old_width;
    static int old_height;
    static int old_fullscreen = -1;

    int width = screen_width();
    int height = screen_height();

    for (size_t i = 0; i < sizeof(resolutions) / sizeof(resolution); i++) {
        if (resolutions[i].width == width && resolutions[i].height == height) {
            current_resolution_index = resolution_index;
        } else if (current_resolution_index == -1 && (resolutions[i].width > width ||
            (resolutions[i].width == width && resolutions[i].height > height))) {
            available_resolutions[resolution_index].width = width;
            available_resolutions[resolution_index].height = height;
            current_resolution_index = resolution_index;
            resolution_index++;
        }
        if (resolutions[i].width == max.width && resolutions[i].height == max.height) {
            display_resolution_index = resolution_index;
        } else if (resolutions[i].width > max.width || resolutions[i].height > max.height) {
            continue;
        }
        available_resolutions[resolution_index++] = resolutions[i];
    }
    if (display_resolution_index == -1) {
        available_resolutions[resolution_index++] = max;
    }

    ranges[RANGE_RESOLUTION].max = resolution_index - 1;

    if (!setting_fullscreen() && (old_width != width || old_height != height)) {
        data.config_values[CONFIG_ORIGINAL_WINDOWED_RESOLUTION].original_value = current_resolution_index;
        data.config_values[CONFIG_ORIGINAL_WINDOWED_RESOLUTION].new_value = current_resolution_index;
        old_width = width; old_height = height;
    }
    if (setting_fullscreen() != old_fullscreen) {
        data.config_values[CONFIG_ORIGINAL_FULLSCREEN].original_value = setting_fullscreen();
        data.config_values[CONFIG_ORIGINAL_FULLSCREEN].new_value = setting_fullscreen();
        old_fullscreen = setting_fullscreen();
    }
}

static inline int config_changed(int key)
{
    return data.config_values[key].original_value != data.config_values[key].new_value;
}
static inline int config_string_changed(int key)
{
    return strcmp(data.config_string_values[key].original_value, data.config_string_values[key].new_value) != 0;
}

static inline void update_has_changes(void)
{
    update_scale();
    calculate_available_resolutions_and_fullscreen();
    data.has_changes = 0;
    for (int i = 0; i < CONFIG_STRING_MAX_ALL && !data.has_changes; i++) {
        if (config_string_changed(i)) {
            data.has_changes = 1;
        }
    }
    for (int i = 0; i < CONFIG_MAX_ALL && !data.has_changes; i++) {
        if (config_changed(i)) {
            data.has_changes = 1;
        }
    }
}

//    buttons actions

static void set_language(int index)
{
    const char *dir = index == 0 ? "" : data.language_options.data_utf8[index];
    snprintf(data.config_string_values[CONFIG_STRING_UI_LANGUAGE_DIR].new_value, CONFIG_STRING_VALUE_MAX, "%s", dir);
    data.language_options.selected = index;
}
static void button_language_select(const generic_button *button)
{
    int height = button->parameter1;
    window_select_list_show_text(screen_dialog_offset_x(), screen_dialog_offset_y() + height, button,
        data.language_options.options, data.language_options.total, set_language);
}
static void set_player_name(const uint8_t *name)
{
    if (!string_length(name)) {
        name = lang_get_string(9, 5);
    }
    string_copy(name, data.player_name, PLAYER_NAME_LENGTH);
    encoding_to_utf8(name, data.config_string_values[CONFIG_STRING_ORIGINAL_PLAYER_NAME].new_value, PLAYER_NAME_LENGTH, 0);
    set_player_name_width();
    window_invalidate();
}
static void button_edit_player_name(const generic_button *button)
{
    uint8_t player_name[PLAYER_NAME_LENGTH];
    encoding_from_utf8(data.config_string_values[CONFIG_STRING_ORIGINAL_PLAYER_NAME].new_value, player_name, PLAYER_NAME_LENGTH);
    window_text_input_show(lang_get_string(31, 0), lang_get_string(9, 5), player_name, PLAYER_NAME_LENGTH, set_player_name);
}
static void button_change_user_directory(const generic_button *button)
{
    window_user_path_setup_show(0);
}

// Category list boxes 
static int page_is_category(unsigned int page)
{
    return (page < CONFIG_PAGES) ? page_is_category_helper[page] : 0;
}

static category_page_properties current_category_properties(void)
{
    category_page_properties properties;
    if (data.page == CONFIG_PAGE_UI_CHANGES) {
        properties = (category_page_properties) { &ui_list_box, CATEGORY_UI_COUNT, CONFIG_PAGE_UI_CHANGES,
                (int *) &selected_categories.ui_category };
    } else {
        properties = (category_page_properties) { &city_mgmt_list_box, CATEGORY_CITY_COUNT,
             CONFIG_PAGE_CITY_MANAGEMENT_CHANGES,  (int *) &selected_categories.city_mgmt_category };
    }
    return properties;
}

static void init_list_boxes(void)
{
    //  UI
    ui_list_box.x = LIST_BOX_X;
    ui_list_box.y = LIST_BOX_Y;
    ui_list_box.width_blocks = LIST_BOX_WIDTH / BLOCK_SIZE;
    ui_list_box.height_blocks = LIST_BOX_HEIGHT / BLOCK_SIZE;
    ui_list_box.item_height = LIST_BOX_ITEM_H;
    ui_list_box.draw_inner_panel = 1;
    ui_list_box.extend_to_hidden_scrollbar = 1;
    ui_list_box.decorate_scrollbar = 1;
    ui_list_box.draw_item = draw_list_box_item;
    ui_list_box.on_select = handle_list_box_select;
    ui_list_box.handle_tooltip = list_box_tooltip;
    list_box_init(&ui_list_box, CATEGORY_UI_COUNT);

    //  City management
    city_mgmt_list_box = ui_list_box; //  copy layout
    list_box_init(&city_mgmt_list_box, CATEGORY_CITY_COUNT);

    list_box_select_index(&ui_list_box, selected_categories.ui_category);
    list_box_select_index(&city_mgmt_list_box, selected_categories.city_mgmt_category);
}

static void draw_list_box_item(const list_box_item *item)
{
    const translation_key *keys = (data.page == CONFIG_PAGE_UI_CHANGES) ? ui_category_keys : city_mgmt_category_keys;

    if (item->is_selected) {
        graphics_draw_inset_rect(item->x + 2, item->y - 1, item->width - 6, 1, COLOR_INSET_DARK, COLOR_INSET_LIGHT);
        graphics_draw_inset_rect(item->x + 2, item->y + item->height - 5, item->width - 6, 1, COLOR_INSET_DARK, COLOR_INSET_LIGHT);
    }
    if (item->is_focused) {
        button_border_draw(item->x, item->y - 2, item->width, item->height, 1);
    }

    font_t f = item->is_selected ? FONT_NORMAL_WHITE : FONT_NORMAL_GREEN;
    const uint8_t *txt = translation_for(keys[item->index]);
    text_draw_ellipsized(txt, item->x + 5, item->y + 4, item->width - 10, f, 0);
}


static void list_box_tooltip(const list_box_item *item, tooltip_context *c)
{
    const translation_key *keys = (data.page == CONFIG_PAGE_UI_CHANGES) ? ui_category_keys : city_mgmt_category_keys;

    if (item->is_focused) {
        const uint8_t *txt = translation_for(keys[item->index]);
        int text_width = text_get_width(txt, FONT_NORMAL_BLACK);
        if (text_width > item->width - 10) {
            c->precomposed_text = txt;
            c->type = TOOLTIP_BUTTON;
        }
    }
}


static void handle_list_box_select(unsigned int index, int is_double_click)
{
    category_page_properties properties = current_category_properties();
    if (index < (unsigned) properties.count) {
        *properties.selected_ref = (int) index;
        scrollbar.scroll_position = 0;
        int count = get_widget_count_for(data.page);
        scrollbar_init(&scrollbar, 0, count);
        window_request_refresh();
    }
}

static void op_measure_space(const config_widget *w, int avail_text_w, int *out_h)
{
    (void) avail_text_w;
    // Default to one item high, but allow overriding via w->height if set.
    *out_h = (w && w->height > 0) ? w->height : ITEM_BASE_H;
}

static int checkbox_text_height(const uint8_t *txt, int w)
{
    int largest = 0;
    int lines = text_measure_multiline(txt, w, FONT_NORMAL_BLACK, &largest);
    return lines * one_line_ml_height(FONT_NORMAL_BLACK);
}
static void op_measure_checkbox(const config_widget *w, int avail_text_w, int *out_h)
{
    const uint8_t *txt = translation_for(w->description);
    int h = checkbox_text_height(txt, avail_text_w) + 8; //  4+4 padding

    if (h < ITEM_BASE_H) {
        h = ITEM_BASE_H;
    }
    *out_h = h;
}
static void op_draw_bg_checkbox(const config_widget *w, int x, int y, int avail_text_w)
{
    const uint8_t *txt = translation_for(w->description);
    int text_h = text_draw_multiline(txt, x + 30, y + 3, avail_text_w, 0, FONT_NORMAL_BLACK, 0);
    int text_center_y = y + (text_h / 2);
    int box_y = text_center_y - (CHECKBOX_CHECK_SIZE / 2);
    button_border_draw(x, box_y, CHECKBOX_CHECK_SIZE, CHECKBOX_CHECK_SIZE, 0);
    if (data.config_values[w->subtype].new_value) {
        text_draw(string_from_ascii("x"), x + 6, box_y + 3, FONT_NORMAL_BLACK, 0);
    }
}
static void op_draw_fg_checkbox(const config_widget *w, int x, int y, int avail_text_w, int focused)
{
    if (!focused) {
        return;
    }
    const uint8_t *txt = translation_for(w->description);
    int text_h = checkbox_text_height(txt, avail_text_w);
    int text_center_y = y + (text_h / 2);
    int box_y = text_center_y - (CHECKBOX_CHECK_SIZE / 2);
    button_border_draw(x, box_y, CHECKBOX_CHECK_SIZE, CHECKBOX_CHECK_SIZE, 1);
}
static int op_input_checkbox(const config_widget *w, int x, int y, int avail_text_w, const mouse *m, unsigned *focused)
{
    int width = avail_text_w + 30; //  30 includes checkbox + gap

    int height = w->height;
    if (!(x <= m->x && m->x < x + width && y <= m->y && m->y < y + height)) {
        return 0;
    }
    *focused = 1;
    if (m->left.went_up) {
        data.config_values[w->subtype].new_value = 1 - data.config_values[w->subtype].new_value;
        window_invalidate();
        return 1;
    }
    return 0;
}

//  Select

static void op_measure_select(const config_widget *w, int avail_text_w, int *out_h)
{
    *out_h = ITEM_BASE_H;
}
static void op_draw_bg_select(const config_widget *w, int x, int y, int avail_text_w)
{
    text_draw(translation_for(w->description), x, y + 6 + w->y_offset, FONT_NORMAL_BLACK, 0);
    const generic_button *btn = &select_buttons[w->subtype];
    text_draw_centered(w->get_display_text(), btn->x + 8, y + btn->y + 6 + w->y_offset,
                       btn->width - 16, FONT_NORMAL_BLACK, 0);
}
static void op_draw_fg_select(const config_widget *w, int x, int y, int avail_text_w, int focused)
{
    const generic_button *btn = &select_buttons[w->subtype];
    button_border_draw(btn->x, y + btn->y + w->y_offset, btn->width, btn->height, focused);
}
static int op_input_select(const config_widget *w, int x, int y, int avail_text_w, const mouse *m, unsigned *focused)
{
    generic_button *btn = &select_buttons[w->subtype];
    btn->parameter1 = y + w->y_offset; //  for popup anchor

    return generic_buttons_handle_mouse(m, 0, y + w->y_offset, btn, 1, focused);
}

//  Numerical - desc

static void op_measure_desc(const config_widget *w, int avail_text_w, int *out_h)
{
    *out_h = ITEM_BASE_H;
}
static void op_draw_bg_desc(const config_widget *w, int x, int y, int avail_text_w)
{
    text_draw(translation_for(w->description), x, y + 10 + w->y_offset, FONT_NORMAL_BLACK, 0);
}
static void op_draw_fg_desc(const config_widget *w, int x, int y, int avail_text_w, int focused)
{
    (void) w;
    (void) x;
    (void) y;
    (void) avail_text_w;
    (void) focused;
}
static int  op_input_desc(const config_widget *w, int x, int y, int avail_text_w, const mouse *m, unsigned *focused)
{
    return 0;
}

//  Numerical - slider

static void numerical_range_draw(const numerical_range_widget *r, int x, int y, const uint8_t *value_text, int extra_w)
{
    text_draw(value_text, x, y + 6, FONT_NORMAL_BLACK, 0);
    inner_panel_draw(x + r->x, y + 4, r->width_blocks + extra_w / 16, 1);
    int width = r->width_blocks * BLOCK_SIZE + extra_w - NUMERICAL_SLIDER_PADDING * 2 - NUMERICAL_DOT_SIZE;
    int pos = (r->min != r->max) ? ((*r->value - r->min) * width / (r->max - r->min)) : width / 2;
    image_draw(image_group(GROUP_PANEL_BUTTON) + 37, x + r->x + NUMERICAL_SLIDER_PADDING + pos, y + 2, COLOR_MASK_NONE, SCALE_NONE);
}
static int is_over_slider(const numerical_range_widget *r, const mouse *m, int x, int y, int extra_w)
{
    if (x + r->x <= m->x && x + r->width_blocks * BLOCK_SIZE + r->x + extra_w >= m->x &&
        y <= m->y && y + 16 > m->y) {
        return 1;
    }
    return 0;
}
static int handle_slider_mouse(const numerical_range_widget *r, const mouse *m, int x, int y, int id, int extra_w)
{
    if (data.active_numerical_range) {
        if (data.active_numerical_range != id) {
            return 0;
        }
        if (!m->left.is_down) {
            data.active_numerical_range = 0;
            return 0;
        }
    } else if (!m->left.went_down || !is_over_slider(r, m, x, y, extra_w)) {
        return 0;
    }
    if (r->min == r->max) {
        return 1;
    }
    int slider_w = r->width_blocks * BLOCK_SIZE - NUMERICAL_SLIDER_PADDING * 2 - NUMERICAL_DOT_SIZE + extra_w;
    int px_per_step = slider_w / (r->max - r->min);
    int dot = m->x - x - r->x - NUMERICAL_DOT_SIZE / 2 + px_per_step / 2;

    int exact = calc_bound(r->min + dot * (r->max - r->min) / slider_w, r->min, r->max);
    int left = (exact / r->step) * r->step;
    int right = calc_bound(left + r->step, r->min, r->max);
    int closest = ((exact - left) < (right - exact)) ? left : right;

    if (closest != *r->value) {
        *r->value = closest;
        window_request_refresh();
    }
    data.active_numerical_range = id;
    return 1;
}

static void op_measure_range(const config_widget *w, int avail_text_w, int *out_h)
{
    *out_h = ITEM_BASE_H;
}
static void op_draw_bg_range(const config_widget *w, int x, int y, int avail_text_w)
{
    int extra_w = data.layout.has_scrollbar ? 0 : 64;
    // content_span_for_page already accounts for category shift; don't offset again.
    numerical_range_draw(&ranges[w->subtype], x, y + w->y_offset, w->get_display_text(), extra_w);
}
static void op_draw_fg_range(const config_widget *w, int x, int y, int avail_text_w, int focused)
{    // We don't need to draw any foreground for range widgets,
    // but the function must exist to satisfy the ops table, therefore the void casts.
    (void) w;
    (void) x;
    (void) y;
    (void) avail_text_w;
    (void) focused;
}
static int op_input_range(const config_widget *w, int x, int y, int avail_text_w, const mouse *m, unsigned *focused)
{
    int extra_w = data.layout.has_scrollbar ? 0 : 64;
    // Keep hit-testing aligned with drawing by using x directly.
    (void) focused;
    return handle_slider_mouse(&ranges[w->subtype], m, x, y + w->y_offset, w->subtype + 1, extra_w);
}

//  Header
static void op_measure_header(const config_widget *w, int avail_text_w, int *out_h)
{
    *out_h = ITEM_BASE_H;
}
static void op_draw_bg_header(const config_widget *w, int x, int y, int avail_text_w)
{
    text_draw(translation_for(w->description ? w->description : w->subtype), x, y + w->y_offset, FONT_NORMAL_BLACK, 0);
}
static void op_draw_fg_header(const config_widget *w, int x, int y, int avail_text_w, int focused)
{
    (void) w;
    (void) x;
    (void) y;
    (void) avail_text_w;
    (void) focused;
}
static int  op_input_header(const config_widget *w, int x, int y, int avail_text_w, const mouse *m, unsigned *focused)
{
    return 0;
}

//  Ops table

static const widget_ops ops_by_type[] = {
    [TYPE_NONE] = {0},
    [TYPE_SPACE] = {op_measure_space, 0, 0, 0},
    [TYPE_HEADER] = {op_measure_header, op_draw_bg_header, op_draw_fg_header, op_input_header},
    [TYPE_CHECKBOX] = {op_measure_checkbox, op_draw_bg_checkbox, op_draw_fg_checkbox, op_input_checkbox},
    [TYPE_SELECT] = {op_measure_select, op_draw_bg_select, op_draw_fg_select, op_input_select},
    [TYPE_NUMERICAL_DESC] = {op_measure_desc, op_draw_bg_desc, op_draw_fg_desc, op_input_desc},
    [TYPE_NUMERICAL_RANGE] = {op_measure_range, op_draw_bg_range, op_draw_fg_range, op_input_range},
};

//   widget lookups

static const config_widget *get_widget_row_for(unsigned int page, int index)
{
    const config_widget *src = 0;
    if (page == CONFIG_PAGE_UI_CHANGES) {
        src = ui_widgets_by_category[selected_categories.ui_category];
    } else if (page == CONFIG_PAGE_CITY_MANAGEMENT_CHANGES) {
        src = city_mgmt_widgets_by_category[selected_categories.city_mgmt_category];
    } else if (page == CONFIG_PAGE_GENERAL) {
        src = page_general;
    } else if (page == CONFIG_PAGE_GAMEPLAY_CHANGES) {
        src = page_difficulty;
    } else {
        return 0; //unknown page
    }
    for (int i = 0, n = 0; i < MAX_WIDGETS; i++) {
        if (src[i].type == TYPE_NONE) {
            break;
        }
        if (!src[i].enabled) {
            continue;
        }
        if (n++ == index) {
            return &src[i];
        }
    }
    return 0;
}
static int get_widget_count_for(unsigned int page)
{
    const config_widget *src = 0;
    switch (page) {
        case CONFIG_PAGE_UI_CHANGES:
            src = ui_widgets_by_category[selected_categories.ui_category];
            break;
        case CONFIG_PAGE_CITY_MANAGEMENT_CHANGES:
            src = city_mgmt_widgets_by_category[selected_categories.city_mgmt_category];
            break;
        case CONFIG_PAGE_GENERAL:
            src = page_general;
            break;
        case CONFIG_PAGE_GAMEPLAY_CHANGES:
            src = page_difficulty;
            break;
        default:
            src = page_general;
    }
    int n = 0;
    for (int i = 0; i < MAX_WIDGETS; i++) {
        if (src[i].type == TYPE_NONE) {
            break;
        }
        if (src[i].enabled) {
            n++;
        }
    }
    return n;
}

//  convenience: compute base x and available text width for checkboxes per page/scrollbar

static content_span content_span_for_page(unsigned int page, int has_scrollbar)
{
    int base_x = 20;
    int text_w = 560 - CHECKBOX_CHECK_SIZE - 15; //  CHECKBOX_TEXT_WIDTH equivalent

    if (!has_scrollbar) {
        text_w += 32;
    }
    if (page_is_category(page)) {
        base_x += LIST_BOX_SHIFT;
        text_w -= LIST_BOX_SHIFT;
    }
    content_span span = { base_x, text_w };
    return span;
}

//    Layout
static void build_layout_for_current_page(void)
{
    // Measure with NO scrollbar first
    const int widget_count = get_widget_count_for(data.page);
    const int available_content_height = LIST_BOTTOM - ITEM_Y_OFFSET;

    int content_height_no_sb = 0;
    content_span span_no_sb = content_span_for_page(data.page, 0);

    for (int i = 0; i < widget_count && i < MAX_WIDGETS; ++i) {
        const config_widget *w = get_widget_row_for(data.page, i);
        if (!w) {
            break;
        }
        int h = ITEM_BASE_H;
        if (ops_by_type[w->type].measure) {
            ops_by_type[w->type].measure(w, span_no_sb.text_w, &h);
        }
        content_height_no_sb += (w->margin_top + h);
    }

    int needs_scrollbar = content_height_no_sb > available_content_height;

    // If a scrollbar is needed, recompute how many items fit using the narrower width.
    int elements_in_view = widget_count; // default when no scrollbar
    if (needs_scrollbar) {
        content_span span_sb = content_span_for_page(data.page, 1);
        int current_y = ITEM_Y_OFFSET;
        int count_fit_from_top = 0;
        for (int i = 0; i < widget_count && i < MAX_WIDGETS; ++i) {
            const config_widget *w = get_widget_row_for(data.page, i);
            if (!w) {
                break;
            }
            int h = ITEM_BASE_H;
            if (ops_by_type[w->type].measure) {
                ops_by_type[w->type].measure(w, span_sb.text_w, &h);
            }
            current_y += w->margin_top;
            if (current_y + h > LIST_BOTTOM) {
                break;
            }
            count_fit_from_top++;
            current_y += h;
        }
        if (count_fit_from_top < 1) {
            count_fit_from_top = 1; // at least one row
        }
        elements_in_view = count_fit_from_top;
    }

    // Sync the scrollbar with the new elements_in_view and total count
    scrollbar.elements_in_view = elements_in_view;
    scrollbar_update_total_elements(&scrollbar, widget_count); //  important re-sync

    // change the slice using the clamped scroll position
    const int start_widget_index = (int) scrollbar.scroll_position;
    data.layout.count = widget_count;
    data.layout.visible_from = start_widget_index;
    data.layout.has_scrollbar = needs_scrollbar;

    content_span span = content_span_for_page(data.page, needs_scrollbar);
    int current_y = ITEM_Y_OFFSET;
    int visible_widget_index = 0;

    for (int i = start_widget_index; i < widget_count && visible_widget_index < MAX_WIDGETS; ++i) {
        const config_widget *w = get_widget_row_for((int) data.page, i);
        if (!w) {
            break;
        }
        int h = ITEM_BASE_H;
        if (ops_by_type[w->type].measure) {
            ops_by_type[w->type].measure(w, span.text_w, &h);
        }
        current_y += w->margin_top;
        if (current_y + h > LIST_BOTTOM) {
            break;
        }
        data.layout.rows[visible_widget_index] = w;
        data.layout.y[visible_widget_index] = current_y;
        data.layout.h[visible_widget_index] = h;
        current_y += h;
        visible_widget_index++;
    }
    data.layout.visible_to = start_widget_index + visible_widget_index;
}

//    Drawing

static void draw_background(void)
{
    update_has_changes();

    if (data.show_background_image) {
        image_draw_fullscreen_background(image_group(GROUP_INTERMEZZO_BACKGROUND) + 5);
    } else {
        window_draw_underlying_window();
    }
    graphics_in_dialog();

    outer_panel_draw(0, 0, 40, 30);
    text_draw_centered(translation_for(TR_CONFIG_TITLE), 16, 16, 608, FONT_LARGE_BLACK, 0);

    //  tabs

    int page_x_offset = 30;
    int open_w = text_get_width(translation_for(page_names[data.page]), FONT_NORMAL_BLACK) + 6;
    int max_closed_w = (600 - page_x_offset * CONFIG_PAGES - open_w) / (CONFIG_PAGES - 1);

    for (unsigned int i = 0; i < CONFIG_PAGES; ++i) {
        page_x_offset += 15;
        int w = 0;
        if (data.page == i) {
            w = text_draw(translation_for(page_names[i]), page_x_offset, 58, FONT_NORMAL_BLACK, 0);
        } else {
            w = text_draw_ellipsized(translation_for(page_names[i]), page_x_offset, 58, max_closed_w, FONT_NORMAL_BLACK, 0);
        }
        page_buttons[i].x = page_x_offset - 10;
        page_buttons[i].width = w + 15;
        data.graphics_behind_tab[i] = graphics_save_to_image(data.graphics_behind_tab[i], page_buttons[i].x, 75, page_buttons[i].width, 3);
        page_x_offset += w;
    }

    button_border_draw(10, 75, 620, 355, 0);

    //  layout rows for current page

    build_layout_for_current_page();
    content_span span = content_span_for_page(data.page, data.layout.has_scrollbar);
    //  draw background of rows
    for (int v = 0; v < data.layout.visible_to - data.layout.visible_from; v++) {
        const config_widget *w = data.layout.rows[v];
        int y = data.layout.y[v] + w->y_offset;
        if (ops_by_type[w->type].draw_bg) {
            ops_by_type[w->type].draw_bg(w, span.x, y, span.text_w);
        }
    }
    //  bottom buttons text
    for (size_t i = 0; i < sizeof(bottom_buttons) / sizeof(*bottom_buttons); i++) {
        int disabled = i == NUM_BOTTOM_BUTTONS - 1 && !data.has_changes;
        text_draw_centered(translation_for(bottom_buttons[i].parameter2),
            bottom_buttons[i].x, bottom_buttons[i].y + 9, bottom_buttons[i].width,
            disabled ? FONT_NORMAL_PLAIN : FONT_NORMAL_BLACK,
            disabled ? COLOR_FONT_LIGHT_GRAY : 0);
    }

    graphics_reset_dialog();
}

static void draw_foreground(void)
{
    graphics_in_dialog();

    //  tab tops & borders
    for (unsigned int i = 0; i < CONFIG_PAGES; ++i) {
        button_border_draw(page_buttons[i].x, page_buttons[i].y, page_buttons[i].width, page_buttons[i].height,
                           data.page_focus_button == i + 1);
        if (data.page == i) {
            graphics_draw_from_image(data.graphics_behind_tab[i], page_buttons[i].x, 75);
        } else {
            graphics_fill_rect(page_buttons[i].x, 75, page_buttons[i].width, 3, COLOR_WHITE);
        }
    }

    //  rows fg (focus, borders, knobs)
    content_span span = content_span_for_page(data.page, data.layout.has_scrollbar);
    for (int v = 0; v < data.layout.visible_to - data.layout.visible_from; v++) {
        const config_widget *w = data.layout.rows[v];
        int focused = (data.focus_button == (unsigned) (v + 1));
        int y = data.layout.y[v] + w->y_offset;
        if (ops_by_type[w->type].draw_fg) {
            ops_by_type[w->type].draw_fg(w, span.x, y, span.text_w, focused);
        }
    }

    //  bottom buttons borders
    for (size_t i = 0; i < sizeof(bottom_buttons) / sizeof(*bottom_buttons); i++) {
        button_border_draw(bottom_buttons[i].x, bottom_buttons[i].y, bottom_buttons[i].width, bottom_buttons[i].height,
                           data.bottom_focus_button == i + 1);
    }

    //  scrollbar (if needed)
    if (data.layout.has_scrollbar) {
        inner_panel_draw(scrollbar.x + 4, scrollbar.y + 28, 2, scrollbar.height / BLOCK_SIZE - 3);
        scrollbar_draw(&scrollbar);
    }
    //  category list box
    if (page_is_category(data.page)) {
        category_page_properties desc = current_category_properties();
        list_box_request_refresh(desc.lb);
        list_box_draw(desc.lb);
    }

    graphics_reset_dialog();
}

//    Input

static void cancel_values(void)
{
    for (int i = 0; i < CONFIG_MAX_ALL; i++) {
        data.config_values[i].new_value = data.config_values[i].original_value;
    }
    for (int i = 0; i < CONFIG_STRING_MAX_ALL; i++) {
        memcpy(data.config_string_values[i].new_value, data.config_string_values[i].original_value, CONFIG_STRING_VALUE_MAX);
    }
}
static int apply_changed_configs(void)
{
    if (!data.has_changes) {
        return 1;
    }
    for (int i = 0; i < CONFIG_MAX_ALL; ++i) {
        if (config_changed(i)) {
            if (!data.config_values[i].change_action(i)) {
                return 0;
            }
        }
    }
    for (int i = 0; i < CONFIG_STRING_MAX_ALL; ++i) {
        if (config_string_changed(i)) {
            if (!data.config_string_values[i].change_action(i)) {
                return 0;
            }
        }
    }
    restart_cursors();
    return 1;
}

static void button_hotkeys(const generic_button *button)
{
    window_hotkey_config_show();
}
static void button_reset_defaults(const generic_button *button)
{
    for (int i = 0; i < CONFIG_MAX_ENTRIES; i++) {
        data.config_values[i].new_value = config_get_default_value(i);
    }

    for (int i = 0; i < CONFIG_STRING_MAX_ENTRIES; i++) {
        snprintf(data.config_string_values[i].new_value, CONFIG_STRING_VALUE_MAX, "%s", config_get_default_string_value(i));
    }
    set_language(0);
    window_invalidate();
}
static void button_close(const generic_button *button)
{
    int save = button->parameter1;
    if (!save) {
        cancel_values();
        window_go_back();
        return;
    }
    if (apply_changed_configs() && save == 1) {
        config_save();
        window_go_back();
        return;
    }
    window_request_refresh();
}
static void button_page(const generic_button *button)
{
    set_page((unsigned) button->parameter1);
}

static void on_scroll(void)
{
    window_invalidate();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *md = mouse_in_dialog(m);
    unsigned prev_focus = data.focus_button;
    data.focus_button = 0;

    //  categories first (so clicks don't fall through)

    if (page_is_category(data.page)) {
        category_page_properties page_properties = current_category_properties();
        if (list_box_handle_input(page_properties.lb, md, 1)) {
            data.page_focus_button = 0;
            data.bottom_focus_button = 0;
            return;
        }
    }
    if (scrollbar_handle_mouse(&scrollbar, md, 1)) {
        data.page_focus_button = 0;
        data.bottom_focus_button = 0;
        window_request_refresh();
        return;
    }

    if (data.active_numerical_range) {
        build_layout_for_current_page();
        content_span span = content_span_for_page((int) data.page, data.layout.has_scrollbar);
        //  continue handling for the same id in op_input_range (we don't know which row, so iterate all visible)

        for (int v = 0; v < data.layout.visible_to - data.layout.visible_from; v++) {
            const config_widget *w = data.layout.rows[v];
            if (w->type != TYPE_NUMERICAL_RANGE) continue;
            unsigned f = 0;
            if (ops_by_type[w->type].handle_input(w, span.x, data.layout.y[v] + w->y_offset, span.text_w, md, &f)) return;
        }
        return;
    }
    //  rows

    build_layout_for_current_page();
    content_span span = content_span_for_page((int) data.page, data.layout.has_scrollbar);
    int handled = 0;

    for (int v = 0; v < data.layout.visible_to - data.layout.visible_from; v++) {
        const config_widget *w = data.layout.rows[v];
        unsigned f = 0;
        if (ops_by_type[w->type].handle_input) {
            handled |= ops_by_type[w->type].handle_input(w, span.x, data.layout.y[v] + w->y_offset, span.text_w, md, &f);
            if (f) {
                data.focus_button = v + 1;
            }
        }
    }

    //  bottom and page buttons

    handled |= generic_buttons_handle_mouse(md, 0, 0, bottom_buttons, data.has_changes ? NUM_BOTTOM_BUTTONS : NUM_BOTTOM_BUTTONS - 1, &data.bottom_focus_button);
    handled |= generic_buttons_handle_mouse(md, 0, 0, page_buttons, CONFIG_PAGES, &data.page_focus_button);

    if (!handled && (m->right.went_up || h->escape_pressed)) {
        window_go_back();
    }
    if (prev_focus != data.focus_button) {
        window_request_refresh();
    }
    window_request_refresh();
}
//    Tooltips

static void get_tooltip(tooltip_context *c)
{
    if (page_is_category(data.page)) {
        category_page_properties desc = current_category_properties();
        list_box_handle_tooltip(desc.lb, c);
    }
    if (data.page_focus_button) {
        unsigned int page = data.page_focus_button - 1;
        if (page == data.page) {
            return;
        }
        int text_width = text_get_width(translation_for(page_names[page]), FONT_NORMAL_BLACK);
        if (page_buttons[page].width - 15 < text_width) { //15 is the margin added
            c->translation_key = page_names[page];
            c->type = TOOLTIP_BUTTON;
        }
    }
}
//    Page setup, disabling unavailable widgets, init

static void disable_widget_globally(int type, int subtype)
{
    //  general pages

    for (int i = 0; page_general[i].type != TYPE_NONE; i++)
        if (page_general[i].type == type && page_general[i].subtype == subtype) {
            page_general[i].enabled = 0;
        }
    for (int i = 0; page_difficulty[i].type != TYPE_NONE; i++)
        if (page_difficulty[i].type == type && page_difficulty[i].subtype == subtype) {
            page_difficulty[i].enabled = 0;
        }

    //  category pages

    for (int c = 0; c < CATEGORY_UI_COUNT; ++c)
        for (int i = 0; i < MAX_WIDGETS && ui_widgets_by_category[c][i].type != TYPE_NONE; ++i)
            if (ui_widgets_by_category[c][i].type == type && ui_widgets_by_category[c][i].subtype == subtype) {
                ui_widgets_by_category[c][i].enabled = 0;
            }
    for (int c = 0; c < CATEGORY_CITY_COUNT; ++c)
        for (int i = 0; i < MAX_WIDGETS && city_mgmt_widgets_by_category[c][i].type != TYPE_NONE; ++i)
            if (city_mgmt_widgets_by_category[c][i].type == type &&
                city_mgmt_widgets_by_category[c][i].subtype == subtype) {
                city_mgmt_widgets_by_category[c][i].enabled = 0;
            }

}


static void set_page(unsigned int page)
{
    data.page = page;
    scrollbar.scroll_position = 0;
    int count = get_widget_count_for((int) page);
    scrollbar_init(&scrollbar, 0, count);

    window_invalidate();
}

static void init(unsigned int page, int show_background_image)
{
    memset(&data, 0, sizeof(data));
    data.page = page;
    data.show_background_image = show_background_image;

    //  init volume prefix

    data.volume_offset = string_copy(translation_for(TR_CONFIG_VOLUME), data.volume_text, 63);
    data.volume_offset = string_copy(string_from_ascii(" "), data.volume_offset, (int) (data.volume_offset - data.volume_text - 1));

    //  prime numeric configs from core config

    for (int i = 0; i < CONFIG_MAX_ENTRIES; i++) {
        data.config_values[i].original_value = config_get(i);
        data.config_values[i].new_value = config_get(i);
    }
    for (int i = 0; i < CONFIG_STRING_MAX_ENTRIES; i++) {
        const char *v = config_get_string(i);
        snprintf(data.config_string_values[i].original_value, CONFIG_STRING_VALUE_MAX, "%s", v);
        snprintf(data.config_string_values[i].new_value, CONFIG_STRING_VALUE_MAX, "%s", v);
    }
    fetch_original_config_values();

    set_custom_config_changes();
    set_range_values();

    //  language options (default + dirs)

    string_copy(translation_for(TR_CONFIG_LANGUAGE_DEFAULT), data.language_options.data[0], CONFIG_STRING_VALUE_MAX);
    data.language_options.options[0] = data.language_options.data[0];
    data.language_options.total = 1;
    data.language_options.selected = 0;

    const dir_listing *subdirs = dir_find_all_subdirectories(".");
    const char *original_value = data.config_string_values[CONFIG_STRING_UI_LANGUAGE_DIR].original_value;
    for (int i = 0; i < subdirs->num_files; i++) {
        if (data.language_options.total < MAX_LANGUAGE_DIRS && lang_dir_is_valid(subdirs->files[i].name)) {
            int opt = data.language_options.total;
            snprintf(data.language_options.data_utf8[opt], CONFIG_STRING_VALUE_MAX, "%s", subdirs->files[i].name);
            encoding_from_utf8(subdirs->files[i].name, data.language_options.data[opt], CONFIG_STRING_VALUE_MAX);
            data.language_options.options[opt] = data.language_options.data[opt];
            if (strcmp(original_value, subdirs->files[i].name) == 0) data.language_options.selected = opt;
            data.language_options.total++;
        }
    }

    //  system constraints

    if (!system_can_scale_display(0, 0)) {
        disable_widget_globally(TYPE_HEADER, TR_CONFIG_VIDEO);
        disable_widget_globally(TYPE_NUMERICAL_DESC, RANGE_RESOLUTION);
        disable_widget_globally(TYPE_NUMERICAL_RANGE, RANGE_RESOLUTION);
        disable_widget_globally(TYPE_NUMERICAL_DESC, RANGE_DISPLAY_SCALE);
        disable_widget_globally(TYPE_NUMERICAL_RANGE, RANGE_DISPLAY_SCALE);
        disable_widget_globally(TYPE_CHECKBOX, CONFIG_ORIGINAL_FULLSCREEN);
    }
    if (system_is_fullscreen_only()) {
        disable_widget_globally(TYPE_NUMERICAL_DESC, RANGE_CURSOR_SCALE);
        disable_widget_globally(TYPE_NUMERICAL_RANGE, RANGE_CURSOR_SCALE);
    }

    init_list_boxes();
}
void window_config_show(window_config_page page, int show_background_image)
{
    window_type window = {
        WINDOW_CONFIG,
        draw_background,
        draw_foreground,
        handle_input,
        get_tooltip
    };
    init(page, show_background_image);
    window_show(&window);
}

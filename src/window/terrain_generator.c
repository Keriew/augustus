#include "terrain_generator.h"

#include "assets/assets.h"
#include "city/view.h"
#include "core/config.h"
#include "core/dir.h"
#include "core/image_group.h"
#include "core/string.h"
#include "editor/editor.h"
#include "game/file.h"
#include "game/file_editor.h"
#include "game/game.h"
#include "graphics/button.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "map/aqueduct.h"
#include "map/building.h"
#include "map/desirability.h"
#include "map/elevation.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/image.h"
#include "map/image_context.h"
#include "map/property.h"
#include "map/random.h"
#include "map/road_network.h"
#include "map/soldier_strength.h"
#include "map/sprite.h"
#include "map/terrain.h"
#include "map/tiles.h"
#include "building/type.h"
#include "scenario/allowed_building.h"
#include "scenario/editor.h"
#include "scenario/data.h"
#include "scenario/map.h"
#include "scenario/property.h"
#include "scenario/terrain_generator/terrain_generator.h"
#include "sound/music.h"
#include "translation/translation.h"
#include "widget/input_box.h"
#include "widget/minimap.h"
#include "window/plain_message_dialog.h"
#include "window/select_list.h"
#include "window/video.h"
#include "window/city.h"
#include "window/editor/map.h"

#include <string.h>

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

#define CONTROL_LABEL_X 32
#define CONTROL_VALUE_X 160
#define CONTROL_BUTTON_WIDTH 120
#define CONTROL_BUTTON_HEIGHT 20

#define SIZE_BUTTON_Y 60
#define ALGORITHM_BUTTON_Y 84
#define SEED_INPUT_Y 108
#define RANDOMIZE_BUTTON_Y 144
#define CLIMATE_BUTTON_Y 168


#define SETTINGS_LABEL_Y 135
#define SETTINGS_ROW_Y 155
#define SETTINGS_ROW_HEIGHT 22

#define VICTORY_LABEL_Y (SETTINGS_ROW_Y + SETTINGS_ROW_HEIGHT * 4 + 8)
#define VICTORY_ROW_Y (VICTORY_LABEL_Y + 20)

#define SETTING_BUTTON_WIDTH 120
#define TOGGLE_BUTTON_WIDTH 60
#define VALUE_BUTTON_WIDTH 90
#define CRITERIA_TOGGLE_X CONTROL_VALUE_X
#define CRITERIA_VALUE_X (CONTROL_VALUE_X + TOGGLE_BUTTON_WIDTH + 8)

#define ACTION_BUTTON_WIDTH 288
#define ACTION_BUTTON_HEIGHT 24
#define ACTION_BUTTON_X 320

#define REGENERATE_BUTTON_Y 356
#define OPEN_EDITOR_BUTTON_Y 384
#define START_GAME_BUTTON_Y 412
#define BACK_BUTTON_Y 440


#define PREVIEW_X 320
#define PREVIEW_Y 60
#define PREVIEW_WIDTH 288
// #define PREVIEW_HEIGHT 240
#define PREVIEW_HEIGHT 288

#define SEED_TEXT_LENGTH 16
#define TERRAIN_GENERATOR_SIZE_COUNT 6

#define CLIMATE_COUNT 3

static void button_select_size(const generic_button *button);
static void button_select_algorithm(const generic_button *button);
static void button_select_climate(const generic_button *button);
static void button_randomize(const generic_button *button);
static void button_regenerate(const generic_button *button);
static void button_open_editor(const generic_button *button);
static void button_start_game(const generic_button *button);
static void button_back(const generic_button *button);

static void size_selected(int id);
static void algorithm_selected(int id);
static void climate_selected(int id);

static const uint8_t label_size[] = "Size";
static const uint8_t label_algorithm[] = "Algorithm";
static const uint8_t label_seed[] = "Seed";
static const uint8_t label_randomize[] = "Randomize";
static const uint8_t label_climate[] = "Climate";
static const uint8_t label_regenerate[] = "Regenerate preview";
static const uint8_t label_open_editor[] = "Open in editor";
static const uint8_t label_start_game[] = "Start game";
static const uint8_t label_back[] = "Back";
static const uint8_t label_seed_placeholder[] = "Random";

static const uint8_t size_label_40[] = "40 x 40";
static const uint8_t size_label_60[] = "60 x 60";
static const uint8_t size_label_80[] = "80 x 80";
static const uint8_t size_label_100[] = "100 x 100";
static const uint8_t size_label_120[] = "120 x 120";
static const uint8_t size_label_160[] = "160 x 160";

static const uint8_t label_climate_central[] = "Central";
static const uint8_t label_climate_northern[] = "Northern";
static const uint8_t label_climate_desert[] = "Desert";


static const uint8_t *climate_labels[CLIMATE_COUNT] = {
    label_climate_central,
    label_climate_northern,
    label_climate_desert
};

static const uint8_t *terrain_generator_size_labels[TERRAIN_GENERATOR_SIZE_COUNT] = {
    size_label_40,
    size_label_60,
    size_label_80,
    size_label_100,
    size_label_120,
    size_label_160
};

static const uint8_t *terrain_generator_algorithm_labels[TERRAIN_GENERATOR_COUNT];


static generic_button buttons[] = {
    {CONTROL_VALUE_X, SIZE_BUTTON_Y, CONTROL_BUTTON_WIDTH, CONTROL_BUTTON_HEIGHT, button_select_size, 0, 0},
    {CONTROL_VALUE_X, ALGORITHM_BUTTON_Y, CONTROL_BUTTON_WIDTH, CONTROL_BUTTON_HEIGHT, button_select_algorithm, 0, 0},
    {CONTROL_VALUE_X, CLIMATE_BUTTON_Y, CONTROL_BUTTON_WIDTH, CONTROL_BUTTON_HEIGHT, button_select_climate, 0, 0},
    {CONTROL_VALUE_X, RANDOMIZE_BUTTON_Y, CONTROL_BUTTON_WIDTH, CONTROL_BUTTON_HEIGHT, button_randomize, 0, 0},
    {ACTION_BUTTON_X, REGENERATE_BUTTON_Y, ACTION_BUTTON_WIDTH, ACTION_BUTTON_HEIGHT, button_regenerate, 0, 0},
    {ACTION_BUTTON_X, OPEN_EDITOR_BUTTON_Y, ACTION_BUTTON_WIDTH, ACTION_BUTTON_HEIGHT, button_open_editor, 0, 0},
    {ACTION_BUTTON_X, START_GAME_BUTTON_Y, ACTION_BUTTON_WIDTH, ACTION_BUTTON_HEIGHT, button_start_game, 0, 0},
    {ACTION_BUTTON_X, BACK_BUTTON_Y, ACTION_BUTTON_WIDTH, ACTION_BUTTON_HEIGHT, button_back, 0, 0}
};

static input_box seed_input = {
    CONTROL_VALUE_X,
    SEED_INPUT_Y,
    11,
    2,
    FONT_NORMAL_WHITE,
    0,
    NULL,
    SEED_TEXT_LENGTH,
    0,
    label_seed_placeholder,
    NULL,
    NULL,
    INPUT_BOX_CHARS_NUMERIC
};

static struct {
    unsigned int focus_button_id;
    int size_index;
    int algorithm_index;
    uint8_t seed_text[SEED_TEXT_LENGTH];
    int settings_initialized;
    scenario_climate climate_index;
} data;

static minimap_functions preview_minimap_functions;

static uint8_t preview_road_flags[GRID_SIZE * GRID_SIZE];

static int get_group_allowed(const building_type *types, unsigned int count)
{
    for (unsigned int i = 0; i < count; i++) {
        if (!scenario_allowed_building(types[i])) {
            return 0;
        }
    }
    return 1;
}

static void set_group_allowed(const building_type *types, unsigned int count, int allowed)
{
    for (unsigned int i = 0; i < count; i++) {
        scenario_allowed_building_set(types[i], allowed);
    }
}

static void preview_viewport(int *x, int *y, int *width, int *height)
{
    *x = 0;
    *y = 0;
    *width = map_grid_width();
    *height = map_grid_height();
}

static void clear_map_data(void)
{
    map_image_clear();
    map_building_clear();
    map_terrain_clear();
    map_aqueduct_clear();
    map_figure_clear();
    map_property_clear();
    map_sprite_clear();
    map_random_clear();
    map_desirability_clear();
    map_elevation_clear();
    map_soldier_strength_clear();
    map_road_network_clear();

    map_image_context_init();
    map_terrain_init_outside_map();
    map_random_init();
    map_property_init_alternate_terrain();
}

static int get_seed_value(unsigned int *seed_out)
{
    if (!string_length(data.seed_text)) {
        return 0;
    }
    int seed = string_to_int(data.seed_text);
    if (seed < 0) {
        seed = -seed;
    }
    *seed_out = (unsigned int) seed;
    return 1;
}

static void generate_preview_map(void)
{
    scenario_editor_create(data.size_index);
    scenario_map_init();
    if (data.settings_initialized) {
        // apply_scenario_settings();
    } else {
        // sync_settings_from_scenario();
        data.settings_initialized = 1;
    }
    clear_map_data();
    map_image_init_edges();

    unsigned int seed = 0;
    int use_seed = get_seed_value(&seed);

    terrain_generator_set_seed(use_seed, seed);
    terrain_generator_generate((terrain_generator_algorithm) data.algorithm_index);
    int width = map_grid_width();
    int height = map_grid_height();
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int grid_offset = map_grid_offset(x, y);
            preview_road_flags[grid_offset] = map_terrain_is(grid_offset, TERRAIN_ROAD) ? 1 : 0;
            if (preview_road_flags[grid_offset]) {
                map_terrain_remove(grid_offset, TERRAIN_ROAD);
            }
        }
    }

    map_tiles_update_all();

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int grid_offset = map_grid_offset(x, y);
            if (preview_road_flags[grid_offset]) {
                map_terrain_add(grid_offset, TERRAIN_ROAD);
            }
        }
    }
    map_tiles_update_all_roads();

    preview_minimap_functions.climate = scenario_property_climate;
    preview_minimap_functions.map.width = map_grid_width;
    preview_minimap_functions.map.height = map_grid_height;
    preview_minimap_functions.offset.figure = 0;
    preview_minimap_functions.offset.terrain = map_terrain_get;
    preview_minimap_functions.offset.building_id = 0;
    preview_minimap_functions.offset.is_draw_tile = map_property_is_draw_tile;
    preview_minimap_functions.offset.tile_size = map_property_multi_tile_size;
    preview_minimap_functions.offset.random = map_random_get;
    preview_minimap_functions.building = 0;
    preview_minimap_functions.viewport = preview_viewport;

    city_view_set_custom_lookup(scenario.map.grid_start, scenario.map.width,
        scenario.map.height, scenario.map.grid_border_size);
    widget_minimap_update(&preview_minimap_functions);
    city_view_restore_lookup();
}

static void draw_background(void)
{
    image_draw_fullscreen_background(image_group(GROUP_INTERMEZZO_BACKGROUND) + 25);

    graphics_set_clip_rectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    graphics_in_dialog();
    outer_panel_draw(0, 0, 40, 30);
    text_draw_centered(translation_for(TR_MAIN_MENU_TERRAIN_GENERATOR), 32, 14, 554, FONT_LARGE_BLACK, 0);

    text_draw(label_size, CONTROL_LABEL_X, SIZE_BUTTON_Y + 6, FONT_NORMAL_BLACK, 0);
    text_draw(label_algorithm, CONTROL_LABEL_X, ALGORITHM_BUTTON_Y + 6, FONT_NORMAL_BLACK, 0);
    text_draw(label_seed, CONTROL_LABEL_X, SEED_INPUT_Y + 6, FONT_NORMAL_BLACK, 0);
    // text_draw(label_randomize, CONTROL_LABEL_X, RANDOMIZE_BUTTON_Y + 6, FONT_NORMAL_BLACK, 0);
    text_draw(label_climate, CONTROL_LABEL_X, CLIMATE_BUTTON_Y + 6, FONT_NORMAL_BLACK, 0);

    inner_panel_draw(PREVIEW_X - 8, PREVIEW_Y - 8, (PREVIEW_WIDTH + 16) / BLOCK_SIZE,
        (PREVIEW_HEIGHT + 16) / BLOCK_SIZE);
    widget_minimap_draw(PREVIEW_X, PREVIEW_Y, PREVIEW_WIDTH, PREVIEW_HEIGHT);

    graphics_reset_clip_rectangle();
    graphics_reset_dialog();
}

static void draw_foreground(void)
{
    graphics_in_dialog();

    for (unsigned int i = 0; i < sizeof(buttons) / sizeof(buttons[0]); i++) {
        button_border_draw(buttons[i].x, buttons[i].y, buttons[i].width, buttons[i].height,
            data.focus_button_id == i + 1);
    }

    text_draw_centered(terrain_generator_size_labels[data.size_index],
        CONTROL_VALUE_X, SIZE_BUTTON_Y + 6, CONTROL_BUTTON_WIDTH, FONT_NORMAL_BLACK, 0);
    text_draw_centered(terrain_generator_algorithm_labels[data.algorithm_index],
        CONTROL_VALUE_X, ALGORITHM_BUTTON_Y + 6, CONTROL_BUTTON_WIDTH, FONT_NORMAL_BLACK, 0);
    text_draw_centered(label_randomize,
        CONTROL_VALUE_X, RANDOMIZE_BUTTON_Y + 6, CONTROL_BUTTON_WIDTH, FONT_NORMAL_BLACK, 0);
    text_draw_centered(climate_labels[data.climate_index],
        CONTROL_VALUE_X, CLIMATE_BUTTON_Y + 6, CONTROL_BUTTON_WIDTH, FONT_NORMAL_BLACK, 0);
    

    text_draw_centered(label_regenerate, ACTION_BUTTON_X, REGENERATE_BUTTON_Y + 8,
        ACTION_BUTTON_WIDTH, FONT_NORMAL_BLACK, 0);
    text_draw_centered(label_open_editor, ACTION_BUTTON_X, OPEN_EDITOR_BUTTON_Y + 8,
        ACTION_BUTTON_WIDTH, FONT_NORMAL_BLACK, 0);
    text_draw_centered(label_start_game, ACTION_BUTTON_X, START_GAME_BUTTON_Y + 8,
        ACTION_BUTTON_WIDTH, FONT_NORMAL_BLACK, 0);
    text_draw_centered(label_back, ACTION_BUTTON_X, BACK_BUTTON_Y + 8,
        ACTION_BUTTON_WIDTH, FONT_NORMAL_BLACK, 0);

    input_box_draw(&seed_input);

    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);
    data.focus_button_id = 0;

    if (input_box_is_accepted()) {
        generate_preview_map();
        window_invalidate();
        return;
    }

    if (input_box_handle_mouse(m_dialog, &seed_input) ||
        generic_buttons_handle_mouse(m_dialog, 0, 0, buttons, sizeof(buttons) / sizeof(buttons[0]),
            &data.focus_button_id)) {
        return;
    }

    if (input_go_back_requested(m, h)) {
        button_back(NULL);
    }
}

static void button_select_size(const generic_button *button)
{
    window_select_list_show_text(0, 0, button, terrain_generator_size_labels,
        TERRAIN_GENERATOR_SIZE_COUNT, size_selected);
}

static void button_select_algorithm(const generic_button *button)
{
    window_select_list_show_text(0, 0, button, terrain_generator_algorithm_labels,
        TERRAIN_GENERATOR_COUNT, algorithm_selected);
}

static void button_select_climate(const generic_button *button)
{
    window_select_list_show_text(0, 0, button, climate_labels, CLIMATE_COUNT, climate_selected);
}

static void button_randomize(const generic_button *button)
{
    (void) button;
    unsigned int random_seed = generate_seed_value();
    sprintf(data.seed_text, "%d", random_seed);
    seed_input.placeholder = data.seed_text;
    // input_box_set_text(&seed_input, data.seed_text);
    generate_preview_map();
    window_invalidate();
}

static void button_regenerate(const generic_button *button)
{
    (void) button;
    generate_preview_map();
    window_invalidate();
}

static void button_open_editor(const generic_button *button)
{
    (void) button;
    unsigned int seed = 0;
    int use_seed = get_seed_value(&seed);
    terrain_generator_set_seed(use_seed, seed);

    if (!editor_is_present() ||
        !game_init_editor_generated(data.size_index, data.algorithm_index)) {
        window_plain_message_dialog_show(TR_NO_EDITOR_TITLE, TR_NO_EDITOR_MESSAGE, 1);
        return;
    }

    // apply_scenario_settings();

    input_box_stop(&seed_input);
    if (config_get(CONFIG_UI_SHOW_INTRO_VIDEO)) {
        window_video_show("map_intro.smk", window_editor_map_show);
    }
    sound_music_play_editor();
}

static void button_start_game(const generic_button *button)
{
    (void) button;
    const uint8_t scenario_name_text[] = "generated_terrain";
    const char scenario_filename[] = "generated_terrain.map";

    const char *scenario_path = dir_append_location(scenario_filename, PATH_LOCATION_SCENARIO);

    input_box_stop(&seed_input);

    if (!scenario_path) {
        window_plain_message_dialog_show(TR_SAVEGAME_NOT_ABLE_TO_SAVE_TITLE,
            TR_SAVEGAME_NOT_ABLE_TO_SAVE_MESSAGE, 1);
        return;
    }

    unsigned int seed = 0;
    int use_seed = get_seed_value(&seed);
    terrain_generator_set_seed(use_seed, seed);

    scenario_editor_create(data.size_index);
    scenario_map_init();
    clear_map_data();
    map_image_init_edges();
    // apply_scenario_settings();
    terrain_generator_generate((terrain_generator_algorithm) data.algorithm_index);
    scenario_set_name(scenario_name_text);
    scenario_set_custom(2);

    if (!game_file_editor_write_scenario(scenario_path)) {
        window_plain_message_dialog_show(TR_SAVEGAME_NOT_ABLE_TO_SAVE_TITLE,
            TR_SAVEGAME_NOT_ABLE_TO_SAVE_MESSAGE, 1);
        return;
    }

    window_city_show();
    if (!game_file_start_scenario_by_name(scenario_name())) {
        window_plain_message_dialog_show_with_extra(TR_REPLAY_MAP_NOT_FOUND_TITLE,
            TR_REPLAY_MAP_NOT_FOUND_MESSAGE, 0, scenario_name());
    }
}

static void button_back(const generic_button *button)
{
    (void) button;
    input_box_stop(&seed_input);
    window_go_back();
}

static void size_selected(int id)
{
    data.size_index = id;
    generate_preview_map();
    window_invalidate();
}

static void algorithm_selected(int id)
{
    data.algorithm_index = id;
    generate_preview_map();
    window_invalidate();
}

static void climate_selected(int id)
{
    data.climate_index = id;
    generate_preview_map();
    window_invalidate();
}

static void init(void)
{
    data.focus_button_id = 0;
    data.size_index = 2;
    data.algorithm_index = 0;
    data.seed_text[0] = 0;
    data.settings_initialized = 0;
    data.climate_index = CLIMATE_CENTRAL;

    seed_input.text = data.seed_text;
    seed_input.allowed_chars = INPUT_BOX_CHARS_NUMERIC;

    unsigned int random_seed = generate_seed_value();
    sprintf(data.seed_text, "%d", random_seed);
    seed_input.placeholder = data.seed_text;

    terrain_generator_algorithm_labels[2] = "Flat Plains";
    terrain_generator_algorithm_labels[1] = "River Valley";
    terrain_generator_algorithm_labels[0] = "Perlin";

    input_box_start(&seed_input);
    generate_preview_map();
}

void window_terrain_generator_show(void)
{
    window_type window = {
        WINDOW_TERRAIN_GENERATOR,
        draw_background,
        draw_foreground,
        handle_input
    };
    init();
    window_show(&window);
}

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

#include <stdio.h>
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

#define SETTINGS_INPUT_WIDTH_BLOCKS 8
#define SETTINGS_INPUT_HEIGHT_BLOCKS 2
#define SETTINGS_INPUT_X CONTROL_VALUE_X
#define SETTINGS_START_Y 196
#define SETTINGS_ROW_SPACING 24
#define SETTINGS_TEXT_LENGTH 12

#define FUNDS_MIN 0
#define FUNDS_MAX 99999
#define CRITERIA_PERCENT_MIN 0
#define CRITERIA_PERCENT_MAX 100
#define POPULATION_GOAL_MIN 0
#define POPULATION_GOAL_MAX 50000

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
static void sync_settings_from_scenario(void);
static void apply_scenario_settings(void);
static void randomize_scenario_settings(void);
static void sync_settings_to_inputs(void);
static void sync_inputs_to_settings(void);
static void set_input_box_value(input_box *box, int value);
static void initial_funds_on_change(int is_addition_at_end);
static void culture_goal_on_change(int is_addition_at_end);
static void prosperity_goal_on_change(int is_addition_at_end);
static void peace_goal_on_change(int is_addition_at_end);
static void favor_goal_on_change(int is_addition_at_end);
static void population_goal_on_change(int is_addition_at_end);

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
static const uint8_t label_initial_funds[] = "Initial funds";
static const uint8_t label_culture_goal[] = "Culture goal";
static const uint8_t label_prosperity_goal[] = "Prosperity goal";
static const uint8_t label_peace_goal[] = "Peace goal";
static const uint8_t label_favor_goal[] = "Favor goal";
static const uint8_t label_population_goal[] = "Population goal";

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

static uint8_t initial_funds_text[SETTINGS_TEXT_LENGTH] = { 0 };
static uint8_t culture_goal_text[SETTINGS_TEXT_LENGTH] = { 0 };
static uint8_t prosperity_goal_text[SETTINGS_TEXT_LENGTH] = { 0 };
static uint8_t peace_goal_text[SETTINGS_TEXT_LENGTH] = { 0 };
static uint8_t favor_goal_text[SETTINGS_TEXT_LENGTH] = { 0 };
static uint8_t population_goal_text[SETTINGS_TEXT_LENGTH] = { 0 };

static input_box initial_funds_input = {
    SETTINGS_INPUT_X,
    SETTINGS_START_Y,
    SETTINGS_INPUT_WIDTH_BLOCKS,
    SETTINGS_INPUT_HEIGHT_BLOCKS,
    FONT_NORMAL_WHITE,
    0,
    initial_funds_text,
    SETTINGS_TEXT_LENGTH,
    0,
    NULL,
    initial_funds_on_change,
    NULL,
    INPUT_BOX_CHARS_NUMERIC
};

static input_box culture_goal_input = {
    SETTINGS_INPUT_X,
    SETTINGS_START_Y + SETTINGS_ROW_SPACING,
    SETTINGS_INPUT_WIDTH_BLOCKS,
    SETTINGS_INPUT_HEIGHT_BLOCKS,
    FONT_NORMAL_WHITE,
    0,
    culture_goal_text,
    SETTINGS_TEXT_LENGTH,
    0,
    NULL,
    culture_goal_on_change,
    NULL,
    INPUT_BOX_CHARS_NUMERIC
};

static input_box prosperity_goal_input = {
    SETTINGS_INPUT_X,
    SETTINGS_START_Y + SETTINGS_ROW_SPACING * 2,
    SETTINGS_INPUT_WIDTH_BLOCKS,
    SETTINGS_INPUT_HEIGHT_BLOCKS,
    FONT_NORMAL_WHITE,
    0,
    prosperity_goal_text,
    SETTINGS_TEXT_LENGTH,
    0,
    NULL,
    prosperity_goal_on_change,
    NULL,
    INPUT_BOX_CHARS_NUMERIC
};

static input_box peace_goal_input = {
    SETTINGS_INPUT_X,
    SETTINGS_START_Y + SETTINGS_ROW_SPACING * 3,
    SETTINGS_INPUT_WIDTH_BLOCKS,
    SETTINGS_INPUT_HEIGHT_BLOCKS,
    FONT_NORMAL_WHITE,
    0,
    peace_goal_text,
    SETTINGS_TEXT_LENGTH,
    0,
    NULL,
    peace_goal_on_change,
    NULL,
    INPUT_BOX_CHARS_NUMERIC
};

static input_box favor_goal_input = {
    SETTINGS_INPUT_X,
    SETTINGS_START_Y + SETTINGS_ROW_SPACING * 4,
    SETTINGS_INPUT_WIDTH_BLOCKS,
    SETTINGS_INPUT_HEIGHT_BLOCKS,
    FONT_NORMAL_WHITE,
    0,
    favor_goal_text,
    SETTINGS_TEXT_LENGTH,
    0,
    NULL,
    favor_goal_on_change,
    NULL,
    INPUT_BOX_CHARS_NUMERIC
};

static input_box population_goal_input = {
    SETTINGS_INPUT_X,
    SETTINGS_START_Y + SETTINGS_ROW_SPACING * 5,
    SETTINGS_INPUT_WIDTH_BLOCKS,
    SETTINGS_INPUT_HEIGHT_BLOCKS,
    FONT_NORMAL_WHITE,
    0,
    population_goal_text,
    SETTINGS_TEXT_LENGTH,
    0,
    NULL,
    population_goal_on_change,
    NULL,
    INPUT_BOX_CHARS_NUMERIC
};

static input_box *settings_inputs[] = {
    &initial_funds_input,
    &culture_goal_input,
    &prosperity_goal_input,
    &peace_goal_input,
    &favor_goal_input,
    &population_goal_input
};

static input_box *active_input = &seed_input;

static struct {
    unsigned int focus_button_id;
    int size_index;
    int algorithm_index;
    uint8_t seed_text[SEED_TEXT_LENGTH];
    int settings_initialized;
    scenario_climate climate_index;
    int initial_funds;
    scenario_win_criteria win_criteria;
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

static int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static int parse_input_box_value(const input_box *box)
{
    if (!string_length(box->text)) {
        return 0;
    }
    return string_to_int(box->text);
}

static int sanitize_input_box_value(input_box *box, int min_value, int max_value)
{
    int value = clamp_int(parse_input_box_value(box), min_value, max_value);
    set_input_box_value(box, value);
    return value;
}

static void set_input_box_value(input_box *box, int value)
{
    snprintf((char *) box->text, box->text_length, "%d", value);
}

static void initial_funds_on_change(int is_addition_at_end)
{
    (void) is_addition_at_end;
    sanitize_input_box_value(&initial_funds_input, FUNDS_MIN, FUNDS_MAX);
}

static void culture_goal_on_change(int is_addition_at_end)
{
    (void) is_addition_at_end;
    sanitize_input_box_value(&culture_goal_input, CRITERIA_PERCENT_MIN, CRITERIA_PERCENT_MAX);
}

static void prosperity_goal_on_change(int is_addition_at_end)
{
    (void) is_addition_at_end;
    sanitize_input_box_value(&prosperity_goal_input, CRITERIA_PERCENT_MIN, CRITERIA_PERCENT_MAX);
}

static void peace_goal_on_change(int is_addition_at_end)
{
    (void) is_addition_at_end;
    sanitize_input_box_value(&peace_goal_input, CRITERIA_PERCENT_MIN, CRITERIA_PERCENT_MAX);
}

static void favor_goal_on_change(int is_addition_at_end)
{
    (void) is_addition_at_end;
    sanitize_input_box_value(&favor_goal_input, CRITERIA_PERCENT_MIN, CRITERIA_PERCENT_MAX);
}

static void population_goal_on_change(int is_addition_at_end)
{
    (void) is_addition_at_end;
    sanitize_input_box_value(&population_goal_input, POPULATION_GOAL_MIN, POPULATION_GOAL_MAX);
}

static void draw_static_input_box(const input_box *box)
{
    inner_panel_draw(box->x, box->y, box->width_blocks, box->height_blocks);
    text_draw(box->text, box->x + 16, box->y + 10, box->font, 0);
}

static int is_mouse_inside_box(const mouse *m, const input_box *box)
{
    return m->x >= box->x && m->x < box->x + box->width_blocks * BLOCK_SIZE
        && m->y >= box->y && m->y < box->y + box->height_blocks * BLOCK_SIZE;
}

static void focus_input_box(input_box *box)
{
    if (active_input == box) {
        return;
    }

    if (active_input) {
        input_box_stop(active_input);
    }
    active_input = box;
    input_box_start(active_input);
}

static void sync_settings_to_inputs(void)
{
    set_input_box_value(&initial_funds_input, data.initial_funds);
    set_input_box_value(&culture_goal_input, data.win_criteria.culture.goal);
    set_input_box_value(&prosperity_goal_input, data.win_criteria.prosperity.goal);
    set_input_box_value(&peace_goal_input, data.win_criteria.peace.goal);
    set_input_box_value(&favor_goal_input, data.win_criteria.favor.goal);
    set_input_box_value(&population_goal_input, data.win_criteria.population.goal);

    if (active_input) {
        input_box_refresh_text(active_input);
    }
}

static void sync_inputs_to_settings(void)
{
    data.initial_funds = sanitize_input_box_value(&initial_funds_input, FUNDS_MIN, FUNDS_MAX);

    data.win_criteria.culture.goal = sanitize_input_box_value(&culture_goal_input, CRITERIA_PERCENT_MIN, CRITERIA_PERCENT_MAX);
    data.win_criteria.prosperity.goal = sanitize_input_box_value(&prosperity_goal_input, CRITERIA_PERCENT_MIN, CRITERIA_PERCENT_MAX);
    data.win_criteria.peace.goal = sanitize_input_box_value(&peace_goal_input, CRITERIA_PERCENT_MIN, CRITERIA_PERCENT_MAX);
    data.win_criteria.favor.goal = sanitize_input_box_value(&favor_goal_input, CRITERIA_PERCENT_MIN, CRITERIA_PERCENT_MAX);
    data.win_criteria.population.goal = sanitize_input_box_value(&population_goal_input, POPULATION_GOAL_MIN, POPULATION_GOAL_MAX);

    data.win_criteria.culture.enabled = data.win_criteria.culture.goal > 0;
    data.win_criteria.prosperity.enabled = data.win_criteria.prosperity.goal > 0;
    data.win_criteria.peace.enabled = data.win_criteria.peace.goal > 0;
    data.win_criteria.favor.enabled = data.win_criteria.favor.goal > 0;
    data.win_criteria.population.enabled = data.win_criteria.population.goal > 0;

    sync_settings_to_inputs();
}

static void generate_preview_map(void)
{
    scenario_editor_create(data.size_index);
    scenario_map_init();
    if (data.settings_initialized) {
        apply_scenario_settings();
    } else {
        sync_settings_from_scenario();
        data.settings_initialized = 1;
        apply_scenario_settings();
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

    text_draw(label_initial_funds, CONTROL_LABEL_X, SETTINGS_START_Y + 6, FONT_NORMAL_BLACK, 0);
    text_draw(label_culture_goal, CONTROL_LABEL_X, SETTINGS_START_Y + SETTINGS_ROW_SPACING + 6, FONT_NORMAL_BLACK, 0);
    text_draw(label_prosperity_goal, CONTROL_LABEL_X, SETTINGS_START_Y + SETTINGS_ROW_SPACING * 2 + 6, FONT_NORMAL_BLACK, 0);
    text_draw(label_peace_goal, CONTROL_LABEL_X, SETTINGS_START_Y + SETTINGS_ROW_SPACING * 3 + 6, FONT_NORMAL_BLACK, 0);
    text_draw(label_favor_goal, CONTROL_LABEL_X, SETTINGS_START_Y + SETTINGS_ROW_SPACING * 4 + 6, FONT_NORMAL_BLACK, 0);
    text_draw(label_population_goal, CONTROL_LABEL_X, SETTINGS_START_Y + SETTINGS_ROW_SPACING * 5 + 6, FONT_NORMAL_BLACK, 0);

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

    if (active_input == &seed_input) {
        input_box_draw(&seed_input);
    } else {
        draw_static_input_box(&seed_input);
    }
    for (unsigned int i = 0; i < sizeof(settings_inputs) / sizeof(settings_inputs[0]); i++) {
        if (settings_inputs[i] == active_input) {
            input_box_draw(settings_inputs[i]);
        } else {
            draw_static_input_box(settings_inputs[i]);
        }
    }

    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);
    data.focus_button_id = 0;

    if (input_box_is_accepted()) {
        sync_inputs_to_settings();
        generate_preview_map();
        window_invalidate();
        return;
    }

    if (active_input && input_box_handle_mouse(m_dialog, active_input)) {
        return;
    }

    if (m_dialog->left.went_up) {
        if (is_mouse_inside_box(m_dialog, &seed_input)) {
            focus_input_box(&seed_input);
            return;
        }
        for (unsigned int i = 0; i < sizeof(settings_inputs) / sizeof(settings_inputs[0]); i++) {
            if (is_mouse_inside_box(m_dialog, settings_inputs[i])) {
                focus_input_box(settings_inputs[i]);
                return;
            }
        }
    }

    if (generic_buttons_handle_mouse(m_dialog, 0, 0, buttons, sizeof(buttons) / sizeof(buttons[0]),
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
    randomize_scenario_settings();
    // input_box_set_text(&seed_input, data.seed_text);
    generate_preview_map();
    window_invalidate();
}

static void button_regenerate(const generic_button *button)
{
    (void) button;
    sync_inputs_to_settings();
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

    apply_scenario_settings();

    if (active_input) {
        input_box_stop(active_input);
    }
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

    if (active_input) {
        input_box_stop(active_input);
    }

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
    apply_scenario_settings();
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
    if (active_input) {
        input_box_stop(active_input);
    }
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

static void sync_settings_from_scenario(void)
{
    data.climate_index = scenario_property_climate();
    data.initial_funds = scenario.initial_funds;
    data.win_criteria = scenario.win_criteria;
    sync_settings_to_inputs();
}

static void apply_scenario_settings(void)
{
    sync_inputs_to_settings();
    scenario_change_climate(data.climate_index);
    scenario.initial_funds = data.initial_funds;
    scenario.win_criteria = data.win_criteria;
}

static void randomize_scenario_settings(void)
{
    data.climate_index = (scenario_climate) (generate_seed_value() % CLIMATE_COUNT);

    data.initial_funds = 500 + (int) (generate_seed_value() % 60) * 250;

    data.win_criteria.culture.enabled = (int) (generate_seed_value() % 2);
    data.win_criteria.prosperity.enabled = (int) (generate_seed_value() % 2);
    data.win_criteria.peace.enabled = (int) (generate_seed_value() % 2);
    data.win_criteria.favor.enabled = (int) (generate_seed_value() % 2);
    data.win_criteria.population.enabled = (int) (generate_seed_value() % 3 == 0);

    data.win_criteria.culture.goal = 10 + (int) (generate_seed_value() % 91);
    data.win_criteria.prosperity.goal = 10 + (int) (generate_seed_value() % 91);
    data.win_criteria.peace.goal = 10 + (int) (generate_seed_value() % 91);
    data.win_criteria.favor.goal = 10 + (int) (generate_seed_value() % 91);
    data.win_criteria.population.goal = 500 + (int) (generate_seed_value() % 56) * 250;

    if (!data.win_criteria.culture.enabled && !data.win_criteria.prosperity.enabled
        && !data.win_criteria.peace.enabled && !data.win_criteria.favor.enabled
        && !data.win_criteria.population.enabled) {
        data.win_criteria.culture.enabled = 1;
    }
    sync_settings_to_inputs();
}

static void init(void)
{
    data.focus_button_id = 0;
    data.size_index = 2;
    data.algorithm_index = 0;
    data.seed_text[0] = 0;
    data.settings_initialized = 0;
    data.climate_index = CLIMATE_CENTRAL;
    data.initial_funds = 1000;
    memset(&data.win_criteria, 0, sizeof(data.win_criteria));

    seed_input.text = data.seed_text;
    seed_input.allowed_chars = INPUT_BOX_CHARS_NUMERIC;

    unsigned int random_seed = generate_seed_value();
    sprintf(data.seed_text, "%d", random_seed);
    seed_input.placeholder = data.seed_text;

    terrain_generator_algorithm_labels[2] = "Flat Plains";
    terrain_generator_algorithm_labels[1] = "River Valley";
    terrain_generator_algorithm_labels[0] = "Perlin";

    active_input = &seed_input;
    input_box_start(active_input);
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

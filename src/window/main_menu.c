#include "main_menu.h"

#include "assets/assets.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/string.h"
#include "editor/editor.h"
#include "game/campaign.h"
#include "game/game.h"
#include "game/system.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/image_button.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/screen.h"
#include "graphics/weather.h"
#include "graphics/window.h"
#include "sound/music.h"
#include "translation/translation.h"
#include "window/cck_selection.h"
#include "window/config.h"
#include "window/editor/map.h"
#include "window/file_dialog.h"
#include "window/plain_message_dialog.h"
#include "window/popup_dialog.h"
#include "window/select_list.h"
#include "window/select_campaign.h"
#include "window/video.h"
#include "scenario/terrain_generator.h"

#define MAX_BUTTONS 7
#define TERRAIN_GENERATOR_SIZE_COUNT 6

static void button_click(const generic_button *button);
static void terrain_generator_size_selected(int id);
static void terrain_generator_algorithm_selected(int id);
static void show_terrain_generator_size_picker(const generic_button *button);
static void show_terrain_generator_algorithm_picker(const generic_button *button);

static struct {
    unsigned int focus_button_id;
    int logo_image_id;
    int selected_map_size;
} data;

static const uint8_t size_label_40[] = "40 x 40";
static const uint8_t size_label_60[] = "60 x 60";
static const uint8_t size_label_80[] = "80 x 80";
static const uint8_t size_label_100[] = "100 x 100";
static const uint8_t size_label_120[] = "120 x 120";
static const uint8_t size_label_160[] = "160 x 160";

static const uint8_t *terrain_generator_size_labels[TERRAIN_GENERATOR_SIZE_COUNT] = {
    size_label_40,
    size_label_60,
    size_label_80,
    size_label_100,
    size_label_120,
    size_label_160
};

static const uint8_t *terrain_generator_algorithm_labels[TERRAIN_GENERATOR_COUNT];

static const generic_button *terrain_generator_anchor_button = NULL;

static generic_button buttons[] = {
    {192, 130, 256, 25, button_click, 0, 1},
    {192, 164, 256, 25, button_click, 0, 2},
    {192, 198, 256, 25, button_click, 0, 3},
    {192, 232, 256, 25, button_click, 0, 4},
    {192, 266, 256, 25, button_click, 0, 5},
    {192, 300, 256, 25, button_click, 0, 6},
    {192, 334, 256, 25, button_click, 0, 7},
};

static void draw_version_string(void)
{
    uint8_t version_string[100] = "Augustus v";
    int version_prefix_length = string_length(version_string);
    int text_y = screen_height() - 54;

    string_copy(string_from_ascii(system_version()), version_string + version_prefix_length, 99);

    int text_width = text_get_width(version_string, FONT_NORMAL_GREEN);
    int width = calc_value_in_step(text_width + 20, 16);

    inner_panel_draw(20, text_y, width / 16, 2);
    text_draw_centered(version_string, 20, text_y + 11, width, FONT_NORMAL_GREEN, 0);
}

static void draw_background(void)
{
    graphics_reset_dialog();
    graphics_reset_clip_rectangle();
    image_draw_fullscreen_background(image_group(GROUP_INTERMEZZO_BACKGROUND));

    if (!window_is(WINDOW_FILE_DIALOG)) {
        graphics_in_dialog();
        outer_panel_draw(162, 32, 20, 22);
        if (!data.logo_image_id) {
            data.logo_image_id = assets_get_image_id("UI", "Main Menu Banner");
        }
        image_draw(data.logo_image_id, 176, 50, COLOR_MASK_NONE, SCALE_NONE);
        graphics_reset_dialog();
        draw_version_string();
    }
}

static void draw_foreground(void)
{
    graphics_in_dialog();

    for (unsigned int i = 0; i < MAX_BUTTONS; i++) {
        large_label_draw(buttons[i].x, buttons[i].y, buttons[i].width / BLOCK_SIZE,
            data.focus_button_id == i + 1 ? 1 : 0);
    }

    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_MAIN_MENU_SELECT_CAMPAIGN, 192, 137, 256, FONT_NORMAL_GREEN);
    lang_text_draw_centered(30, 2, 192, 171, 256, FONT_NORMAL_GREEN);
    lang_text_draw_centered(30, 3, 192, 205, 256, FONT_NORMAL_GREEN);
    lang_text_draw_centered(9, 8, 192, 239, 256, FONT_NORMAL_GREEN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_MAIN_MENU_TERRAIN_GENERATOR, 192, 273, 256, FONT_NORMAL_GREEN);
    lang_text_draw_centered(2, 0, 192, 307, 256, FONT_NORMAL_GREEN);
    lang_text_draw_centered(30, 5, 192, 341, 256, FONT_NORMAL_GREEN);

    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);
    if (generic_buttons_handle_mouse(m_dialog, 0, 0, buttons, MAX_BUTTONS, &data.focus_button_id)) {
        return;
    }
    if (h->escape_pressed) {
        hotkey_handle_escape();
    }
    if (h->load_file) {
        window_file_dialog_show(FILE_TYPE_SAVED_GAME, FILE_DIALOG_LOAD);
    }
}

static void confirm_exit(int accepted, int checked)
{
    if (accepted) {
        system_exit();
    }
}

static void terrain_generator_size_selected(int id)
{
    data.selected_map_size = id;
    show_terrain_generator_algorithm_picker(terrain_generator_anchor_button);
}

static void terrain_generator_algorithm_selected(int id)
{
    if (!editor_is_present() ||
        !game_init_editor_generated(data.selected_map_size, id)) {
        window_plain_message_dialog_show(
            TR_NO_EDITOR_TITLE, TR_NO_EDITOR_MESSAGE, 1);
        return;
    }

    if (config_get(CONFIG_UI_SHOW_INTRO_VIDEO)) {
        window_video_show("map_intro.smk", window_editor_map_show);
    }
    sound_music_play_editor();
}

static void show_terrain_generator_size_picker(const generic_button *button)
{
    terrain_generator_anchor_button = button;
    window_select_list_show_text(0, 0, button, terrain_generator_size_labels,
        TERRAIN_GENERATOR_SIZE_COUNT, terrain_generator_size_selected);
}

static void show_terrain_generator_algorithm_picker(const generic_button *button)
{
    terrain_generator_algorithm_labels[0] = translation_for(TR_TERRAIN_GENERATOR_FLAT_PLAINS);
    terrain_generator_algorithm_labels[1] = translation_for(TR_TERRAIN_GENERATOR_RIVER_VALLEY);
    window_select_list_show_text(0, 0, button, terrain_generator_algorithm_labels, TERRAIN_GENERATOR_COUNT,
        terrain_generator_algorithm_selected);
}

static void button_click(const generic_button *button)
{
    int type = button->parameter1;

    if (type == 1) {
        window_select_campaign_show();
    } else if (type == 2) {
        window_file_dialog_show(FILE_TYPE_SAVED_GAME, FILE_DIALOG_LOAD);
    } else if (type == 3) {
        window_cck_selection_show();
    } else if (type == 4) {
        if (!editor_is_present() || !game_init_editor()) {
            window_plain_message_dialog_show(
                TR_NO_EDITOR_TITLE, TR_NO_EDITOR_MESSAGE, 1);
        } else {
            if (config_get(CONFIG_UI_SHOW_INTRO_VIDEO)) window_video_show("map_intro.smk", window_editor_map_show);
            sound_music_play_editor();
        }
    } else if (type == 5) {
        show_terrain_generator_size_picker(button);
    } else if (type == 6) {
        window_config_show(CONFIG_FIRST_PAGE, 0, 1);
    } else if (type == 7) {
        window_popup_dialog_show(POPUP_DIALOG_QUIT, confirm_exit, 1);
    }
}

void window_main_menu_show(int restart_music)
{
    if (restart_music) {
        sound_music_play_intro();
    }
    weather_reset();
    game_campaign_clear();
    window_type window = {
        WINDOW_MAIN_MENU,
        draw_background,
        draw_foreground,
        handle_input
    };
    window_show(&window);
}

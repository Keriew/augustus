#include "main_menu.h"

#include "assets/assets.h"
#include "core/calc.h"
#include "core/string.h"
#include "editor/editor.h"
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
#include "graphics/window.h"
#include "sound/music.h"
#include "window/cck_selection.h"
#include "window/config.h"
#include "window/file_dialog.h"
#include "window/new_career.h"
#include "window/plain_message_dialog.h"
#include "window/popup_dialog.h"

static void confirm_exit(int accepted, int checked)
{
    if (accepted) {
        system_exit();
    }
}

static void mm_start_new_career(int p1, int p2)
{
    window_new_career_show();
}
static void mm_load_file(int p1, int p2)
{
    window_file_dialog_show(FILE_TYPE_SAVED_GAME, FILE_DIALOG_LOAD);
}
static void mm_open_city_creator(int p1, int p2)
{
    window_cck_selection_show();
}
static void mm_show_option_window(int p1, int p2)
{
    window_config_show(CONFIG_FIRST_PAGE, 1);
}
static void mm_quit_game(int p1, int p2)
{
    window_popup_dialog_show(POPUP_DIALOG_QUIT, confirm_exit, 1);
}

static void mm_open_mission_editor(int p1, int p2)
{
    if (!editor_is_present() || !game_init_editor()) {
        window_plain_message_dialog_show(
            TR_NO_EDITOR_TITLE, TR_NO_EDITOR_MESSAGE, 1);
    } else {
        sound_music_play_editor();
    }
}

enum main_menu_cfg { MM_BTN_LEFT = 192,
                     MM_BTN_TOP = 130,
                     MM_BTN_NEXT = 40,
                     MM_BTN_WIDTH = 256,
                     MM_BTN_HEIGHT = 25
                   };

#define MM_BTN_TOP_NEXT(i) (MM_BTN_TOP + MM_BTN_NEXT * i)
#define MM_MAKE_TEXT(g,t) ((g << 8) + t)
#define MM_TEXT_GROUP(b) ((b >> 8) & 0xff)
#define MM_TEXT_ID(b) (b & 0xff)

static struct {
    int focus_button_id;
    int logo_image_id;
} data;

static generic_button buttons[] = {
    {MM_BTN_LEFT, MM_BTN_TOP_NEXT(0), MM_BTN_WIDTH, MM_BTN_HEIGHT, mm_start_new_career,    button_none, 0, MM_MAKE_TEXT(30, 1)},
    {MM_BTN_LEFT, MM_BTN_TOP_NEXT(1), MM_BTN_WIDTH, MM_BTN_HEIGHT, mm_load_file,           button_none, 0, MM_MAKE_TEXT(30, 2)},
    {MM_BTN_LEFT, MM_BTN_TOP_NEXT(2), MM_BTN_WIDTH, MM_BTN_HEIGHT, mm_open_city_creator,   button_none, 0, MM_MAKE_TEXT(30, 3)},
    {MM_BTN_LEFT, MM_BTN_TOP_NEXT(3), MM_BTN_WIDTH, MM_BTN_HEIGHT, mm_open_mission_editor, button_none, 0, MM_MAKE_TEXT(9, 8)},
    {MM_BTN_LEFT, MM_BTN_TOP_NEXT(4), MM_BTN_WIDTH, MM_BTN_HEIGHT, mm_show_option_window,  button_none, 0, MM_MAKE_TEXT(2, 0)},
    {MM_BTN_LEFT, MM_BTN_TOP_NEXT(5), MM_BTN_WIDTH, MM_BTN_HEIGHT, mm_quit_game,           button_none, 0, MM_MAKE_TEXT(30, 5)}
};

static int mm_buttons_size()
{
    return sizeof(buttons) / sizeof(generic_button);
}
static void mm_lang_text_draw_centered(int group, int text, int i)
{
    lang_text_draw_centered(group, text, MM_BTN_LEFT, MM_BTN_TOP_NEXT(i) + 6, MM_BTN_WIDTH, FONT_NORMAL_GREEN);
}

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

static void mm_draw_background(void)
{
    image_draw_fullscreen_background(image_group(GROUP_INTERMEZZO_BACKGROUND));

    graphics_in_dialog();
    if (!data.logo_image_id) {
        data.logo_image_id = assets_get_image_id("UI_Elements", "Main Menu Banner");
    }
    image_draw(data.logo_image_id, 110, -50);
    graphics_reset_dialog();
    if (window_is(WINDOW_MAIN_MENU)) {
        draw_version_string();
    }
}

static void mm_draw_foreground(void)
{
    graphics_in_dialog();

    for (int i = 0; i < mm_buttons_size(); i++) {
        large_label_draw(buttons[i].x, buttons[i].y, buttons[i].width / BLOCK_SIZE, data.focus_button_id == i + 1 ? 1 : 0);
        mm_lang_text_draw_centered(MM_TEXT_GROUP(buttons[i].parameter2), MM_TEXT_ID(buttons[i].parameter2), i);
    }

    graphics_reset_dialog();
}

static void mm_handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);
    if (generic_buttons_handle_mouse(m_dialog, 0, 0, buttons, mm_buttons_size(), &data.focus_button_id)) {
        return;
    }
    if (h->escape_pressed) {
        hotkey_handle_escape();
    }
    if (h->load_file) {
        window_file_dialog_show(FILE_TYPE_SAVED_GAME, FILE_DIALOG_LOAD);
    }
}

void window_main_menu_show(int restart_music)
{
    if (restart_music) {
        sound_music_play_intro();
    }
    window_type window = {
        WINDOW_MAIN_MENU,
        mm_draw_background,
        mm_draw_foreground,
        mm_handle_input
    };
    window_show(&window);
}

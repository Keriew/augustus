#include "main_menu.h"

#include "assets/assets.h"
#include "core/calc.h"
#include "core/log.h"
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
#include "jsvm/jsvm.h"
#include "sound/music.h"
#include "window/cck_selection.h"
#include "window/config.h"
#include "window/file_dialog.h"
#include "window/new_career.h"
#include "window/plain_message_dialog.h"
#include "window/popup_dialog.h"

static struct {
    int focus_button_id;
    int logo_image_id;
} data;

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

static void draw_foreground(void)
{
    graphics_in_dialog();

    js_vm_exec_function("main_menu_foreground");

    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);

    js_vm_exec_function_args("main_menu_handle_input", "ii", m_dialog->x, m_dialog->y);
}

void window_main_menu_show(int restart_music)
{
    if (restart_music) {
        sound_music_play_intro();
    }
    window_type window = {
        WINDOW_MAIN_MENU,
        draw_background,
        draw_foreground,
        handle_input
    };
    window_show(&window);
}
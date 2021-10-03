#include "js_ui.h"

#include "js_defines.h"

#include "editor/editor.h"
#include "game/game.h"
#include "game/system.h"
#include "input/hotkey.h"
#include "mujs/mujs.h"
#include "sound/music.h"
#include "translation/translation.h"
#include "window/cck_selection.h"
#include "window/config.h"
#include "window/file_dialog.h"
#include "window/new_career.h"
#include "window/plain_message_dialog.h"
#include "window/popup_dialog.h"

void js_ui_show_escape_dialog(js_State *J)
{
    hotkey_handle_escape();
    js_pushundefined(J);
}

void js_ui_window_new_career_show(js_State *J)
{
    window_new_career_show();
    js_pushundefined(J);
}

void js_ui_window_cck_selection_show(js_State *J)
{
    window_cck_selection_show();
    js_pushundefined(J);
}

void js_ui_window_file_dialog_show(js_State *J)
{
    int type = js_tointeger(J, 1);
    int dialog_type = js_tointeger(J, 2);
    window_file_dialog_show(type, dialog_type);
    js_pushundefined(J);
}

void js_ui_window_mission_editor_show(js_State *J)
{
    if (!editor_is_present() || !game_init_editor()) {
        window_plain_message_dialog_show(
            TR_NO_EDITOR_TITLE, TR_NO_EDITOR_MESSAGE, 1);
    } else {
        sound_music_play_editor();
    }
    js_pushundefined(J);
}

void js_ui_window_config_show(js_State *J)
{
    window_config_show(CONFIG_FIRST_PAGE, 1);
    js_pushundefined(J);
}

static void confirm_exit(int accepted, int checked)
{
    if (accepted) {
        system_exit();
    }
}

void js_ui_window_popup_dialog_show(js_State *J)
{
    window_popup_dialog_show(POPUP_DIALOG_QUIT, confirm_exit, 1);
    js_pushundefined(J);
}

void js_register_ui_functions(js_State *J)
{
    DEF_GLOBAL_OBJECT(J, ui)
    REGISTER_GLOBAL_OBJECT(J, ui)

    REGISTER_GLOBAL_FUNCTION(J, js_ui_show_escape_dialog, "show_escape_dialog", 0);
    REGISTER_GLOBAL_FUNCTION(J, js_ui_window_file_dialog_show, "window_file_dialog_show", 2);
    REGISTER_GLOBAL_FUNCTION(J, js_ui_window_new_career_show, "window_new_career_show", 0);
    REGISTER_GLOBAL_FUNCTION(J, js_ui_window_cck_selection_show, "window_cck_selection_show", 0);
    REGISTER_GLOBAL_FUNCTION(J, js_ui_window_mission_editor_show, "window_mission_editor_show", 0);
    REGISTER_GLOBAL_FUNCTION(J, js_ui_window_config_show, "window_config_show", 0);
    REGISTER_GLOBAL_FUNCTION(J, js_ui_window_popup_dialog_show, "window_popup_dialog_show", 0);
}

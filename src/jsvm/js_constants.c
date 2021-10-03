#include "js_hotkey.h"

#include "js_defines.h"

#include "input/hotkey.h"
#include "mujs/mujs.h"
#include "window/file_dialog.h"

void js_register_game_constants(js_State *J)
{
    REGISTER_GLOBAL_CONSTANT(J, FILE_TYPE_SAVED_GAME);
    REGISTER_GLOBAL_CONSTANT(J, FILE_DIALOG_LOAD);
}
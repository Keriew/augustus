#include "js_mouse.h"

#include "core/file.h"
#include "core/log.h"
#include "jsvm/js_defines.h"
#include "mujs/mujs.h"

void js_game_log_info(js_State *J)
{
    if (js_isundefined(J, 1)) {
        log_info("log() Try to print undefined object", 0, 0);
    } else {
        log_info("", js_tostring(J, 1), 0);
    }

    js_pushundefined(J);
}

void js_game_log_warn(js_State *J)
{
    if (js_isundefined(J, 1)) {
        log_info("warning() Try to print undefined object", 0, 0);
    } else {
        log_info("WARN: ", js_tostring(J, 1), 0);
    }

    js_pushundefined(J);
}

void js_game_load_text(js_State *J)
{
    const char *path = js_tostring(J, 1);
    char *text = 0;

    FILE *ftext = file_open(path, "rb");
    fseek(ftext, 0, SEEK_END);
    long fsize = ftell(ftext);
    fseek(ftext, 0, SEEK_SET);  /* same as rewind(f); */

    text = (char *)malloc(fsize + 1);
    fread(text, 1, fsize, ftext);
    fclose(ftext);

    text[fsize] = 0;
    js_pushstring(J, text);
    free(text);
}

void js_register_game_functions(js_State *J)
{
    REGISTER_GLOBAL_FUNCTION(J, js_game_log_info, "log_info", 1);
    REGISTER_GLOBAL_FUNCTION(J, js_game_log_warn, "log_warning", 1);
    REGISTER_GLOBAL_FUNCTION(J, js_game_load_text, "load_text", 1);
}


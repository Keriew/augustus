#include "jsvm.h"

#include "core/file.h"
#include "core/log.h"
#include "graphics/window.h"
#include "jsvm/js_constants.h"
#include "jsvm/js_defines.h"
#include "jsvm/js_folder_notifier.h"
#include "jsvm/js_graphics.h"
#include "jsvm/js_game.h"
#include "jsvm/js_hotkey.h"
#include "jsvm/js_ui.h"
#include "jsvm/js_mouse.h"
#include "graphics/panel.h"
#include "mujs/mujs.h"
#include "mujs/jsi.h"
#include "mujs/jsvalue.h"


#define MAX_PATH 256
#define MAX_FILES_RELOAD 255
#define ASSETS_SCRIPTS "assets/scripts"
struct {
    char files2load[MAX_FILES_RELOAD][MAX_PATH];
    int files2load_num;
    int have_error;
    char error_str[MAX_PATH];
    js_State *J;
} vm;

void js_reset_vm_state(void);

int js_vm_trypcall(js_State *J, int params)
{
    if (vm.have_error)
        return 0;

    int error = js_pcall(vm.J, params);
    if (error) {
        vm.have_error = 1;
        const char *cur_symbol = js_tostring(vm.J, -1);
        const char *start_str = cur_symbol;
        while (*cur_symbol) {
            if (*cur_symbol != '\n') {
                cur_symbol++;
                continue;
            }

            memset(vm.error_str, 0, MAX_PATH);
            memcpy(vm.error_str, start_str, cur_symbol - start_str);
            start_str = cur_symbol + 1;
            cur_symbol += 2;
            log_info("!!! pcall error ", vm.error_str, 0);
        }
        log_info("!!! pcall error ", start_str, 0);
        js_pop(J, 1);
        return 0;
    }

    js_pop(vm.J, -1);
    return 1;
}

int js_vm_load_file_and_exec(const char *path)
{
    char rpath[MAX_PATH];
    int error = 0;
    if (!path)
        return 0;

    if (*path == ':')
        snprintf(rpath, MAX_PATH, "%s/%s", ASSETS_SCRIPTS, path + 1);

    if (!file_exists(rpath, 0)) {
        log_info("!!! Cant find script at", rpath, 0);
        return 0;
    }

    error = js_ploadfile(vm.J, rpath);
    if (error) {
        log_info("!!! Error on open file ", js_tostring(vm.J, -1), 0);
        return 0;
    }

    js_getglobal(vm.J, "");
    int ok = js_vm_trypcall(vm.J, 0);
    if (!ok) {
        log_info("Fatal error on call base after load ", path, 0);
        //js_pop(internal_J, 1);
        return 0;
    }
    return 1;
}

void js_vm_sync()
{
    if (!vm.files2load_num)
        return;

    if (vm.have_error) {
        js_reset_vm_state();
    }

    if (vm.files2load_num > 0) {
        for (int i = 0; i < vm.files2load_num; i++) {
            log_info("JS: script reloaded ", vm.files2load[i], 0);
            js_vm_load_file_and_exec(vm.files2load[i]);
        }

    }

    for (int i = 0; i < MAX_FILES_RELOAD; ++i)
        memset(vm.files2load[i], 0, MAX_PATH);

    vm.files2load_num = 0;
    vm.have_error = 0;
    window_request_refresh();
}

void js_vm_reload_file(const char *path)
{
    strncpy(vm.files2load[vm.files2load_num], path, MAX_PATH);
    vm.files2load_num++;
}

int js_vm_exec_function_args(const char *funcname, const char *szTypes, ...)
{
    if (vm.have_error)
        return 0;
    int i, ok, savetop;
    char msg[2] = { 0, 0 };
    va_list vl;

    if (vm.J == 0)
        return 1;

    //log_info("script-if:// exec function ", funcname, 0);

    savetop = js_gettop(vm.J);
    js_getglobal(vm.J, funcname);
    js_pushnull(vm.J);

    //  szTypes is the last argument specified; you must access
    //  all others using the variable-argument macros.
    va_start( vl, szTypes );

    // Step through the list.
    for( i = 0; szTypes[i] != '\0'; ++i ) {
        switch( szTypes[i] ) {   // Type to expect.
            case 'i':
                js_pushnumber(vm.J, va_arg(vl, int));
                break;
            case 'f':
                js_pushnumber(vm.J, va_arg(vl, float));
                break;
            case 'c':
                msg[0] = va_arg(vl, char);
                js_pushstring(vm.J, msg);
                break;
            case 's':
                js_pushstring(vm.J, va_arg(vl, char *));
                break;

            default:
                js_pushnull(vm.J);
                log_info("!!! Undefined value for js.pcall engine_js_push when find ", 0, 0);
                break;
        }
    }
    va_end( vl );

    ok = js_vm_trypcall(vm.J, (int)strlen(szTypes));
    if (!ok) {
        log_info("Fatal error on call function ", funcname, 0);
        return 0;
    }

    js_pop(vm.J, 2);
    if( savetop - js_gettop(vm.J) != 0 ) {
        log_info( "STACK grow for ", funcname, js_gettop(vm.J) );
    }
    return ok;
}

int js_vm_exec_function(const char *funcname)
{
    return js_vm_exec_function_args(funcname, "");
}

void js_vm_load_module(js_State *J)
{
    const char *scriptName = js_tostring(J, 1);

    strncpy(vm.files2load[vm.files2load_num], scriptName, MAX_PATH - 1);
    vm.files2load_num++;
}

int js_vm_get_absolute_path(const char *path, char *rpath, int max_path)
{
#if defined(_WIN32)
    char *p = _fullpath(rpath, path, _MAX_PATH);
#elif defined(__linux__) || defined(__macosx__)
    realpath(path, fpath);
#endif

    for (int i = 0; rpath != 0 && i < max_path; ++i)
        if (rpath[i] == '\\')
            rpath[i] = '/';

    return 0;
}

void js_game_panic(js_State *J)
{
    log_info("JSE !!! Uncaught exception: ", js_tostring(J, -1), 0);
}


void js_register_vm_functions(js_State *J)
{
    REGISTER_GLOBAL_FUNCTION(J, js_vm_load_module, "load_js_module", 1);
}

void js_reset_vm_state()
{
    if (vm.J) {
        js_freestate(vm.J);
        vm.J = NULL;
    }

    for (int i = 0; i < MAX_FILES_RELOAD; ++i)
        memset(vm.files2load[i], 0, MAX_PATH);
    vm.files2load_num = 0;
    vm.have_error = 0;

    vm.J = js_newstate(NULL, NULL, JS_STRICT);
    js_atpanic(vm.J, js_game_panic);

    js_register_vm_functions(vm.J);
    js_register_graphics_functions(vm.J);
    js_register_game_functions(vm.J);
    js_register_mouse_functions(vm.J);
    js_register_hotkey_functions(vm.J);
    js_register_game_constants(vm.J);
    js_register_ui_functions(vm.J);

    int ok = js_vm_load_file_and_exec(":modules.js");
    if (ok)
        js_pop(vm.J, 2); //restore stack after call js-function
    log_info( "STACK state ", 0, js_gettop(vm.J));
}

const char *js_vm_scripts_folder(void) {
    return ASSETS_SCRIPTS;
}

void js_vm_setup(void)
{
    vm.J = NULL;
    js_reset_vm_state();

    char abspath[MAX_PATH];
    js_vm_get_absolute_path(ASSETS_SCRIPTS, abspath, MAX_PATH);

    js_vm_notifier_watch_directory_init(abspath);
}
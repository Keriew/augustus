#include <SDL3/SDL.h>

#include <SDL3/SDL_main.h>

#include "core/config.h"
#include "core/encoding.h"
#include "core/file.h"
#include "core/lang.h"
#include "core/log.h"
#include "core/time.h"
#include "game/game.h"
#include "game/settings.h"
#include "game/system.h"
#include "graphics/screen.h"
#include "graphics/window.h"
#include "input/mouse.h"
#include "input/touch.h"
#include "platform/file_manager.h"
#include "platform/android/android.h"
#include "platform/arguments.h"
#include "platform/cursor.h"
#include "platform/emscripten/emscripten.h"
#include "platform/file_manager_cache.h"
#include "platform/ios/ios.h"
#include "platform/joystick.h"
#include "platform/keyboard_input.h"
#include "platform/log.h"
#include "platform/platform.h"
#include "platform/prefs.h"
#include "platform/renderer.h"
#include "platform/SDL3/screen.h"
#include "platform/touch.h"
#include "platform/switch/switch.h"
#include "platform/vita/vita.h"
#include "window/asset_previewer.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#include <windows.h>
#endif

#define INTPTR(d) (*(int*)(d))

enum {
    USER_EVENT_QUIT,
    USER_EVENT_RESIZE,
    USER_EVENT_FULLSCREEN,
    USER_EVENT_WINDOWED,
    USER_EVENT_CENTER_WINDOW,
};

static struct {
    int active;
    int quit;
    Uint32 user_event;
    struct {
        int frame_count;
        int last_fps;
        Uint32 last_update_time;
    } fps;
} data = { 1 };

#ifdef __IOS__
static augustus_args args;
static void setup(const augustus_args *args);
#endif

static void post_event(int code)
{
    SDL_Event event;
    event.user.type = data.user_event;
    event.user.code = code;
    SDL_PushEvent(&event);
}

int system_supports_select_folder_dialog(void)
{
#if !defined(__VITA__) && !defined(__SWITCH__) && !defined(__EMSCRIPTEN__)
    return 1;
#else
    return 0;
#endif
}

typedef struct {
    SDL_Mutex *mutex;
    SDL_Condition *condition;
    const char *folder_path;
    int dialog_closed;
} select_folder_dialog_data;

static void folder_dialog_closed(void *userdata, const char *const *filelist, int filter)
{
    select_folder_dialog_data *dialog_data = (select_folder_dialog_data *)userdata;
    SDL_LockMutex(dialog_data->mutex);

    if (!filelist) {
        printf("Error creating the file dialog window.\n");
        dialog_data->dialog_closed = 1;

        SDL_SignalCondition(dialog_data->condition);
        SDL_UnlockMutex(dialog_data->mutex);

        return;
    }

    if (!*filelist || !**filelist) {
        dialog_data->dialog_closed = 1;

        SDL_SignalCondition(dialog_data->condition);
        SDL_UnlockMutex(dialog_data->mutex);

        return;
    }

    static char folder_path[FILE_NAME_MAX];

    snprintf(folder_path, FILE_NAME_MAX, "%s", filelist[0]);

    dialog_data->folder_path = folder_path;

    dialog_data->dialog_closed = 1;

    SDL_SignalCondition(dialog_data->condition);
    SDL_UnlockMutex(dialog_data->mutex);
}

const char *system_show_select_folder_dialog(const char *title, const char *default_path)
{
    if (!system_supports_select_folder_dialog()) {
        return 0;
    }

    select_folder_dialog_data dialog_data = { 0 };

    dialog_data.mutex = SDL_CreateMutex();
    if (!dialog_data.mutex) {
        printf("Failed to create file dialog mutex. Reason: %s\n", SDL_GetError());
        return 0;
    }

    dialog_data.condition = SDL_CreateCondition();
    if (!dialog_data.condition) {
        printf("Failed to create file dialog condition. Reason: %s\n", SDL_GetError());
        SDL_DestroyMutex(dialog_data.mutex);
        return 0;
    }

    SDL_LockMutex(dialog_data.mutex);
    SDL_Window *window = platform_screen_get_window();

    if (title) {
        SDL_PropertiesID folder_dialog_properties = SDL_CreateProperties();
        if (folder_dialog_properties) {
            SDL_SetStringProperty(folder_dialog_properties, SDL_PROP_FILE_DIALOG_TITLE_STRING, title);
            if (default_path) {
                SDL_SetStringProperty(folder_dialog_properties, SDL_PROP_FILE_DIALOG_LOCATION_STRING, default_path);
            }
            if (window) {
                SDL_SetPointerProperty(folder_dialog_properties, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, window);
            }
            SDL_ShowFileDialogWithProperties(SDL_FILEDIALOG_OPENFOLDER, folder_dialog_closed,
                &dialog_data, folder_dialog_properties);
            SDL_DestroyProperties(folder_dialog_properties);
        } else {
            SDL_ShowOpenFolderDialog(folder_dialog_closed, &dialog_data, window, default_path, false);
        }
    } else {
        SDL_ShowOpenFolderDialog(folder_dialog_closed, &dialog_data, window, default_path, false);
    }

    while (!dialog_data.dialog_closed) {
        SDL_WaitConditionTimeout(dialog_data.condition, dialog_data.mutex, 30);
        SDL_PumpEvents();
    }

    SDL_UnlockMutex(dialog_data.mutex);

    SDL_DestroyCondition(dialog_data.condition);
    SDL_DestroyMutex(dialog_data.mutex);

    dialog_data.mutex = NULL;
    dialog_data.condition = NULL;

    return dialog_data.folder_path;
}

void system_exit(void)
{
    post_event(USER_EVENT_QUIT);
}

void system_resize(int width, int height)
{
    static int s_width;
    static int s_height;

    s_width = width;
    s_height = height;
    SDL_Event event;
    event.user.type = data.user_event;
    event.user.code = USER_EVENT_RESIZE;
    event.user.data1 = &s_width;
    event.user.data2 = &s_height;
    SDL_PushEvent(&event);
}

void system_center(void)
{
    post_event(USER_EVENT_CENTER_WINDOW);
}

void system_set_fullscreen(int fullscreen)
{
    post_event(fullscreen ? USER_EVENT_FULLSCREEN : USER_EVENT_WINDOWED);
}

uint64_t system_get_ticks(void)
{
    return SDL_GetTicks();
}

#ifdef _WIN32
#define PLATFORM_ENABLE_PER_FRAME_CALLBACK
static void platform_per_frame_callback(void)
{
    platform_screen_recreate_texture();
}
#endif

static void run_and_draw(void)
{
    time_millis time_before_run = system_get_ticks();
    time_set_millis(time_before_run);

    game_run();
    game_draw();
    Uint32 time_after_draw = system_get_ticks();

    data.fps.frame_count++;
    if (time_after_draw - data.fps.last_update_time > 1000) {
        data.fps.last_fps = data.fps.frame_count;
        data.fps.last_update_time = time_after_draw;
        data.fps.frame_count = 0;
    }

    if (config_get(CONFIG_UI_DISPLAY_FPS)) {
        game_display_fps(data.fps.last_fps);
    }

    platform_renderer_render();
}

static void handle_mouse_position(Uint32 which, Uint32 windowID, float x, float y)
{
    if (which != SDL_TOUCH_MOUSEID) {
        SDL_Window *window = SDL_GetWindowFromID(windowID);
        if (window && !SDL_GetWindowRelativeMouseMode(window)) {
            float fx = x;
            float fy = y;
            SDL_Renderer *renderer = SDL_GetRenderer(window);
            if (renderer) {
                SDL_RenderCoordinatesFromWindow(renderer, x, y, &fx, &fy);
            }
            mouse_set_position((int) fx, (int) fy);
        }
    }
}

static void handle_mouse_button(SDL_MouseButtonEvent *event, int is_down)
{
    handle_mouse_position(event->which, event->windowID, event->x, event->y);

    if (event->button == SDL_BUTTON_LEFT) {
        mouse_set_left_down(is_down);
    } else if (event->button == SDL_BUTTON_MIDDLE) {
        mouse_set_middle_down(is_down);
    } else if (event->button == SDL_BUTTON_RIGHT) {
        mouse_set_right_down(is_down);
    } else if (event->button == SDL_BUTTON_X1) {
        if (is_down) {
            hotkey_key_pressed(KEY_TYPE_MOUSE_BUTTON_4, hotkey_get_modifiers(), 0);
        } else {
            hotkey_key_released(KEY_TYPE_MOUSE_BUTTON_4, hotkey_get_modifiers());
        }
    } else if (event->button == SDL_BUTTON_X2) {
        if (is_down) {
            hotkey_key_pressed(KEY_TYPE_MOUSE_BUTTON_5, hotkey_get_modifiers(), 0);
        } else {
            hotkey_key_released(KEY_TYPE_MOUSE_BUTTON_5, hotkey_get_modifiers());
        }
    }
}

static void handle_window_event(SDL_WindowEvent *event, int *window_active)
{
    switch (event->type) {
            case SDL_EVENT_WINDOW_MOUSE_ENTER:
            mouse_set_inside_window(1);
            break;
        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            mouse_set_inside_window(0);
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            mouse_set_window_focus(0);
            break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            mouse_set_window_focus(1);
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            SDL_Log("Window resized to %d x %d", (int) event->data1, (int) event->data2);
            platform_screen_resize(event->data1, event->data2, 1);
            break;
        case SDL_EVENT_WINDOW_RESIZED:
            SDL_Log("System resize to %d x %d", (int) event->data1, (int) event->data2);
            break;
        case SDL_EVENT_WINDOW_MOVED:
            SDL_Log("Window move to coordinates x: %d y: %d\n", (int) event->data1, (int) event->data2);
            platform_screen_move(event->data1, event->data2);
            break;

        case SDL_EVENT_WINDOW_SHOWN:
            SDL_Log("Window %u shown", (unsigned int) event->windowID);
#ifdef USE_FILE_CACHE
            platform_file_manager_cache_invalidate();
#endif
            *window_active = 1;
            break;
        case SDL_EVENT_WINDOW_HIDDEN:
            SDL_Log("Window %u hidden", (unsigned int) event->windowID);
            *window_active = 0;
            break;

        case SDL_EVENT_WINDOW_EXPOSED:
            SDL_Log("Window %u exposed", (unsigned int) event->windowID);
            window_invalidate();
            break;

        default:
            break;

    }
}

static bool handle_event_immediate(void *unused, SDL_Event *event)
{
    switch (event->type) {
        case SDL_EVENT_WILL_ENTER_BACKGROUND:
            platform_renderer_pause();
            return 0;
        case SDL_EVENT_DID_ENTER_FOREGROUND:
            platform_renderer_resume();
            // fallthrough
        case SDL_EVENT_RENDER_TARGETS_RESET:
            platform_renderer_invalidate_target_textures();
            window_invalidate();
            return 0;
        case SDL_EVENT_RENDER_DEVICE_RESET:
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING,
                "Render device lost",
                "The rendering context was lost.The game will likely blackscreen.\n\n"
                "Please restart the game to fix the issue.",
                NULL);
            return 0;
        case SDL_EVENT_TERMINATING:
            data.quit = 1;
            return 0;
        default:
            return 1;
    }
}

static void handle_event(SDL_Event *event)
{
    switch (event->type) {
        case SDL_EVENT_KEY_DOWN:
            platform_handle_key_down(&event->key);
            break;
        case SDL_EVENT_KEY_UP:
            platform_handle_key_up(&event->key);
            break;
        case SDL_EVENT_TEXT_INPUT:
            platform_handle_text(&event->text);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            handle_mouse_position(event->motion.which, event->motion.windowID, event->motion.x, event->motion.y);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event->button.which != SDL_TOUCH_MOUSEID) {
                handle_mouse_button(&event->button, 1);
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event->button.which != SDL_TOUCH_MOUSEID) {
                handle_mouse_button(&event->button, 0);
            }
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            if (event->wheel.which != SDL_TOUCH_MOUSEID) {
                mouse_set_scroll(event->wheel.y > 0 ? SCROLL_UP : event->wheel.y < 0 ? SCROLL_DOWN : SCROLL_NONE);
            }
            break;

        case SDL_EVENT_FINGER_DOWN:
            platform_touch_start(&event->tfinger);
            break;
        case SDL_EVENT_FINGER_MOTION:
            platform_touch_move(&event->tfinger);
            break;
        case SDL_EVENT_FINGER_UP:
            platform_touch_end(&event->tfinger);
            break;

        case SDL_EVENT_JOYSTICK_AXIS_MOTION:
            if (platform_joystick_is_enabled()) {
                joystick_update_element(event->jaxis.which, JOYSTICK_ELEMENT_AXIS,
                    event->jaxis.axis, event->jaxis.value, 0);
            }
            break;
        case SDL_EVENT_JOYSTICK_BALL_MOTION:
            if (platform_joystick_is_enabled()) {
                joystick_update_element(event->jball.which, JOYSTICK_ELEMENT_TRACKBALL,
                    event->jball.ball, event->jball.xrel, event->jball.yrel);
            }
            break;
        case SDL_EVENT_JOYSTICK_HAT_MOTION:
            if (platform_joystick_is_enabled()) {
                joystick_update_element(event->jhat.which, JOYSTICK_ELEMENT_HAT,
                    event->jhat.hat, platform_joystick_convert_hat_position(event->jhat.value), 0);
            }
            break;
        case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
            if (platform_joystick_is_enabled()) {
                joystick_update_element(event->jbutton.which, JOYSTICK_ELEMENT_BUTTON,
                    event->jbutton.button, 1, 0);
            }
            break;
        case SDL_EVENT_JOYSTICK_BUTTON_UP:
            if (platform_joystick_is_enabled()) {
                joystick_update_element(event->jbutton.which, JOYSTICK_ELEMENT_BUTTON,
                    event->jbutton.button, 0, 0);
            }
            break;
        case SDL_EVENT_JOYSTICK_ADDED:
            platform_joystick_device_changed(event->jdevice.which, 1);
            break;
        case SDL_EVENT_JOYSTICK_REMOVED:
            platform_joystick_device_changed(event->jdevice.which, 0);
            break;

        case SDL_EVENT_QUIT:
            data.quit = 1;
            break;

        default:
            if (event->type >= SDL_EVENT_WINDOW_FIRST && event->type <= SDL_EVENT_WINDOW_LAST) {
                handle_window_event(&event->window, &data.active);
            } else if (event->type == data.user_event) {
                switch (event->user.code) {
                    case USER_EVENT_QUIT:
                        data.quit = 1;
                        break;
                    case USER_EVENT_RESIZE:
                        platform_screen_set_window_size(INTPTR(event->user.data1), INTPTR(event->user.data2));
                        break;
                    case USER_EVENT_FULLSCREEN:
                        platform_screen_set_fullscreen();
                        break;
                    case USER_EVENT_WINDOWED:
                        platform_screen_set_windowed();
                        break;
                    case USER_EVENT_CENTER_WINDOW:
                        platform_screen_center_window();
                        break;
                    default:
                        break;
                }
            }
            break;
    }
}

static void teardown(void)
{
    SDL_Log("Exiting game");
    game_exit();
    platform_screen_destroy();
    platform_log_teardown();
    SDL_Quit();

#ifdef __IOS__
    // iOS apps are not allowed to self-terminate. To avoid being stuck on a blank screen here, we start the game again.
    setup(&args);
#endif
}

static void main_loop(void)
{
    SDL_Event event;
#ifdef PLATFORM_ENABLE_PER_FRAME_CALLBACK
    platform_per_frame_callback();
#endif
    /* Process event queue */
    while (SDL_PollEvent(&event)) {
        handle_event(&event);
    }
    if (data.quit) {
#ifdef __EMSCRIPTEN__
        emscripten_cancel_main_loop();
#endif
        teardown();
#ifdef __EMSCRIPTEN__
        EM_ASM(
            Module.quitGame();
        );
#endif
        return;
    }
    if (data.active) {
        run_and_draw();
    } else {
        SDL_WaitEvent(NULL);
    }
}

static int init_sdl(int enable_joysticks)
{
    SDL_Log("Initializing SDL");

    // Windows: use directsound by default, as wasapi has issues
    // This needs to be done here before SDL_Init, otherwise SDL may initialize the wrong driver
#ifdef _WIN32
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "directsound");
#endif

    SDL_SetHint(SDL_HINT_TRACKPAD_IS_TOUCH_ONLY, "0");
    SDL_SetHint(SDL_HINT_VITA_ENABLE_BACK_TOUCH, "0");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");

    if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_JOYSTICK)) {
        // Try starting SDL without joystick support
        if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not initialize SDL: %s", SDL_GetError());
            return 0;
        } else {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Could not enable joystick support");
        }
    } else {
        platform_joystick_init(enable_joysticks);
    }
    data.user_event = SDL_RegisterEvents(1);
    if (!data.user_event) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not register user event: %s", SDL_GetError());
        return 0;
    }

    SDL_SetEventFilter(handle_event_immediate, 0);

    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    
    int version = SDL_GetVersion();
    SDL_Log("SDL initialized, version %u.%u.%u",
        SDL_VERSIONNUM_MAJOR(version), SDL_VERSIONNUM_MINOR(version), SDL_VERSIONNUM_MICRO(version));
    return 1;
}

static const char *ask_for_data_dir(int again)
{
    if (again) {
        const SDL_MessageBoxButtonData buttons[] = {
            {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "OK"},
            {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel"}
        };
        const SDL_MessageBoxData messageboxdata = {
            SDL_MESSAGEBOX_WARNING, NULL, "Wrong folder selected",
            "The selected folder is not a proper Caesar 3 folder.\n\n"
#ifndef __IOS__
            "Please select a path directly from either the internal storage "
            "or the SD card, otherwise the path may not be recognised.\n\n"
#endif
            "Press OK to select another folder or Cancel to exit.",
            SDL_arraysize(buttons), buttons, NULL
        };
        int result;
        SDL_ShowMessageBox(&messageboxdata, &result);
        if (!result) {
            return NULL;
        }
    }
#ifdef __ANDROID__
    if (!android_show_c3_path_dialog(again)) {
        return 0;
    }
    while (!android_has_c3_path()) {
        SDL_WaitEventTimeout(NULL, 2000);
    }
    return android_get_c3_path();
#elif defined __IOS__
    return ios_show_c3_path_dialog(again);
#else
    return system_show_select_folder_dialog("Please select your Caesar 3 folder", 0);
#endif
}

static int pre_init(const char *custom_data_dir)
{
    if (custom_data_dir) {
        SDL_Log("Loading game from %s", custom_data_dir);
        if (!platform_file_manager_set_base_path(custom_data_dir)) {
            SDL_Log("%s: directory not found", custom_data_dir);
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                "Error",
                "Augustus requires the original files from Caesar 3.\n\n"
                "Please enter the proper directory or copy the files to the selected directory.",
                NULL);
            return 0;
        }
        return game_pre_init();
    }

    SDL_Log("Loading game from working directory");
    if (game_pre_init()) {
        return 1;
    }

#ifdef __IOS__
    const char *base_path = ios_get_base_path();
#else
    const char *base_path = platform_get_base_path();
#endif
    if (base_path) {
        if (platform_file_manager_set_base_path(base_path)) {
            SDL_Log("Loading game from base path %s", base_path);
            if (game_pre_init()) {
                return 1;
            }
        }
    }

    if (system_supports_select_folder_dialog()) {

        const char *user_dir = pref_data_dir();
        if (*user_dir) {
            SDL_Log("Loading game from user pref %s", user_dir);
            if (platform_file_manager_set_base_path(user_dir) && game_pre_init()) {
                return 1;
            }
        }

        user_dir = ask_for_data_dir(0);
        while (user_dir) {
            SDL_Log("Loading game from user-selected dir %s", user_dir);
            if (platform_file_manager_set_base_path(user_dir) && game_pre_init()) {
                pref_save_data_dir(user_dir);
#ifdef __ANDROID__
                SDL_ShowAndroidToast("C3 files found. Path saved.", 0, 0, 0, 0);
#endif
                return 1;
            }
            user_dir = ask_for_data_dir(1);
        }
    } else {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
            "Augustus requires the original files from Caesar 3 to run.",
            "Please move the Augustus executable to the directory containing an existing "
            "Caesar 3 installation, or run:\naugustus path-to-c3-directory",
            NULL);
    }

    return 0;
}

static void setup(const augustus_args *args)
{
    system_setup_crash_handler();
    platform_log_setup();

    SDL_Log("Augustus version %s, %s build", system_version(), system_architecture());
    SDL_Log("Running on: %s", system_OS());

    if (!init_sdl(args->enable_joysticks)) {
        SDL_Log("Exiting: SDL init failed");
        exit_with_status(-1);
    }

#ifdef __vita__
    const char *base_dir = VITA_PATH_PREFIX;
#else
    const char *base_dir = args->data_directory;
#endif

    if (!pre_init(base_dir)) {
        SDL_Log("Exiting: game pre-init failed");
        exit_with_status(1);
    }

    // If starting the log file failed (because, for example, the executable path isn't writable)
    // try again, placing the log file on the C3 path
    if (!platform_log_is_ready()) {
        platform_log_setup();
    }

    if (args->force_windowed && setting_fullscreen()) {
        int w, h;
        setting_window(&w, &h);
        setting_set_display(0, w, h);
        SDL_Log("Forcing windowed mode with size %d x %d", w, h);
    }
    if (args->force_fullscreen && !setting_fullscreen()) {
        setting_set_display(1, 0, 0);
        SDL_Log("Forcing fullscreen mode");
    }

    // handle arguments
    if (args->display_scale_percentage) {
        config_set(CONFIG_SCREEN_DISPLAY_SCALE, args->display_scale_percentage);
    }
    if (args->cursor_scale_percentage) {
        config_set(CONFIG_SCREEN_CURSOR_SCALE, args->cursor_scale_percentage);
    }

    char title[100];
    encoding_to_utf8(lang_get_string(9, 0), title, 100, 0);
    if (!platform_screen_create(title, config_get(CONFIG_SCREEN_DISPLAY_SCALE), args->display_id)) {
        SDL_Log("Exiting: SDL create window failed");
        exit_with_status(-2);
    }

#ifdef PLATFORM_ENABLE_INIT_CALLBACK
    platform_init_callback();
#endif

    if (args->use_software_cursor) {
        platform_cursor_force_software_mode();
    }

    // This has to come after platform_screen_create, otherwise it fails on Nintendo Switch
    system_init_cursors(config_get(CONFIG_SCREEN_CURSOR_SCALE));

    // If there's no hardware cursor support and no joysticks, let's assume there are touch controls and hide the cursor,
    // otherwise it would be annoying to have a cursor permanently on screen with no way to move it
    if (!platform_cursor_has_hardware_cursor() && !joysticks_are_connected()) {
        system_hide_cursor();
    }

    time_set_millis(system_get_ticks());

    int result = args->launch_asset_previewer ? window_asset_previewer_show() : game_init();

    if (!result) {
        SDL_Log("Exiting: game init failed");
        exit_with_status(2);
    }

    data.quit = 0;
    data.active = 1;
}

int main(int argc, char **argv)
{
    augustus_args args;
    if (!platform_parse_arguments(argc, argv, &args)) {
#if !defined(_WIN32) && !defined(__vita__) && !defined(__SWITCH__) && !defined(__ANDROID__) && !defined(__APPLE__)
        // Only exit on Linux platforms where we know the system will not throw any weird arguments our way
        exit_with_status(1);
#endif
    }

    setup(&args);

    mouse_set_inside_window(1);
    mouse_set_window_focus(1);
    run_and_draw();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(main_loop, 0, 1);
#else
    while (!data.quit) {
        main_loop();
    }
#endif

    return 0;
}

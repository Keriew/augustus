#include "platform/android/android.h"

#include "platform/android/jni.h"
#include "platform/SDL3/screen.h"

#include <SDL3/SDL.h>

JNIEnv *jni_get_env(void)
{
    return SDL_GetAndroidJNIEnv();
}

jobject jni_get_activity(void)
{
    return SDL_GetAndroidActivity();
}

void platform_show_virtual_keyboard(void)
{
    SDL_Window *window = platform_screen_get_window();
    if (!SDL_TextInputActive(window)) {
        SDL_StartTextInput(window);
    }
}

void platform_hide_virtual_keyboard(void)
{
    SDL_Window *window = platform_screen_get_window();
    if (SDL_TextInputActive(window)) {
        SDL_StopTextInput(window);
    }
}

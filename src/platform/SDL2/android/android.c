#include "platform/android/android.h"

#include "platform/android/jni.h"

#include "SDL.h"

JNIEnv *jni_get_env(void)
{
    return SDL_AndroidGetJNIEnv();
}

jobject jni_get_activity(void)
{
    return SDL_AndroidGetActivity();
}

void platform_show_virtual_keyboard(void)
{
    if (!SDL_IsTextInputActive()) {
        SDL_StartTextInput();
    }
}

void platform_hide_virtual_keyboard(void)
{
    if (SDL_IsTextInputActive()) {
        SDL_StopTextInput();
    }
}

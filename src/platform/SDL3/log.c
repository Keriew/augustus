#include "platform/log.h"

#include <SDL3/SDL.h>

static void (*write_function)(const char *message, int is_error);

static void write_log(void *userdata, int category, SDL_LogPriority priority, const char *message)
{
    if (write_function) {
        write_function(message, priority == SDL_LOG_PRIORITY_ERROR);
    }
}

void platform_log_set_output_function(void(*callback)(const char *message, int is_error))
{
    write_function = callback;
    SDL_SetLogOutputFunction(write_log, NULL);
}

void platform_log_message(const char *message, int is_error)
{
    if (is_error) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", message);
    } else {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s", message);
    }
}

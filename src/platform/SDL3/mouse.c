#include "core/calc.h"
#include "core/config.h"
#include "game/system.h"
#include "input/mouse.h"
#include "platform/SDL3/screen.h"

#include "SDL3/SDL.h"

static struct {
    int x;
    int y;
    int enabled;
} data;

void system_mouse_get_relative_state(int *x, int *y)
{
    float fx, fy;
    SDL_GetRelativeMouseState(&fx, &fy);
    if (x) {
        *x = (int) fx;
    }
    if (y) {
        *y = (int) fy;
    }
}

void system_mouse_set_relative_mode(int enabled)
{
    if (enabled == data.enabled) {
        return;
    }
    if (enabled) {
        float fx, fy;
        SDL_GetMouseState(&fx, &fy);
        int scale_percentage = calc_percentage(100, platform_screen_get_scale());
        data.x = calc_adjust_with_percentage((int) fx, scale_percentage);
        data.y = calc_adjust_with_percentage((int) fy, scale_percentage);
        SDL_SetWindowRelativeMouseMode(platform_screen_get_window(), true);
        system_mouse_get_relative_state(NULL, NULL);
    } else {
        SDL_SetWindowRelativeMouseMode(platform_screen_get_window(), false);
        system_set_mouse_position(&data.x, &data.y);
        mouse_set_position(data.x, data.y);
    }
    data.enabled = enabled;
}

void system_move_mouse_cursor(int delta_x, int delta_y)
{
    int x = mouse_get()->x + delta_x;
    int y = mouse_get()->y + delta_y;
    system_set_mouse_position(&x, &y);
    mouse_set_position(x, y);
}

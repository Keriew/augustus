#include "platform/touch.h"

#include "core/time.h"
#include "game/settings.h"
#include "graphics/screen.h"
#include "input/touch.h"

#include <SDL3/SDL.h>

static SDL_FingerID touch_id[MAX_ACTIVE_TOUCHES];

static touch_coords get_touch_coordinates(float x, float y, SDL_WindowID windowID)
{
    touch_coords coords;
    float fx = x;
    float fy = y;
    int width = screen_width();
    int height = screen_height();
    SDL_Window *window = SDL_GetWindowFromID(windowID);
    if (window) {
        SDL_Renderer *renderer = SDL_GetRenderer(window);
        if (renderer) {
            SDL_RenderCoordinatesFromWindow(renderer, x, y, &fx, &fy);
            SDL_GetWindowSize(window, &width, &height);
        }
    }
    coords.x = (int) (fx * width);
    coords.y = (int) (fy * height);
    return coords;
}

static int get_touch_index(SDL_FingerID id)
{
    for (int i = 0; i < MAX_ACTIVE_TOUCHES; ++i) {
        if (touch_id[i] == id && touch_in_use(i)) {
            return i;
        }
    }
    return MAX_ACTIVE_TOUCHES;
}

void platform_touch_start(void *event)
{
    SDL_TouchFingerEvent *e = (SDL_TouchFingerEvent *)event;
    int index = touch_create(get_touch_coordinates(e->x, e->y, e->windowID), e->timestamp / 1000000);
    if (index != MAX_ACTIVE_TOUCHES) {
        touch_id[index] = e->fingerID;
    }
}

void platform_touch_move(void *event)
{
    SDL_TouchFingerEvent *e = (SDL_TouchFingerEvent *)event;
    touch_move(get_touch_index(e->fingerID), get_touch_coordinates(e->x, e->y, e->windowID), e->timestamp / 1000000);
}

void platform_touch_end(void *event)
{
    SDL_TouchFingerEvent *e = (SDL_TouchFingerEvent *)event;
    touch_end(get_touch_index(e->fingerID), e->timestamp / 1000000);
}

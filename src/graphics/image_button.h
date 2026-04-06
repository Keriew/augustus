#pragma once

#include "input/mouse.h"

#include "core/time.h"

#ifdef __cplusplus
extern "C" {
#endif

void button_none(int param1, int param2);

enum {
    IB_NORMAL = 4,
    IB_SCROLL = 6,
    IB_BUILD = 2
};

typedef struct {
    short x_offset;
    short y_offset;
    short width;
    short height;
    short button_type;
    short image_collection;
    short image_offset;
    void (*left_click_handler)(int param1, int param2);
    void (*right_click_handler)(int param1, int param2);
    int parameter1;
    int parameter2;
    char enabled;
    // optional, used for buttons made with Augustus assets
    const char *assetlist_name;
    const char *image_name;
    // state
    char pressed;
    char focused;
    time_millis pressed_since;
    char dont_draw; // use for skipping buttons dynamically when drawing an array
    char static_image; // use for buttons that do not need handling of focused or pressed state
} image_button;

void image_buttons_draw(int x, int y, image_button *buttons, unsigned int num_buttons);

int image_buttons_handle_mouse(
    const mouse *m, int x, int y, image_button *buttons, unsigned int num_buttons, unsigned int *focus_button_id);

#ifdef __cplusplus
}
#endif

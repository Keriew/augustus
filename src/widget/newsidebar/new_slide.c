#include "new_slide.h"

#include "core/speed.h"
#include "graphics/graphics.h"
#include "graphics/menu.h"
#include "graphics/window.h"
#include "sound/effect.h"
#include "widget/newsidebar/new_common.h"
#include "widget/top_menu.h"
#include "window/city.h"

#define SLIDE_SPEED 7
#define SLIDE_ACCELERATION_MILLIS 65
#define SIDEBAR_DECELERATION_OFFSET 125

static struct {
    int position;
    speed_type slide_speed;
    slide_direction direction;
    back_new_sidebar_draw_function back_new_sidebar_draw;
    front_new_sidebar_draw_function front_new_sidebar_draw;
    slide_finished_function finished_callback;
} data;

static void draw_background(void)
{
    widget_top_menu_draw(1);
    window_city_draw();
}

static void draw_sliding_foreground(void)
{
    window_request_refresh();
    data.position += speed_get_delta(&data.slide_speed);
    int is_finished = 0;
    if (data.position >= SIDEBAR_ADVANCED_WIDTH) {
        data.position = SIDEBAR_ADVANCED_WIDTH;
        is_finished = 1;
    }

    int x_offset = new_sidebar_common_get_x_offset_advanced();
    graphics_set_clip_rectangle(x_offset, TOP_MENU_HEIGHT, SIDEBAR_ADVANCED_WIDTH, new_sidebar_common_get_height());

    if (data.direction == SLIDE_DIRECTION_IN) {
        if (data.position > SIDEBAR_DECELERATION_OFFSET) {
            speed_set_target(&data.slide_speed, 1, SLIDE_ACCELERATION_MILLIS, 1);
        }
        x_offset += SIDEBAR_ADVANCED_WIDTH - data.position;
    } else {
        x_offset += data.position;
    }

    data.back_new_sidebar_draw();
    data.front_new_sidebar_draw(x_offset);

    graphics_reset_clip_rectangle();

    if (is_finished) {
        data.finished_callback();
    }
}

void new_sidebar_slide(slide_direction direction, back_new_sidebar_draw_function back_new_sidebar_callback,
    front_new_sidebar_draw_function front_new_sidebar_callback, slide_finished_function finished_callback)
{
    data.direction = direction;
    data.position = 0;
    speed_clear(&data.slide_speed);
    speed_set_target(&data.slide_speed, SLIDE_SPEED,
        direction == SLIDE_DIRECTION_OUT ? SLIDE_ACCELERATION_MILLIS : SPEED_CHANGE_IMMEDIATE, 1);
    data.back_new_sidebar_draw = back_new_sidebar_callback;
    data.front_new_sidebar_draw = front_new_sidebar_callback;
    data.finished_callback = finished_callback;
    sound_effect_play(SOUND_EFFECT_SIDEBAR);

    window_type window = {
        WINDOW_SLIDING_SIDEBAR,
        draw_background,
        draw_sliding_foreground,
        0,
        0
    };
    window_show(&window);
}

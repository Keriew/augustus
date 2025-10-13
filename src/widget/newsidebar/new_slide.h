#ifndef WIDGET_NEW_SIDEBAR_SLIDE_H
#define WIDGET_NEW_SIDEBAR_SLIDE_H

typedef enum {
    SLIDE_DIRECTION_IN = 0,
    SLIDE_DIRECTION_OUT = 1
} slide_direction;

typedef void (*back_new_sidebar_draw_function)(void);
typedef back_new_sidebar_draw_function slide_finished_function;
typedef void (*front_new_sidebar_draw_function)(int x_offset);

void new_sidebar_slide(slide_direction direction, back_new_sidebar_draw_function back_new_sidebar_callback,
    front_new_sidebar_draw_function front_new_sidebar_callback, slide_finished_function finished_callback);

#endif // WIDGET_NEW_SIDEBAR_SLIDE_H

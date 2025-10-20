#include "city.h"

#include "city/view.h"
#include "graphics/window.h"
#include "widget/city.h"
#include "widget/sidebar/slide.h"
#include "window/city.h"

#include "advanced/sidebar.h"
#include "collapsed/sidebar.h"
#include "expanded/sidebar.h"
#include "graphics/screen.h"

typedef enum {EXPANDED, COLLAPSED, ADVANCED } States;

static struct {
    States currentState;
    States previousState;
} data;


static void draw_slide_old(void)
{
    int x_offset = screen_width();

    switch (data.previousState) {
        case EXPANDED:
            x_offset -= expanded_sidebar_width();
            draw_expanded_sidebar_background(x_offset);
            break;
        case COLLAPSED:
            x_offset -= collapsed_sidebar_width();
            draw_collapsed_sidebar_background(x_offset);
            break;
        case ADVANCED:
            x_offset -= advanced_sidebar_width();
            draw_advanced_sidebar_background(x_offset);
            break;
        default:;
    }
}

static void draw_slide_new(const int x_offset)
{
    switch (data.currentState) {
        case EXPANDED:
            draw_expanded_sidebar_background(x_offset);
            break;
        case COLLAPSED:
            draw_collapsed_sidebar_background(x_offset);
            break;
        case ADVANCED:
            draw_advanced_sidebar_background(x_offset);
            break;
        default: ;
    }
}

static void slide_finished(void)
{
    city_view_toggle_sidebar();
    window_city_show();
}

void sidebar_next(void)
{
    data.previousState = data.currentState;
    const int previous_sidebar_width = widget_sidebar_city_get_width();
    const int previous_sidebar_x_offset = widget_sidebar_calculate_x_offset();
    slide_direction slide_direction = SLIDE_DIRECTION_OUT;

    if (data.currentState == COLLAPSED) {
        slide_direction = SLIDE_DIRECTION_IN;
        data.currentState = ADVANCED;
    } else if (data.currentState == ADVANCED) {
        data.currentState = EXPANDED;
    } else if (data.currentState == EXPANDED) {
        slide_direction = SLIDE_DIRECTION_IN;
        data.currentState = COLLAPSED;
    }

    city_view_start_sidebar_toggle();
    sidebar_slide(slide_direction,
                  draw_slide_old,
                  draw_slide_new,
                  slide_finished,
                  previous_sidebar_width,
                  previous_sidebar_x_offset);

    window_city_show();
}

int widget_sidebar_city_get_width(void)
{
    switch (data.currentState) {
        case EXPANDED:
            return expanded_sidebar_width();
        case COLLAPSED:
            return collapsed_sidebar_width();
        case ADVANCED:
            return advanced_sidebar_width();
        default:
            return 0;
    }
}

int widget_sidebar_is_collapsed(void)
{
    return data.currentState == COLLAPSED ? 1 : 0;
}

void widget_sidebar_city_draw_background(void)
{
    int x_offset  = screen_width();

    switch (data.currentState) {
        case EXPANDED:
            x_offset -= expanded_sidebar_width();
            draw_expanded_sidebar_background(x_offset);
            break;
        case COLLAPSED:
            x_offset -= collapsed_sidebar_width();
            draw_collapsed_sidebar_background(x_offset);
            break;
        case ADVANCED:
            x_offset -= advanced_sidebar_width();
            draw_advanced_sidebar_background(x_offset);
            break;
        default:;
    }
}

void widget_sidebar_city_draw_foreground(void)
{
    switch (data.currentState) {
        case EXPANDED:
            draw_expanded_sidebar_foreground();
            break;
        case COLLAPSED:
            draw_collapsed_sidebar_foreground();
            break;
        case ADVANCED:
            draw_advanced_sidebar_foreground();
            break;
        default:;
    }
}

int widget_sidebar_city_handle_mouse(const mouse *m)
{
    switch (data.currentState) {
        case EXPANDED:
            return handle_expanded_sidebar_mouse(m);
        case COLLAPSED:
            return handle_collapsed_sidebar_mouse(m);
        case ADVANCED:
            return handle_advanced_sidebar_mouse(m);
        default:
            return 0;
    }
}

int widget_sidebar_city_handle_mouse_build_menu(const mouse *m)
{
    switch (data.currentState) {
        case EXPANDED:
            return handle_expanded_sidebar_mouse_build_menu(m);
        case COLLAPSED:
            return handle_collapsed_sidebar_mouse_build_menu(m);
        case ADVANCED:
            return handle_advanced_sidebar_mouse_build_menu(m);
        default:
            return 0;
    }
}

unsigned int widget_sidebar_city_get_tooltip_text(tooltip_context *c)
{
    switch (data.currentState) {
        case EXPANDED:
            return get_expanded_sidebar_tooltip_text(c);
        case COLLAPSED:
            return get_collapsed_sidebar_tooltip_text(c);
        case ADVANCED:
            return get_advanced_sidebar_tooltip_text(c);
        default:
            return 0;
    }
}

int widget_sidebar_calculate_x_offset(void)
{
    switch (data.currentState) {
        case EXPANDED:
            return calculate_x_offset_sidebar_expanded();
        case COLLAPSED:
            return calculate_x_offset_sidebar_collapsed();
        case ADVANCED:
            return calculate_x_offset_sidebar_advanced();
        default:
            return 0;
    }
}

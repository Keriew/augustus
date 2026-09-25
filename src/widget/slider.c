#include "widget/slider.h"

#include "assets/assets.h"
#include "core/calc.h"
#include "core/image.h"
#include "graphics/image.h"
#include "graphics/panel.h"
#include "graphics/window.h"
#include "sound/effect.h"

#include <string.h>

#define SLIDER_BUTTON_SIDE 24
#define SLIDER_BOX_HEIGHT 48
#define SLIDER_BOX_WIDTH 24
#define TOTAL_SLIDER_BUTTON_HEIGHT (2 * SLIDER_BUTTON_SIDE + SLIDER_BOX_HEIGHT)
#define MIN_THUMB_OFFSET SLIDER_BUTTON_SIDE // prevents the thumb from overlapping the decrease button

typedef enum slider_element {
    SLIDER_NONE = 0,
    SLIDER_DECREASE = 1,
    SLIDER_INCREASE = 2,
    SLIDER_THUMB = 3,
    SLIDER_BG = 4
} slider_element;

static void draw_slider_horizontal(slider_t *slider);
static void draw_slider_vertical(slider_t *slider);
static slider_element slider_get_hovered_element(slider_t *slider, const mouse *m);
static int slider_get_value_from_thumb_offset(slider_t *slider, int thumb_offset);
static void slider_zero_cache_and_state(slider_t *slider);

#pragma region Helpers

static int get_slider_image_id(const slider_t *slider, slider_element element)
{
    int id = 0;

    switch (element) {
        case SLIDER_DECREASE:
            if (slider->show_plus_minus_buttons) {
                id = ASSET_UI_SCROLLBAR_MINUS_01;
            } else {
                id = slider->is_vertical ? ASSET_UI_SCROLLBAR_UP : ASSET_UI_SCROLLBAR_LEFT_01;
            }
            break;

        case SLIDER_INCREASE:
            if (slider->show_plus_minus_buttons) {
                id = ASSET_UI_SCROLLBAR_PLUS_01;
            } else {
                id = slider->is_vertical ? ASSET_UI_SCROLLBAR_DOWN : ASSET_UI_SCROLLBAR_RIGHT_01;
            }
            break;

        case SLIDER_THUMB:
            id = slider->force_mini_thumb ? ASSET_UI_SCROLLBAR_MINI_THUMB_01 : 0;
            break;

        case SLIDER_BG:
            id = ASSET_UI_SCROLL_BG_01;
            break;

        default:
            return 0;
    }

    return assets_lookup_image_id(id);
}

static int get_thumb_middle_section_size(int is_vertical)
{
    if (is_vertical) {
        return image_get(assets_lookup_image_id(ASSET_UI_SCROLLBAR_MIDDLE_01_TRIMMED))->original.height;
    }

    return image_get(assets_lookup_image_id(ASSET_UI_SCROLLBAR_MIDDLE_01B_TRIMMED))->original.width;
}

static int get_thumb_end_size(int is_vertical)
{
    if (is_vertical) {
        return image_get(assets_lookup_image_id(ASSET_UI_SCROLLBAR_MIDDLE_01_END_TOP))->original.height;
    }

    return image_get(assets_lookup_image_id(ASSET_UI_SCROLLBAR_MIDDLE_01B_END_LEFT))->original.width;
}

static int get_thumb_midsections_count(slider_t *slider)
{
    if (slider->force_mini_thumb) {
        return -1;
    }

    int middle_size = get_thumb_middle_section_size(slider->is_vertical);
    int end_size = get_thumb_end_size(slider->is_vertical);
    int range = slider->max_value - slider->min_value;
    int track_length = slider->length - 2 * SLIDER_BUTTON_SIDE;
    int total_positions = (range + slider->value_step - 1) / slider->value_step + 1;
    int max_mid_sections = (track_length - 2 * end_size) / middle_size;

    if (max_mid_sections < 1) {
        return -1;
    }
    // Aim for a thumb roughly proportional to one selectable position.
    int ideal_thumb_size = (track_length + total_positions / 2) / total_positions;
    int mid_sections_count = (ideal_thumb_size - 2 * end_size + middle_size / 2) / middle_size;
    mid_sections_count = calc_bound(mid_sections_count, 1, max_mid_sections);
    int thumb_length = 2 * end_size + mid_sections_count * middle_size;
    int travel_length = track_length - thumb_length;

    // If advancing one slider step moves the thumb less than 25% of its own length,
    // the movement is too subtle to be useful. Use mini thumb.

    int movement_per_step = (travel_length + (total_positions - 1) / 2) / (total_positions - 1);
    if (movement_per_step * 4 < thumb_length) {
        return -1;
    }
    slider->cached_thumb_middle_sections = mid_sections_count;
    return mid_sections_count;
}

static int get_thumb_length(slider_t *slider)
{
    if (slider->force_mini_thumb) {
        slider->cached_thumb_length = SLIDER_BUTTON_SIDE;
        return slider->cached_thumb_length;
    }
    slider->cached_thumb_length = 2 * get_thumb_end_size(slider->is_vertical) +
        get_thumb_midsections_count(slider) * get_thumb_middle_section_size(slider->is_vertical);
    return slider->cached_thumb_length;
}

//  Amount of space through which the TOP/LEFT edge of the thumb may move.
//  Slider geometry:

//   | button |<---------- track ---------->| button |
//            |<------ thumb travel ------->|

//  The thumb therefore always stays entirely between the two buttons.
static int get_thumb_travel_length(slider_t *slider)
{
    int travel_length = slider->length - 2 * SLIDER_BUTTON_SIDE - get_thumb_length(slider);

    return travel_length > 0 ? travel_length : 0;
}

static int slider_snap_value(const slider_t *slider, int value)
{
    value = calc_bound(value, slider->min_value, slider->max_value);

    if (value == slider->max_value) {
        return slider->max_value;
    }

    int offset = value - slider->min_value;
    int lower_step = offset / slider->value_step;
    int lower_value = slider->min_value + lower_step * slider->value_step;
    int upper_value = lower_value + slider->value_step;

    if (upper_value > slider->max_value) {
        upper_value = slider->max_value;
    }

    if (value - lower_value < upper_value - value) {
        return lower_value;
    }

    return upper_value;
}

static int slider_get_thumb_offset_from_value(slider_t *slider)
{
    int range = slider->max_value - slider->min_value;
    int travel_length = get_thumb_travel_length(slider);

    if (range <= 0 || travel_length <= 0) {
        return MIN_THUMB_OFFSET;
    }

    int value_offset = calc_bound(slider->value, slider->min_value, slider->max_value) - slider->min_value;
    int scaled_offset = value_offset * travel_length + range / 2;
    int thumb_offset = MIN_THUMB_OFFSET + (int) (scaled_offset / range);
    int max = slider->length - SLIDER_BUTTON_SIDE - get_thumb_length(slider);
    thumb_offset = calc_bound(thumb_offset, MIN_THUMB_OFFSET, max);

    return thumb_offset;
}

static void slider_update_cached_thumb_offset(slider_t *slider)
{
    slider->cached_thumb_offset = slider_get_thumb_offset_from_value(slider);
}

static int slider_decrease_value(slider_t *slider)
{
    if (slider->value <= slider->min_value) {
        slider->value = slider->min_value;
        return 0; // no decrease
    }

    int range = slider->max_value - slider->min_value;

    if (slider->value == slider->max_value && range % slider->value_step != 0) {
        // for cases where range is not divisble by step, e.g.
        // [step = 2, min=0, max = 9] -> 0, 2, 4, 6, 8, 9 ; the decrease from 9 should go to 8 not 7.
        slider->value = slider->min_value + (range / slider->value_step) * slider->value_step;
        return 1;
    }

    slider->value -= slider->value_step;

    if (slider->value < slider->min_value) {
        slider->value = slider->min_value;
    }
    return 1; // standard decrease by step
}

static int slider_increase_value(slider_t *slider)
{
    if (slider->value >= slider->max_value) {
        slider->value = slider->max_value;
        return 0; // no increase
    }

    if (slider->max_value - slider->value <= slider->value_step) {
        slider->value = slider->max_value;
        return 1; // increased to max_value, because + step would have exceeded it
    }

    slider->value += slider->value_step; // standard increase by step
    return 1; // standard increase by step
}

static int mouse_is_inside_rect(const mouse *m, int x, int y, int width, int height)
{
    if (width <= 0 || height <= 0) {
        return 0;
    }

    return m->x >= x && m->x < x + width && m->y >= y && m->y < y + height;
}

#pragma endregion Helpers
#pragma region Initialization

int widget_slider_init(slider_t *slider, int x, int y, int length, int min_value, int max_value,
    int value_step, int initial_value, unsigned char is_vertical, slider_display_text display_text)
{
    memset(slider, 0, sizeof(*slider));

    if (max_value <= min_value || value_step <= 0 || length <= 2 * SLIDER_BUTTON_SIDE) {
        // Leave an invalid slider in a harmless state rather than as
        // a zeroed object which could accidentally still be drawn.
        slider->is_hidden = 1;
        slider->is_disabled = 1;
        return 0;
    }

    slider->x = x;
    slider->y = y;
    slider->length = length;

    slider->min_value = min_value;
    slider->max_value = max_value;
    slider->value_step = value_step;

    slider->draw_background = 1;
    slider->is_vertical = is_vertical;

    slider->value = slider_snap_value(slider, initial_value);
    slider->display_text = display_text;
    slider_update_cached_thumb_offset(slider);
    return 1;
}

int widget_slider_text_block_init(text_block *block, int x, int y, int width, int height, lang_sequence *sequence,
    sequence_positioning position)
{
    lang_sequence seq;
    lang_seq_init(&seq, (lang_fragment *) sequence->fragments, sequence->count);
    return widget_text_block_init_simple(block, x, y, width, height, &seq, position, TEXT_BLOCK_STYLE_RAW);
}

#pragma endregion Initialization
#pragma region Drawing

void widget_slider_draw(const slider_t *slider)
{
    if (slider->is_hidden) {
        return;
    }

    if (slider->is_vertical) {
        draw_slider_vertical((slider_t *) slider);
    } else {
        draw_slider_horizontal((slider_t *) slider);
    }
}

void widget_slider_draw_array(const slider_t *sliders, unsigned int num_sliders)
{
    for (unsigned int i = 0; i < num_sliders; i++) {
        widget_slider_draw(&sliders[i]);
    }
}

static void draw_slider_horizontal(slider_t *slider)
{

    if (slider->draw_background) {
        scrollbar_panel_draw(slider->x, slider->y, slider->length, 0);
    }
    int decrease = get_slider_image_id(slider, SLIDER_DECREASE);
    int increase = get_slider_image_id(slider, SLIDER_INCREASE);
    int thumb = 0;
    if (slider->hovered_element != SLIDER_NONE) {
        switch (slider->hovered_element) {
            case SLIDER_DECREASE:
                decrease += slider->hovered_element_animation_frame;
                break;
            case SLIDER_INCREASE:
                increase += slider->hovered_element_animation_frame;
                break;
            case SLIDER_THUMB:
                thumb += slider->hovered_element_animation_frame;
                break;
            default:
                break;
        }
    }
    image_draw(decrease, slider->x, slider->y, COLOR_MASK_NONE, SCALE_NONE);
    image_draw(increase, slider->x + slider->length - SLIDER_BUTTON_SIDE, slider->y, COLOR_MASK_NONE, SCALE_NONE);
    //value remains authoritative

    int thumb_offset = slider_get_thumb_offset_from_value(slider);

    scrollbar_thumb_draw(slider->x + thumb_offset, slider->y, get_thumb_midsections_count(slider), 0, thumb);
}

static void draw_slider_vertical(slider_t *slider)
{

    if (slider->draw_background) {
        scrollbar_panel_draw(slider->x, slider->y, slider->length, 1);
    }
    int decrease = get_slider_image_id(slider, SLIDER_DECREASE);
    int increase = get_slider_image_id(slider, SLIDER_INCREASE);
    int thumb = 0;
    if (slider->hovered_element != SLIDER_NONE) {
        switch (slider->hovered_element) {
            case SLIDER_DECREASE:
                decrease += slider->hovered_element_animation_frame;
                break;
            case SLIDER_INCREASE:
                increase += slider->hovered_element_animation_frame;
                break;
            case SLIDER_THUMB:
                thumb += slider->hovered_element_animation_frame;
                break;
            default:
                break;
        }
        decrease = get_slider_image_id(slider, SLIDER_DECREASE) + slider->hovered_element_animation_frame;
    }
    image_draw(get_slider_image_id(slider, SLIDER_DECREASE), slider->x, slider->y, COLOR_MASK_NONE, SCALE_NONE);
    image_draw(get_slider_image_id(slider, SLIDER_INCREASE), slider->x, slider->y + slider->length - SLIDER_BUTTON_SIDE,
        COLOR_MASK_NONE, SCALE_NONE);

    int thumb_offset = slider_get_thumb_offset_from_value(slider);

    scrollbar_thumb_draw(slider->x, slider->y + thumb_offset, get_thumb_midsections_count(slider), 1, thumb);
}

#pragma endregion Drawing
#pragma region Input Handling

static slider_element slider_get_hovered_element(slider_t *slider, const mouse *m)
{
    slider_element hovered_element = SLIDER_NONE;

    /*
     * Disabled sliders have no interactive elements.
     */
    if (!slider->is_hidden && !slider->is_disabled && m->is_inside_window) {
        if (slider->is_vertical) {
            if (mouse_is_inside_rect(m, slider->x, slider->y, SLIDER_BUTTON_SIDE, SLIDER_BUTTON_SIDE)) {
                hovered_element = SLIDER_DECREASE;
            } else if (mouse_is_inside_rect(m, slider->x, slider->y + slider->length - SLIDER_BUTTON_SIDE,
                SLIDER_BUTTON_SIDE, SLIDER_BUTTON_SIDE)) {
                hovered_element = SLIDER_INCREASE;
            } else if (mouse_is_inside_rect(m, slider->x, slider->y + slider->cached_thumb_offset,
                SLIDER_BUTTON_SIDE, get_thumb_length(slider))) {
                hovered_element = SLIDER_THUMB;
            } else if (mouse_is_inside_rect(m, slider->x, slider->y + SLIDER_BUTTON_SIDE,
                SLIDER_BUTTON_SIDE, slider->length - 2 * SLIDER_BUTTON_SIDE)) {
                hovered_element = SLIDER_BG;
            }
        } else {
            if (mouse_is_inside_rect(m, slider->x, slider->y, SLIDER_BUTTON_SIDE, SLIDER_BUTTON_SIDE)) {
                hovered_element = SLIDER_DECREASE;
            } else if (mouse_is_inside_rect(m, slider->x + slider->length - SLIDER_BUTTON_SIDE, slider->y,
                SLIDER_BUTTON_SIDE, SLIDER_BUTTON_SIDE)) {
                hovered_element = SLIDER_INCREASE;
            } else if (mouse_is_inside_rect(m, slider->x + slider->cached_thumb_offset, slider->y,
                get_thumb_length(slider), SLIDER_BUTTON_SIDE)) {
                hovered_element = SLIDER_THUMB;
            } else if (mouse_is_inside_rect(m, slider->x + SLIDER_BUTTON_SIDE, slider->y,
                slider->length - 2 * SLIDER_BUTTON_SIDE, SLIDER_BUTTON_SIDE)) {
                hovered_element = SLIDER_BG;
            }
        }
    }

    slider->is_hovered = hovered_element != SLIDER_NONE;
    slider->hovered_element = hovered_element;

    return hovered_element;
}

static int slider_get_value_from_thumb_offset(slider_t *slider, int thumb_offset)
{
    int range = slider->max_value - slider->min_value;
    int travel_length = get_thumb_travel_length(slider);

    if (range <= 0 || travel_length <= 0) {
        return slider->min_value;
    }
    // Convert from an absolute thumb offset inside the slider to relative travel inside the track.
    int relative_offset = calc_bound(thumb_offset - MIN_THUMB_OFFSET, 0, travel_length);

    // Convert the thumb's relative pixel position into the logical value range, rounding to the nearest value first.

    int scaled_value = relative_offset * range + travel_length / 2;
    int value = slider->min_value + (scaled_value / travel_length);
    return slider_snap_value(slider, value);
}

static void slider_zero_cache_and_state(slider_t *slider)
{
    slider->cached_thumb_offset = 0;
    slider->cached_thumb_length = 0;
    slider->cached_thumb_middle_sections = 0;
    slider->is_hovered = 0;
    slider->hovered_element = SLIDER_NONE;
    slider->hovered_element_animation_frame = 0;
    slider->is_clicked = 0;
    slider->is_dragging = 0;
}

int widget_slider_handle_mouse(slider_t *slider, const mouse *m)
{
    if (slider->is_hidden || slider->is_disabled) {
        slider_zero_cache_and_state(slider);
        return 0;
    }

    slider_element previous_hovered_element = slider->hovered_element;
    slider->value = slider_snap_value(slider, slider->value);
    slider_update_cached_thumb_offset(slider);
    slider->is_clicked = 0;

    if (slider->is_dragging) {
        if (!m->left.is_down) {
            slider->is_dragging = 0;
            slider_update_cached_thumb_offset(slider);
            window_request_refresh();
            return 1; // handled 
        }

        int mouse_position = slider->is_vertical ? m->y : m->x;
        int slider_position = slider->is_vertical ? slider->y : slider->x;
        int thumb_offset = mouse_position - slider_position - slider->cached_drag_offset;
        int max_thumb_offset = slider->length - SLIDER_BUTTON_SIDE - get_thumb_length(slider);

        thumb_offset = calc_bound(thumb_offset, MIN_THUMB_OFFSET, max_thumb_offset);
        slider->value = slider_get_value_from_thumb_offset(slider, thumb_offset);
        slider_update_cached_thumb_offset(slider);
        window_request_refresh();
        return 1; // handled
    }

    slider_get_hovered_element(slider, m);

    if (slider->hovered_element != SLIDER_NONE && slider->hovered_element == previous_hovered_element) {
        slider->hovered_element_animation_frame++;
        if (slider->hovered_element_animation_frame > 3) {
            slider->hovered_element_animation_frame = 3;
        }
    } else {
        slider->hovered_element_animation_frame = 0;
    }

    if (m->left.went_down && slider->hovered_element != SLIDER_NONE) {
        slider->is_clicked = 1;

        if (slider->hovered_element == SLIDER_DECREASE) {
            slider_decrease_value(slider);
        } else if (slider->hovered_element == SLIDER_INCREASE) {
            slider_increase_value(slider);
        } else if (slider->hovered_element == SLIDER_BG) {
            int click_offset = slider->is_vertical ? m->y - slider->y : m->x - slider->x;
            int thumb_offset = click_offset - get_thumb_length(slider) / 2;
            int max_thumb_offset = slider->length - SLIDER_BUTTON_SIDE - get_thumb_length(slider);

            thumb_offset = calc_bound(thumb_offset, MIN_THUMB_OFFSET, max_thumb_offset);
            slider->value = slider_get_value_from_thumb_offset(slider, thumb_offset);
        } else if (slider->hovered_element == SLIDER_THUMB) {
            int mouse_position = slider->is_vertical ? m->y : m->x;
            int slider_position = slider->is_vertical ? slider->y : slider->x;

            slider->is_dragging = 1;
            slider->cached_drag_offset = mouse_position - slider_position - slider->cached_thumb_offset;
        }

        slider_update_cached_thumb_offset(slider);
        sound_effect_play(SOUND_EFFECT_ICON);
        window_request_refresh();
        return 1; // handled
    }

    return 0;
}

int widget_slider_handle_mouse_array(slider_t *sliders, const mouse *m, unsigned int num_sliders)
{
    int handled = 0;
    for (unsigned int i = 0; i < num_sliders; i++) {
        if (widget_slider_handle_mouse(&sliders[i], m)) {
            handled = 1;
        }
    }
    return handled;
}

#pragma endregion Input Handling
#pragma region Tooltip

int widget_slider_handle_tooltip(const slider_t *slider, tooltip_context *c)
{
    if (!slider || !c || slider->is_hidden || !slider->is_hovered || tooltip_context_is_empty(&slider->tooltip_c)) {
        return 0;
    }

    tooltip_copy_context(c, &slider->tooltip_c);
    c->type = TOOLTIP_BUTTON;
    return 1;
}

int widget_slider_handle_tooltip_array(const slider_t *sliders, tooltip_context *c, unsigned int num_sliders)
{
    for (unsigned int i = 0; i < num_sliders; i++) {
        if (widget_slider_handle_tooltip(&sliders[i], c)) {
            return 1;
        }
    }
    return 0;
}

#pragma endregion Tooltip

#include "complex_button.h"

#include "core/time.h"
#include "graphics/button.h"
#include "graphics/graphics.h"
#include "graphics/panel.h"
#include "graphics/tooltip.h"
#include "graphics/window.h"
#include "input/mouse.h"
#include "sound/effect.h"
#include "widget/text_block.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define DISABLED_SHADING 3

static void complex_button_ellipsized(complex_button *button, int was_ellipsized);
static void draw_button_contents(const complex_button *button, font_t font, color_t font_primary, color_t font_secondary);
static void end_animation(complex_button_animation *anim);

static const cycling_button_state *cycling_button_get_state(const cycling_button *button)
{
    if (!button || button->state_count <= 0 || button->state_count > MAX_CYCLE_BUTTON_STATES) {
        return NULL;
    }

    int index = button->state_index;
    if (index < 0) {
        index = 0;
    }
    if (index >= button->state_count) {
        index = button->state_count - 1;
    }
    return &button->states[index];
}

color_t complex_button_basic_colors(int id)
{
    switch (id) {
        case 1: return COLOR_MASK_PASTEL_GREEN;
        case 2: return COLOR_MASK_PASTEL_PURPLE;
        case 3: return COLOR_MASK_PASTEL_ORANGE;
        case 4: return COLOR_MASK_PASTEL_OLIVE;
        case 5: return COLOR_MASK_PASTEL_TURQUOISE;
        case 6: return COLOR_MASK_PASTEL_CORAL;
        case 7: return COLOR_MASK_PASTEL_GRAY;
        case 8: return COLOR_MASK_PASTEL_BLUE;
        case 9: return COLOR_MASK_PASTEL_DARK_BLUE;
        case 10: return COLOR_MASK_PASTEL_BLACK;
        case 11: return COLOR_MASK_PASTEL_BROWN;
        default: return COLOR_MASK_NONE;
    }
}

font_t complex_button_font_for_style(complex_button_style style)
{
    switch (style) {
        case COMPLEX_BUTTON_STYLE_DEFAULT:
        case COMPLEX_BUTTON_STYLE_SUNKEN:
            return FONT_NORMAL_BLACK;
        case COMPLEX_BUTTON_STYLE_GRAY:
            return FONT_NORMAL_GREEN;
        case COMPLEX_BUTTON_STYLE_BROWN:
            return FONT_NORMAL_BROWN;
        case COMPLEX_BUTTON_STYLE_RAW:
        case COMPLEX_BUTTON_STYLE_IMAGE:
        case COMPLEX_BUTTON_STYLE_CUSTOM:
        default:
            return FONT_NORMAL_BLACK;
    }
}

static color_t complex_button_bg_primary_for_style(complex_button_style style)
{
    switch (style) {
        case COMPLEX_BUTTON_STYLE_BROWN:
            return COLOR_MASK_PASTEL_BROWN;
        case COMPLEX_BUTTON_STYLE_DEFAULT:
        case COMPLEX_BUTTON_STYLE_SUNKEN:
        case COMPLEX_BUTTON_STYLE_GRAY:
        case COMPLEX_BUTTON_STYLE_RAW:
        case COMPLEX_BUTTON_STYLE_IMAGE:
        case COMPLEX_BUTTON_STYLE_CUSTOM:
        default:
            return COLOR_MASK_NONE;
    }
}

static color_t complex_button_font_primary_for_style(complex_button_style style)
{
    switch (style) {
        case COMPLEX_BUTTON_STYLE_DEFAULT:
        case COMPLEX_BUTTON_STYLE_SUNKEN:
        case COMPLEX_BUTTON_STYLE_GRAY:
        case COMPLEX_BUTTON_STYLE_BROWN:
        case COMPLEX_BUTTON_STYLE_RAW:
        case COMPLEX_BUTTON_STYLE_IMAGE:
        case COMPLEX_BUTTON_STYLE_CUSTOM:
        default:
            return COLOR_MASK_NONE;
    }
}

void complex_button_init_style(complex_button *button, complex_button_style style)
{
    if (!button) {
        return;
    }
    if (style == COMPLEX_BUTTON_STYLE_CUSTOM) {
        return; // custom style has no defaults
    }
    button->style = style;
    button->font = complex_button_font_for_style(style);
    button->font_primary = complex_button_font_primary_for_style(style);
    button->bg_primary = complex_button_bg_primary_for_style(style);
    button->draw_hover_state = 1;
    button->draw_border = 1;
    button->draw_background = 1;
    button->border_on_hover = 1;

    switch (style) {
        case COMPLEX_BUTTON_STYLE_GRAY:
            button->border_on_hover = 0;
            button->shade_on_hover = 1;
            break;
        case COMPLEX_BUTTON_STYLE_SUNKEN:
            button->border_on_hover = 0;
            button->shade_on_hover = 2;
            break;
        case COMPLEX_BUTTON_STYLE_IMAGE:
        case COMPLEX_BUTTON_STYLE_RAW:
            button->draw_border = 0;
            button->draw_background = 0;
            button->border_on_hover = 0;
            break;
        case COMPLEX_BUTTON_STYLE_CUSTOM:
        case COMPLEX_BUTTON_STYLE_DEFAULT:
        case COMPLEX_BUTTON_STYLE_BROWN:
        default:
            break;
    }
}

static int sequence_position_is_centered(sequence_positioning position)
{
    switch (position) {
        case SEQUENCE_POSITION_TOP_CENTER:
        case SEQUENCE_POSITION_CENTER:
        case SEQUENCE_POSITION_BOTTOM_CENTER:
            return 1;
        default:
            return 0;
    }
}

static int sequence_y_offset(const complex_button *button, sequence_positioning position, font_t font)
{
    const int inner_margin = 2;
    int text_height = font_definition_for(font)->line_height;

    switch (position) {
        case SEQUENCE_POSITION_TOP_LEFT:
        case SEQUENCE_POSITION_TOP_CENTER:
        case SEQUENCE_POSITION_TOP_RIGHT:
            return button->y + inner_margin;

        case SEQUENCE_POSITION_BOTTOM_LEFT:
        case SEQUENCE_POSITION_BOTTOM_CENTER:
        case SEQUENCE_POSITION_BOTTOM_RIGHT:
            return button->y + button->height - text_height - inner_margin;

        case SEQUENCE_POSITION_CENTER_LEFT:
        case SEQUENCE_POSITION_CENTER:
        case SEQUENCE_POSITION_CENTER_RIGHT:
        default:
            return button->y + (button->height - text_height) / 2;
    }
}

int complex_button_animation_init(complex_button *button, const btn_img *frames, unsigned short frame_count,
    animation_trigger trigger, animation_mode mode)
{
    if (!button || !frames || frame_count == 0) {
        return 0;
    }

    btn_img *new_frames = malloc(sizeof(btn_img) * frame_count);
    if (!new_frames) {
        return 0;
    }

    memcpy(new_frames, frames, sizeof(btn_img) * frame_count);

    if (button->has_animation) {
        free(button->animation.frames);
    }

    button->animation = (complex_button_animation) {
        .frames = new_frames,
        .frame_count = frame_count,
        .frame_duration = DEFAULT_ANIMATION_FRAME_DURATION,
        .trigger = trigger,
        .loop_mode = mode,
        .skip_zero_frame = trigger == BUTTON_ANIMATION_TRIGGER_CLICK
    };

    button->has_animation = 1;
    return 1;
}

void complex_button_animation_destroy(complex_button *button)
{
    if (!button || !button->has_animation) {
        return;
    }

    free(button->animation.frames);
    button->animation = (complex_button_animation) { 0 };
    button->has_animation = 0;
}

void complex_button_animation_start(complex_button *button)
{
    if (!button || !button->has_animation || button->animation.trigger != BUTTON_ANIMATION_TRIGGER_CUSTOM) {
        return;
    }

    complex_button_animation *anim = &button->animation;

    anim->is_active = 1;
    anim->is_looping = anim->loop_mode != BUTTON_ANIMATION_ONCE;
    anim->loops_left = anim->max_loop_count;
    anim->is_reversed = 0;
    anim->current_frame = 0;
    anim->last_change = 0;
}

void complex_button_animation_stop(complex_button *button)
{
    if (!button || !button->has_animation || button->animation.trigger != BUTTON_ANIMATION_TRIGGER_CUSTOM) {
        return;
    }
    end_animation(&button->animation);
}

static void end_animation(complex_button_animation *anim)
{
    anim->is_active = 0;
    anim->is_looping = 0;
    anim->loops_left = 0;
    anim->is_reversed = 0;
    anim->is_holding = 0;
    anim->current_frame = 0;
    anim->last_change = 0;
}

static void handle_animation(complex_button *button)
{
    complex_button_animation *anim = &button->animation;
    int started_animation = 0;
    int should_advance = 0;
    int has_finished = 0;
    int trigger_active = 1;

    if (anim->frame_count <= 0) {
        return;
    }

    // frame 1 is the lower boundary when frame 0 is excluded from repeating cycles
    unsigned short first_loop_frame = anim->skip_zero_frame ? 1 : 0;
    unsigned short last_frame = anim->frame_count;

    if (!anim->is_active) {
        switch (anim->trigger) {
            case BUTTON_ANIMATION_TRIGGER_CLICK:
                started_animation = button->is_clicked;
                break;

            case BUTTON_ANIMATION_TRIGGER_HOVER:
                started_animation = button->is_hovered;
                break;

            case BUTTON_ANIMATION_TRIGGER_NONE:
                started_animation = 1;
                break;

            case BUTTON_ANIMATION_TRIGGER_CUSTOM:
            default:
                break;
        }

        if (!started_animation) {
            return;
        }
        anim->is_active = 1;
        anim->loops_left = anim->max_loop_count;
        // establish whether this animation can repeat after its first run

        anim->is_looping = anim->loop_mode != BUTTON_ANIMATION_ONCE && (!anim->max_loop_count || anim->loops_left > 0);
    } else {
        if (anim->trigger == BUTTON_ANIMATION_TRIGGER_CLICK && !button->is_clicked) {
            trigger_active = 0;
        } else if (anim->trigger == BUTTON_ANIMATION_TRIGGER_HOVER && !button->is_hovered) {
            trigger_active = 0;
        }

        if (!trigger_active) {
            if (anim->allow_immediate_stop) {
                end_animation(anim);
                return;
            }

            // trigger disappeared; finish the current run but do not begin another
            anim->loops_left = 0;
            anim->is_looping = 0;
            if (anim->skip_zero_frame) {
                if (anim->loop_mode == BUTTON_ANIMATION_PINGPONG) {
                    if (!anim->is_reversed && anim->current_frame == first_loop_frame) {
                        end_animation(anim);
                        return;
                    }
                } else if (anim->current_frame == last_frame) {
                    end_animation(anim);
                    return;
                }
            }
        }
    }

    if (anim->skip_zero_frame && trigger_active && !anim->is_looping) {
        if (anim->loop_mode == BUTTON_ANIMATION_PINGPONG) {
            if (!anim->is_reversed && anim->current_frame == first_loop_frame && anim->last_change) {
                return;
            }
        } else if (anim->current_frame == last_frame && anim->last_change) {
            return;
        }
    }

    if (anim->last_change) {
        time_millis now = time_get_millis();

        if (now - anim->last_change >= anim->frame_duration) {
            should_advance = 1;
        } else {
            return;
        }
    } else {
        should_advance = 1;
    }

    if (should_advance) {
        anim->last_change = time_get_millis();

        if (anim->is_reversed) {
            if (anim->current_frame > first_loop_frame) {
                anim->current_frame--;
            } else {
                has_finished = 1;
                anim->is_reversed = 0;
            }
        } else {
            if (anim->current_frame < last_frame) {
                anim->current_frame++;
            } else {
                if (anim->loop_mode == BUTTON_ANIMATION_PINGPONG) {
                    anim->is_reversed = 1;
                } else {
                    has_finished = 1;
                }
            }
        }
    }

    if (!has_finished) {
        return;
    }

    if (anim->max_loop_count && anim->loops_left > 0) { // decrement finite loop count 
        anim->loops_left--;
    }

    // decide whether another complete cycle should begin
    if (!anim->loop_mode) {
        anim->is_looping = 0;
    } else if (!trigger_active) {
        anim->is_looping = 0;
    } else if (!anim->max_loop_count) {
        anim->is_looping = 1;
    } else {
        anim->is_looping = anim->loops_left > 0;
    }

    if (anim->is_looping) {
        if (anim->loop_mode == BUTTON_ANIMATION_PINGPONG) {
            if (first_loop_frame < last_frame) {
                anim->current_frame = first_loop_frame + 1;
            }
        } else {
            // normal loops restart from 0 or 1 depending on skip_zero_frame
            anim->current_frame = first_loop_frame;
        }
        return;
    }

    if (anim->skip_zero_frame && trigger_active) {
        anim->is_looping = 0;
        return;
    }
    end_animation(anim);
}

static const btn_img *get_current_animation_frame_info(const complex_button *button)
{
    if (!button) {
        return NULL;
    }

    if (!button->has_animation || button->animation.frame_count <= 0) {
        return &button->image;
    }

    const complex_button_animation *anim = &button->animation;
    const btn_img *frame_img = &button->image;
    if (anim->current_frame > 0) {
        unsigned short frame_index = anim->current_frame - 1;
        if (!anim->frames || frame_index >= anim->frame_count) {
            return NULL;
        }
        frame_img = &anim->frames[frame_index];
    }

    return frame_img;
}

const static image *get_current_animation_frame(const complex_button *button)
{
    const btn_img *frame_img = get_current_animation_frame_info(button);
    if (!frame_img || frame_img->id <= 0) {
        return NULL;
    }
    return image_get(frame_img->id);
}

static void draw_button_style_image(const complex_button *button)
{

    if (button->has_animation) {
        // de-const cast to allow animation handling
        complex_button *mutable_button = (complex_button *) button;
        handle_animation(mutable_button);
    }

    const btn_img *frame_img = get_current_animation_frame_info(button);
    const image *image_main = get_current_animation_frame(button);
    const color_t image_mask = button->is_disabled ? COLOR_MASK_GRAY : COLOR_MASK_NONE;
    graphics_set_clip_rectangle(button->x, button->y, button->width, button->height);

    if (image_main && frame_img) {
        int x, y;
        if (frame_img->auto_center) {
            int image_width = image_main->width;
            int image_height = image_main->height;
            x = button->x + (button->width - image_width) / 2 + frame_img->image_x_offset;
            y = button->y + (button->height - image_height) / 2 + frame_img->image_y_offset;
        } else {
            x = button->x + frame_img->image_x_offset;
            y = button->y + frame_img->image_y_offset;
        }
        image_draw(frame_img->id, x, y, image_mask, SCALE_NONE);
        graphics_reset_clip_rectangle();
    }


    if (button->draw_hover_state && button->shade_on_hover && button->is_hovered && !button->is_disabled) {
        graphics_shade_rect(button->x, button->y, button->width, button->height, button->shade_on_hover);
    }
    if (button->draw_hover_state && button->light_on_hover && button->is_hovered && !button->is_disabled) {
        graphics_light_up_rect(button->x, button->y, button->width, button->height, button->light_on_hover);
    }
    return;
}

static void draw_button_contents(const complex_button *button, font_t font, color_t font_primary, color_t font_secondary)
{
    const int inner_margin = 2;
    const color_t image_mask = button->is_disabled ? COLOR_MASK_GRAY : COLOR_MASK_NONE;

    sequence_positioning position = button->sequence_position ?
        button->sequence_position : SEQUENCE_POSITION_CENTER;

    int text_y = sequence_y_offset(button, position, font);
    const lang_sequence *sequence = &button->sequence;
    int sequence_width = (sequence->fragments && sequence->count > 0) ? lang_seq_get_width(sequence, font) : 0;
    sequence_width -= sequence_width % 2;

    const image *image_before = NULL;
    const image *image_after = NULL;
    const image *image_main = button->image.id > 0 ? image_get(button->image.id) : NULL;
    int image_before_width = 0;
    int image_after_width = 0;
    int image_before_margin_x = inner_margin;

    if (button->image_before > 0) {
        image_before = image_get(button->image_before);
        if (image_before->original.width >= button->width) {
            image_before_margin_x = 0;
        }
        image_before_width = image_before->original.width + image_before_margin_x;
    }
    if (button->image_after > 0) {
        image_after = image_get(button->image_after);
        image_after_width = image_after->original.width + inner_margin;
    }

    int total_width = image_before_width + sequence_width + image_after_width;
    int cursor_x;

    switch (position) {
        case SEQUENCE_POSITION_TOP_RIGHT:
        case SEQUENCE_POSITION_CENTER_RIGHT:
        case SEQUENCE_POSITION_BOTTOM_RIGHT:
            cursor_x = button->x + button->width - inner_margin - total_width;
            break;

        case SEQUENCE_POSITION_TOP_LEFT:
        case SEQUENCE_POSITION_CENTER_LEFT:
        case SEQUENCE_POSITION_BOTTOM_LEFT:
            cursor_x = button->x + inner_margin;
            break;

        case SEQUENCE_POSITION_TOP_CENTER:
        case SEQUENCE_POSITION_CENTER:
        case SEQUENCE_POSITION_BOTTOM_CENTER:
        default:
            cursor_x = button->x + (button->width - total_width) / 2;
            break;
    }

    if (image_main) {
        int x, y;
        if (button->image.auto_center) {
            int image_width = image_main->width;
            int image_height = image_main->height;
            x = button->x + (button->width - image_width) / 2 + button->image.image_x_offset;
            y = button->y + (button->height - image_height) / 2 + button->image.image_y_offset;
        } else {
            x = button->x + button->image.image_x_offset;
            y = button->y + button->image.image_y_offset;
        }
        image_draw(button->image.id, x, y, image_mask, SCALE_NONE);
        graphics_reset_clip_rectangle();
        return;
    }

    if (image_before) {
        int image_x = image_before->original.width >= button->width ? button->x : cursor_x;
        int image_y = image_before->original.height >= button->height
            ? button->y
            : button->y + (button->height - image_before->original.height) / 2;
        image_draw(button->image_before, image_x, image_y, image_mask, SCALE_NONE);
        cursor_x += image_before->original.width + image_before_margin_x;
    }

    int was_ellipsized = 0;
    if (sequence->fragments && sequence->count > 0) {
        if (font == FONT_NORMAL_PLAIN || font == FONT_LARGE_PLAIN || font == FONT_SMALL_PLAIN) {
            lang_seq_draw_with_shadow(sequence, button->x, text_y, button->width, font, font_primary, font_secondary,
                sequence_position_is_centered(position), 1);
        } else {
            if (sequence_position_is_centered(position)) {
                lang_seq_draw_centered_ellipsized(sequence, button->x, text_y, button->width, font, font_primary,
                    &was_ellipsized);
            } else {
                cursor_x += lang_seq_draw_ellipsized(sequence, cursor_x, text_y, button->width, font, font_primary,
                    &was_ellipsized);
            }
        }
    }

    complex_button_ellipsized((complex_button *) button, was_ellipsized);

    if (image_after) {
        int image_y = button->y + (button->height - image_after->original.height) / 2;
        image_draw(button->image_after, cursor_x + inner_margin, image_y, image_mask, SCALE_NONE);
    }
}

static void draw_default_style(const complex_button *button, font_t base_font,
    color_t font_primary, color_t font_secondary)
{
    graphics_set_clip_rectangle(button->x, button->y, button->width, button->height);

    int height_blocks = button->height / BLOCK_SIZE;
    if (button->draw_background) {
        switch (button->style) {
            case COMPLEX_BUTTON_STYLE_CUSTOM:
                unbordered_panel_draw_colored(button->x, button->y, button->width / BLOCK_SIZE + 1,
                    height_blocks + 1, button->bg_primary);
                break;
            case COMPLEX_BUTTON_STYLE_RAW:
            case COMPLEX_BUTTON_STYLE_IMAGE:
                break; // no bg fill
            case COMPLEX_BUTTON_STYLE_SUNKEN:
            case COMPLEX_BUTTON_STYLE_BROWN:
                inner_panel_draw_colored(button->x, button->y, button->width, button->height, button->bg_primary);
                break;
            case COMPLEX_BUTTON_STYLE_DEFAULT:
            case COMPLEX_BUTTON_STYLE_GRAY:
            default:
                unbordered_panel_draw_colored(button->x, button->y, button->width / BLOCK_SIZE + 1,
                    height_blocks + 1, button->bg_primary);
                break;
        }
    }

    int draw_red_border = button->draw_hover_state && button->border_on_hover &&
        !button->is_disabled && button->is_hovered;
    if (button->draw_border) {
        if (button->style == COMPLEX_BUTTON_STYLE_SUNKEN) {
            if (button->draw_hover_state) {
                graphics_shade_rect(button->x, button->y, button->width, button->height, 2 * button->is_hovered);
            }
        } else {
            if (button->flush_with_background) {
                button_border_draw_colored_flush(button->x, button->y, button->width, button->height,
                    draw_red_border, COLOR_MASK_NONE);
            } else {
                button_border_draw_colored(button->x, button->y, button->width, button->height,
                    draw_red_border, COLOR_MASK_NONE);
            }
        }
    }
    draw_button_contents(button, base_font, font_primary, font_secondary);
    if (button->draw_hover_state && button->shade_on_hover && button->is_hovered && !button->is_disabled) {
        graphics_shade_rect(button->x, button->y, button->width, button->height, button->shade_on_hover);
    }
    graphics_reset_clip_rectangle();
}

static void draw_main_menu_style(const complex_button *button, font_t base_font, color_t font_primary, color_t font_secondary)
{
    graphics_set_clip_rectangle(button->x, button->y, button->width, button->height);
    if (button->draw_background) {
        large_label_draw_bg_colored(button->x, button->y, button->width, button->height, button->bg_primary);
    }

    if (button->draw_hover_state && !button->is_disabled && button->is_hovered) {
        graphics_shade_rect(button->x, button->y, button->width, button->height, button->shade_on_hover);
    }
    if (button->is_disabled) {
        graphics_shade_rect(button->x, button->y, button->width, button->height, DISABLED_SHADING);
    }
    draw_button_contents(button, base_font, font_primary, font_secondary);
    if (button->draw_border) {
        large_label_draw_border(button->x, button->y, button->width, button->height);
    }
    if (button->draw_hover_state && button->shade_on_hover && button->is_hovered && !button->is_disabled) {
        graphics_shade_rect(button->x, button->y, button->width, button->height, button->shade_on_hover);
    }
    if (button->draw_hover_state && button->light_on_hover && button->is_hovered && !button->is_disabled) {
        graphics_light_up_rect(button->x, button->y, button->width, button->height, button->light_on_hover);
    }
    graphics_reset_clip_rectangle();
}

static void complex_button_ellipsized(complex_button *button, int was_ellipsized)
{
    button->is_ellipsized = was_ellipsized;
}

// === Draw a single button ===
void complex_button_draw(const complex_button *button)
{
    if (!button || button->is_hidden) {
        return;
    }
    int is_large = button->height > 32 && !button->dont_enlarge_font;
    font_t base_font = button->font ? button->font : (is_large ? FONT_LARGE_BLACK : FONT_NORMAL_BLACK);
    color_t font_primary = button->font_primary;
    color_t font_secondary = COLOR_MASK_NONE;
    if (button->is_disabled) {
        base_font = is_large ? FONT_LARGE_PLAIN : FONT_NORMAL_PLAIN;
        font_primary = COLOR_FONT_GRAY;
    }

    switch (button->style) {
        case COMPLEX_BUTTON_STYLE_IMAGE:
            draw_button_style_image(button);
            break;
        case COMPLEX_BUTTON_STYLE_GRAY:
            draw_main_menu_style(button, base_font, font_primary, font_secondary);
            break;
        default:
            draw_default_style(button, base_font, font_primary, font_secondary);
    }
}

void complex_button_draw_array(const complex_button *buttons, unsigned int num_buttons)
{
    for (unsigned int i = 0; i < num_buttons; i++) {
        complex_button_draw(&buttons[i]);
    }
}

int complex_button_handle_mouse(complex_button *btn, const mouse *m)
{
    if (btn->is_disabled || btn->is_hidden) {
        btn->is_clicked = 0;
        if (btn->is_hidden) {
            return 0; // hidden buttons do not handle mouse events
        }
    }
    int handled = 0;
    int was_clicked = btn->is_clicked;
    // Expanded hitbox
    int left = btn->x - btn->expanded_hitbox_radius;
    int right = btn->x + btn->width + btn->expanded_hitbox_radius;
    int top = btn->y - btn->expanded_hitbox_radius;
    int bottom = btn->y + btn->height + btn->expanded_hitbox_radius;

    int inside = (m->x >= left && m->x < right && m->y >= top && m->y < bottom);
    if (btn->is_hovered != inside) {
        btn->is_hovered = inside;

        if (btn->hover_handler && !btn->is_disabled) {
            btn->hover_handler(btn); // run the hover handler on hover state change
        }
        window_request_refresh(); // redraw to show focus change
    } else {
        btn->is_hovered = inside;
    }
    if (btn->is_disabled) {
        return 0; // disabled buttons do not handle mouse past establishing focus state for tooltip
    }
    if (btn->is_ellipsized && btn->is_hovered) { //if the button is ellipsized, show tooltip
        static uint8_t tooltip_text[512];
        lang_seq_concatenate(&btn->sequence, tooltip_text, 512);
        btn->tooltip_c.type = TOOLTIP_BUTTON;
        btn->tooltip_c.precomposed_text = tooltip_text; // reset precomposed text to force re-generation
    }

    if (inside) {

        // on mouse down, set clicked to true to activate animations,
        // but don't trigger callback or sound until button goes up
        if (m->left.went_down) {
            btn->is_clicked = 1;
            handled = 1;
        } else if (m->right.went_down) {
            btn->is_clicked = 1;
            handled = 1;
        };

        // --- Left click ---
        if (m->left.went_up) {
            btn->is_clicked = 0;

            sound_effect_play(SOUND_EFFECT_ICON);
            btn->is_active = !btn->is_active; // persistent toggle
            handled = 1;

            if (btn->left_click_handler) {
                btn->left_click_handler(btn);
            }

            // --- Right click ---
        } else if (m->right.went_up) {
            handled = 1;

            if (btn->right_click_handler) {
                btn->right_click_handler(btn);
            }
        }

        if (was_clicked && !btn->is_clicked && btn->unclick_handler) {
            btn->unclick_handler(btn);
        }

    } else {
        btn->is_clicked = 0;

        if (was_clicked && btn->unclick_handler) {
            btn->unclick_handler(btn);
        }
    }

    return handled;
}

int complex_button_handle_mouse_array(complex_button *buttons, const mouse *m, unsigned int num_buttons)
{
    int handled = 0;

    for (unsigned int i = 0; i < num_buttons; i++) {
        if (complex_button_handle_mouse(&buttons[i], m)) {
            handled = 1;
        }
    }

    return handled;
}

//TO SOLVE: manually set tooltips will be overwritten if the button is ellipsized.
int complex_button_handle_tooltip(const complex_button *button, tooltip_context *c)
{
    if (button->is_hovered) {
        if (!tooltip_context_is_empty(&button->tooltip_c)) {
            tooltip_copy_context(c, &button->tooltip_c);
            c->type = TOOLTIP_BUTTON; // constant - for all buttons.
            return 1;
        }
    }
    return 0;
}

int complex_button_handle_tooltip_array(const complex_button *buttons, tooltip_context *c, unsigned int num_buttons)
{
    for (unsigned int i = 0; i < num_buttons; i++) {
        if (complex_button_handle_tooltip(&buttons[i], c)) {
            return 1;
        }
    }
    return 0;
}









































void checkbox_button_draw(const checkbox_button *button)
{
    if (!button) {
        return;
    }

    const int spacing = 6;
    font_t font = button->font ? button->font : FONT_NORMAL_BROWN;
    color_t text_color = button->color_mask ? button->color_mask : COLOR_MASK_NONE;
    color_t image_color = COLOR_MASK_NONE;

    int box_size = button->height;
    if (box_size < 12) {
        box_size = 12;
    }
    if (box_size > button->width) {
        box_size = button->width;
    }

    int box_x = button->box_on_right ? button->x + button->width - box_size : button->x;
    int box_y = button->y;
    int content_x = button->box_on_right ? button->x : box_x + box_size + spacing;
    int content_width = button->width - box_size - spacing;
    if (content_width < 0) {
        content_width = 0;
    }

    const image *img_before = NULL;
    const image *img_after = NULL;
    int img_before_w = 0;
    int img_after_w = 0;

    if (button->image_before > 0) {
        img_before = image_get(button->image_before);
        img_before_w = img_before->width + spacing;
    }
    if (button->image_after > 0) {
        img_after = image_get(button->image_after);
        img_after_w = img_after->width + spacing;
    }

    graphics_set_clip_rectangle(button->x, button->y, button->width, button->height);
    if (button->fill_bg) {
        unbordered_panel_draw_px(button->x, button->y, button->width, button->height);
    }
    button_border_draw(box_x, box_y, box_size, box_size, button->is_hovered);
    if (button->is_checked) {
        int mark_x = box_x + (box_size * 6) / 20;
        int mark_y = box_y + (box_size * 3) / 20;
        text_draw((const uint8_t *) "x", mark_x, mark_y, FONT_NORMAL_BROWN, COLOR_MASK_NONE);
    }

    int cursor_x = content_x;
    if (img_before) {
        int img_y = button->y + (button->height - img_before->height) / 2;
        image_draw(button->image_before, cursor_x, img_y, image_color, SCALE_NONE);
        cursor_x += img_before_w;
    }

    int text_y = button->y + (button->height - font_definition_for(font)->line_height) / 2;
    int max_text_width = content_width - img_before_w - img_after_w;
    if (max_text_width < 0) {
        max_text_width = 0;
    }
    int was_ellipsized = 0;
    if (button->sequence.fragments && button->sequence.count > 0) {
        cursor_x += lang_seq_draw_ellipsized(&button->sequence, cursor_x, text_y, max_text_width, font, text_color,
            &was_ellipsized);
    }
    ((checkbox_button *) button)->is_ellipsized = was_ellipsized;

    if (img_after) {
        int img_y = button->y + (button->height - img_after->height) / 2;
        image_draw(button->image_after, cursor_x, img_y, image_color, SCALE_NONE);
    }

    graphics_reset_clip_rectangle();
}

void checkbox_button_draw_array(const checkbox_button *buttons, unsigned int num_buttons)
{
    for (unsigned int i = 0; i < num_buttons; i++) {
        checkbox_button_draw(&buttons[i]);
    }
}

color_t cycling_button_color_for_style(cycling_button_style style)
{
    // Cycling buttons inherit color from state, but this selector allows future style variations
    switch (style) {
        case CYCLING_BUTTON_STYLE_GRAY:
        default:
            return COLOR_MASK_NONE;
    }
}

font_t cycling_button_font_for_style(cycling_button_style style)
{
    // Cycling buttons inherit font from state, but this selector allows future style variations
    switch (style) {
        case CYCLING_BUTTON_STYLE_GRAY:
            return FONT_NORMAL_GREEN;
        default:
            return FONT_NORMAL_BLACK;
    }
}

static void draw_cycling_button_contents(const cycling_button *button, const cycling_button_state *state, font_t font)
{
    const int inner_margin = 2;
    const color_t text_color = state->color_mask ? state->color_mask : COLOR_MASK_NONE;
    const color_t image_color = COLOR_MASK_NONE;

    int img_before_w = 0;
    int img_after_w = 0;
    int img_before_margin_x = inner_margin;
    const image *img_before = NULL;
    const image *img_after = NULL;

    if (state->image_before > 0) {
        img_before = image_get(state->image_before);
        if (img_before->original.width >= button->width) {
            img_before_margin_x = 0;
        }
        img_before_w = img_before->original.width + img_before_margin_x;
    }
    if (state->image_after > 0) {
        img_after = image_get(state->image_after);
        img_after_w = img_after->original.width + inner_margin;
    }

    int text_max_width = button->width - 2 * inner_margin - img_before_w - img_after_w;
    if (text_max_width < 0) {
        text_max_width = 0;
    }

    int seq_width = 0;
    if (state->sequence.fragments && state->sequence.count > 0) {
        seq_width = lang_seq_get_width(&state->sequence, font);
    }
    int visible_seq_width = seq_width < text_max_width ? seq_width : text_max_width;

    int total_width = img_before_w + visible_seq_width + img_after_w;
    int cursor_x = button->x + (button->width - total_width) / 2;
    if (cursor_x < button->x + inner_margin) {
        cursor_x = button->x + inner_margin;
    }
    int text_y = button->y + (button->height - font_definition_for(font)->line_height) / 2;

    if (img_before) {
        int img_x = img_before->original.width >= button->width ? button->x : cursor_x;
        int img_y = img_before->original.height >= button->height
            ? button->y
            : button->y + (button->height - img_before->original.height) / 2;
        image_draw(state->image_before, img_x, img_y, image_color, SCALE_NONE);
        cursor_x += img_before_w;
    }

    if (state->sequence.fragments && state->sequence.count > 0) {
        cursor_x += lang_seq_draw_ellipsized(&state->sequence, cursor_x, text_y, text_max_width, font, text_color, 0);
    }

    if (img_after) {
        int img_y = button->y + (button->height - img_after->original.height) / 2;
        image_draw(state->image_after, cursor_x + inner_margin, img_y, image_color, SCALE_NONE);
    }
}

void cycling_button_draw_default_style(const cycling_button *button)
{
    if (!button) {
        return;
    }

    const cycling_button_state *state = cycling_button_get_state(button);
    if (!state) {
        return;
    }

    font_t font = state->font ? state->font : cycling_button_font_for_style(button->style);

    graphics_set_clip_rectangle(button->x, button->y, button->width, button->height);

    if (button->fill_bg) {
        unbordered_panel_draw_px(button->x, button->y, button->width, button->height);
    }

    draw_cycling_button_contents(button, state, font);
    if (button->style != CYCLING_BUTTON_STYLE_RAW) {
        button_border_draw(button->x, button->y, button->width, button->height, button->is_hovered);

    }
    graphics_reset_clip_rectangle();
}

void cycling_button_draw_gray_style(const cycling_button *button)
{
    if (!button) {
        return;
    }

    const cycling_button_state *state = cycling_button_get_state(button);
    if (!state) {
        return;
    }

    font_t font = state->font ? state->font : cycling_button_font_for_style(button->style);
    if (button->style != CYCLING_BUTTON_STYLE_RAW) {
        large_label_draw_bg(button->x, button->y, button->width, button->height);
    }
    if (button->is_hovered) {
        graphics_shade_rect(button->x, button->y, button->width, button->height, 2);
    }
    draw_cycling_button_contents(button, state, font);
    if (button->style != CYCLING_BUTTON_STYLE_RAW) {
        large_label_draw_border(button->x, button->y, button->width, button->height);
    }
}

void cycling_button_draw(const cycling_button *button)
{
    switch (button->style) {
        case CYCLING_BUTTON_STYLE_GRAY:
            cycling_button_draw_gray_style(button);
            break;
        default:
            cycling_button_draw_default_style(button);
    }
}

void cycling_button_draw_array(const cycling_button *buttons, unsigned int num_buttons)
{
    for (unsigned int i = 0; i < num_buttons; i++) {
        cycling_button_draw(&buttons[i]);
    }
}


int checkbox_button_handle_mouse(checkbox_button *btn, const mouse *m)
{
    if (!btn) {
        return 0;
    }

    int inside = (m->x >= btn->x && m->x < btn->x + btn->width && m->y >= btn->y && m->y < btn->y + btn->height);
    if (btn->is_hovered != inside) {
        window_request_refresh();
    }
    btn->is_hovered = inside;

    if (btn->is_ellipsized && btn->is_hovered) {
        static uint8_t tooltip_text[512];
        lang_seq_concatenate(&btn->sequence, tooltip_text, 512);
        btn->tooltip_c.type = TOOLTIP_BUTTON;
        btn->tooltip_c.precomposed_text = tooltip_text;
    }

    if (inside && btn->hover_handler) {
        btn->hover_handler(btn);
    }

    if (inside && m->left.went_up) {
        sound_effect_play(SOUND_EFFECT_ICON);
        btn->is_checked = !btn->is_checked;
        if (btn->left_click_handler) {
            btn->left_click_handler(btn);
        }
        return 1;
    }

    return 0;
}

int checkbox_button_handle_mouse_array(checkbox_button *buttons, const mouse *m, unsigned int num_buttons)
{
    int handled = 0;

    for (unsigned int i = 0; i < num_buttons; i++) {
        if (checkbox_button_handle_mouse(&buttons[i], m)) {
            handled = 1;
        }
    }

    return handled;
}

int cycling_button_handle_mouse(cycling_button *btn, const mouse *m)
{
    if (!btn) {
        return 0;
    }

    int inside = (m->x >= btn->x && m->x < btn->x + btn->width && m->y >= btn->y && m->y < btn->y + btn->height);
    if (btn->is_hovered != inside) {
        window_request_refresh();
    }
    btn->is_hovered = inside;

    if (inside && btn->hover_handler) {
        btn->hover_handler(btn);
    }

    int handled = 0;

    if (inside && m->left.went_up) {
        sound_effect_play(SOUND_EFFECT_ICON);
        if (btn->state_count > 0) {
            btn->state_index = (btn->state_index + 1) % btn->state_count;
            window_request_refresh();
        }
        if (btn->left_click_handler) {
            btn->left_click_handler(btn);
        }
        handled = 1;
    }

    if (inside && m->right.went_up) {
        if (btn->state_count > 0) {
            btn->state_index = (btn->state_index + btn->state_count - 1) % btn->state_count;
            window_request_refresh();
        }
        if (btn->right_click_handler) {
            btn->right_click_handler(btn);
        }
        handled = 1;
    }

    return handled;
}

int cycling_button_handle_mouse_array(cycling_button *buttons, const mouse *m, unsigned int num_buttons)
{
    int handled = 0;

    for (unsigned int i = 0; i < num_buttons; i++) {
        if (cycling_button_handle_mouse(&buttons[i], m)) {
            handled = 1;
        }
    }

    return handled;
}

int checkbox_button_handle_tooltip(const checkbox_button *button, tooltip_context *c)
{
    if (button->is_hovered) {
        tooltip_copy_context(c, &button->tooltip_c);
        return 1;
    }
    return 0;
}

int checkbox_button_handle_tooltip_array(const checkbox_button *buttons, tooltip_context *c, unsigned int num_buttons)
{
    for (unsigned int i = 0; i < num_buttons; i++) {
        if (checkbox_button_handle_tooltip(&buttons[i], c)) {
            return 1;
        }
    }
    return 0;
}

int cycling_button_handle_tooltip(const cycling_button *button, tooltip_context *c)
{
    if (!button || !c || !button->is_hovered) {
        return 0;
    }

    const cycling_button_state *state = cycling_button_get_state(button);
    if (!state) {
        return 0;
    }

    tooltip_copy_context(c, &state->tooltip_c);
    c->type = TOOLTIP_BUTTON; // constant - for all buttons.
    return 1;
}

int cycling_button_handle_tooltip_array(const cycling_button *buttons, tooltip_context *c, unsigned int num_buttons)
{
    for (unsigned int i = 0; i < num_buttons; i++) {
        if (cycling_button_handle_tooltip(&buttons[i], c)) {
            return 1;
        }
    }
    return 0;
}


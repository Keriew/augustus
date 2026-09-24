#include "text_block.h"

#include "graphics/button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/lang_sequence.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"


#include <stddef.h>
#include <string.h>

#define DEFAULT_PADDING 2

font_t text_block_font_for_style(text_block_style style)
{
    switch (style) {
        case TEXT_BLOCK_STYLE_DEFAULT:
        case TEXT_BLOCK_STYLE_RAW:
            return FONT_NORMAL_BLACK;
        case TEXT_BLOCK_STYLE_GRAY:
            return FONT_NORMAL_GREEN;
        case TEXT_BLOCK_STYLE_BROWN:
            return FONT_NORMAL_BROWN;
        case TEXT_BLOCK_STYLE_RAISED:
        case TEXT_BLOCK_STYLE_SUNKEN:
        default:
            return FONT_NORMAL_BLACK;
    }
}

color_t text_block_bg_primary_for_style(text_block_style style)
{
    switch (style) {
        case TEXT_BLOCK_STYLE_BROWN:
            return COLOR_MASK_PASTEL_BROWN;
        case TEXT_BLOCK_STYLE_DEFAULT:
        case TEXT_BLOCK_STYLE_SUNKEN:
        case TEXT_BLOCK_STYLE_RAISED:
        case TEXT_BLOCK_STYLE_GRAY:
        case TEXT_BLOCK_STYLE_RAW:
        default:
            return COLOR_MASK_NONE;
    }
}

color_t text_block_font_primary_for_style(text_block_style style)
{
    switch (style) {
        case TEXT_BLOCK_STYLE_BROWN:
        case TEXT_BLOCK_STYLE_DEFAULT:
        case TEXT_BLOCK_STYLE_SUNKEN:
        case TEXT_BLOCK_STYLE_RAISED:
        case TEXT_BLOCK_STYLE_GRAY:
        case TEXT_BLOCK_STYLE_RAW:
        default:
            return COLOR_MASK_NONE; // atm no styles using custom font coloring
    }
}

static int text_block_content_width(const text_block *block)
{
    return block->width - 2 * block->inner_padding_x;
}

static int text_block_content_height(const text_block *block)
{
    return block->height - 2 * block->inner_padding_y;
}

static sequence_positioning text_block_position(const text_block *block)
{
    if (block->position < SEQUENCE_POSITION_TOP_LEFT || block->position > SEQUENCE_POSITION_BOTTOM_RIGHT) {
        return SEQUENCE_POSITION_CENTER; // default to center if the value is out of bounds
    }
    return block->position;
}

static int text_block_position_column(const text_block *block)
{
    return (text_block_position(block) - 1) % 3;
}

static int text_block_position_row(const text_block *block)
{
    return (text_block_position(block) - 1) / 3;
}

static int text_block_get_x(const text_block *block, int text_width)
{
    int x = block->x + block->inner_padding_x;
    int content_width = text_block_content_width(block);

    if (text_block_position_column(block) == 1) {
        return x + (content_width - text_width) / 2;
    }
    if (text_block_position_column(block) == 2) {
        return x + content_width - text_width;
    }
    return x;
}

static int text_block_get_y(const text_block *block, int text_height)
{
    int y = block->y + block->inner_padding_y;
    int content_height = text_block_content_height(block);

    // Preserve the beginning of content when it is taller than the available area
    if (text_height >= content_height) {
        return y;
    }

    if (text_block_position_row(block) == 1) {
        return y + (content_height - text_height) / 2;
    }
    if (text_block_position_row(block) == 2) {
        return y + content_height - text_height;
    }
    return y;
}

static color_t text_block_color(const text_block *block)
{
    return block->is_disabled ? COLOR_FONT_GRAY : block->font_primary;
}

static void text_block_draw_background_and_border(const text_block *block)
{

    if (block->draw_background) {
        switch (block->style) {
            case TEXT_BLOCK_STYLE_DEFAULT:
                unbordered_panel_draw_px_colored(block->x, block->y, block->width, block->height, block->bg_primary);
                break;
            case TEXT_BLOCK_STYLE_SUNKEN:
            case TEXT_BLOCK_STYLE_BROWN: // brown has pastel color pre-set
            case TEXT_BLOCK_STYLE_RAISED:
                inner_panel_draw_colored(block->x, block->y, block->width, block->height, block->bg_primary);
                break;
            case TEXT_BLOCK_STYLE_GRAY:
                large_label_draw_bg_colored(block->x, block->y, block->width, block->height, block->bg_primary);
                break;
            case TEXT_BLOCK_STYLE_RAW:
                break;
        }

    }
    int red;
    if (block->draw_border) {
        switch (block->style) {
            case TEXT_BLOCK_STYLE_DEFAULT:
            case TEXT_BLOCK_STYLE_BROWN:
            case TEXT_BLOCK_STYLE_RAISED:
                red = block->draw_hover_state ? block->state_is_hovered : 0;
                button_border_draw(block->x, block->y, block->width, block->height, red);
                break;
            case TEXT_BLOCK_STYLE_GRAY:
                large_label_draw_border(block->x, block->y, block->width, block->height); //intentional fall through
            case TEXT_BLOCK_STYLE_SUNKEN:
                if (block->draw_hover_state) {
                    graphics_shade_rect(block->x, block->y, block->width, block->height, 2 * block->state_is_hovered);
                }
                break;
            case TEXT_BLOCK_STYLE_RAW:
                break;
        }
    }
}

static void text_block_draw_sequence(const text_block *block)
{
    int content_width = text_block_content_width(block);
    int sequence_width = lang_seq_get_width(&block->sequence, block->font);
    int line_height = font_definition_for(block->font)->line_height;
    color_t color = text_block_color(block);

    if (sequence_width <= content_width) {
        int x = text_block_get_x(block, sequence_width) + block->text_offset_x;
        int y = text_block_get_y(block, line_height) + block->text_offset_y;
        lang_seq_draw(&block->sequence, x, y, block->font, color);
        return;
    }

    int text_height = lang_seq_get_multiline_height(&block->sequence, content_width, 0, block->font);
    int x = block->x + block->inner_padding_x + block->text_offset_x;
    int y = text_block_get_y(block, text_height) + block->text_offset_y;

    if (text_block_position_column(block) == 1) {
        lang_seq_draw_multiline_aligned_center(&block->sequence, x, y, content_width, 0, block->font, color);
    } else if (text_block_position_column(block) == 2) {
        lang_seq_draw_multiline_aligned_right(&block->sequence, x, y, content_width, 0, block->font, color);
    } else {
        lang_seq_draw_multiline_aligned_left(&block->sequence, x, y, content_width, 0, block->font, color);
    }
}

static void text_block_draw_raw(const text_block *block)
{
    int content_width = text_block_content_width(block);
    int text_width = text_get_width(block->raw_text, block->font);
    int line_height = font_definition_for(block->font)->line_height;
    color_t color = text_block_color(block);

    // Single-line raw text gets normal positioning because it requires no extra layout logic.
    if (text_width <= content_width) {
        int x = text_block_get_x(block, text_width) + block->text_offset_x;
        int y = text_block_get_y(block, line_height) + block->text_offset_y;
        text_draw(block->raw_text, x, y, block->font, color);
        return;
    }

    // Multiline raw text is intentionally only a simple fallback.
    int x = block->x + block->inner_padding_x + block->text_offset_x;
    int y = block->y + block->inner_padding_y + block->text_offset_y;
    text_draw_multiline(block->raw_text, x, y, content_width, 0, block->font, color);
}

static void text_block_draw_with_images(const text_block *block)
{
    const int inner_margin = 2;
    const color_t image_mask = block->is_disabled ? COLOR_MASK_GRAY : COLOR_MASK_NONE;
    const image *image_before = NULL;
    const image *image_after = NULL;
    int image_before_width = 0;
    int image_after_width = 0;
    int image_before_margin_x = inner_margin;
    int content_width = text_block_content_width(block);
    int line_height = font_definition_for(block->font)->line_height;
    int text_width = 0;
    int has_sequence = block->sequence.fragments && block->sequence.count > 0;
    int has_raw_text = block->raw_text && *block->raw_text;

    if (block->image_before > 0) {
        image_before = image_get(block->image_before);
        if (image_before->original.width >= block->width) {
            image_before_margin_x = 0;
        }
        image_before_width = image_before->original.width + image_before_margin_x;
    }
    if (block->image_after > 0) {
        image_after = image_get(block->image_after);
        image_after_width = image_after->original.width + inner_margin;
    }

    int max_text_width = content_width - image_before_width - image_after_width;
    if (max_text_width < 0) {
        max_text_width = 0;
    }

    if (has_sequence) {
        text_width = lang_seq_get_width(&block->sequence, block->font);
    } else if (has_raw_text) {
        text_width = text_get_width(block->raw_text, block->font);
    }
    if (text_width > max_text_width) {
        text_width = max_text_width;
    }

    int total_width = image_before_width + text_width + image_after_width;
    int cursor_x = text_block_get_x(block, total_width);
    int text_y = text_block_get_y(block, line_height) + block->text_offset_y;
    color_t color = text_block_color(block);

    if (image_before) {
        int image_x = image_before->original.width >= block->width ? block->x : cursor_x;
        int image_y = image_before->original.height >= block->height
            ? block->y
            : block->y + (block->height - image_before->original.height) / 2;
        image_draw(block->image_before, image_x, image_y, image_mask, SCALE_NONE);
        cursor_x += image_before->original.width + image_before_margin_x;
    }

    if (has_sequence) {
        cursor_x += lang_seq_draw_ellipsized(&block->sequence, cursor_x + block->text_offset_x, text_y,
            max_text_width, block->font, color, NULL);
    } else if (has_raw_text) {
        cursor_x += text_draw_ellipsized(block->raw_text, cursor_x + block->text_offset_x, text_y, max_text_width,
            block->font, color);
    }

    if (image_after) {
        int image_y = image_after->original.height >= block->height
            ? block->y : block->y + (block->height - image_after->original.height) / 2;
        image_draw(block->image_after, cursor_x + inner_margin, image_y, image_mask, SCALE_NONE);
    }
}

int widget_text_block_init_simple(text_block *block, int x, int y, int width, int height, const lang_sequence *sequence,
    sequence_positioning position, text_block_style style)
{
    if (!block) {
        return 0;
    }

    memset(block, 0, sizeof(*block));

    if (sequence) {
        block->sequence = *sequence;
    }

    block->position = position ? position : SEQUENCE_POSITION_CENTER;
    block->style = style;
    block->font = text_block_font_for_style(block->style);
    block->font_primary = text_block_font_primary_for_style(block->style);
    block->bg_primary = text_block_bg_primary_for_style(block->style);
    block->x = x;
    block->y = y;
    block->width = width;
    block->height = height;
    block->inner_padding_x = DEFAULT_PADDING;
    block->inner_padding_y = DEFAULT_PADDING;
    block->draw_border = 1;
    block->draw_hover_state = 1;
    block->draw_background = 1;
    block->is_disabled = 0;
    block->is_hidden = 0;
    block->tooltip_c.type = TOOLTIP_BUTTON;
    // if tooltip type is not set and forgotten by user, tooltip won't show, so set it preemptively in the simple init 
    return 1;
}

static void text_block_draw_content(const text_block *block)
{
    text_block_draw_background_and_border(block);

    if (block->image_before > 0 || block->image_after > 0) {
        text_block_draw_with_images(block);
    } else if (block->sequence.count > 0) {
        text_block_draw_sequence(block);
    } else if (block->raw_text) {
        text_block_draw_raw(block);
    }
}

void widget_text_block_draw(const text_block *block)
{
    if (!block || block->is_hidden) {
        return;
    }

    graphics_set_clip_rectangle(block->x, block->y, block->width, block->height);
    text_block_draw_content(block);
    graphics_reset_clip_rectangle();
}

void widget_text_block_draw_clipped(const text_block *block, int clip_x, int clip_y, int clip_width, int clip_height)
{
    if (!block || block->is_hidden) {
        return;
    }

    int x1 = block->x > clip_x ? block->x : clip_x;
    int y1 = block->y > clip_y ? block->y : clip_y;
    int x2 = block->x + block->width < clip_x + clip_width ? block->x + block->width : clip_x + clip_width;
    int y2 = block->y + block->height < clip_y + clip_height ? block->y + block->height : clip_y + clip_height;

    if (x2 <= x1 || y2 <= y1) {
        return;
    }

    // Preserve the block's normal layout while restricting final pixels to the caller's visible area.
    graphics_set_clip_rectangle(x1, y1, x2 - x1, y2 - y1);
    text_block_draw_content(block);
    graphics_reset_clip_rectangle();
}

int widget_text_block_handle_mouse(text_block *block, const mouse *m)
{
    // currently no mouse functionality, only hover state tracking for tooltip support
    if (!block || !m) {
        return 0;
    }

    if (block->is_hidden || block->is_disabled) {
        if (block->state_is_hovered) {
            block->state_is_hovered = 0;
            window_request_refresh();
        }
        return 0;
    }

    int inside = m->x >= block->x && m->x < block->x + block->width &&
        m->y >= block->y && m->y < block->y + block->height;

    if (block->state_is_hovered != inside) {
        block->state_is_hovered = inside;
        window_request_refresh();
    }

    return 0;
}

int widget_text_block_handle_tooltip(const text_block *block, tooltip_context *c)
{
    if (!block || !c || block->is_hidden || !block->state_is_hovered || tooltip_context_is_empty(&block->tooltip_c)) {
        return 0;
    }

    tooltip_copy_context(c, &block->tooltip_c);
    return 1;
}

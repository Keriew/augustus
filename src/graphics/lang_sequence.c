#include "lang_sequence.h"

#include "core/lang.h"
#include "core/string.h"
#include "graphics/lang_text.h"
#include "graphics/text.h"

#include <stddef.h>

typedef enum {
    LANG_SEQ_ALIGN_LEFT,
    LANG_SEQ_ALIGN_CENTER,
    LANG_SEQ_ALIGN_RIGHT
} lang_seq_alignment;

void lang_seq_init(lang_sequence *seq, lang_fragment *fragments, int count)
{
    seq->fragments = fragments;
    seq->count = count;
}

void lang_seq_frag_label(lang_fragment *f, int text_group, int text_id)
{
    f->type = LANG_FRAG_LABEL;
    f->text_group = text_group;
    f->text_id = text_id;
}

void lang_seq_frag_amount(lang_fragment *f, int text_group, int text_id, int number)
{
    f->type = LANG_FRAG_AMOUNT;
    f->text_group = text_group;
    f->text_id = text_id;
    f->number = number;
}

void lang_seq_frag_number(lang_fragment *f, int number)
{
    f->type = LANG_FRAG_NUMBER;
    f->number = number;
}

void lang_seq_frag_float(lang_fragment *f, float number, int decimal_places)
{
    f->type = LANG_FRAG_FLOAT;
    f->float_number = number;
    f->decimal_places = decimal_places;
}

void lang_seq_frag_text(lang_fragment *f, const uint8_t *text)
{
    f->type = LANG_FRAG_TEXT;
    f->text = text;
}

void lang_seq_frag_space(lang_fragment *f, int space_width)
{
    f->type = LANG_FRAG_SPACE;
    f->space_width = space_width;
}

static int lang_seq_frag_width(const lang_fragment *f, font_t font, int trim_trailing_space)
{
    int width = 0;
    switch (f->type) {
        case LANG_FRAG_LABEL:
            width = lang_text_get_width(f->text_group, f->text_id, font);
            return trim_trailing_space ? width - font_definition_for(font)->space_width : width;
        case LANG_FRAG_AMOUNT:
            width = lang_text_get_amount_width(f->text_group, f->text_id, f->number, font);
            return trim_trailing_space ? width - font_definition_for(font)->space_width : width;
        case LANG_FRAG_NUMBER:
            return text_get_number_width(f->number, '\0', "\0", font);
        case LANG_FRAG_TEXT:
            return text_get_width(f->text, font);
        case LANG_FRAG_SPACE:
            return f->space_width;
        case LANG_FRAG_FLOAT:
            return text_get_number_float_width(f->float_number, f->decimal_places, '\0', "", font);
    }
    return 0;
}

static int lang_seq_draw_frag(const lang_fragment *f, int x, int y, font_t font, color_t color)
{
    switch (f->type) {
        case LANG_FRAG_LABEL:
            return lang_text_draw_colored(f->text_group, f->text_id, x, y, font, color);
        case LANG_FRAG_AMOUNT:
            return lang_text_draw_amount_colored(f->text_group, f->text_id, f->number, x, y, font, color);
        case LANG_FRAG_NUMBER:
            return text_draw_number(f->number, '\0', "\0", x, y, font, color);
        case LANG_FRAG_TEXT:
            return text_draw(f->text, x, y, font, color);
        case LANG_FRAG_SPACE:
            return f->space_width;
        case LANG_FRAG_FLOAT:
            return text_draw_number_float(f->float_number, f->decimal_places, '\0', "", x, y, font, color);
    }
    return 0;
}

int lang_seq_get_width(const lang_sequence *seq, font_t font)
{
    int width = 0;
    for (int i = 0; i < seq->count; i++) {
        width += lang_seq_frag_width(&seq->fragments[i], font, 1);
    }
    return width;
}

int lang_seq_draw(const lang_sequence *seq, int x, int y, font_t font, color_t color)
{
    int width = 0;
    for (int i = 0; i < seq->count; i++) {
        width += lang_seq_draw_frag(&seq->fragments[i], x + width, y, font, color);
    }
    return width;
}

static int line_x_for_alignment(int x, int box_width, int line_width, lang_seq_alignment alignment)
{
    if (line_width >= box_width) {
        return x;
    }
    if (alignment == LANG_SEQ_ALIGN_CENTER) {
        return x + (box_width - line_width) / 2;
    }
    if (alignment == LANG_SEQ_ALIGN_RIGHT) {
        return x + box_width - line_width;
    }
    return x;
}

static int draw_line(const lang_sequence *seq, int start, int count, int x, int y, int box_width,
    int line_width, font_t font, color_t color, lang_seq_alignment alignment)
{
    lang_sequence line;
    lang_seq_init(&line, &seq->fragments[start], count);
    return lang_seq_draw(&line, line_x_for_alignment(x, box_width, line_width, alignment), y, font, color);
}

static int draw_oversized_text_frag(const lang_fragment *f, int x, int y, int box_width,
    int height_offset, font_t font, color_t color)
{
    if (f->type == LANG_FRAG_LABEL) {
        return text_draw_multiline(lang_get_string(f->text_group, f->text_id), x, y, box_width, height_offset, font, color);
    }
    if (f->type == LANG_FRAG_TEXT) {
        return text_draw_multiline(f->text, x, y, box_width, height_offset, font, color);
    }
    return 0;
}

static int lang_seq_draw_multiline_aligned(const lang_sequence *seq, int x, int y, int box_width,
    int height_offset, font_t font, color_t color, lang_seq_alignment alignment)
{
    int current_y = y;
    int line_start = 0;
    int line_width = 0;
    int line_height = height_offset > 0 ? height_offset : font_definition_for(font)->line_height;

    for (int i = 0; i < seq->count; i++) {
        const lang_fragment *f = &seq->fragments[i];
        int frag_width = lang_seq_frag_width(f, font, 0);

        if (line_width > 0 && line_width + frag_width > box_width) {
            draw_line(seq, line_start, i - line_start, x, current_y, box_width, line_width, font, color, alignment);
            current_y += line_height;
            line_start = i;
            line_width = 0;
        }

        if (frag_width > box_width && (f->type == LANG_FRAG_LABEL || f->type == LANG_FRAG_TEXT)) {
            current_y += draw_oversized_text_frag(f, x, current_y, box_width, height_offset, font, color);
            line_start = i + 1;
            line_width = 0;
            continue;
        }

        line_width += frag_width;
    }

    if (line_start < seq->count) {
        draw_line(seq, line_start, seq->count - line_start, x, current_y, box_width, line_width, font, color, alignment);
        current_y += line_height;
    }

    return current_y - y;
}

int lang_seq_draw_multiline_aligned_left(const lang_sequence *seq, int x, int y, int box_width,
    int height_offset, font_t font, color_t color)
{
    return lang_seq_draw_multiline_aligned(seq, x, y, box_width, height_offset, font, color, LANG_SEQ_ALIGN_LEFT);
}

int lang_seq_draw_multiline_aligned_center(const lang_sequence *seq, int x, int y, int box_width,
    int height_offset, font_t font, color_t color)
{
    return lang_seq_draw_multiline_aligned(seq, x, y, box_width, height_offset, font, color, LANG_SEQ_ALIGN_CENTER);
}

int lang_seq_draw_multiline_aligned_right(const lang_sequence *seq, int x, int y, int box_width,
    int height_offset, font_t font, color_t color)
{
    return lang_seq_draw_multiline_aligned(seq, x, y, box_width, height_offset, font, color, LANG_SEQ_ALIGN_RIGHT);
}

int lang_seq_draw_centered(const lang_sequence *seq, int x, int y, int box_width, font_t font, color_t color)
{
    int total_width = lang_seq_get_width(seq, font);
    return lang_seq_draw(seq, x + (box_width - total_width) / 2, y, font, color);
}

static int lang_seq_draw_ellipsized_internal(const lang_sequence *seq, int x, int y, int box_width,
    font_t font, color_t color, int *was_ellipsized)
{
    if (was_ellipsized) {
        *was_ellipsized = 0;
    }

    int width = 0;
    int remaining_width = box_width;

    for (int i = 0; i < seq->count; i++) {
        const lang_fragment *f = &seq->fragments[i];
        int frag_width = lang_seq_frag_width(f, font, 1);

        if (frag_width <= remaining_width) {
            width += lang_seq_draw_frag(f, x + width, y, font, color);
            remaining_width -= frag_width;
            continue;
        }

        if (was_ellipsized) {
            *was_ellipsized = 1;
        }
        if (f->type == LANG_FRAG_LABEL) {
            text_draw_ellipsized(lang_get_string(f->text_group, f->text_id), x + width, y, remaining_width, font, color);
        } else if (f->type == LANG_FRAG_TEXT) {
            text_draw_ellipsized(f->text, x + width, y, remaining_width, font, color);
        }
        width += remaining_width;
        break;
    }

    return width;
}

int lang_seq_draw_ellipsized(const lang_sequence *seq, int x, int y, int box_width, font_t font, color_t color,
    int *was_ellipsized)
{
    return lang_seq_draw_ellipsized_internal(seq, x, y, box_width, font, color, was_ellipsized);
}

int lang_seq_draw_centered_ellipsized(const lang_sequence *seq, int x, int y, int box_width, font_t font,
    color_t color, int *was_ellipsized)
{
    int total_width = lang_seq_get_width(seq, font);
    if (total_width <= box_width) {
        return lang_seq_draw_ellipsized_internal(seq, x + (box_width - total_width) / 2, y, box_width, font, color,
            was_ellipsized);
    }
    return lang_seq_draw_ellipsized_internal(seq, x, y, box_width, font, color, was_ellipsized);
}

int lang_seq_concatenate(const lang_sequence *seq, uint8_t *dst, int dst_size)
{
    if (!dst || dst_size <= 0) {
        return 0;
    }

    uint8_t *cursor = dst;
    int remaining = dst_size - 1;

    for (int i = 0; i < seq->count && remaining > 0; i++) {
        const lang_fragment *f = &seq->fragments[i];
        const uint8_t *str = NULL;
        uint8_t number_buffer[20];
        int len = 0;

        switch (f->type) {
            case LANG_FRAG_LABEL:
                str = lang_get_string(f->text_group, f->text_id);
                len = string_length(str);
                if (len > remaining) len = remaining;
                string_copy(str, cursor, len + 1);
                cursor += len;
                remaining -= len;
                break;

            case LANG_FRAG_AMOUNT:
            {
                int amount_offset = (f->number == 1 || f->number == -1) ? 0 : 1;
                if (f->number >= 0) {
                    string_from_int(number_buffer, f->number, 0);
                } else {
                    number_buffer[0] = '-';
                    string_from_int(number_buffer + 1, -f->number, 0);
                }
                len = string_length(number_buffer);
                if (len > remaining) len = remaining;
                string_copy(number_buffer, cursor, len + 1);
                cursor += len;
                remaining -= len;
                if (remaining > 0) {
                    *cursor++ = ' ';
                    remaining--;
                }
                if (remaining > 0) {
                    str = lang_get_string(f->text_group, f->text_id + amount_offset);
                    len = string_length(str);
                    if (len > remaining) len = remaining;
                    string_copy(str, cursor, len + 1);
                    cursor += len;
                    remaining -= len;
                }
                break;
            }

            case LANG_FRAG_NUMBER:
                string_from_int(number_buffer, f->number, 0);
                len = string_length(number_buffer);
                if (len > remaining) len = remaining;
                string_copy(number_buffer, cursor, len + 1);
                cursor += len;
                remaining -= len;
                break;

            case LANG_FRAG_TEXT:
                str = f->text;
                len = string_length(str);
                if (len > remaining) len = remaining;
                string_copy(str, cursor, len + 1);
                cursor += len;
                remaining -= len;
                break;

            case LANG_FRAG_SPACE:
                for (int j = 0; j < f->space_width && remaining > 0; j++) {
                    *cursor++ = ' ';
                    remaining--;
                }
                break;

            case LANG_FRAG_FLOAT:
                len = string_from_float(number_buffer, f->float_number, f->decimal_places, 0);
                if (len > remaining) len = remaining;
                string_copy(number_buffer, cursor, len + 1);
                cursor += len;
                remaining -= len;
                break;
        }
    }

    *cursor = 0;
    return cursor - dst;
}

int lang_seq_get_multiline_height(const lang_sequence *seq, int box_width, int height_offset, font_t font)
{
    int line_height = height_offset > 0 ? height_offset : font_definition_for(font)->line_height;
    int height = 0;
    int line_width = 0;

    for (int i = 0; i < seq->count; i++) {
        const lang_fragment *f = &seq->fragments[i];
        int frag_width = lang_seq_frag_width(f, font, 0);

        if (line_width > 0 && line_width + frag_width > box_width) {
            height += line_height;
            line_width = 0;
        }

        if (frag_width > box_width && (f->type == LANG_FRAG_LABEL || f->type == LANG_FRAG_TEXT)) {
            // Oversized text fragments are delegated to text_draw_multiline when drawing.
            // Without a text-level measurement API, count this as one sequence line.
            height += line_height;
            line_width = 0;
            continue;
        }

        line_width += frag_width;
    }

    if (line_width > 0) {
        height += line_height;
    }

    return height;
}

static font_t font_to_plain(font_t font)
{
    switch (font) {
        case FONT_NORMAL_BLACK:
        case FONT_NORMAL_WHITE:
        case FONT_NORMAL_RED:
        case FONT_NORMAL_GREEN:
        case FONT_NORMAL_BROWN:
            return FONT_NORMAL_PLAIN;
        case FONT_LARGE_BLACK:
        case FONT_LARGE_BROWN:
            return FONT_LARGE_PLAIN;
        default:
            return font;
    }
}

void lang_seq_draw_with_shadow(const lang_sequence *seq, int x, int y, int width, font_t font,
    color_t primary, color_t secondary, int centered, int sunken)
{
    int shadow_y = sunken ? y + 1 : y - 1;
    font_t plain_f = font_to_plain(font);
    if (centered) {
        lang_seq_draw_centered_ellipsized(seq, x + 1, shadow_y, width, font, secondary, NULL);
        lang_seq_draw_centered_ellipsized(seq, x, y, width, plain_f, primary, NULL);
    } else {
        lang_seq_draw_ellipsized(seq, x + 1, shadow_y, width, font, secondary, NULL);
        lang_seq_draw_ellipsized(seq, x, y, width, plain_f, primary, NULL);
    }
}

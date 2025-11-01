#include "lang_text.h"

#include "core/lang.h"
#include "core/locale.h"
#include "core/string.h"
#include "graphics/text.h"

int lang_text_get_width(int group, int number, font_t font)
{
    const uint8_t *str = lang_get_string(group, number);
    return text_get_width(str, font) + font_definition_for(font)->space_width;
}

int lang_text_draw(int group, int number, int x_offset, int y_offset, font_t font)
{
    const uint8_t *str = lang_get_string(group, number);
    return text_draw(str, x_offset, y_offset, font, 0);
}

int lang_text_draw_colored(int group, int number, int x_offset, int y_offset, font_t font, color_t color)
{
    const uint8_t *str = lang_get_string(group, number);
    return text_draw(str, x_offset, y_offset, font, color);
}

void lang_text_draw_centered(int group, int number, int x_offset, int y_offset, int box_width, font_t font)
{
    const uint8_t *str = lang_get_string(group, number);
    text_draw_centered(str, x_offset, y_offset, box_width, font, 0);
}

void lang_text_draw_right_aligned(int group, int number, int x_offset, int y_offset, int box_width, font_t font)
{
    const uint8_t *str = lang_get_string(group, number);
    text_draw_right_aligned(str, x_offset, y_offset, box_width, font, 0);
}

void lang_text_draw_centered_colored(
    int group, int number, int x_offset, int y_offset, int box_width, font_t font, color_t color)
{
    const uint8_t *str = lang_get_string(group, number);
    text_draw_centered(str, x_offset, y_offset, box_width, font, color);
}

void lang_text_draw_ellipsized(int group, int number, int x_offset, int y_offset, int box_width, font_t font)
{
    const uint8_t *str = lang_get_string(group, number);
    text_draw_ellipsized(str, x_offset, y_offset, box_width, font, 0);
}

int lang_text_draw_amount(int group, int number, int amount, int x_offset, int y_offset, font_t font)
{
    return lang_text_draw_amount_colored(group, number, amount, x_offset, y_offset, font, COLOR_MASK_NONE);
}

int lang_text_get_amount_width(int group, int number, int amount, font_t font)
{
    int amount_offset = (amount == 1 || amount == -1) ? 0 : 1;
    int width;
    if (amount >= 0) {
        width = text_get_number_width(amount, ' ', " ", font);
    } else {
        width = text_get_number_width(-amount, '-', " ", font);
    }
    width += lang_text_get_width(group, number + amount_offset, font);
    return width;
}

int lang_text_draw_amount_centered(int group, int number, int amount, int x_offset, int y_offset, int box_width,
    font_t font)
{
    int width;
    if (amount >= 0) {
        width = text_get_number_width(amount, ' ', " ", font);
    } else {
        width = text_get_number_width(-amount, '-', " ", font);
    }
    int text_offset = (amount == 1 || amount == -1) ? 0 : 1;
    width += lang_text_get_width(group, number + text_offset, font);
    return lang_text_draw_amount_colored(group, number, amount, x_offset + (box_width - width) / 2, y_offset,
        font, COLOR_MASK_NONE);
}

int lang_text_draw_amount_colored(int group, int number, int amount, int x_offset, int y_offset,
    font_t font, color_t color)
{
    int amount_offset = 1;
    if (amount == 1 || amount == -1) {
        amount_offset = 0;
    }
    int desc_offset_x;
    if (amount >= 0) {
        desc_offset_x = text_draw_number(amount, ' ', " ",
            x_offset, y_offset, font, color);
    } else {
        desc_offset_x = text_draw_number(-amount, '-', " ",
            x_offset, y_offset, font, color);
    }
    return desc_offset_x + lang_text_draw_colored(group, number + amount_offset,
        x_offset + desc_offset_x, y_offset, font, color);
}

int lang_text_draw_year(int year, int x_offset, int y_offset, font_t font)
{
    int width = 0;
    if (year >= 0) {
        int use_year_ad = locale_year_before_ad();
        if (use_year_ad) {
            width += text_draw_number(year, ' ', "", x_offset + width, y_offset, font, 0);
            width += lang_text_draw(20, 1, x_offset + width, y_offset, font);
        } else {
            width += lang_text_draw(20, 1, x_offset + width, y_offset, font);
            width += text_draw_number(year, ' ', "", x_offset + width, y_offset, font, 0);
        }
    } else {
        width += text_draw_number(-year, ' ', "", x_offset + width, y_offset, font, 0);
        width += lang_text_draw(20, 0, x_offset + width, y_offset, font);
    }
    return width;
}

void lang_text_draw_month_year_max_width(
    int month, int year, int x_offset, int y_offset, int box_width, font_t font, color_t color)
{
    int month_width = lang_text_get_width(25, month, font);
    int ad_bc_width = lang_text_get_width(20, year >= 0 ? 1 : 0, font);
    int space_width = font_definition_for(font)->space_width;

    int negative_padding = 0;
    // assume 3 digits in the year times 11 pixels plus letter spacing = approx 35px
    int total_width = month_width + ad_bc_width + 35 + 2 * space_width;
    if (total_width > box_width) {
        // take the overflow and divide it by two since we have two places to correct: after month, and after year
        negative_padding = (box_width - total_width) / 2;
        if (negative_padding < -2 * (space_width - 2)) {
            negative_padding = -2 * (space_width - 2);
        }
    }

    int width = negative_padding + lang_text_draw_colored(25, month, x_offset, y_offset, font, color);
    if (year >= 0) {
        int use_year_ad = locale_year_before_ad();
        if (use_year_ad) {
            width += negative_padding +
                text_draw_number(year, ' ', " ", x_offset + width, y_offset, font, color);
            lang_text_draw_colored(20, 1, x_offset + width, y_offset, font, color);
        } else {
            width += space_width;
            width += negative_padding + lang_text_draw_colored(20, 1, x_offset + width, y_offset,
                font, color);
            text_draw_number(year, ' ', " ", x_offset + width, y_offset, font, color);
        }
    } else {
        width += negative_padding + text_draw_number(-year, ' ', " ", x_offset + width, y_offset, font, color);
        lang_text_draw_colored(20, 0, x_offset + width, y_offset, font, color);
    }
}

int lang_text_draw_multiline(int group, int number, int x_offset, int y_offset, int box_width, font_t font)
{
    const uint8_t *str = lang_get_string(group, number);
    return text_draw_multiline(str, x_offset, y_offset, box_width, 0, font, 0);
}

int lang_text_get_sequence_width(const lang_fragment *seq, int count, font_t font)
{
    int width = 0;
    for (int i = 0; i < count; i++) {
        const lang_fragment *f = &seq[i];
        switch (f->type) {
            case LANG_FRAG_LABEL:
                width += lang_text_get_width(f->text_group, f->text_id, font);
                width -= font_definition_for(font)->space_width;
                break;
            case LANG_FRAG_AMOUNT:
                width += lang_text_get_amount_width(f->text_group, f->text_id, f->number, font);
                width -= font_definition_for(font)->space_width;
                break;
            case LANG_FRAG_NUMBER:
                width += text_get_number_width(f->number, '\0', "\0", font);
                break;
            case LANG_FRAG_TEXT:
                width += text_get_width(f->text, font);
                break;
            case LANG_FRAG_SPACE:
                width += f->space_width;
                break;
        }
    }
    return width;
}

int lang_text_draw_sequence(const lang_fragment *seq, int count, int x, int y, font_t font, color_t color)
{
    int width = 0;
    for (int i = 0; i < count; i++) {
        const lang_fragment *f = &seq[i];
        switch (f->type) {
            case LANG_FRAG_LABEL:
                width += lang_text_draw_colored(f->text_group, f->text_id, x + width, y, font, color);
                break;
            case LANG_FRAG_AMOUNT:
                width += lang_text_draw_amount_colored(f->text_group, f->text_id, f->number, x + width, y, font, color);
                break;
            case LANG_FRAG_NUMBER:
                width += text_draw_number(f->number, '\0', "\0", x + width, y, font, color);
                break;
            case LANG_FRAG_TEXT:
                width += text_draw(f->text, x + width, y, font, color);
                break;
            case LANG_FRAG_SPACE:
                width += f->space_width;
                break;
        }
    }
    return width;
}


int lang_text_draw_sequence_multiline(const lang_fragment *seq, int count, int x, int y, int box_width,
    int height_offset, font_t font, color_t color)
{
    int current_x = x;
    int current_y = y;
    int line_height = height_offset > 0 ? height_offset : font_definition_for(font)->line_height;

    for (int i = 0; i < count; i++) {
        const lang_fragment *f = &seq[i];
        int fragment_width = 0;

        // Calculate the width of this fragment
        switch (f->type) {
            case LANG_FRAG_LABEL:
                fragment_width = lang_text_get_width(f->text_group, f->text_id, font);
                break;
            case LANG_FRAG_AMOUNT:
                fragment_width = lang_text_get_amount_width(f->text_group, f->text_id, f->number, font);
                break;
            case LANG_FRAG_NUMBER:
                fragment_width = text_get_number_width(f->number, '\0', "\0", font);
                break;
            case LANG_FRAG_TEXT:
                fragment_width = text_get_width(f->text, font);
                break;
            case LANG_FRAG_SPACE:
                fragment_width = f->space_width;
                break;
        }

        // Check if fragment exceeds remaining width on current line
        int remaining_width = x + box_width - current_x;

        // If we're not at the start of a line and the fragment doesn't fit, wrap to next line
        if (current_x > x && fragment_width > remaining_width) {
            current_x = x;
            current_y += line_height;
            remaining_width = box_width;
        }

        // If fragment is still wider than remaining width, use multiline drawing for text fragments
        if (fragment_width > remaining_width) {
            const uint8_t *str = 0;
            int height_drawn = 0;
            switch (f->type) {
                case LANG_FRAG_LABEL:
                    str = lang_get_string(f->text_group, f->text_id);
                    height_drawn = text_draw_multiline(str, current_x, current_y, remaining_width, 0, font, color);
                    current_x = x;
                    current_y += height_drawn;
                    break;
                case LANG_FRAG_TEXT:
                    height_drawn = text_draw_multiline(f->text, current_x, current_y, remaining_width, 0, font, color);
                    current_x = x;
                    current_y += height_drawn;
                    break;
                default:
                    // For other fragment types that don't support multiline, draw them anyway
                    // (they'll overflow but at least they'll be visible)
                    switch (f->type) {
                        case LANG_FRAG_AMOUNT:
                            current_x += lang_text_draw_amount_colored(f->text_group, f->text_id, f->number,
                                current_x, current_y, font, color);
                            break;
                        case LANG_FRAG_NUMBER:
                            current_x += text_draw_number(f->number, '\0', "\0", current_x, current_y, font, color);
                            break;
                        case LANG_FRAG_SPACE:
                            current_x += f->space_width;
                            break;
                        default:
                            break;
                    }
                    break;
            }
        } else {
            // Fragment fits, draw normally
            switch (f->type) {
                case LANG_FRAG_LABEL:
                    current_x += lang_text_draw_colored(f->text_group, f->text_id, current_x, current_y, font, color);
                    break;
                case LANG_FRAG_AMOUNT:
                    current_x += lang_text_draw_amount_colored(f->text_group, f->text_id, f->number,
                        current_x, current_y, font, color);
                    break;
                case LANG_FRAG_NUMBER:
                    current_x += text_draw_number(f->number, '\0', "\0", current_x, current_y, font, color);
                    break;
                case LANG_FRAG_TEXT:
                    current_x += text_draw(f->text, current_x, current_y, font, color);
                    break;
                case LANG_FRAG_SPACE:
                    current_x += f->space_width;
                    break;
            }
        }
    }

    return current_y - y + line_height; // Return total height used
}

int lang_text_draw_sequence_centered(

    const lang_fragment *seq, int count, int x, int y, int box_width, font_t font, color_t color)
{
    int total_width = lang_text_get_sequence_width(seq, count, font);
    int start_x = x + (box_width - total_width) / 2;
    return lang_text_draw_sequence(seq, count, start_x, y, font, color);
}

static int lang_text_draw_sequence_ellipsized_internal(const lang_fragment *seq, int count, int x, int y,
    int box_width, font_t font, color_t color, int *was_ellipsized)
{
    if (was_ellipsized) {
        *was_ellipsized = 0;
    }

    int width = 0;
    int remaining_width = box_width;

    for (int i = 0; i < count; i++) {
        const lang_fragment *f = &seq[i];
        int fragment_width = 0;

        // Calculate the width of this fragment
        switch (f->type) {
            case LANG_FRAG_LABEL:
                fragment_width = lang_text_get_width(f->text_group, f->text_id, font);
                fragment_width -= font_definition_for(font)->space_width;
                break;
            case LANG_FRAG_AMOUNT:
                fragment_width = lang_text_get_amount_width(f->text_group, f->text_id, f->number, font);
                fragment_width -= font_definition_for(font)->space_width;
                break;
            case LANG_FRAG_NUMBER:
                fragment_width = text_get_number_width(f->number, '\0', "\0", font);
                break;
            case LANG_FRAG_TEXT:
                fragment_width = text_get_width(f->text, font);
                break;
            case LANG_FRAG_SPACE:
                fragment_width = f->space_width;
                break;
        }

        // Check if this fragment fits in the remaining width
        if (fragment_width <= remaining_width) {
            // Fragment fits, draw it normally
            switch (f->type) {
                case LANG_FRAG_LABEL:
                    width += lang_text_draw_colored(f->text_group, f->text_id, x + width, y, font, color);
                    break;
                case LANG_FRAG_AMOUNT:
                    width += lang_text_draw_amount_colored(f->text_group, f->text_id, f->number,
                        x + width, y, font, color);
                    break;
                case LANG_FRAG_NUMBER:
                    width += text_draw_number(f->number, '\0', "\0", x + width, y, font, color);
                    break;
                case LANG_FRAG_TEXT:
                    width += text_draw(f->text, x + width, y, font, color);
                    break;
                case LANG_FRAG_SPACE:
                    width += f->space_width;
                    break;
            }
            remaining_width -= fragment_width;
        } else {
            // Fragment doesn't fit, ellipsize it
            if (was_ellipsized) {
                *was_ellipsized = 1;
            }

            const uint8_t *str = 0;
            switch (f->type) {
                case LANG_FRAG_LABEL:
                    str = lang_get_string(f->text_group, f->text_id);
                    text_draw_ellipsized(str, x + width, y, remaining_width, font, color);
                    break;
                case LANG_FRAG_TEXT:
                    text_draw_ellipsized(f->text, x + width, y, remaining_width, font, color);
                    break;
                case LANG_FRAG_AMOUNT:
                case LANG_FRAG_NUMBER:
                case LANG_FRAG_SPACE:
                    // For non-text fragments, just draw them (they'll overflow or be cut off)
                    // Numbers/amounts don't ellipsize well, so we just skip them if they don't fit
                    break;
            }

            // Stop processing further fragments since we've run out of space
            width += remaining_width;
            remaining_width = 0;
            break;
        }
    }

    return width;
}

int lang_text_draw_sequence_ellipsized(const lang_fragment *seq, int count, int x, int y, int box_width,
    font_t font, color_t color, int *was_ellipsized)
{
    return lang_text_draw_sequence_ellipsized_internal(seq, count, x, y, box_width, font, color, was_ellipsized);
}

int lang_text_draw_sequence_centered_ellipsized(const lang_fragment *seq, int count, int x, int y, int box_width,
    font_t font, color_t color, int *was_ellipsized)
{
    int total_width = lang_text_get_sequence_width(seq, count, font);

    // If it fits without ellipsizing, center it normally
    if (total_width <= box_width) {
        int start_x = x + (box_width - total_width) / 2;
        return lang_text_draw_sequence_ellipsized_internal(seq, count, start_x, y, box_width, font, color, was_ellipsized);
    }

    // Otherwise, draw from the left edge with ellipsizing
    return lang_text_draw_sequence_ellipsized_internal(seq, count, x, y, box_width, font, color, was_ellipsized);
}

int lang_text_concatenate_sequence(const lang_fragment *seq, int count, uint8_t *dst, int dst_size)
{
    if (!dst || dst_size <= 0) {
        return 0;
    }

    uint8_t *cursor = dst;
    int remaining = dst_size - 1; // Reserve space for null terminator

    for (int i = 0; i < count && remaining > 0; i++) {
        const lang_fragment *f = &seq[i];
        const uint8_t *str = 0;
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
                // For amounts, we need to format the number and add the appropriate text
            {
                int amount_offset = (f->number == 1 || f->number == -1) ? 0 : 1;

                // Format the number
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

                // Add a space
                if (remaining > 0) {
                    *cursor = ' ';
                    cursor++;
                    remaining--;
                }

                // Add the text
                if (remaining > 0) {
                    str = lang_get_string(f->text_group, f->text_id + amount_offset);
                    len = string_length(str);
                    if (len > remaining) len = remaining;
                    string_copy(str, cursor, len + 1);
                    cursor += len;
                    remaining -= len;
                }
            }
            break;

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
                // Add the specified number of spaces
                for (int j = 0; j < f->space_width && remaining > 0; j++) {
                    *cursor = ' ';
                    cursor++;
                    remaining--;
                }
                break;
        }
    }

    *cursor = 0; // Null terminate
    return cursor - dst; // Return length of concatenated string
}
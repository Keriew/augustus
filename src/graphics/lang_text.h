#ifndef GRAPHICS_LANG_TEXT_H
#define GRAPHICS_LANG_TEXT_H

#include "graphics/font.h"
#include "graphics/color.h"
#include "translation/translation.h"

/** Discriminator tag for @ref lang_fragment. */
typedef enum {
    LANG_FRAG_LABEL,    ///< A lang string identified by group + id.
    LANG_FRAG_AMOUNT,   ///< A number with an associated singular/plural lang string.
    LANG_FRAG_NUMBER,   ///< A raw integer.
    LANG_FRAG_FLOAT,    ///< A raw float with a configurable number of decimal places.
    LANG_FRAG_TEXT,     ///< A raw UTF-8 string pointer.
    LANG_FRAG_SPACE,    ///< A blank gap of explicit pixel width.
} lang_fragment_type;

/**
 * A single renderable unit in a fragment sequence.
 * Caller owns the memory; use the constructor helpers below to fill fields.
 * Only the fields relevant to the fragment's @ref type are read.
 */
typedef struct {
    lang_fragment_type type;
    int text_group;         ///< Lang string group  (@ref LANG_FRAG_LABEL, @ref LANG_FRAG_AMOUNT).
    int text_id;            ///< Lang string id     (@ref LANG_FRAG_LABEL, @ref LANG_FRAG_AMOUNT).
    int number;             ///< Integer value      (@ref LANG_FRAG_NUMBER, @ref LANG_FRAG_AMOUNT).
    float float_number;     ///< Float value        (@ref LANG_FRAG_FLOAT).
    int decimal_places;     ///< Decimal precision  (@ref LANG_FRAG_FLOAT).
    int space_width;        ///< Gap in pixels      (@ref LANG_FRAG_SPACE).
    const uint8_t *text;    ///< UTF-8 string       (@ref LANG_FRAG_TEXT).
} lang_fragment;

/**
 * @name Fragment constructors
 * Convenience helpers that populate a caller-owned @ref lang_fragment.
 * @{
 */
/** Initialises @p f as a lang string label (group + id). */
void lang_text_fragment_label(lang_fragment *f, int text_group, int text_id);
/** Initialises @p f as a number with a singular/plural lang string (group + id + number). */
void lang_text_fragment_amount(lang_fragment *f, int text_group, int text_id, int number);
/** Initialises @p f as a raw integer. */
void lang_text_fragment_number(lang_fragment *f, int number);
/** Initialises @p f as a float rendered to @p decimal_places places. */
void lang_text_fragment_float(lang_fragment *f, float number, int decimal_places);
/** Initialises @p f as a raw UTF-8 string pointer. */
void lang_text_fragment_text(lang_fragment *f, const uint8_t *text);
/** Initialises @p f as a blank gap of @p space_width pixels. */
void lang_text_fragment_space(lang_fragment *f, int space_width);
/** @} */

int lang_text_get_width(int group, int number, font_t font);

int lang_text_draw(int group, int number, int x_offset, int y_offset, font_t font);
int lang_text_draw_colored(int group, int number, int x_offset, int y_offset, font_t font, color_t color);

void lang_text_draw_centered(int group, int number, int x_offset, int y_offset, int box_width, font_t font);
void lang_text_draw_right_aligned(int group, int number, int x_offset, int y_offset, int box_width, font_t font);
void lang_text_draw_centered_colored(
    int group, int number, int x_offset, int y_offset, int box_width, font_t font, color_t color);

void lang_text_draw_ellipsized(int group, int number, int x_offset, int y_offset, int box_width, font_t font);
int lang_text_get_amount_width(int group, int number, int amount, font_t font);
int lang_text_draw_amount(int group, int number, int amount, int x_offset, int y_offset, font_t font);
int lang_text_draw_amount_centered(int group, int number, int amount, int x_offset, int y_offset, int box_width,
    font_t font);
int lang_text_draw_amount_colored(int group, int number, int amount, int x_offset, int y_offset,
    font_t font, color_t color);

int lang_text_draw_year(int year, int x_offset, int y_offset, font_t font);
void lang_text_draw_month_year_max_width(
    int month, int year, int x_offset, int y_offset, int box_width, font_t font, color_t color);

int lang_text_draw_multiline(int group, int number, int x_offset, int y_offset, int box_width, font_t font);

/**
 * @name Fragment sequence API
 * Functions that operate on an array of @ref lang_fragment (a "sequence").
 * @{
 */
/** Returns the total pixel width of the sequence (no trailing space). */
int lang_text_get_sequence_width(const lang_fragment *seq, int count, font_t font);
/** Draws the sequence left-aligned at (@p x, @p y). Returns the width drawn. */
int lang_text_draw_sequence(const lang_fragment *seq, int count, int x, int y, font_t font, color_t color);
/**
 * Draws the sequence with word-wrap inside @p box_width.
 * @p height_offset overrides the per-line advance; pass 0 to use the font default.
 * Returns the total pixel height used.
 */
int lang_text_draw_sequence_multiline(const lang_fragment *seq, int count, int x, int y, int box_width, int height_offset, font_t font, color_t color);
/** Draws the sequence horizontally centred within @p box_width. Returns the width drawn. */
int lang_text_draw_sequence_centered(const lang_fragment *seq, int count, int x, int y, int box_width, font_t font, color_t color);
/**
 * Draws the sequence, truncating with an ellipsis if it exceeds @p box_width.
 * Sets @p *was_ellipsized (may be NULL) to 1 if truncation occurred.
 * Returns the width drawn.
 */
int lang_text_draw_sequence_ellipsized(const lang_fragment *seq, int count, int x, int y, int box_width,
    font_t font, color_t color, int *was_ellipsized);
/**
 * Centred variant of @ref lang_text_draw_sequence_ellipsized.
 * Centres the sequence when it fits; falls back to left-aligned with ellipsis when it does not.
 */
int lang_text_draw_sequence_centered_ellipsized(const lang_fragment *seq, int count, int x, int y, int box_width, font_t font, color_t color, int *was_ellipsized);
/**
 * Concatenates the text content of the sequence into @p dst (size @p dst_size), null-terminated.
 * Returns the number of bytes written, excluding the null terminator.
 */
int lang_text_concatenate_sequence(const lang_fragment *seq, int count, uint8_t *dst, int dst_size);
/** @} */

#endif // GRAPHICS_LANG_TEXT_H

#ifndef GRAPHICS_LANG_SEQUENCE_H
#define GRAPHICS_LANG_SEQUENCE_H

#include "graphics/color.h"
#include "graphics/font.h"

#include <stdint.h>

/** Discriminator tag for @ref lang_fragment. */
typedef enum {
    LANG_FRAG_LABEL,    ///< A lang string identified by group + id.
    LANG_FRAG_AMOUNT,   ///< A number with an associated singular/plural lang string.
    LANG_FRAG_NUMBER,   ///< A raw integer.
    LANG_FRAG_FLOAT,    ///< A raw float with a configurable number of decimal places.
    LANG_FRAG_TEXT,     ///< A raw UTF-8 string pointer.
    LANG_FRAG_SPACE,    ///< A blank gap of explicit pixel width.
} lang_frag_type;

/**
 * A single renderable unit in a lang sequence.
 * Caller owns the memory; use the constructor helpers below to fill fields.
 * Only the fields relevant to the lang frag's @ref type are read.
 */
typedef struct lang_fragment {
    lang_frag_type type;
    int text_group;         ///< Lang string group  (@ref LANG_FRAG_LABEL, @ref LANG_FRAG_AMOUNT).
    int text_id;            ///< Lang string id     (@ref LANG_FRAG_LABEL, @ref LANG_FRAG_AMOUNT).
    int number;             ///< Integer value      (@ref LANG_FRAG_NUMBER, @ref LANG_FRAG_AMOUNT).
    float float_number;     ///< Float value        (@ref LANG_FRAG_FLOAT).
    int decimal_places;     ///< Decimal precision  (@ref LANG_FRAG_FLOAT).
    int space_width;        ///< Gap in pixels      (@ref LANG_FRAG_SPACE).
    const uint8_t *text;    ///< UTF-8 string       (@ref LANG_FRAG_TEXT).
} lang_fragment;

typedef struct lang_sequence {
    lang_fragment *fragments;
    int count;
} lang_sequence;

void lang_seq_init(lang_sequence *seq, lang_fragment *fragments, int count);

/**
 * @name Lang Frag Constructors
 * Convenience helpers that populate a caller-owned @ref lang_fragment.
 * @{
 */
 /** Initialises @p f as a lang string label (group + id). */
void lang_seq_frag_label(lang_fragment *f, int text_group, int text_id);
/** Initialises @p f as a number with a singular/plural lang string (group + id + number). */
void lang_seq_frag_amount(lang_fragment *f, int text_group, int text_id, int number);
/** Initialises @p f as a raw integer. */
void lang_seq_frag_number(lang_fragment *f, int number);
/** Initialises @p f as a float rendered to @p decimal_places places. */
void lang_seq_frag_float(lang_fragment *f, float number, int decimal_places);
/** Initialises @p f as a raw UTF-8 string pointer. */
void lang_seq_frag_text(lang_fragment *f, const uint8_t *text);
/** Initialises @p f as a blank gap of @p space_width pixels. */
void lang_seq_frag_space(lang_fragment *f, int space_width);
/** @} */

/**
 * @name Lang Sequence API
 * Functions that operate on a @ref lang_sequence.
 * @{
 */
 /** Returns the total pixel width of the sequence (no trailing space). */
int lang_seq_get_width(const lang_sequence *seq, font_t font);
/** Draws the sequence left-aligned at (@p x, @p y). Returns the width drawn. */
int lang_seq_draw(const lang_sequence *seq, int x, int y, font_t font, color_t color);
/**
 * Draws the sequence with word-wrap inside @p box_width.
 * @p height_offset overrides the per-line advance; pass 0 to use the font default.
 * Returns the total pixel height used.
 */
int lang_seq_draw_multiline_aligned_left(const lang_sequence *seq, int x, int y, int box_width,
    int height_offset, font_t font, color_t color);
int lang_seq_draw_multiline_aligned_center(const lang_sequence *seq, int x, int y, int box_width,
    int height_offset, font_t font, color_t color);
int lang_seq_draw_multiline_aligned_right(const lang_sequence *seq, int x, int y, int box_width,
    int height_offset, font_t font, color_t color);
/** Draws the sequence horizontally centred within @p box_width. Returns the width drawn. */
int lang_seq_draw_centered(const lang_sequence *seq, int x, int y, int box_width,
    font_t font, color_t color);
/**
 * Draws the sequence, truncating with an ellipsis if it exceeds @p box_width.
 * Sets @p *was_ellipsized (may be NULL) to 1 if truncation occurred.
 * Returns the width drawn.
 */
int lang_seq_draw_ellipsized(const lang_sequence *seq, int x, int y, int box_width,
    font_t font, color_t color, int *was_ellipsized);
/**
 * Centred variant of @ref lang_seq_draw_ellipsized.
 * Centres the sequence when it fits; falls back to left-aligned with ellipsis when it does not.
 */
int lang_seq_draw_centered_ellipsized(const lang_sequence *seq, int x, int y, int box_width,
    font_t font, color_t color, int *was_ellipsized);
/**
 * Concatenates the text content of the sequence into @p dst (size @p dst_size), null-terminated.
 * Returns the number of bytes written, excluding the null terminator.
 */
int lang_seq_concatenate(const lang_sequence *seq, uint8_t *dst, int dst_size);

int lang_seq_get_multiline_height(const lang_sequence *seq, int box_width, int height_offset, font_t font);

void lang_seq_draw_with_shadow(const lang_sequence *seq, int x, int y, int width, font_t font,
    color_t primary, color_t secondary, int centered, int sunken);
/** @} */


#endif // GRAPHICS_LANG_SEQUENCE_H

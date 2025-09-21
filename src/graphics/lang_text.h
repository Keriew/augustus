#ifndef GRAPHICS_LANG_TEXT_H
#define GRAPHICS_LANG_TEXT_H

#include "graphics/font.h"
#include "graphics/color.h"
#include "translation/translation.h"

typedef enum {
    LANG_FRAG_LABEL,    // a lang string
    LANG_FRAG_AMOUNT,   // a number + associated lang string
    LANG_FRAG_NUMBER,   // raw number
    LANG_FRAG_TEXT,     // raw utf-8 string
    LANG_FRAG_SPACE,    // a space of given width
} lang_fragment_type;

typedef struct {
    lang_fragment_type type;
    int text_group;
    int text_id;
    int number;
    int space_width;
    const uint8_t *text;
} lang_fragment;

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

int lang_text_get_sequence_width(const lang_fragment *seq, int count, font_t font);
int lang_text_draw_sequence(const lang_fragment *seq, int count, int x, int y, font_t font);
int lang_text_draw_sequence_centered(const lang_fragment *seq, int count, int x, int y, int box_width, font_t font);

#endif // GRAPHICS_LANG_TEXT_H
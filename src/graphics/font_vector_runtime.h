#pragma once

#include "graphics/color.h"
#include "graphics/font.h"

#include <string_view>

enum class FontSurfaceSemantic {
    Body = 0,
    Header = 1,
    Title = 2,
    Link = 3
};

enum FontInlineStyleFlags {
    FONT_INLINE_STYLE_NONE = 0,
    FONT_INLINE_STYLE_BOLD = 1 << 0,
    FONT_INLINE_STYLE_ITALIC = 1 << 1,
    FONT_INLINE_STYLE_UNDERLINE = 1 << 2
};

bool font_vector_runtime_load_pack();
void font_vector_runtime_reset();
bool font_vector_runtime_is_active();
const char *font_vector_runtime_failure_reason();

int font_vector_runtime_space_width(font_t font);
int font_vector_runtime_line_height(font_t font);
int font_vector_runtime_letter_spacing(font_t font);
int font_vector_runtime_baseline_offset(font_t font);
FontSurfaceSemantic font_vector_runtime_semantic(font_t font);

int font_vector_runtime_can_display_utf8(std::string_view utf8_character);
int font_vector_runtime_measure_utf8(std::string_view text, font_t font, unsigned style_flags, float scale);
unsigned int font_vector_runtime_fit_utf8_bytes(
    std::string_view text,
    font_t font,
    unsigned int requested_width,
    int invert,
    unsigned style_flags);
int font_vector_runtime_draw_utf8(
    std::string_view text,
    int x,
    int y,
    font_t font,
    color_t color,
    float scale,
    unsigned style_flags);

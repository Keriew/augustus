#pragma once

#include "graphics/color.h"
#include "graphics/font.h"
#include "graphics/ui_primitive.h"

#include <stdint.h>

enum class UiTextContentType {
    None,
    Language,
    Raw,
    Number,
    Amount,
};

enum class UiTextAlignment {
    Left,
    Center,
    Right,
};

struct UiTextSpec {
    UiTextContentType content_type = UiTextContentType::None;
    UiTextAlignment alignment = UiTextAlignment::Left;
    int text_group = 0;
    int text_id = 0;
    const uint8_t *raw_text = nullptr;
    int value = 0;
    char prefix = '\0';
    const char *postfix = nullptr;
    int x = 0;
    int y = 0;
    int box_width = 0;
    font_t font = FONT_NORMAL_BLACK;
    color_t color = COLOR_MASK_NONE;
};

class UiTextPrimitive : public UiPrimitive {
public:
    UiTextPrimitive(UiPrimitives &primitives, UiTextSpec spec);

    void draw() const;

private:
    bool has_renderable_payload() const;
    int measure_width() const;

    UiTextSpec spec_;
};

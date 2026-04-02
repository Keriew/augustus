#ifndef GRAPHICS_UI_ONE_ROW_BORDER_PRIMITIVE_H
#define GRAPHICS_UI_ONE_ROW_BORDER_PRIMITIVE_H

#include "graphics/ui_primitive.h"

class UiOneRowBorderPrimitive : public UiPrimitive {
public:
    UiOneRowBorderPrimitive(
        UiPrimitives &primitives,
        int x,
        int y,
        int width_pixels,
        int height_pixels,
        int has_focus,
        color_t color = COLOR_MASK_NONE);

    void draw() const;

private:
    int x_;
    int y_;
    int width_pixels_;
    int height_pixels_;
    int has_focus_;
    color_t color_;
};

#endif // GRAPHICS_UI_ONE_ROW_BORDER_PRIMITIVE_H

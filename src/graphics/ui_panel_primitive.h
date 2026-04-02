#ifndef GRAPHICS_UI_PANEL_PRIMITIVE_H
#define GRAPHICS_UI_PANEL_PRIMITIVE_H

#include "graphics/ui_primitive.h"

enum class UiPanelPrimitiveStyle {
    Outer,
    Unbordered,
    Inner
};

class UiPanelPrimitive : public UiPrimitive {
public:
    UiPanelPrimitive(
        UiPrimitives &primitives,
        UiPanelPrimitiveStyle style,
        int x,
        int y,
        int width_blocks,
        int height_blocks,
        color_t color = COLOR_MASK_NONE);

    void draw() const;

private:
    void draw_outer() const;
    void draw_unbordered() const;
    void draw_inner() const;

    UiPanelPrimitiveStyle style_;
    int x_;
    int y_;
    int width_blocks_;
    int height_blocks_;
    color_t color_;
};

#endif // GRAPHICS_UI_PANEL_PRIMITIVE_H

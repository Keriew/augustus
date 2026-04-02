#ifndef GRAPHICS_BORDERED_BUTTON_WIDGET_H
#define GRAPHICS_BORDERED_BUTTON_WIDGET_H

#include "graphics/button_widget.h"

class BorderedButtonWidget : public ButtonWidget {
public:
    BorderedButtonWidget(
        UiPrimitives &primitives,
        int x,
        int y,
        int width_pixels,
        int height_pixels,
        int has_focus,
        color_t color);

    void draw() const;

private:
    int width_pixels_;
    int height_pixels_;
    int has_focus_;
    color_t color_;
};

#endif // GRAPHICS_BORDERED_BUTTON_WIDGET_H

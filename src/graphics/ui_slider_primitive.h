#ifndef GRAPHICS_UI_SLIDER_PRIMITIVE_H
#define GRAPHICS_UI_SLIDER_PRIMITIVE_H

#include "graphics/ui_primitive.h"

class UiSliderPrimitive : public UiPrimitive {
public:
    UiSliderPrimitive(
        UiPrimitives &primitives,
        int x,
        int y,
        int width_pixels,
        int min_value,
        int max_value,
        int current_value);

    void draw() const;

private:
    int track_width_blocks() const;
    int trailing_width_pixels() const;
    int thumb_offset() const;

    int x_;
    int y_;
    int width_pixels_;
    int min_value_;
    int max_value_;
    int current_value_;
};

#endif // GRAPHICS_UI_SLIDER_PRIMITIVE_H

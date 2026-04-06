#ifndef GRAPHICS_UI_SPRITE_PRIMITIVE_H
#define GRAPHICS_UI_SPRITE_PRIMITIVE_H

#include "graphics/ui_primitive.h"

class UiSpritePrimitive : public UiPrimitive {
public:
    UiSpritePrimitive(
        UiPrimitives &primitives,
        int image_id,
        int x,
        int y,
        color_t color = COLOR_MASK_NONE,
        render_scaling_policy scaling_policy = RENDER_SCALING_POLICY_AUTO);

    void set_logical_size(int width, int height);
    void draw() const;

private:
    int image_id_;
    int x_;
    int y_;
    int logical_width_;
    int logical_height_;
    color_t color_;
    render_scaling_policy scaling_policy_;
};

#endif // GRAPHICS_UI_SPRITE_PRIMITIVE_H

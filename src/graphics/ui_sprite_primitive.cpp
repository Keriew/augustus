#include "graphics/ui_sprite_primitive.h"

UiSpritePrimitive::UiSpritePrimitive(
    UiPrimitives &primitives,
    int image_id,
    int x,
    int y,
    color_t color,
    render_scaling_policy scaling_policy)
    : UiPrimitive(primitives)
    , image_id_(image_id)
    , x_(x)
    , y_(y)
    , logical_width_(0)
    , logical_height_(0)
    , color_(color)
    , scaling_policy_(scaling_policy)
{
}

void UiSpritePrimitive::set_logical_size(int width, int height)
{
    logical_width_ = width;
    logical_height_ = height;
}

void UiSpritePrimitive::draw() const
{
    primitives_.draw_image_rect(
        image_id_,
        x_,
        y_,
        logical_width_,
        logical_height_,
        color_,
        scaling_policy_);
}

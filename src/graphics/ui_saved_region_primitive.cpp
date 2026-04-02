#include "graphics/ui_saved_region_primitive.h"

UiSavedRegionPrimitive::UiSavedRegionPrimitive(
    UiPrimitives &primitives,
    int image_id,
    int x,
    int y,
    int width,
    int height)
    : UiPrimitive(primitives)
    , image_id_(image_id)
    , x_(x)
    , y_(y)
    , width_(width)
    , height_(height)
{
}

int UiSavedRegionPrimitive::save() const
{
    return primitives_.save_region(image_id_, x_, y_, width_, height_);
}

void UiSavedRegionPrimitive::draw() const
{
    primitives_.draw_saved_region(image_id_, x_, y_);
}

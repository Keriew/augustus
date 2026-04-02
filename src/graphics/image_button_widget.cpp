#include "graphics/image_button_widget.h"

#include "graphics/ui_sprite_primitive.h"

extern "C" {
#include "core/image_group.h"
}

#include <string_view>

ImageButtonWidget::ImageButtonWidget(UiPrimitives &primitives, int x, int y, const image_button &button)
    : ButtonWidget(primitives, x, y)
    , button_(button)
{
}

int ImageButtonWidget::should_skip() const
{
    return button_.dont_draw != 0;
}

int ImageButtonWidget::resolve_image_id() const
{
    if (button_.assetlist_name) {
        return primitives_.image_id_from_asset_names(
            std::string_view(button_.assetlist_name),
            button_.image_name ? std::string_view(button_.image_name) : std::string_view());
    }
    if (button_.image_collection) {
        return image_group(button_.image_collection) + button_.image_offset;
    }
    return button_.image_offset;
}

void ImageButtonWidget::draw() const
{
    if (should_skip()) {
        return;
    }

    int image_id = resolve_image_id();
    if (!button_.static_image) {
        if (button_.enabled) {
            if (button_.pressed) {
                image_id += 2;
            } else if (button_.focused) {
                image_id += 1;
            }
        } else {
            image_id += 3;
        }
    }

    UiSpritePrimitive(
        primitives_,
        image_id,
        x_ + button_.x_offset,
        y_ + button_.y_offset,
        COLOR_MASK_NONE)
        .draw();
}

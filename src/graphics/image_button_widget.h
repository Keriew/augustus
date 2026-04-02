#ifndef GRAPHICS_IMAGE_BUTTON_WIDGET_H
#define GRAPHICS_IMAGE_BUTTON_WIDGET_H

#include "graphics/button_widget.h"

extern "C" {
#include "graphics/image_button.h"
}

class ImageButtonWidget : public ButtonWidget {
public:
    ImageButtonWidget(UiPrimitives &primitives, int x, int y, const image_button &button);

    void draw() const;

private:
    int resolve_image_id() const;
    int should_skip() const;

    const image_button &button_;
};

#endif // GRAPHICS_IMAGE_BUTTON_WIDGET_H

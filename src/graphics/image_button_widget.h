#pragma once

#include "graphics/button_widget.h"
#include "graphics/image_button.h"

class ImageButtonWidget : public ButtonWidget {
public:
    ImageButtonWidget(UiPrimitives &primitives, int x, int y, const image_button &button);

    void draw() const;

private:
    int resolve_image_id() const;
    int should_skip() const;

    const image_button &button_;
};

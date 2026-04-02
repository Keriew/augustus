#ifndef GRAPHICS_UI_WIDGET_H
#define GRAPHICS_UI_WIDGET_H

#include "graphics/ui_primitives.h"

class UiWidget {
public:
    virtual ~UiWidget() = default;

protected:
    explicit UiWidget(UiPrimitives &primitives)
        : primitives_(primitives)
    {
    }

    UiPrimitives &primitives_;
};

#endif // GRAPHICS_UI_WIDGET_H

#ifndef GRAPHICS_UI_PRIMITIVE_H
#define GRAPHICS_UI_PRIMITIVE_H

#include "graphics/ui_primitives.h"

class UiPrimitive {
public:
    virtual ~UiPrimitive() = default;

protected:
    explicit UiPrimitive(UiPrimitives &primitives)
        : primitives_(primitives)
    {
    }

    UiPrimitives &primitives_;
};

#endif // GRAPHICS_UI_PRIMITIVE_H

#ifndef GRAPHICS_SCROLLBAR_WIDGET_H
#define GRAPHICS_SCROLLBAR_WIDGET_H

#include "graphics/ui_widget.h"

extern "C" {
#include "graphics/scrollbar.h"
}

class ScrollbarWidget : public UiWidget {
public:
    ScrollbarWidget(UiPrimitives &primitives, const scrollbar_type &scrollbar);

    void draw() const;

private:
    const scrollbar_type &scrollbar_;
};

#endif // GRAPHICS_SCROLLBAR_WIDGET_H

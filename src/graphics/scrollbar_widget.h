#pragma once

#include "graphics/ui_widget.h"
#include "graphics/scrollbar.h"

class ScrollbarWidget : public UiWidget {
public:
    ScrollbarWidget(UiPrimitives &primitives, const scrollbar_type &scrollbar);

    void draw() const;

private:
    const scrollbar_type &scrollbar_;
};

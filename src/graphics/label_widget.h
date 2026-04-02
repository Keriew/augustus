#ifndef GRAPHICS_LABEL_WIDGET_H
#define GRAPHICS_LABEL_WIDGET_H

#include "graphics/ui_widget.h"

enum class LabelWidgetStyle {
    Normal,
    Large
};

class LabelWidget : public UiWidget {
public:
    LabelWidget(UiPrimitives &primitives, LabelWidgetStyle style, int x, int y, int width_blocks, int type);

    void draw() const;

private:
    LabelWidgetStyle style_;
    int x_;
    int y_;
    int width_blocks_;
    int type_;
};

#endif // GRAPHICS_LABEL_WIDGET_H

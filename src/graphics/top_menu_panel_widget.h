#ifndef GRAPHICS_TOP_MENU_PANEL_WIDGET_H
#define GRAPHICS_TOP_MENU_PANEL_WIDGET_H

#include "graphics/ui_widget.h"

class TopMenuPanelWidget : public UiWidget {
public:
    TopMenuPanelWidget(UiPrimitives &primitives, int x, int y, int width);

    void draw();
    int actual_width() const;

private:
    int x_;
    int y_;
    int width_;
    int actual_width_;
};

#endif // GRAPHICS_TOP_MENU_PANEL_WIDGET_H

#include "graphics/panel_widget.h"

#include "graphics/ui_panel_primitive.h"

PanelWidget::PanelWidget(
    UiPrimitives &primitives,
    PanelWidgetStyle style,
    int x,
    int y,
    int width_blocks,
    int height_blocks,
    color_t color)
    : UiWidget(primitives)
    , style_(style)
    , x_(x)
    , y_(y)
    , width_blocks_(width_blocks)
    , height_blocks_(height_blocks)
    , color_(color)
{
}

void PanelWidget::draw() const
{
    switch (style_) {
        case PanelWidgetStyle::Outer:
            draw_outer();
            break;
        case PanelWidgetStyle::Unbordered:
            draw_unbordered();
            break;
        case PanelWidgetStyle::Inner:
        default:
            draw_inner();
            break;
    }
}

void PanelWidget::draw_outer() const
{
    UiPanelPrimitive(primitives_, UiPanelPrimitiveStyle::Outer, x_, y_, width_blocks_, height_blocks_, COLOR_MASK_NONE).draw();
}

void PanelWidget::draw_unbordered() const
{
    UiPanelPrimitive(primitives_, UiPanelPrimitiveStyle::Unbordered, x_, y_, width_blocks_, height_blocks_, color_).draw();
}

void PanelWidget::draw_inner() const
{
    UiPanelPrimitive(primitives_, UiPanelPrimitiveStyle::Inner, x_, y_, width_blocks_, height_blocks_, COLOR_MASK_NONE).draw();
}

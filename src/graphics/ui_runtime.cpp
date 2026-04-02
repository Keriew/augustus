#include "ui_runtime.h"

#include "graphics/bordered_button_widget.h"
#include "graphics/image_button_widget.h"
#include "graphics/label_widget.h"
#include "graphics/panel_widget.h"
#include "graphics/scrollbar_widget.h"
#include "graphics/top_menu_panel_widget.h"
#include "graphics/ui_runtime_api.h"

SharedUiRuntime &shared_ui_runtime(void)
{
    static SharedUiRuntime runtime;
    return runtime;
}

void SharedUiRuntime::draw_button_border(
    int x,
    int y,
    int width_pixels,
    int height_pixels,
    int has_focus,
    color_t color)
{
    BorderedButtonWidget(primitives_, x, y, width_pixels, height_pixels, has_focus, color).draw();
}

void SharedUiRuntime::draw_outer_panel(int x, int y, int width_blocks, int height_blocks)
{
    PanelWidget(primitives_, PanelWidgetStyle::Outer, x, y, width_blocks, height_blocks, COLOR_MASK_NONE).draw();
}

void SharedUiRuntime::draw_unbordered_panel(int x, int y, int width_blocks, int height_blocks, color_t color)
{
    PanelWidget(primitives_, PanelWidgetStyle::Unbordered, x, y, width_blocks, height_blocks, color).draw();
}

void SharedUiRuntime::draw_inner_panel(int x, int y, int width_blocks, int height_blocks)
{
    PanelWidget(primitives_, PanelWidgetStyle::Inner, x, y, width_blocks, height_blocks, COLOR_MASK_NONE).draw();
}

void SharedUiRuntime::draw_label(int x, int y, int width_blocks, int type)
{
    LabelWidget(primitives_, LabelWidgetStyle::Normal, x, y, width_blocks, type).draw();
}

void SharedUiRuntime::draw_large_label(int x, int y, int width_blocks, int type)
{
    LabelWidget(primitives_, LabelWidgetStyle::Large, x, y, width_blocks, type).draw();
}

int SharedUiRuntime::draw_top_menu_black_panel(int x, int y, int width)
{
    TopMenuPanelWidget widget(primitives_, x, y, width);
    widget.draw();
    return widget.actual_width();
}

void SharedUiRuntime::draw_image_button(int x, int y, const image_button &button)
{
    ImageButtonWidget(primitives_, x, y, button).draw();
}

void SharedUiRuntime::draw_scrollbar_dot(const scrollbar_type &scrollbar)
{
    ScrollbarWidget(primitives_, scrollbar).draw();
}

int SharedUiRuntime::save_region(int image_id, int x, int y, int width, int height)
{
    return primitives_.save_region(image_id, x, y, width, height);
}

void SharedUiRuntime::draw_saved_region(int image_id, int x, int y)
{
    primitives_.draw_saved_region(image_id, x, y);
}

extern "C" {

void ui_runtime_draw_button_border(
    int x,
    int y,
    int width_pixels,
    int height_pixels,
    int has_focus,
    color_t color)
{
    shared_ui_runtime().draw_button_border(x, y, width_pixels, height_pixels, has_focus, color);
}

void ui_runtime_draw_outer_panel(int x, int y, int width_blocks, int height_blocks)
{
    shared_ui_runtime().draw_outer_panel(x, y, width_blocks, height_blocks);
}

void ui_runtime_draw_unbordered_panel(int x, int y, int width_blocks, int height_blocks, color_t color)
{
    shared_ui_runtime().draw_unbordered_panel(x, y, width_blocks, height_blocks, color);
}

void ui_runtime_draw_inner_panel(int x, int y, int width_blocks, int height_blocks)
{
    shared_ui_runtime().draw_inner_panel(x, y, width_blocks, height_blocks);
}

void ui_runtime_draw_label(int x, int y, int width_blocks, int type)
{
    shared_ui_runtime().draw_label(x, y, width_blocks, type);
}

void ui_runtime_draw_large_label(int x, int y, int width_blocks, int type)
{
    shared_ui_runtime().draw_large_label(x, y, width_blocks, type);
}

int ui_runtime_draw_top_menu_black_panel(int x, int y, int width)
{
    return shared_ui_runtime().draw_top_menu_black_panel(x, y, width);
}

void ui_runtime_draw_image_button(int x, int y, const image_button *button)
{
    if (!button) {
        return;
    }
    shared_ui_runtime().draw_image_button(x, y, *button);
}

void ui_runtime_draw_scrollbar_dot(const scrollbar_type *scrollbar)
{
    if (!scrollbar) {
        return;
    }
    shared_ui_runtime().draw_scrollbar_dot(*scrollbar);
}

int ui_runtime_save_to_image(int image_id, int x, int y, int width, int height)
{
    return shared_ui_runtime().save_region(image_id, x, y, width, height);
}

void ui_runtime_draw_from_image(int image_id, int x, int y)
{
    shared_ui_runtime().draw_saved_region(image_id, x, y);
}

} // extern "C"

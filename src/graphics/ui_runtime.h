#ifndef GRAPHICS_UI_RUNTIME_H
#define GRAPHICS_UI_RUNTIME_H

#include "graphics/bordered_button_widget.h"
#include "graphics/color.h"
#include "graphics/image_button.h"
#include "graphics/scrollbar.h"
#include "graphics/ui_text_primitive.h"

#include "graphics/ui_primitives.h"

struct EmpireTradeRouteButtonSpec;

class SharedUiRuntime {
public:
    UiPrimitives &primitives()
    {
        return primitives_;
    }

    void draw_button_border(
        int x,
        int y,
        int width_pixels,
        int height_pixels,
        int has_focus,
        color_t color);
    void draw_one_row_button_border(
        int x,
        int y,
        int width_pixels,
        int height_pixels,
        int has_focus,
        color_t color);

    void draw_outer_panel(int x, int y, int width_blocks, int height_blocks);
    void draw_unbordered_panel(int x, int y, int width_blocks, int height_blocks, color_t color);
    void draw_inner_panel(int x, int y, int width_blocks, int height_blocks);
    void draw_label(int x, int y, int width_blocks, int type);
    void draw_large_label(int x, int y, int width_blocks, int type);
    int draw_top_menu_black_panel(int x, int y, int width);

    void draw_image_button(int x, int y, const image_button &button);
    void draw_bordered_icon_button(
        int x,
        int y,
        int width_pixels,
        int height_pixels,
        int has_focus,
        BorderedButtonIconSpec icon,
        color_t color = COLOR_MASK_NONE,
        BorderedButtonFillStyle fill_style = BorderedButtonFillStyle::PanelTexture);
    void draw_advisor_text_button(
        int x,
        int y,
        int width_pixels,
        int height_pixels,
        int has_focus,
        const UiTextSpec &text_spec,
        color_t color = COLOR_MASK_NONE);
    void draw_advisor_card_button(
        int x,
        int y,
        int width_pixels,
        int height_pixels,
        int has_focus,
        const UiTextSpec *text_specs,
        int text_spec_count,
        color_t color = COLOR_MASK_NONE);
    void draw_empire_trade_route_button(
        int x,
        int y,
        int width_pixels,
        int height_pixels,
        int has_focus,
        const EmpireTradeRouteButtonSpec &spec,
        color_t color = COLOR_MASK_NONE);
    void draw_scrollbar_dot(const scrollbar_type &scrollbar);
    void draw_slider(int x, int y, int width_pixels, int min_value, int max_value, int current_value);

    int save_region(int image_id, int x, int y, int width, int height);
    void draw_saved_region(int image_id, int x, int y);

private:
    UiPrimitives primitives_;
};

SharedUiRuntime &shared_ui_runtime(void);

#endif // GRAPHICS_UI_RUNTIME_H

#include "graphics/top_menu_panel_widget.h"

extern "C" {
#include "core/image_group.h"
#include "graphics/panel.h"
}

TopMenuPanelWidget::TopMenuPanelWidget(UiPrimitives &primitives, int x, int y, int width)
    : UiWidget(primitives)
    , x_(x)
    , y_(y)
    , width_(width)
    , actual_width_(0)
{
}

void TopMenuPanelWidget::draw()
{
    int blocks = ((width_ + BLACK_PANEL_BLOCK_WIDTH - 1) / BLACK_PANEL_BLOCK_WIDTH) - 2;
    if (blocks < BLACK_PANEL_MIDDLE_BLOCKS) {
        blocks = BLACK_PANEL_MIDDLE_BLOCKS;
    }
    actual_width_ = (blocks + 2) * BLACK_PANEL_BLOCK_WIDTH;

    int draw_x = x_;
    const image *left_cap = primitives_.resolve_image(image_group(GROUP_TOP_MENU) + 14);
    primitives_.draw_image(left_cap, static_cast<float>(draw_x), static_cast<float>(y_),
        BLACK_PANEL_BLOCK_WIDTH, left_cap ? left_cap->height : 0, COLOR_MASK_NONE);
    draw_x += BLACK_PANEL_BLOCK_WIDTH;

    int black_panel_base_id = primitives_.image_id_from_asset_names("UI", "Top_UI_Panel");
    for (int i = 0; i < blocks; ++i) {
        const image *mid = primitives_.resolve_image(black_panel_base_id + (i % BLACK_PANEL_MIDDLE_BLOCKS) + 1);
        primitives_.draw_image(mid, static_cast<float>(draw_x), static_cast<float>(y_),
            BLACK_PANEL_BLOCK_WIDTH, mid ? mid->height : 0, COLOR_MASK_NONE);
        draw_x += BLACK_PANEL_BLOCK_WIDTH;
    }

    const image *right_cap = primitives_.resolve_image(black_panel_base_id + 5);
    primitives_.draw_image(right_cap, static_cast<float>(draw_x), static_cast<float>(y_),
        BLACK_PANEL_BLOCK_WIDTH, right_cap ? right_cap->height : 0, COLOR_MASK_NONE);
}

int TopMenuPanelWidget::actual_width() const
{
    return actual_width_;
}

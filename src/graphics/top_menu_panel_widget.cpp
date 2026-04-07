#include "graphics/top_menu_panel_widget.h"

#include "graphics/ui_tiled_strip_primitive.h"

#include "graphics/ui_constants.h"

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
    const int black_panel_base_id = primitives_.image_id_from_asset_names("UI", "Top_UI_Panel");
    const image *left_cap = primitives_.resolve_image(black_panel_base_id);
    const int panel_height = left_cap ? left_cap->height : 0;

    UiTiledStripPrimitive(
        primitives_,
        black_panel_base_id,
        black_panel_base_id + 1,
        black_panel_base_id + 5,
        x_,
        y_,
        blocks + 2,
        BLACK_PANEL_BLOCK_WIDTH,
        panel_height,
        COLOR_MASK_NONE,
        BLACK_PANEL_MIDDLE_BLOCKS)
        .draw();
}

int TopMenuPanelWidget::actual_width() const
{
    return actual_width_;
}

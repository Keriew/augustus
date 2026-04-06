#include "graphics/top_menu_panel_widget.h"

#include "graphics/ui_sprite_primitive.h"

#include "core/image_group.h"
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
    int black_panel_base_id = primitives_.image_id_from_asset_names("UI", "Top_UI_Panel");

    int draw_x = x_;
    const int left_cap_id = black_panel_base_id;
    const image *left_cap = primitives_.resolve_image(left_cap_id);
    UiSpritePrimitive left(primitives_, left_cap_id, draw_x, y_, COLOR_MASK_NONE);
    left.set_logical_size(BLACK_PANEL_BLOCK_WIDTH, left_cap ? left_cap->height : 0);
    left.draw();
    draw_x += BLACK_PANEL_BLOCK_WIDTH;

    for (int i = 0; i < blocks; ++i) {
        int image_id = black_panel_base_id + (i % BLACK_PANEL_MIDDLE_BLOCKS) + 1;
        const image *mid = primitives_.resolve_image(image_id);
        UiSpritePrimitive center(primitives_, image_id, draw_x, y_, COLOR_MASK_NONE);
        center.set_logical_size(BLACK_PANEL_BLOCK_WIDTH, mid ? mid->height : 0);
        center.draw();
        draw_x += BLACK_PANEL_BLOCK_WIDTH;
    }

    const int right_cap_id = black_panel_base_id + 5;
    const image *right_cap = primitives_.resolve_image(right_cap_id);
    UiSpritePrimitive right(primitives_, right_cap_id, draw_x, y_, COLOR_MASK_NONE);
    right.set_logical_size(BLACK_PANEL_BLOCK_WIDTH, right_cap ? right_cap->height : 0);
    right.draw();
}

int TopMenuPanelWidget::actual_width() const
{
    return actual_width_;
}

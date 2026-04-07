#include "graphics/top_menu_panel_widget.h"

#include "assets/image_group_entry.h"
#include "assets/image_group_payload.h"
#include "core/crash_context.h"
#include "graphics/ui_tiled_strip_primitive.h"

#include "graphics/ui_constants.h"

#include <utility>
#include <vector>

namespace {

constexpr const char *TOP_MENU_PANEL_GROUP_KEY = "UI\\Top_Menu";
constexpr int TOP_MENU_PANEL_LEFT_CAP_INDEX = 0;
constexpr int TOP_MENU_PANEL_FIRST_MIDDLE_INDEX = 1;
constexpr int TOP_MENU_PANEL_MIDDLE_PATTERN_LENGTH = 4;
constexpr int TOP_MENU_PANEL_RIGHT_CAP_INDEX = 5;

} // namespace

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
    ErrorContextScope error_scope("ui.top_menu_panel", TOP_MENU_PANEL_GROUP_KEY);
    int blocks = ((width_ + BLACK_PANEL_BLOCK_WIDTH - 1) / BLACK_PANEL_BLOCK_WIDTH) - 2;
    if (blocks < BLACK_PANEL_MIDDLE_BLOCKS) {
        blocks = BLACK_PANEL_MIDDLE_BLOCKS;
    }

    actual_width_ = (blocks + 2) * BLACK_PANEL_BLOCK_WIDTH;
    const ImageGroupPayload *group = primitives_.resolve_image_group(TOP_MENU_PANEL_GROUP_KEY, false);
    const bool report_group_entry_failure = group != nullptr;
    const ImageGroupEntry *left_cap = primitives_.resolve_group_entry(
        group,
        TOP_MENU_PANEL_GROUP_KEY,
        TOP_MENU_PANEL_LEFT_CAP_INDEX,
        report_group_entry_failure);
    const ImageGroupEntry *right_cap = primitives_.resolve_group_entry(
        group,
        TOP_MENU_PANEL_GROUP_KEY,
        TOP_MENU_PANEL_RIGHT_CAP_INDEX,
        report_group_entry_failure);

    std::vector<RuntimeDrawSlice> middle_slices;
    middle_slices.reserve(TOP_MENU_PANEL_MIDDLE_PATTERN_LENGTH);
    for (int i = 0; i < TOP_MENU_PANEL_MIDDLE_PATTERN_LENGTH; ++i) {
        const ImageGroupEntry *middle = primitives_.resolve_group_entry(
            group,
            TOP_MENU_PANEL_GROUP_KEY,
            TOP_MENU_PANEL_FIRST_MIDDLE_INDEX + i,
            report_group_entry_failure);
        if (!middle || !middle->footprint()) {
            middle_slices.clear();
            break;
        }
        middle_slices.push_back(*middle->footprint());
    }

    if (left_cap && left_cap->footprint() && right_cap && right_cap->footprint() && !middle_slices.empty()) {
        const int panel_height = left_cap->footprint()->height;
        UiTiledStripPrimitive(
            primitives_,
            *left_cap->footprint(),
            std::move(middle_slices),
            *right_cap->footprint(),
            x_,
            y_,
            blocks + 2,
            BLACK_PANEL_BLOCK_WIDTH,
            panel_height,
            COLOR_MASK_NONE)
            .draw();
        return;
    }

    const int black_panel_base_id = primitives_.image_id_from_asset_names("UI", "Top_UI_Panel");
    const image *legacy_left_cap = primitives_.resolve_image(black_panel_base_id);
    const int panel_height = legacy_left_cap ? legacy_left_cap->height : 0;
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

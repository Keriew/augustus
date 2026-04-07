#include "graphics/ui_bordered_button_skin.h"

#include "assets/image_group_entry.h"
#include "assets/image_group_payload.h"
#include "core/image.h"
#include "core/image_group.h"
#include "graphics/ui_constants.h"
#include "graphics/ui_primitives.h"

namespace {

constexpr const char *BORDERED_BUTTON_GROUP_KEY = "UI\\Bordered_Button";
constexpr int BORDERED_BUTTON_SLICE_COUNT = 8;
constexpr int BORDERED_BUTTON_FOCUSED_OFFSET = 8;

} // namespace

UiBorderedButtonSkin ui_bordered_button_skin_resolve(const UiPrimitives &primitives, int has_focus)
{
    UiBorderedButtonSkin skin;

    const ImageGroupPayload *group = primitives.resolve_image_group(BORDERED_BUTTON_GROUP_KEY, false);
    const bool report_group_entry_failure = group != nullptr;
    if (group) {
        const int state_offset = has_focus ? BORDERED_BUTTON_FOCUSED_OFFSET : 0;
        bool valid_runtime_group = true;
        for (int i = 0; i < BORDERED_BUTTON_SLICE_COUNT; ++i) {
            const ImageGroupEntry *entry = primitives.resolve_group_entry(
                group,
                BORDERED_BUTTON_GROUP_KEY,
                state_offset + i,
                report_group_entry_failure);
            if (!entry || !entry->footprint()) {
                valid_runtime_group = false;
                break;
            }
            skin.slices[static_cast<size_t>(i)] = *entry->footprint();
        }

        if (valid_runtime_group) {
            skin.use_runtime = true;
            skin.tile_width = skin.slices[1].is_valid() ? skin.slices[1].width : BLOCK_SIZE;
            skin.tile_height = skin.slices[7].is_valid() ? skin.slices[7].height : BLOCK_SIZE;
            return skin;
        }
    }

    skin.image_base = image_group(GROUP_BORDERED_BUTTON) + (has_focus ? BORDERED_BUTTON_FOCUSED_OFFSET : 0);

    const image *top_middle = primitives.resolve_image(skin.image_base + 1);
    skin.tile_width = top_middle && top_middle->width > 0 ? top_middle->width : BLOCK_SIZE;

    const image *left_side = primitives.resolve_image(skin.image_base + 7);
    skin.tile_height = left_side && left_side->height > 0 ? left_side->height : BLOCK_SIZE;

    return skin;
}

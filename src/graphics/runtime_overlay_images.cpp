#include "graphics/runtime_overlay_images.h"

#include "core/image_payload.h"

extern "C" {
#include "core/log.h"
#include "graphics/color.h"
#include "graphics/renderer.h"
}

#include <array>
#include <cmath>
#include <cstring>

namespace {

constexpr int kOverlayWidth = FOOTPRINT_WIDTH;
constexpr int kOverlayHeight = FOOTPRINT_HEIGHT;
constexpr const char *kWaterRangePayloadKey = "Graphics/RuntimeOverlay/WaterRange/Base";

struct RuntimeOverlayEntry {
    image img = {};
    const char *payload_key = nullptr;
};

std::array<RuntimeOverlayEntry, RUNTIME_OVERLAY_IMAGE_MAX> g_overlays = {
    RuntimeOverlayEntry { {}, kWaterRangePayloadKey }
};

void generate_water_range_pixels(std::array<color_t, kOverlayWidth * kOverlayHeight> &pixels)
{
    pixels.fill(ALPHA_TRANSPARENT);

    // Keep the generated art neutral; water, reservoir, and disabled states
    // reuse the same payload and apply their tint through the draw color mask.
    constexpr float center_x = (kOverlayWidth - 1) / 2.0f;
    constexpr float center_y = (kOverlayHeight - 1) / 2.0f;
    constexpr float half_width = kOverlayWidth / 2.0f;
    constexpr float half_height = kOverlayHeight / 2.0f;

    for (int y = 0; y < kOverlayHeight; y++) {
        for (int x = 0; x < kOverlayWidth; x++) {
            const float distance =
                std::fabs((x - center_x) / half_width) +
                std::fabs((y - center_y) / half_height);
            if (distance <= 1.0f) {
                pixels[y * kOverlayWidth + x] = COLOR_WHITE;
            }
        }
    }
}

int upload_overlay(RuntimeOverlayEntry &entry, const color_t *pixels, int width, int height)
{
    image_payload_release(&entry.img);
    std::memset(&entry.img, 0, sizeof(entry.img));

    entry.img.width = width;
    entry.img.height = height;
    entry.img.original.width = width;
    entry.img.original.height = height;
    entry.img.is_isometric = 1;

    const graphics_renderer_interface *renderer = graphics_renderer();
    if (!renderer || !renderer->upload_image_resource) {
        log_error("Runtime overlay image upload is unavailable", 0, 0);
        return 0;
    }

    renderer->upload_image_resource(&entry.img, pixels, width, height);
    if (entry.img.resource_handle <= 0) {
        log_error("Runtime overlay image upload failed", 0, 0);
        return 0;
    }

    if (!image_payload_register(&entry.img, entry.payload_key)) {
        log_error("Runtime overlay image payload registration failed", entry.payload_key, 0);
        image_payload_release(&entry.img);
        return 0;
    }
    return 1;
}

} // namespace

extern "C" int runtime_overlay_images_init_or_reload(void)
{
    std::array<color_t, kOverlayWidth * kOverlayHeight> water_range_pixels;
    generate_water_range_pixels(water_range_pixels);
    return upload_overlay(
        g_overlays[RUNTIME_OVERLAY_IMAGE_WATER_RANGE],
        water_range_pixels.data(),
        kOverlayWidth,
        kOverlayHeight);
}

extern "C" void runtime_overlay_images_reset(void)
{
    for (RuntimeOverlayEntry &entry : g_overlays) {
        image_payload_release(&entry.img);
        std::memset(&entry.img, 0, sizeof(entry.img));
    }
}

extern "C" const image *runtime_overlay_image_get(runtime_overlay_image type)
{
    if (type < 0 || type >= RUNTIME_OVERLAY_IMAGE_MAX) {
        return nullptr;
    }

    const image *payload_image = image_payload_legacy_view(&g_overlays[type].img);
    return payload_image && payload_image->resource_handle > 0 ? payload_image : nullptr;
}

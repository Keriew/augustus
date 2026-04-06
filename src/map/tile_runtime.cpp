#include "map/tile_runtime_internal.h"

#include "assets/image_group_payload.h"
#include "map/tile_runtime_graphics.h"
#include "core/crash_context.h"

extern "C" {
#include "core/log.h"
#include "map/grid.h"
#include "map/tile_runtime_api.h"
}

#include <cstdio>
#include <string>
#include <unordered_set>

namespace {

std::unordered_set<std::string> g_logged_tile_graphics_fallbacks;

void make_tile_context(char *buffer, size_t buffer_size, int grid_offset)
{
    snprintf(buffer, buffer_size, "grid_offset=%d", grid_offset);
}

void log_tile_scope_state(void *userdata)
{
    const tile_runtime *runtime = static_cast<const tile_runtime *>(userdata);
    if (!runtime || !runtime->definition()) {
        return;
    }

    char details[256];
    snprintf(
        details,
        sizeof(details),
        "grid_offset=%d graphics_path=%s plaza_image=%s",
        runtime->grid_offset(),
        runtime->definition()->graphics_path() ? runtime->definition()->graphics_path() : "",
        runtime->plaza_image_id());
    log_info("Graphics tile state", details, 0);
}

void log_tile_graphics_fallback_once(const char *message, const tile_runtime *runtime, const char *detail)
{
    char key_buffer[512];
    snprintf(
        key_buffer,
        sizeof(key_buffer),
        "%s|grid_offset=%d|path=%s|detail=%s",
        message ? message : "",
        runtime ? runtime->grid_offset() : -1,
        runtime && runtime->definition() && runtime->definition()->graphics_path() ?
            runtime->definition()->graphics_path() : "",
        detail ? detail : "");

    if (!g_logged_tile_graphics_fallbacks.insert(key_buffer).second) {
        return;
    }

    error_context_report_error(message, detail);
}

}

namespace tile_runtime_impl {

std::unordered_map<int, std::unique_ptr<tile_runtime>> g_runtime_tiles;

tile_runtime *get_instance(int grid_offset)
{
    auto it = g_runtime_tiles.find(grid_offset);
    return it == g_runtime_tiles.end() ? nullptr : it->second.get();
}

tile_runtime *get_or_create_instance(int grid_offset, tile_type_registry_impl::TileKind kind)
{
    if (!map_grid_is_valid_offset(grid_offset) || kind == tile_type_registry_impl::TileKind::None) {
        return nullptr;
    }

    const tile_type_registry_impl::TileType *definition = tile_type_registry_impl::get_tile_type(kind);
    if (!definition || !definition->has_graphics()) {
        return nullptr;
    }

    std::unique_ptr<tile_runtime> &slot = g_runtime_tiles[grid_offset];
    if (!slot || slot->grid_offset() != grid_offset || slot->definition() != definition) {
        slot = std::make_unique<tile_runtime>(grid_offset, definition);
    }
    return slot.get();
}

}

// Input: one runtime tile wrapper that already knows its authored plaza image id.
// Output: the native footprint slice for that tile, or null when the authored runtime tile graphic cannot be resolved.
const RuntimeDrawSlice *tile_runtime::resolve_graphic_slice() const
{
    if (!definition_ || !definition_->has_graphics() || !plaza_image_id_[0]) {
        return nullptr;
    }

    char context[128];
    make_tile_context(context, sizeof(context), grid_offset_);
    CrashContextScope crash_scope(
        "tile_runtime.resolve_graphic_image",
        context,
        log_tile_scope_state,
        const_cast<tile_runtime *>(this));
    if (!image_group_payload_load(definition_->graphics_path())) {
        char detail[256];
        snprintf(
            detail,
            sizeof(detail),
            "%s image=%s",
            definition_->graphics_path(),
            plaza_image_id());
        log_tile_graphics_fallback_once(
            "Tile graphics image could not be resolved. Falling back to legacy rendering.",
            this,
            detail);
        return nullptr;
    }

    const ImageGroupPayload *payload = image_group_payload_get(definition_->graphics_path());
    if (!payload) {
        return nullptr;
    }

    const ImageGroupEntry *entry = payload->entry_for(plaza_image_id());
    if (!entry || !entry->footprint()) {
        char detail[256];
        snprintf(
            detail,
            sizeof(detail),
            "%s image=%s",
            definition_->graphics_path(),
            plaza_image_id());
        log_tile_graphics_fallback_once(
            "Tile graphics image could not be resolved. Falling back to legacy rendering.",
            this,
            detail);
        return nullptr;
    }
    return entry->footprint();
}

extern "C" void tile_runtime_reset(void)
{
    tile_runtime_impl::g_runtime_tiles.clear();
    g_logged_tile_graphics_fallbacks.clear();
}

extern "C" void tile_runtime_clear(int grid_offset)
{
    tile_runtime_impl::g_runtime_tiles.erase(grid_offset);
}

extern "C" void tile_runtime_set_plaza_image_id(int grid_offset, const char *image_id)
{
    if (!image_id || !*image_id) {
        tile_runtime_clear(grid_offset);
        return;
    }

    if (tile_runtime *instance = tile_runtime_impl::get_or_create_instance(grid_offset, tile_type_registry_impl::TileKind::Plaza)) {
        instance->set_plaza_image_id(image_id);
    }
}

const RuntimeDrawSlice *tile_runtime_get_graphic_footprint_slice(int grid_offset)
{
    if (tile_runtime *instance = tile_runtime_impl::get_instance(grid_offset)) {
        return instance->resolve_graphic_slice();
    }
    return nullptr;
}

#include "map/tile_runtime_internal.h"

#include "assets/image_group_payload.h"

extern "C" {
#include "map/grid.h"
#include "map/tile_runtime_api.h"
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

const image *tile_runtime::resolve_graphic_image() const
{
    if (!definition_ || !definition_->has_graphics() || plaza_image_index_ < 0) {
        return nullptr;
    }
    return image_group_payload_get_image_at_index(definition_->graphics_path(), plaza_image_index_);
}

extern "C" void tile_runtime_reset(void)
{
    tile_runtime_impl::g_runtime_tiles.clear();
}

extern "C" void tile_runtime_clear(int grid_offset)
{
    tile_runtime_impl::g_runtime_tiles.erase(grid_offset);
}

extern "C" void tile_runtime_set_plaza_image_index(int grid_offset, int image_index)
{
    if (image_index < 0) {
        tile_runtime_clear(grid_offset);
        return;
    }

    if (tile_runtime *instance = tile_runtime_impl::get_or_create_instance(grid_offset, tile_type_registry_impl::TileKind::Plaza)) {
        instance->set_plaza_image_index(image_index);
    }
}

extern "C" const image *tile_runtime_get_graphic_image(int grid_offset)
{
    if (tile_runtime *instance = tile_runtime_impl::get_instance(grid_offset)) {
        return instance->resolve_graphic_image();
    }
    return nullptr;
}

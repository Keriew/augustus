#ifndef MAP_TILE_RUNTIME_H
#define MAP_TILE_RUNTIME_H

#include "graphics/runtime_texture.h"
#include "map/tile_type_registry_internal.h"

#ifdef __cplusplus

#include <string>

class tile_runtime {
public:
    tile_runtime(int grid_offset, const tile_type_registry_impl::TileType *definition)
        : grid_offset_(grid_offset)
        , definition_(definition)
    {
    }

    int grid_offset() const
    {
        return grid_offset_;
    }

    const tile_type_registry_impl::TileType *definition() const
    {
        return definition_;
    }

    void set_plaza_image_id(const char *image_id)
    {
        plaza_image_id_ = image_id ? image_id : "";
    }

    const char *plaza_image_id() const
    {
        return plaza_image_id_.c_str();
    }

    const RuntimeDrawSlice *resolve_graphic_slice() const;

private:
    int grid_offset_ = -1;
    const tile_type_registry_impl::TileType *definition_ = nullptr;
    std::string plaza_image_id_;
};

namespace tile_runtime_impl {

tile_runtime *get_or_create_instance(int grid_offset, tile_type_registry_impl::TileKind kind);
tile_runtime *get_instance(int grid_offset);

}

#endif

#endif // MAP_TILE_RUNTIME_H

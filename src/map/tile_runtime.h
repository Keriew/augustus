#ifndef MAP_TILE_RUNTIME_H
#define MAP_TILE_RUNTIME_H

#include "core/image.h"
#include "map/tile_type_registry_internal.h"

#ifdef __cplusplus

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

    void set_plaza_image_index(int image_index)
    {
        plaza_image_index_ = image_index;
    }

    int plaza_image_index() const
    {
        return plaza_image_index_;
    }

    const image *resolve_graphic_image() const;

private:
    int grid_offset_ = -1;
    const tile_type_registry_impl::TileType *definition_ = nullptr;
    int plaza_image_index_ = -1;
};

namespace tile_runtime_impl {

tile_runtime *get_or_create_instance(int grid_offset, tile_type_registry_impl::TileKind kind);
tile_runtime *get_instance(int grid_offset);

}

#endif

#endif // MAP_TILE_RUNTIME_H

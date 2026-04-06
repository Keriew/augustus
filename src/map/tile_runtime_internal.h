#ifndef MAP_TILE_RUNTIME_INTERNAL_H
#define MAP_TILE_RUNTIME_INTERNAL_H

#include "map/tile_runtime.h"

#include <memory>
#include <unordered_map>

namespace tile_runtime_impl {

extern std::unordered_map<int, std::unique_ptr<tile_runtime>> g_runtime_tiles;

}

#endif // MAP_TILE_RUNTIME_INTERNAL_H

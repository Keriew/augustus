#include "map/tile_type_registry_internal.h"

extern "C" {
#include "platform/file_manager.h"
}

#include <cstring>

namespace tile_type_registry_impl {

std::string g_tile_type_path;
std::array<std::unique_ptr<TileType>, static_cast<size_t>(TileKind::Count)> g_tile_types;
ParseState g_parse_state;

static int stop_on_first_entry(const char *name, long unused)
{
    (void) name;
    (void) unused;
    return LIST_MATCH;
}

int directory_exists(const char *path)
{
    return platform_file_manager_list_directory_contents(path, TYPE_DIR | TYPE_FILE, 0, stop_on_first_entry) != LIST_ERROR;
}

void refresh_tile_type_path()
{
    g_tile_type_path = std::string(mod_manager_get_mod_path()) + "Tiles/";
}

TileKind tile_kind_from_attr(const char *attr)
{
    if (attr && strcmp(attr, "plaza") == 0) {
        return TileKind::Plaza;
    }
    return TileKind::None;
}

const TileType *get_tile_type(TileKind kind)
{
    const size_t index = static_cast<size_t>(kind);
    if (index >= g_tile_types.size()) {
        return nullptr;
    }
    return g_tile_types[index].get();
}

}

extern "C" const char *tile_type_registry_get_tile_type_path(void)
{
    tile_type_registry_impl::refresh_tile_type_path();
    return tile_type_registry_impl::g_tile_type_path.c_str();
}

extern "C" int tile_type_registry_validate_mod(void)
{
    using namespace tile_type_registry_impl;
    refresh_tile_type_path();
    return static_cast<int>(
        static_cast<bool>(mod_manager_validate_mod_path()) &&
        static_cast<bool>(directory_exists(g_tile_type_path.c_str())));
}

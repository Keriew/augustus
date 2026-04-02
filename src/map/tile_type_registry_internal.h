#ifndef MAP_TILE_TYPE_REGISTRY_INTERNAL_H
#define MAP_TILE_TYPE_REGISTRY_INTERNAL_H

#include "map/tile_type_registry.h"

extern "C" {
#include "game/mod_manager.h"
}

#include <array>
#include <cstddef>
#include <memory>
#include <string>

namespace tile_type_registry_impl {

enum class TileKind {
    None = 0,
    Plaza,
    Count
};

struct GraphicsDefinition {
    void set_path(std::string path)
    {
        path_ = std::move(path);
    }

    int has_path() const
    {
        return !path_.empty();
    }

    const char *path() const
    {
        return path_.c_str();
    }

private:
    std::string path_;
};

class TileType {
public:
    TileType(TileKind kind, std::string attr)
        : kind_(kind)
        , attr_(std::move(attr))
    {
    }

    TileKind kind() const
    {
        return kind_;
    }

    const char *attr() const
    {
        return attr_.c_str();
    }

    void set_graphics_path(std::string path)
    {
        graphics_.set_path(std::move(path));
    }

    int has_graphics() const
    {
        return graphics_.has_path();
    }

    const char *graphics_path() const
    {
        return graphics_.path();
    }

private:
    TileKind kind_ = TileKind::None;
    std::string attr_;
    GraphicsDefinition graphics_;
};

struct ParseState {
    std::unique_ptr<TileType> definition;
    int saw_graphics = 0;
    int parsing_graphics = 0;
    int current_graphics_has_path = 0;
    int error = 0;
};

extern std::string g_tile_type_path;
extern std::array<std::unique_ptr<TileType>, static_cast<size_t>(TileKind::Count)> g_tile_types;
extern ParseState g_parse_state;

int directory_exists(const char *path);
void refresh_tile_type_path();
TileKind tile_kind_from_attr(const char *attr);
const TileType *get_tile_type(TileKind kind);

}

#endif // MAP_TILE_TYPE_REGISTRY_INTERNAL_H

#ifndef BUILDING_BUILDING_TYPE_REGISTRY_INTERNAL_H
#define BUILDING_BUILDING_TYPE_REGISTRY_INTERNAL_H

#include "building/building_type.h"
#include "building/building_type_api.h"

#include <array>
#include <memory>
#include <string>

namespace building_type_registry_impl {

enum class GraphicsParseTargetScope {
    Root,
    Default,
    Variant,
    Overlay
};

struct ParseState {
    std::unique_ptr<BuildingType> definition;
    size_t current_spawn_group_index = 0;
    int has_current_spawn_group = 0;
    int saw_graphic = 0;
    int saw_state = 0;
    int parsing_state = 0;
    int parsing_graphics = 0;
    int saw_legacy_graphics_nodes = 0;
    int has_current_graphics_variant = 0;
    int has_current_graphics_overlay = 0;
    size_t current_graphics_variant_index = 0;
    size_t current_graphics_overlay_index = 0;
    GraphicsParseTargetScope current_graphics_target_scope = GraphicsParseTargetScope::Root;
    int saw_spawn = 0;
    int error = 0;
};

extern std::string g_building_type_path;
extern std::array<std::unique_ptr<BuildingType>, BUILDING_TYPE_MAX> g_building_types;
extern ParseState g_parse_state;

int directory_exists(const char *path);
void refresh_building_type_path();

}

#endif // BUILDING_BUILDING_TYPE_REGISTRY_INTERNAL_H

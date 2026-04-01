#include "building/building_type_registry_internal.h"

extern "C" {
#include "platform/file_manager.h"
}

namespace building_type_registry_impl {

std::string g_building_type_path;
std::array<std::unique_ptr<BuildingType>, BUILDING_TYPE_MAX> g_building_types;
ParseState g_parse_state;

static int stop_on_first_entry(const char *name, long unused)
{
    return LIST_MATCH;
}

int directory_exists(const char *path)
{
    return platform_file_manager_list_directory_contents(path, TYPE_DIR | TYPE_FILE, 0, stop_on_first_entry) != LIST_ERROR;
}

void refresh_building_type_path()
{
    g_building_type_path = std::string(mod_manager_get_mod_path()) + "BuildingType/";
}

}

extern "C" const char *building_type_registry_get_building_type_path(void)
{
    building_type_registry_impl::refresh_building_type_path();
    return building_type_registry_impl::g_building_type_path.c_str();
}

extern "C" int building_type_registry_validate_mod(void)
{
    using namespace building_type_registry_impl;
    refresh_building_type_path();
    return static_cast<int>(
        static_cast<bool>(mod_manager_validate_mod_path()) &&
        static_cast<bool>(directory_exists(g_building_type_path.c_str())));
}

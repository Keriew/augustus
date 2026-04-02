#include "game/mod_manager.h"

extern "C" {
#include "assets/assets.h"
#include "core/dir.h"
#include "platform/file_manager.h"
}

#include <string>

namespace {

std::string g_mod_name = "Vespasian";
std::string g_mod_path = "Mods/Vespasian/";
std::string g_graphics_path = "Mods/Vespasian/Graphics/";
std::string g_augustus_graphics_path = "Mods/Augustus/Graphics/";
std::string g_julius_graphics_path = "Mods/Julius/Graphics/";

static int stop_on_first_entry(const char *name, long unused)
{
    return LIST_MATCH;
}

static int validate_directory_path(const char *path)
{
    return static_cast<int>(static_cast<bool>(
        platform_file_manager_list_directory_contents(path, TYPE_DIR | TYPE_FILE, 0, stop_on_first_entry) != LIST_ERROR));
}

static void rebuild_mod_path()
{
    g_mod_path = "Mods/" + g_mod_name + "/";
    g_graphics_path = g_mod_path + "Graphics/";
}

}

extern "C" void mod_manager_set_mod_name(const char *mod_name)
{
    if (mod_name && *mod_name) {
        g_mod_name = mod_name;
    } else {
        g_mod_name = "Vespasian";
    }
    rebuild_mod_path();
}

extern "C" const char *mod_manager_get_mod_name(void)
{
    return g_mod_name.c_str();
}

extern "C" const char *mod_manager_get_mod_path(void)
{
    return g_mod_path.c_str();
}

extern "C" const char *mod_manager_get_graphics_path(void)
{
    return g_graphics_path.c_str();
}

extern "C" const char *mod_manager_get_augustus_graphics_path(void)
{
    return g_augustus_graphics_path.c_str();
}

extern "C" const char *mod_manager_get_julius_graphics_path(void)
{
    return g_julius_graphics_path.c_str();
}

extern "C" int mod_manager_validate_mod_path(void)
{
    return validate_directory_path(g_mod_path.c_str());
}

extern "C" int mod_manager_validate_graphics_path(void)
{
    const int has_mod_graphics = validate_directory_path(g_graphics_path.c_str()) != 0;
    const int has_augustus_graphics = validate_directory_path(g_augustus_graphics_path.c_str()) != 0;
    const int has_julius_graphics = validate_directory_path(g_julius_graphics_path.c_str()) != 0;
    const int has_root_graphics = validate_directory_path(ASSETS_DIRECTORY "/" ASSETS_IMAGE_PATH) != 0;
    return has_mod_graphics || has_augustus_graphics || has_julius_graphics || has_root_graphics;
}

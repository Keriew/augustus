#include "game/mod_manager.h"

extern "C" {
#include "platform/file_manager.h"
}

#include <string>

namespace {

std::string g_mod_name = "Vespasian";
std::string g_mod_path = "Mods/Vespasian/";

static int stop_on_first_entry(const char *name, long unused)
{
    return LIST_MATCH;
}

static void rebuild_mod_path()
{
    g_mod_path = "Mods/" + g_mod_name + "/";
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

extern "C" int mod_manager_validate_mod_path(void)
{
    return static_cast<int>(static_cast<bool>(
        platform_file_manager_list_directory_contents(g_mod_path.c_str(), TYPE_DIR | TYPE_FILE, 0, stop_on_first_entry) != LIST_ERROR));
}

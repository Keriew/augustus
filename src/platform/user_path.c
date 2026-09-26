#include "user_path.h"

#include "core/file.h"
#include "core/string.h"
#include "platform/file_manager.h"
#include "platform/prefs.h"
#include "platform/platform.h"

#include <stdlib.h>
#include <string.h>

#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__) && !defined(__SWITCH__)
static int is_container(void)
{
    return getenv("container") || getenv("APPIMAGE") || getenv("SNAP");
}
#endif

const char *platform_user_path_recommend(void)
{
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__) || defined(__SWITCH__)
    return 0;
#else
    if (is_container()) {
        return 0;
    }

    static const char *user_path;
    if (!user_path) {
        user_path = platform_get_pref_path();
    }
    return user_path;
#endif
}

void platform_user_path_create_subdirectories(void)
{
    for (int i = 0; i < PATH_LOCATION_MAX; i++) {
        if (i == PATH_LOCATION_ROOT || i == PATH_LOCATION_ASSET) {
            continue;
        }
        const char *new_directory = platform_file_manager_get_directory_for_location(i, 0);
        if (*new_directory) {
            if (!(platform_file_manager_create_directory(new_directory, pref_user_dir(), 0))) {
                continue; // don't copy again if the path already exists
            }
            if (i == PATH_LOCATION_EDITOR_CONTENT) {
                // copy the community folder into editor/contents on creation
                char *paths[] = {
                    "audio",
                    "image",
                    "video",
                    0
                };
                for (int j = 0; paths[j]; j++) {
                    char source[FILE_NAME_MAX];
                    char destination[FILE_NAME_MAX];
                    snprintf(source, FILE_NAME_MAX, "%s/%s",
                        platform_file_manager_get_directory_for_location(PATH_LOCATION_COMMUNITY, 0), paths[j]);
                    snprintf(destination, FILE_NAME_MAX, "%s/%s",
                        platform_file_manager_get_directory_for_location(PATH_LOCATION_EDITOR_CONTENT, 0), paths[j]);
                    if (!source[0] || !destination[0]) {
                        continue;
                    }
                    platform_file_manager_copy_directory(source, destination, 0);
                }
            }
        }
    }
}

void plaform_user_path_wrap_scenarios_in_dirs(void)
{
    const dir_listing *listing = dir_find_files_with_extension_at_location(PATH_LOCATION_SCENARIO, "map");
    listing = dir_append_files_with_extension("mapx");
    if (!listing) {
        return;
    }
    for (int i = 0; i < listing->num_files; i++) {
        char foldername[FILE_NAME_MAX];
        string_copy((const uint8_t *)listing->files[i].name, (uint8_t *)foldername, FILE_NAME_MAX);
        file_remove_extension(foldername);
        char new_directory[FILE_NAME_MAX];
        snprintf(new_directory, FILE_NAME_MAX, "%s/%s",
            platform_file_manager_get_directory_for_location(PATH_LOCATION_SCENARIO, 0), foldername);
        if (!(platform_file_manager_create_directory(new_directory, pref_user_dir(), 0))) {
            continue; // if creating directory fails continue with next
        }

        char filename[FILE_NAME_MAX];
        char new_filename[FILE_NAME_MAX];
        snprintf(filename, FILE_NAME_MAX, "%s/%s",
            platform_file_manager_get_directory_for_location(PATH_LOCATION_SCENARIO, 0), listing->files[i].name);
        snprintf(new_filename, FILE_NAME_MAX, "%s/%s", new_directory, listing->files[i].name);

        if (!(platform_file_manager_copy_file(filename, new_filename))) {
            continue; // if copying file fails continue with next
        }

        // after copy remove file
        platform_file_manager_remove_file(filename);
    }
}

static int is_same_directory(const char *original_directory, const char *new_directory)
{
    if (strncmp(original_directory, "./", 2) == 0) {
        original_directory += 2;
    }
    if (strncmp(new_directory, "./", 2) == 0) {
        new_directory += 2;
    }
    return strcmp(new_directory, original_directory) == 0;
}

void platform_user_path_copy_files(const char *original_user_path, int overwrite)
{
    for (int location = 0; location < PATH_LOCATION_MAX; location++) {
        if (location == PATH_LOCATION_ROOT || location == PATH_LOCATION_ASSET) {
            continue;
        }
        char original_directory[FILE_NAME_MAX];
        char new_directory[FILE_NAME_MAX];
        snprintf(original_directory, FILE_NAME_MAX, "%s",
            platform_file_manager_get_directory_for_location(location, original_user_path));
        snprintf(new_directory, FILE_NAME_MAX, "%s",
            platform_file_manager_get_directory_for_location(location, 0));
        if (is_same_directory(original_directory, new_directory)) {
            continue;
        }
        const dir_listing *listing = 0;
        int has_subdirectories = 0;
        switch (location) {
            case PATH_LOCATION_CONFIG:
                listing = dir_find_files_with_extension(original_directory, "inf");
                listing = dir_append_files_with_extension("ini");
                break;
            case PATH_LOCATION_SAVEGAME:
                listing = dir_find_files_with_extension(original_directory, "sav");
                listing = dir_append_files_with_extension("svx");
                break;
            case PATH_LOCATION_SCENARIO:
                has_subdirectories = 1;
                break;
            case PATH_LOCATION_CAMPAIGN:
                listing = dir_find_files_with_extension(original_directory, "campaign");
                has_subdirectories = 1;
                break;
            case PATH_LOCATION_SCREENSHOT:
                listing = dir_find_files_with_extension(original_directory, "png");
                listing = dir_append_files_with_extension("bmp");
                break;
            case PATH_LOCATION_COMMUNITY:
            case PATH_LOCATION_EDITOR_CONTENT:
                has_subdirectories = 1;
                break;
            case PATH_LOCATION_EDITOR_CUSTOM_EMPIRES:
            case PATH_LOCATION_EDITOR_CUSTOM_MESSAGES:
            case PATH_LOCATION_EDITOR_CUSTOM_EVENTS:
            case PATH_LOCATION_EDITOR_MODEL_DATA:
                listing = dir_find_files_with_extension(original_directory, "xml");
                break;
            default:
                continue;
        }
        if (listing) {
            char original_name[FILE_NAME_MAX];
            char new_name[FILE_NAME_MAX];
            for (int i = 0; i < listing->num_files; i++) {
                snprintf(original_name, FILE_NAME_MAX, "%s%s", original_directory, listing->files[i].name);
                snprintf(new_name, FILE_NAME_MAX, "%s%s", new_directory, listing->files[i].name);
                if (overwrite || !dir_get_file_at_location(listing->files[i].name, location)) {
                    platform_file_manager_copy_file(original_name, new_name);
                }
            }
        }
        if (has_subdirectories) {
            listing = dir_find_all_subdirectories(original_directory);
            char original_name[FILE_NAME_MAX];
            char new_name[FILE_NAME_MAX];
            for (int i = 0; i < listing->num_files; i++) {
                snprintf(original_name, FILE_NAME_MAX, "%s%s", original_directory, listing->files[i].name);
                snprintf(new_name, FILE_NAME_MAX, "%s%s", new_directory, listing->files[i].name);
                platform_file_manager_copy_directory(original_name, new_name, overwrite);
            }
        }
    }
}

void platform_user_path_copy_campaigns_and_custom_empires(void)
{
    if (*pref_user_dir()) {
        return;
    }
    const dir_listing *listing = dir_find_files_with_extension(0, "campaign");
    if (listing->num_files > 0) {
        char new_name[FILE_NAME_MAX];
        const char *campaign_directory = platform_file_manager_get_directory_for_location(PATH_LOCATION_CAMPAIGN, 0);
        platform_file_manager_create_directory(campaign_directory, pref_user_dir(), 0);
        for (int i = 0; i < listing->num_files; i++) {
            snprintf(new_name, FILE_NAME_MAX, "%s%s", campaign_directory, listing->files[i].name);
            if (!dir_get_file_at_location(listing->files[i].name, PATH_LOCATION_CAMPAIGN)) {
                platform_file_manager_copy_file(listing->files[i].name, new_name);
            }
        }
    }
    platform_file_manager_copy_directory("custom_empires",
        platform_file_manager_get_directory_for_location(PATH_LOCATION_EDITOR_CUSTOM_EMPIRES, 0), 0);
}

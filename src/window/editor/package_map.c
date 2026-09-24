#include "package_map.h"

#include "core/array.h"
#include "core/file.h"
#include "core/image.h"
#include "core/image_group.h"
#include "core/image_group_editor.h"
#include "core/zlib_helper.h"
#include "editor/editor.h"
#include "empire/empire.h"
#include "game/campaign.h"
#include "graphics/font.h"
#include "graphics/graphics.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "scenario/custom_messages.h"
#include "translation/translation.h"

#define WINDOW_WIDTH 30
#define WINDOW_HEIGHT 20

static struct {
    int exporting;
    char (*files)[FILE_NAME_MAX]; // A list of files of length FILE_NAME_MAX
    int file_count;
    int capacity;
    long long zip_size;
    const char *scenario_file;
    int file_idx;
} data;

const char *image_paths[] = {
    CAMPAIGNS_DIRECTORY "/image",
    "image",
    0
};

const char *video_paths[] = {
    CAMPAIGNS_DIRECTORY "/video",
    "video",
    0
};

const char *audio_paths[] = {
    CAMPAIGNS_DIRECTORY "/audio",
    "audio",
    0
};

static void add_file(const char *name)
{
    if (!name || !*name) {
        return;
    }
    if (data.file_count == data.capacity) {
        size_t new_cap = data.capacity ? data.capacity * 2 : 8;
        char (*tmp)[FILE_NAME_MAX] = realloc(data.files, new_cap * sizeof *tmp);
        if (!tmp) {
            return;
        }
        data.files = tmp;
        data.capacity = new_cap;
    }
    snprintf(data.files[data.file_count], FILE_NAME_MAX, "%s", name);  // safe, always terminated
    data.file_count++;
}

static const char *find_asset(const char *path, const char **paths)
{
    if (!path || !*path) {
        return 0;
    }
    for (int i = 0; paths[i]; i++) {
        char full_path[FILE_NAME_MAX];
        const char *found_path = 0;
        snprintf(full_path, FILE_NAME_MAX, "%s/%s", paths[i], path);
        if (game_campaign_has_file(full_path)) {
            found_path = full_path; // first look in campaigns directory
        } else {
            char scenario_dir_path[FILE_NAME_MAX];
            snprintf(scenario_dir_path, FILE_NAME_MAX, "%s/%s", dir_get_scenario_dir(), full_path);
            if (!(found_path = dir_get_file(scenario_dir_path, 0))) { // then in the scenarios own asset directories
                if (!(found_path = dir_get_file_at_location(full_path, PATH_LOCATION_EDITOR_CONTENT))) { // then in editor/content
                    found_path = dir_get_file_at_location(full_path, PATH_LOCATION_COMMUNITY); // at last in community
                }
            }
        }
        if (found_path) {
            return found_path;
        }
    }
    return 0;
}

static void find_files(void)
{
    // Add the empire background image if existant
    if (empire_get_image_id() != image_group(editor_is_active() ? GROUP_EDITOR_EMPIRE_MAP : GROUP_EMPIRE_MAP)) {
        add_file(find_asset(empire_get_image_path(), image_paths));
    }

    // Add all assets used in custom messages
    for (int i = 1; i < custom_messages_count(); i++) {
        custom_message_t *message = custom_messages_get(i);

        // linked media
        // storing all files (needed to not crash since allocated)
        char video_name[FILE_NAME_MAX];
        char audio_name[FILE_NAME_MAX];
        char speech_name[FILE_NAME_MAX];
        char music_name[FILE_NAME_MAX];
        char image_name[FILE_NAME_MAX];
        snprintf(video_name, FILE_NAME_MAX, "%s", (const char *)custom_messages_get_video(message));
        snprintf(audio_name, FILE_NAME_MAX, "%s", custom_messages_get_audio(message));
        snprintf(speech_name, FILE_NAME_MAX, "%s", custom_messages_get_speech(message));
        snprintf(music_name, FILE_NAME_MAX, "%s", custom_messages_get_background_music(message));
        snprintf(image_name, FILE_NAME_MAX, "%s", (const char *)custom_messages_get_background_image(message));
        // removing all paths and finding the files again ensures no vanilla files are copied
        add_file(find_asset(file_remove_path(video_name), video_paths));
        add_file(find_asset(file_remove_path(audio_name), audio_paths));
        add_file(find_asset(file_remove_path(speech_name), audio_paths));
        add_file(find_asset(file_remove_path(music_name), audio_paths));
        add_file(find_asset(file_remove_path(image_name), image_paths));

        // text images
        if (!message->display_text) {
            continue;
        }
        const uint8_t *text = message->display_text->text;
        while (*text) {
            if (*text++ == '@' && *text++ == 'G' && *text++ == '[') {
                const char *begin = (const char *) text;
                const char *end = strchr(begin, ']');
                if (!end) {
                    break;
                }
                size_t length = end - begin;
                text += length + 1;
                char *location = malloc((length + 1) * sizeof(char));
                if (location) {
                    snprintf(location, length + 1, "%s", begin);
                    char *divider = strchr(location, ':');
                    if (!divider) {
                        // this means we have the form @G[filename.png]
                        add_file(find_asset(location, image_paths));
                    }
                    free(location);
                }
            }
        }
    }
}

static const char *find_scenario_file(void)
{
    const char *filename;
    const char *foldername = dir_get_scenario_dir();
    filename = dir_get_first_file_with_extension(foldername, "map");
    if (!filename || !*filename) {
        filename = dir_get_first_file_with_extension(foldername, "mapx");
    }
    if (!filename || !*filename) {
        filename = dir_get_first_file_with_extension(foldername, "sav");
    }
    if (!filename || !*filename) {
        filename = dir_get_first_file_with_extension(foldername, "svx");
    }
    return filename;
}

static void export_stop(void)
{
    free(data.files);
    data.files = NULL;
    data.file_count = 0;
    data.capacity = 0;
    data.exporting = 0;
    data.zip_size = 0;
    data.file_idx = 0;
}

static void export_start(void)
{
    data.exporting = 1;
    data.file_idx = 0;
    char zip_path[FILE_NAME_MAX];
    snprintf(zip_path, FILE_NAME_MAX, "%s%s", dir_get_scenario_dir(), ".zip");
    if (!zip_package_map_start(zip_path)) {
        export_stop();
        window_go_back();
    }
    zip_package_map_add_file(data.scenario_file, MZ_DEFAULT_LEVEL);
}

static void init(void)
{
    find_files();
    data.scenario_file = find_scenario_file();
    data.zip_size = estimate_zip_size(data.files, data.file_count, data.scenario_file);
}

static void draw_foreground(void)
{
    graphics_in_dialog_with_size(WINDOW_WIDTH * BLOCK_SIZE, WINDOW_HEIGHT * BLOCK_SIZE);

    outer_panel_draw(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_PACKAGE_MAP, 0, 24, WINDOW_WIDTH * BLOCK_SIZE, FONT_LARGE_BLACK);
    if (data.exporting) {
        zip_package_map_add_file(data.files[data.file_idx], MZ_DEFAULT_LEVEL);
        data.file_idx++;
    } else {
        int height = lang_text_draw_multiline(CUSTOM_TRANSLATION, TR_EDITOR_PACKAGE_MAP_INFO,
            24, 64, WINDOW_WIDTH * BLOCK_SIZE - 48, FONT_NORMAL_BLACK);
        uint8_t size_message[128];
        float magnitude = 1.0;
        char extension[3] = "B";


        if (data.zip_size > 1073741823) {
            magnitude = 1073741824.0;
            snprintf(extension, 3, "GB");
        } else if (data.zip_size > 1048575) {
            magnitude = 1048576.0;
            snprintf(extension, 3, "MB");
        } else if (data.zip_size > 1023) {
            magnitude = 1024.0;
            snprintf(extension, 3, "kB");
        }
        snprintf((char *)size_message, 128, "%s %.2f%s.", translation_for(TR_EDITOR_PACKAGE_MAP_SIZE), data.zip_size / magnitude, extension);
        text_draw(size_message, 24, 64 + height, FONT_NORMAL_BLACK, COLOR_MASK_NONE);
    }

    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);

    if (input_go_back_requested(m, h) && !data.exporting) {
        export_stop();
        window_go_back();
    }
}


void window_map_editor_package_map_show(void)
{
    init();
    window_type window = {
        WINDOW_EDITOR_PACKAGE_MAP,
        window_draw_underlying_window,
        draw_foreground,
        handle_input
    };
    window_show(&window);
}

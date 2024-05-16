#include "platform/prefs.h"

#include "core/log.h"
#include "core/file.h"
#include "platform/platform.h"

#include "SDL.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static char stored_user_dir[FILE_NAME_MAX];
static char stored_data_dir[FILE_NAME_MAX];

static FILE *open_pref_file(const char *filename, const char *mode)
{
    char *pref_dir = platform_get_pref_path();
    if (!pref_dir) {
        return NULL;
    }
    log_info("Location:", pref_dir, 0);
    size_t file_len = strlen(filename) + strlen(pref_dir) + 1;
    char *pref_file = malloc(file_len * sizeof(char));
    if (!pref_file) {
        SDL_free(pref_dir);
        return NULL;
    }
    snprintf(pref_file, file_len, "%s%s", pref_dir, filename);
    SDL_free(pref_dir);

    FILE *fp = fopen(pref_file, mode);
    free(pref_file);
    return fp;
}

const char *pref_data_dir(void)
{
    if (*stored_data_dir) {
        return stored_data_dir;
    }
    char data_dir[FILE_NAME_MAX] = { 0 };
    FILE *fp = open_pref_file("data_dir.txt", "r");
    if (fp) {
        size_t length = fread(data_dir, 1, FILE_NAME_MAX - 1, fp);
        fclose(fp);
        if (length > 0) {
            data_dir[length] = 0;
        }
    }
    snprintf(stored_data_dir, FILE_NAME_MAX, "%s", data_dir);
    return stored_data_dir;
}

void pref_save_data_dir(const char *data_dir)
{
    snprintf(stored_data_dir, FILE_NAME_MAX, "%s", data_dir);
    FILE *fp = open_pref_file("data_dir.txt", "w");
    if (fp) {
        fwrite(data_dir, 1, strlen(data_dir), fp);
        fclose(fp);
    }
}

const char *pref_user_dir(void)
{
    if (*stored_user_dir) {
        return stored_user_dir;
    }
    char user_dir[FILE_NAME_MAX] = { 0 };
    FILE *fp = open_pref_file("user_dir.txt", "r");
    if (fp) {
        size_t length = fread(user_dir, 1, FILE_NAME_MAX - 1, fp);
        fclose(fp);
        if (length > 0) {
            user_dir[length] = 0;
        }
    }
    snprintf(stored_user_dir, FILE_NAME_MAX, "%s", user_dir);
    return stored_user_dir;
}

void pref_save_user_dir(const char *user_dir)
{
    snprintf(stored_user_dir, FILE_NAME_MAX, "%s", user_dir);
    FILE *fp = open_pref_file("user_dir.txt", "w");
    if (fp) {
        fwrite(user_dir, 1, strlen(user_dir), fp);
        fclose(fp);
    }
}

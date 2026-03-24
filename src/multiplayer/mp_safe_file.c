#include "mp_safe_file.h"

#ifdef ENABLE_MULTIPLAYER

#include "mp_debug_log.h"
#include "core/dir.h"
#include "core/log.h"
#include "platform/file_manager.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>     /* _fileno, _commit */
#else
#include <unistd.h> /* fileno, fsync */
#endif

#define TMP_SUFFIX ".tmp"
#define MAX_PATH_LEN 512
#define AUTOSAVE_FILENAME_FMT "mp_autosave_%02d.sav"

static char path_buffer[MAX_PATH_LEN];
static char tmp_path_buffer[MAX_PATH_LEN];

static int write_and_validate(const char *path, const uint8_t *data, uint32_t size)
{
    FILE *fp = platform_file_manager_open_file(path, "wb");
    if (!fp) {
        log_error("mp_safe_file: cannot open for write", path, 0);
        return 0;
    }

    size_t written = fwrite(data, 1, size, fp);
    if (written != size) {
        log_error("mp_safe_file: fwrite incomplete", 0, (int)size);
        fclose(fp);
        platform_file_manager_remove_file(path);
        return 0;
    }

    /* Flush user-space buffers */
    if (fflush(fp) != 0) {
        log_error("mp_safe_file: fflush failed", path, 0);
        fclose(fp);
        platform_file_manager_remove_file(path);
        return 0;
    }

#ifdef _WIN32
    /* On Windows, use _commit via fileno for fsync equivalent */
    {
        int fd = _fileno(fp);
        if (fd >= 0) {
            _commit(fd);
        }
    }
#else
    /* On POSIX, use fsync */
    {
        int fd = fileno(fp);
        if (fd >= 0) {
            fsync(fd);
        }
    }
#endif

    fclose(fp);

    /* Verify by reopening and checking file size */
    fp = platform_file_manager_open_file(path, "rb");
    if (!fp) {
        log_error("mp_safe_file: cannot reopen for verify", path, 0);
        return 0;
    }
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fclose(fp);

    if (file_size != (long)size) {
        log_error("mp_safe_file: size mismatch after write", 0, (int)size);
        platform_file_manager_remove_file(path);
        return 0;
    }

    return 1;
}

int mp_safe_file_write(const char *target_path, const char *backup_path,
                       const uint8_t *data, uint32_t size)
{
    if (!target_path || !data || size == 0) {
        return 0;
    }

    /* 1. Build tmp path */
    snprintf(tmp_path_buffer, sizeof(tmp_path_buffer), "%s%s", target_path, TMP_SUFFIX);

    /* 2. Write to temporary file */
    if (!write_and_validate(tmp_path_buffer, data, size)) {
        MP_LOG_ERROR("SAFE_FILE", "Failed to write tmp file: %s", tmp_path_buffer);
        return 0;
    }

    /* 3. Move existing target to backup (if backup_path provided and target exists) */
    if (backup_path) {
        FILE *existing = platform_file_manager_open_file(target_path, "rb");
        if (existing) {
            fclose(existing);
            /* Remove old backup if it exists */
            platform_file_manager_remove_file(backup_path);
            /* Rename current to backup */
            if (rename(target_path, backup_path) != 0) {
                MP_LOG_WARN("SAFE_FILE", "Could not move %s to %s (non-fatal)",
                            target_path, backup_path);
                /* Non-fatal: we still have the .tmp with good data */
            }
        }
    }

    /* 4. Atomic rename: tmp -> target */
    /* Remove target first (Windows rename fails if target exists) */
    platform_file_manager_remove_file(target_path);
    if (rename(tmp_path_buffer, target_path) != 0) {
        MP_LOG_ERROR("SAFE_FILE", "Failed atomic rename %s -> %s", tmp_path_buffer, target_path);
        /* The .tmp file still exists with valid data; caller can retry */
        return 0;
    }

    MP_LOG_INFO("SAFE_FILE", "Atomic write successful: %s (%u bytes)", target_path, size);
    return 1;
}

int mp_safe_file_read(const char *path, uint8_t *buffer,
                      uint32_t max_size, uint32_t *out_size)
{
    if (!path || !buffer || !out_size) {
        return 0;
    }

    *out_size = 0;

    FILE *fp = platform_file_manager_open_file(path, "rb");
    if (!fp) {
        return 0;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size <= 0 || (uint32_t)file_size > max_size) {
        log_error("mp_safe_file: file too large or empty", 0, (int)file_size);
        fclose(fp);
        return 0;
    }

    size_t read = fread(buffer, 1, (size_t)file_size, fp);
    fclose(fp);

    if (read != (size_t)file_size) {
        log_error("mp_safe_file: incomplete read", 0, (int)file_size);
        return 0;
    }

    *out_size = (uint32_t)file_size;
    return 1;
}

int mp_safe_file_write_autosave(const char *base_dir, int slot,
                                const uint8_t *data, uint32_t size)
{
    char filename[64];
    snprintf(filename, sizeof(filename), AUTOSAVE_FILENAME_FMT, slot);

    const char *target = mp_safe_file_get_save_path(filename);
    if (!target) {
        return 0;
    }

    /* Autosaves don't need a .prev backup — each slot acts as its own backup */
    return mp_safe_file_write(target, NULL, data, size);
}

const char *mp_safe_file_get_save_path(const char *filename)
{
    const char *dir = dir_append_location(filename, PATH_LOCATION_SAVEGAME);
    if (!dir) {
        return NULL;
    }
    strncpy(path_buffer, dir, MAX_PATH_LEN - 1);
    path_buffer[MAX_PATH_LEN - 1] = '\0';
    return path_buffer;
}

#endif /* ENABLE_MULTIPLAYER */

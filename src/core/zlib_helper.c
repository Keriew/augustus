#include "zlib_helper.h"

#include "core/file.h"
#include "core/log.h"
#include "miniz/miniz.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define DIR_VIDEO "video/"
#define DIR_IMAGE "image/"
#define DIR_AUDIO "audio/"

static const char *VIDEO_EXT[] = { "mp4", "mkv", "avi", "mov", "wmv", "flv",
                                   "webm", "m4v", "mpg", "mpeg", "smk", NULL };
static const char *IMAGE_EXT[] = { "jpg", "jpeg", "png", "gif", "bmp", "webp",
                                   "tif", "tiff", "svg", NULL };
static const char *AUDIO_EXT[] = { "mp3", "wav", "flac", "aac", "ogg", "m4a",
                                   "wma", "opus", NULL };


int zlib_helper_decompress(void *input_buffer, const int input_length, void *output_buffer, const int output_buffer_length, int *output_length)
{
    z_stream strm;

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    if (inflateInit(&strm) != Z_OK) {
        return 0;
    }

    strm.avail_in = input_length;
    strm.next_in = input_buffer;
    strm.avail_out = output_buffer_length;
    strm.next_out = output_buffer;
    int result = inflate(&strm, Z_NO_FLUSH);
    inflateEnd(&strm);
    if (result != Z_STREAM_END || strm.avail_out != 0) {
        return 0;
    }
    *output_length = output_buffer_length - strm.avail_out;
    return 1;
}

int zlib_helper_compress(void *input_buffer, const int input_length, void *output_buffer, const int output_buffer_length, int *output_length)
{
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    if (deflateInit(&strm, Z_BEST_SPEED) != Z_OK) {
        return 0;
    }

    strm.avail_in = input_length;
    strm.next_in = input_buffer;
    strm.avail_out = output_buffer_length;
    strm.next_out = output_buffer;
    int result = deflate(&strm, Z_FINISH);
    deflateEnd(&strm);
    if (result != Z_STREAM_END || strm.avail_in != 0) {
        return 0;
    }

    *output_length = output_buffer_length - strm.avail_out;
    return 1;
}

static const char *path_basename(const char *path)
{
    const char *s1 = strrchr(path, '/');
    const char *s2 = strrchr(path, '\\');
    const char *last = (s1 > s2) ? s1 : s2;
    return last ? last + 1 : path;
}

static int has_extension(const char *path, const char **list)
{
    for (int i = 0; list[i]; i++)
        if (file_has_extension(path, list[i])) {
            return 1;
        }
    return 0;
}

/* Returns the folder prefix for a file, or NULL if it's not a known media type */
static const char *folder_for(const char *path)
{
    if (has_extension(path, VIDEO_EXT)) return DIR_VIDEO;
    if (has_extension(path, IMAGE_EXT)) return DIR_IMAGE;
    if (has_extension(path, AUDIO_EXT)) return DIR_AUDIO;
    return NULL;
}

static int add_entry(mz_zip_archive *zip, const char *folder,
                     const char *src_path, mz_uint level)
{
    char name[FILE_NAME_MAX];
    int n = snprintf(name, sizeof(name), "%s%s", folder, path_basename(src_path));
    if (n < 0 || n >= sizeof(name)) {
        log_error("Archive name too long for", src_path, 0);
        return 0;
    }
    if (!mz_zip_writer_add_file(zip, name, src_path, NULL, 0, level, 0)) {
        log_error("Failed to add", src_path, 0);
        log_error("Reason:", mz_zip_get_error_string(mz_zip_get_last_error(zip)), 0);
        return 0;
    }
    return 1;
}

int zip_package_map(const char *zip_path, char (*files)[FILE_NAME_MAX], int count,
                    const char *map_file, mz_uint level)
{
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    if (!mz_zip_writer_init_file(&zip, zip_path, 0)) {
        log_error("Failed to create", zip_path, 0);
        log_error("Reason:", mz_zip_get_error_string(mz_zip_get_last_error(&zip)), 0);
        return 0;
    }

    for (int i = 0; i < count; i++) {
        const char *folder = folder_for(files[i]);
        if (!folder) {
            log_error("Unrecognized file type: Skipping", files[i], 0);
            continue;
        }
        add_entry(&zip, folder, files[i], level);
    }

    if (map_file && !add_entry(&zip, "", map_file, level))
        goto fail;

    if (!mz_zip_writer_finalize_archive(&zip)) {
        log_error("Failed to finalize archive:",
                mz_zip_get_error_string(mz_zip_get_last_error(&zip)), 0);
        goto fail;
    }

    mz_zip_writer_end(&zip);
    return 1;

fail: // I know gotos aren't very clean but it was the easiest here
    mz_zip_writer_end(&zip);
    remove(zip_path);
    return 0;
}

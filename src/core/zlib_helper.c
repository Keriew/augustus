#include "zlib_helper.h"

#include "core/file.h"
#include "core/io.h"
#include "core/log.h"
#include "miniz/miniz.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define DIR_VIDEO "video/"
#define DIR_IMAGE "image/"
#define DIR_AUDIO "audio/"
#define DIR_ROOT ""

// Fixed overhead miniz/zip format adds per entry, independent of data size
#define ZIP_LOCAL_HEADER_SIZE   30   // local file header, excludes name/extra
#define ZIP_CENTRAL_HEADER_SIZE 46   // central directory record, excludes name/extra
#define ZIP_END_RECORD_SIZE     22   // end of central directory record

static const char *VIDEO_EXT[] = { "webm", "mpg", "mpeg", "smk", NULL };
static const char *IMAGE_EXT[] = { "png", "bmp", NULL };
static const char *AUDIO_EXT[] = { "mp3", "wav", NULL };
static const char *MAP_EXT[] = { "map", "mapx", "sav", "svx", NULL };

static const char *incompressible[] = {"webm", "mpg", "mpeg", "smk", "png", "mp3", NULL};
static const char *compressible[] = {"wav","bmp", "map", "mapx", NULL};

static struct {
    mz_zip_archive map_zip;
    const char *zip_path;
} data;

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
    if (has_extension(path, MAP_EXT)) return DIR_ROOT;
    if (has_extension(path, VIDEO_EXT)) return DIR_VIDEO;
    if (has_extension(path, IMAGE_EXT)) return DIR_IMAGE;
    if (has_extension(path, AUDIO_EXT)) return DIR_AUDIO;
    return NULL;
}

static int add_entry(mz_zip_archive *zip, const char *folder,
                     const char *src_path, mz_uint level)
{
    char name[FILE_NAME_MAX];
    int n = snprintf(name, sizeof(name), "%s%s", folder, file_remove_path(src_path));
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

static void packaging_fail(void)
{
    mz_zip_writer_end(&data.map_zip);
    remove(data.zip_path);
}

int zip_package_map_start(const char *zip_path)
{
    data.zip_path = zip_path; // this is okay since it's just about having access to the path not to modify it or anything
    mz_zip_archive *zip = &data.map_zip;
    memset(zip, 0, sizeof(*zip));

    if (!mz_zip_writer_init_file(zip, zip_path, 0)) {
        log_error("Failed to create", zip_path, 0);
        log_error("Reason:", mz_zip_get_error_string(mz_zip_get_last_error(zip)), 0);
        return 0;
    }

    return 1;
}

void zip_package_map_add_file(const char *file, mz_uint level)
{
    const char *folder = folder_for(file);
    if (!folder) {
        log_error("Unrecognized file type: Skipping", file, 0);
        return;
    }
    add_entry(&data.map_zip, folder, file, level);
}

int zip_package_map_finalize(void)
{
    if (!mz_zip_writer_finalize_archive(&data.map_zip)) {
        log_error("Failed to finalize archive:", mz_zip_get_error_string(mz_zip_get_last_error(&data.map_zip)), 0);
        packaging_fail();
        return 0;
    }

    mz_zip_writer_end(&data.map_zip);
    return 1;
}

/**
 * Rough compression ratio for a given file, as (output_size / input_size).
 * Already-compressed formats (video/image/audio/archives) barely shrink,
 * so they get a ratio near 1.0. Text-like / uncompressed formats compress
 * much better with deflate, so they get a lower ratio.
 * These are ballpark figures, not guarantees, actual results vary with content.
 */
static double estimated_ratio(const char *path)
{
    float ratio = 0.7;
    if (has_extension(path, incompressible)) {
        ratio =  1.0;
    } else if (file_has_extension(path, "wav")) {
        ratio = 0.9; // while some wavs can be greatly reduced in size (e.g. ones with a lot of empty space) usually reduction is small
    } else if (has_extension(path, compressible)) {
        ratio = 0.4;
    }

    return ratio;
}

long long estimate_zip_size(char (*files)[FILE_NAME_MAX], size_t count, const char *extra_file)
{
    long long total = ZIP_END_RECORD_SIZE;

    for (size_t i = 0; i <= count; i++) {
        if (i == count && !extra_file) {
            break; // Ensure extra file can be omitted
        }
        const char *file = i == count ? extra_file : files[i];

        const char *folder = folder_for(file);
        const char *base = file_remove_path(file);
        size_t name_length = (folder ? strlen(folder) : 0) + strlen(base);

        double ratio = estimated_ratio(file);
        long long compressed = (long long)((double)io_get_file_size(file, 0) * ratio);

        total += ZIP_LOCAL_HEADER_SIZE + name_length + compressed;
        total += ZIP_CENTRAL_HEADER_SIZE + name_length;
    }

    return total;
}

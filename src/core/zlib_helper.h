#ifndef CORE_ZLIB_HELPER_H
#define CORE_ZLIB_HELPER_H

#include "core/file.h"

#include "miniz/miniz.h"

int zlib_helper_decompress(void *input_buffer, const int input_length, void *output_buffer, const int output_buffer_length, int *output_length);

int zlib_helper_compress(void *input_buffer, const int input_length, void *output_buffer, const int output_buffer_length, int *output_length);

/**
 * Creates zip_path containing:
 *   video/   all files from `files` with a video extension
 *   image/   all files from `files` with an image extension
 *   audio/   all files from `files` with an audio extension
 *   savegame or map file stored at the archive root
 *
 * @param zip_path The path to the zip which gets create
 * @param files The file list checked to package only necessary assets
 * @param count The length of the file list
 * @param map_file The main map or savegame file
 * @param level The compression level (just left at default for now but a setting may follow)
 * @return 1 on success, 0 on failure (no zip is left behind on failure).
 */
int zip_package_map(const char *zip_path, char (*files)[FILE_NAME_MAX], int count,
                    const char *map_file, mz_uint level);

/**
 * Estimates the final zip size in bytes for a list of files plus one
 * optional extra file. archive_name_extra_len bytes should be the length
 * of the folder prefix you'll add per file (e.g. strlen("video/")), passed
 * per category via the folder_for()-style logic if you want it exact;
 * here we just take the plain file list and reuse folder_for() from the
 * previous answer to size the name correctly.
 */
long long estimate_zip_size(char (*files)[FILE_NAME_MAX], size_t count, const char *extra_file);

#endif // CORE_ZLIB_HELPER_H

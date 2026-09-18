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
 * @param zip_path The path to the dir the zip gets created in
 * @param files The file list checked to package only necessary assets
 * @param count The length of the file list
 * @param map_file The main map or savegame file
 * @param level The compression level (just left at default for now but a setting may follow)
 * @return 1 on success, 0 on failure (no zip is left behind on failure).
 */
int zip_package_map(const char *zip_path, char (*files)[FILE_NAME_MAX], int count,
                    const char *map_file, mz_uint level);

#endif // CORE_ZLIB_HELPER_H

#ifndef CORE_ZLIB_HELPER_H
#define CORE_ZLIB_HELPER_H

#include "core/file.h"

#include "miniz/miniz.h"

int zlib_helper_decompress(void *input_buffer, const int input_length, void *output_buffer, const int output_buffer_length, int *output_length);

int zlib_helper_compress(void *input_buffer, const int input_length, void *output_buffer, const int output_buffer_length, int *output_length);

/**
 * Functions to create zip_path containing:
 *   video/   all files from `files` with a video extension
 *   image/   all files from `files` with an image extension
 *   audio/   all files from `files` with an audio extension
 *   savegame or map file stored at the archive root
 **
 * zip_package_map_start
 * @param zip_path The path to the zip which gets create
 * @return 1 on success, 0 on failure (no zip is left behind on failure).
 **
 * zip_package_map_add_file
 * @param file The file which gets added to the zip
 * @param level The compression level (just left at default for now but a setting may follow)
 **
 * zip_package_map_finalize
 * @return 1 on success, 0 on failure (no zip is left behind on failure).
 */
int zip_package_map_start(const char *zip_path);
void zip_package_map_add_file(const char *file, mz_uint level);
int zip_package_map_finalize(void);

/**
 * Estimates the final zip size in bytes for a list of files plus one
 * optional extra file.
 * @param files The file list to check it's compressed size
 * @param count The length of the file list
 * @param extra_file The path to the extra file
 */
long long estimate_zip_size(char (*files)[FILE_NAME_MAX], size_t count, const char *extra_file);

#endif // CORE_ZLIB_HELPER_H

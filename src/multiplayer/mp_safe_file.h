#ifndef MULTIPLAYER_MP_SAFE_FILE_H
#define MULTIPLAYER_MP_SAFE_FILE_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>

/**
 * Atomic file write utility for multiplayer saves.
 *
 * Guarantees that the target file is never left in a partially-written state:
 *   1. Write to a temporary file (.tmp suffix)
 *   2. Flush and sync to disk
 *   3. Validate written size matches expected
 *   4. Rename previous file to backup (.prev suffix) if it exists
 *   5. Atomic rename of .tmp to final target
 *
 * On crash/power loss, the previous valid file remains intact.
 */

/**
 * Write data atomically to the target path.
 * Creates a .tmp file, writes, validates, then renames.
 * If a previous file exists at target_path, it is moved to backup_path
 * (if backup_path is non-NULL).
 *
 * @param target_path   Final destination file path
 * @param backup_path   Path to move the old file to (NULL to skip backup)
 * @param data          Data to write
 * @param size          Size of data in bytes
 * @return 1 on success, 0 on failure
 */
int mp_safe_file_write(const char *target_path, const char *backup_path,
                       const uint8_t *data, uint32_t size);

/**
 * Read a file into a caller-provided buffer.
 * Validates that the file size does not exceed max_size.
 *
 * @param path      File path to read
 * @param buffer    Output buffer
 * @param max_size  Maximum allowed file size
 * @param out_size  Actual bytes read (output)
 * @return 1 on success, 0 on failure
 */
int mp_safe_file_read(const char *path, uint8_t *buffer,
                      uint32_t max_size, uint32_t *out_size);

/**
 * Rotate autosave files.
 * Writes data to autosave_XX.sav using rotating slot index.
 *
 * @param base_dir       Directory for autosave files
 * @param slot           Slot index (0..max_slots-1), caller manages rotation
 * @param data           Save data
 * @param size           Size of data
 * @return 1 on success, 0 on failure
 */
int mp_safe_file_write_autosave(const char *base_dir, int slot,
                                const uint8_t *data, uint32_t size);

/**
 * Build the full path for a multiplayer save file.
 * Uses the savegame directory location.
 *
 * @param filename  Base filename (e.g., "mp_session_current.sav")
 * @return Static buffer containing the full path (valid until next call)
 */
const char *mp_safe_file_get_save_path(const char *filename);

#endif /* ENABLE_MULTIPLAYER */

#endif /* MULTIPLAYER_MP_SAFE_FILE_H */

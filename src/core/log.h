#ifndef CORE_DEBUG_H
#define CORE_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * Logging
 */

/**
 * Severity definitions used across the runtime:
 * Warning = probably unintended, probably safe.
 * Error = definitely unintended, survivable, and does not damage game state, save data, or campaign data.
 * Fatal error = unrecoverable or likely to cause data loss, and the process must stop.
 */

/**
 * Logs an info message.
 * @param msg Message
 * @param param_str Extra info (string)
 * @param param_int Extra info (integer)
 */
void log_info(const char *msg, const char *param_str, int param_int);
void log_set_debug_enabled(int enabled);
int log_is_debug_enabled(void);

/**
 * Logs a warning message.
 * @param msg Message
 * @param param_str Extra info (string)
 * @param param_int Extra info (integer)
 */
void log_warning(const char *msg, const char *param_str, int param_int);

/**
 * Logs an error message.
 * @param msg Message
 * @param param_str Extra info (string)
 * @param param_int Extra info (integer)
 */
void log_error(const char *msg, const char *param_str, int param_int);

/**
 * Logs the repeated messages, saying how many times it was repeated
 */
void log_repeated_messages(void);

#ifdef __cplusplus
}
#endif

#endif // CORE_DEBUG_H

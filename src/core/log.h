#ifndef CORE_DEBUG_H
#define CORE_DEBUG_H

/**
 * @file
 * Logging
 */

/**
 * Logs an info message
 * @param msg Message
 * @param param_str Extra info (string)
 * @param param_int Extra info (integer)
 */
void log_info(const char *msg, const char *param_str, int param_int);

/**
 * Logs an error message
 * @param msg Message
 * @param param_str Extra info (string)
 * @param param_int Extra info (integer)
 */
void log_error(const char *msg, const char *param_str, int param_int);

const char *log_history_get_line(unsigned int line);
void log_history_clear(void);
unsigned int log_history_capacity(void);

#endif // CORE_DEBUG_H

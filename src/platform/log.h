#ifndef PLATFORM_LOG_H
#define PLATFORM_LOG_H

int platform_log_is_ready(void);
void platform_log_setup(void);
void platform_log_teardown(void);
void platform_log_message(const char *message, int is_error);
void platform_log_set_output_function(void (*callback)(const char *message, int is_error));

#endif /* PLATFORM_LOG_H */

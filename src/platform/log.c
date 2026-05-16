#include "log.h"

#include "core/file.h"
#include "core/log.h"
#include "platform/file_manager.h"
#include "platform/platform.h"

#include <stdio.h>
#include <string.h>

#ifdef _MSC_VER
#include <windows.h>
#endif

#define LOG_MESSAGES_STORE_STEP 10
#define LOG_TEXT_SIZE 300
#define MAX_PREVIOUS_MESSAGES 5

static struct {
    FILE *file;
    char (*messages)[LOG_TEXT_SIZE];
    unsigned int total;
    unsigned int size;
    int retrying;
    struct {
        char message[LOG_TEXT_SIZE];
        unsigned int count;
    } previous[MAX_PREVIOUS_MESSAGES];
} data;

static void write_to_output(FILE *output, const char *message)
{
    fwrite(message, sizeof(char), strlen(message), output);
    fflush(output);
}

static void write_to_buffer(const char *message)
{
    if (data.total == data.size) {
        char (*new_messages)[LOG_TEXT_SIZE] = realloc(data.messages,
            sizeof(char) * LOG_TEXT_SIZE * (data.size + LOG_MESSAGES_STORE_STEP));
        if (!new_messages) {
            return;
        }
        data.messages = new_messages;
        data.size += LOG_MESSAGES_STORE_STEP;
    }
    snprintf(data.messages[data.total], LOG_TEXT_SIZE, "%s", message);
    data.total++;
}

static void write_log(const char *message)
{
    if (data.file) {
        write_to_output(data.file, message);
    } else if (data.retrying) {
        write_to_buffer(message);
    }
    // On Windows MSVC, we can at least get output to the debug window
#if defined(_MSC_VER) && !defined(NDEBUG)
    OutputDebugStringA(message);
#else
    write_to_output(stdout, message);
#endif
}

static void log_repeated_messages(void)
{
    for (unsigned int i = 0; i < MAX_PREVIOUS_MESSAGES; i++) {
        if (data.previous[i].count > 1) {
            char final_message[LOG_TEXT_SIZE * 2];
            snprintf(final_message, LOG_TEXT_SIZE * 2, "INFO:  Message \"%s\" repeated %u more %s.\n",
                data.previous[i].message, data.previous[i].count - 1, data.previous[i].count == 2 ? "time" : "times");
            write_log(final_message);
        }
        data.previous[i].message[0] = 0;
        data.previous[i].count = 0;
    }
}

static void finish_message(const char *message, int is_error)
{
    int found_empty_slot = 0;

    for (unsigned int i = 0; i < MAX_PREVIOUS_MESSAGES; i++) {
        if (strncmp(message, data.previous[i].message, LOG_TEXT_SIZE) == 0 && data.previous[i].count > 0) {
            data.previous[i].count++;
            return;
        }
        if (data.previous[i].count == 0) {
            snprintf(data.previous[i].message, LOG_TEXT_SIZE, "%s", message);
            data.previous[i].count = 1;
            found_empty_slot = 1;
            break;
        }
    }

    if (!found_empty_slot) {
        log_repeated_messages();
        snprintf(data.previous[0].message, LOG_TEXT_SIZE, "%s", message);
        data.previous[0].count = 1;
    }

    char log_text[LOG_TEXT_SIZE] = { 0 };
    snprintf(log_text, LOG_TEXT_SIZE, "%s %s\n", is_error ? "ERROR: " : "INFO: ", message);

    write_log(log_text);
}

static void write_messages_in_buffer(void)
{
    if (data.file) {
        for (unsigned int i = 0; i < data.total; i++) {
            write_to_output(data.file, data.messages[i]);
        }
    }

    data.total = 0;
    free(data.messages);
    data.messages = 0;
    data.size = 0;
}

static void backup_log(const char *filename, const char *filename_old)
{
    // On some platforms (vita, android), not removing the file will not empty it when reopening for writing
    file_remove(filename_old);
    platform_file_manager_copy_file(filename, filename_old);
}

int platform_log_is_ready(void)
{
    return data.file != 0;
}

void platform_log_setup(void)
{
    const char *filename = "augustus-log.txt";
    const char *backup_filename = "augustus-log-backup.txt";
    char log_file[FILE_NAME_MAX];
    char log_file_old[FILE_NAME_MAX];
    const char *pref_dir = platform_get_logging_path();
    snprintf(log_file, FILE_NAME_MAX, "%s%s", pref_dir ? pref_dir : "", filename);
    snprintf(log_file_old, FILE_NAME_MAX, "%s%s", pref_dir ? pref_dir : "", backup_filename);
    backup_log(log_file, log_file_old);

    // On some platforms (vita, android), not removing the file will not empty it when reopening for writing
    file_remove(log_file);
    data.file = file_open(log_file, "wt");
    platform_log_set_output_function(finish_message);

    write_messages_in_buffer();

    if (!data.file && !data.retrying) {
        data.retrying = 1;
    } else {
        data.retrying = 0;
    }
}

void platform_log_teardown(void)
{
    log_repeated_messages();

    if (data.file) {
        file_close(data.file);
    }
}

static const char *build_message(const char *msg, const char *param_str, int param_int)
{
    static char log_buffer[LOG_TEXT_SIZE];

    int index = 0;
    index += snprintf(&log_buffer[index], LOG_TEXT_SIZE - index, "%s", msg);
    if (param_str) {
        index += snprintf(&log_buffer[index], LOG_TEXT_SIZE - index, "  %s", param_str);
    }
    if (param_int) {
        index += snprintf(&log_buffer[index], LOG_TEXT_SIZE - index, "  %d", param_int);
    }

    return log_buffer;
}

void log_info(const char *msg, const char *param_str, int param_int)
{
    platform_log_message(build_message(msg, param_str, param_int), 0);
}

void log_error(const char *msg, const char *param_str, int param_int)
{
    platform_log_message(build_message(msg, param_str, param_int), 1);
}

#include "mp_debug_log.h"

#ifdef ENABLE_MULTIPLAYER

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/time.h>
#endif

#define MP_LOG_FILE "mp_debug.log"
#define MP_LOG_LINE_MAX 1024

static FILE *log_file;
static mp_log_level min_level = MP_LOG_LEVEL_TRACE;
static int initialized;

static const char *level_names[] = {
    "TRACE", "DEBUG", "INFO ", "WARN ", "ERROR"
};

static void write_timestamp(void)
{
    if (!log_file) {
        return;
    }

#ifdef _WIN32
    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(log_file, "[%02d:%02d:%02d.%03d] ",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *tm_info = localtime(&tv.tv_sec);
    fprintf(log_file, "[%02d:%02d:%02d.%03d] ",
            tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
            (int)(tv.tv_usec / 1000));
#endif
}

void mp_debug_log_init(void)
{
    if (initialized) {
        return;
    }

    log_file = fopen(MP_LOG_FILE, "a");
    if (!log_file) {
        return;
    }

    initialized = 1;

    /* Write session separator */
    fprintf(log_file, "\n");
    fprintf(log_file, "================================================================\n");

#ifdef _WIN32
    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(log_file, "=== Augustus Multiplayer Debug Log - %04d-%02d-%02d %02d:%02d:%02d ===\n",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
#else
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(log_file, "=== Augustus Multiplayer Debug Log - %s ===\n", time_buf);
#endif

    fprintf(log_file, "================================================================\n");
    fflush(log_file);
}

void mp_debug_log_shutdown(void)
{
    if (!initialized) {
        return;
    }

    mp_debug_log("SESSION", "Debug log shutdown");

    if (log_file) {
        fflush(log_file);
        fclose(log_file);
        log_file = NULL;
    }
    initialized = 0;
}

void mp_debug_log_set_level(mp_log_level level)
{
    min_level = level;
}

void mp_debug_log(const char *category, const char *fmt, ...)
{
    if (!initialized || !log_file) {
        return;
    }

    write_timestamp();
    fprintf(log_file, "[INFO ] [%-10s] ", category);

    va_list args;
    va_start(args, fmt);
    vfprintf(log_file, fmt, args);
    va_end(args);

    fprintf(log_file, "\n");
    fflush(log_file);
}

void mp_debug_log_level(mp_log_level level, const char *category, const char *fmt, ...)
{
    if (!initialized || !log_file) {
        return;
    }
    if (level < min_level) {
        return;
    }

    write_timestamp();

    int lvl_idx = (int)level;
    if (lvl_idx < 0) lvl_idx = 0;
    if (lvl_idx > 4) lvl_idx = 4;

    fprintf(log_file, "[%s] [%-10s] ", level_names[lvl_idx], category);

    va_list args;
    va_start(args, fmt);
    vfprintf(log_file, fmt, args);
    va_end(args);

    fprintf(log_file, "\n");
    fflush(log_file);
}

void mp_debug_log_hex(const char *category, const char *label,
                      const uint8_t *data, uint32_t size)
{
    if (!initialized || !log_file) {
        return;
    }
    if (MP_LOG_LEVEL_DEBUG < min_level) {
        return;
    }

    write_timestamp();
    fprintf(log_file, "[DEBUG] [%-10s] %s (%u bytes):", category, label, size);

    uint32_t max_dump = size > 128 ? 128 : size;
    for (uint32_t i = 0; i < max_dump; i++) {
        if (i % 16 == 0) {
            fprintf(log_file, "\n    %04x: ", i);
        }
        fprintf(log_file, "%02x ", data[i]);
    }
    if (size > max_dump) {
        fprintf(log_file, "\n    ... (%u bytes truncated)", size - max_dump);
    }
    fprintf(log_file, "\n");
    fflush(log_file);
}

#endif /* ENABLE_MULTIPLAYER */

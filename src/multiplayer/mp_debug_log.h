#ifndef MULTIPLAYER_DEBUG_LOG_H
#define MULTIPLAYER_DEBUG_LOG_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>

/**
 * File-based multiplayer debug logger.
 *
 * Writes structured logs to mp_debug.log in the working directory.
 * Categories: NET, SESSION, DISCOVERY, HANDSHAKE, LOBBY, GAME, CHAT, SYNC, UI
 *
 * Usage:
 *   mp_debug_log("NET", "TCP send failed: peer=%s bytes=%d", name, size);
 *   mp_debug_log_hex("NET", "Payload", data, len);
 */

typedef enum {
    MP_LOG_LEVEL_TRACE = 0,
    MP_LOG_LEVEL_DEBUG = 1,
    MP_LOG_LEVEL_INFO  = 2,
    MP_LOG_LEVEL_WARN  = 3,
    MP_LOG_LEVEL_ERROR = 4
} mp_log_level;

/* Init/shutdown — call once at session init/shutdown */
void mp_debug_log_init(void);
void mp_debug_log_shutdown(void);

/* Set minimum log level (default: MP_LOG_LEVEL_TRACE during dev) */
void mp_debug_log_set_level(mp_log_level level);

/* Main logging functions */
void mp_debug_log(const char *category, const char *fmt, ...);
void mp_debug_log_level(mp_log_level level, const char *category, const char *fmt, ...);

/* Hex dump for packet debugging */
void mp_debug_log_hex(const char *category, const char *label,
                      const uint8_t *data, uint32_t size);

/* Convenience macros */
#define MP_LOG_TRACE(cat, ...) mp_debug_log_level(MP_LOG_LEVEL_TRACE, cat, __VA_ARGS__)
#define MP_LOG_DEBUG(cat, ...) mp_debug_log_level(MP_LOG_LEVEL_DEBUG, cat, __VA_ARGS__)
#define MP_LOG_INFO(cat, ...)  mp_debug_log_level(MP_LOG_LEVEL_INFO, cat, __VA_ARGS__)
#define MP_LOG_WARN(cat, ...)  mp_debug_log_level(MP_LOG_LEVEL_WARN, cat, __VA_ARGS__)
#define MP_LOG_ERROR(cat, ...) mp_debug_log_level(MP_LOG_LEVEL_ERROR, cat, __VA_ARGS__)

#else

/* No-op when multiplayer disabled */
#define mp_debug_log_init()
#define mp_debug_log_shutdown()
#define mp_debug_log_set_level(l)
#define mp_debug_log(cat, ...)
#define mp_debug_log_level(lvl, cat, ...)
#define mp_debug_log_hex(cat, label, data, size)
#define MP_LOG_TRACE(cat, ...)
#define MP_LOG_DEBUG(cat, ...)
#define MP_LOG_INFO(cat, ...)
#define MP_LOG_WARN(cat, ...)
#define MP_LOG_ERROR(cat, ...)

#endif /* ENABLE_MULTIPLAYER */

#endif /* MULTIPLAYER_DEBUG_LOG_H */

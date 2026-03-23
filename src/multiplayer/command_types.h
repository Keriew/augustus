#ifndef MULTIPLAYER_COMMAND_TYPES_H
#define MULTIPLAYER_COMMAND_TYPES_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>

typedef enum {
    MP_CMD_SET_READY = 1,
    MP_CMD_OPEN_TRADE_ROUTE,
    MP_CMD_CLOSE_TRADE_ROUTE,
    MP_CMD_SET_IMPORT_POLICY,
    MP_CMD_SET_EXPORT_POLICY,
    MP_CMD_SET_ROUTE_LIMIT,
    MP_CMD_REQUEST_PAUSE,
    MP_CMD_REQUEST_RESUME,
    MP_CMD_REQUEST_SPEED,
    MP_CMD_CHAT_MESSAGE,
    MP_CMD_COUNT
} mp_command_type;

typedef enum {
    MP_CMD_STATUS_PENDING = 0,
    MP_CMD_STATUS_ACCEPTED,
    MP_CMD_STATUS_REJECTED,
    MP_CMD_STATUS_APPLIED
} mp_command_status;

typedef enum {
    MP_CMD_REJECT_INVALID = 0,
    MP_CMD_REJECT_NOT_OWNER,
    MP_CMD_REJECT_INSUFFICIENT_FUNDS,
    MP_CMD_REJECT_ROUTE_NOT_FOUND,
    MP_CMD_REJECT_ALREADY_OPEN,
    MP_CMD_REJECT_ALREADY_CLOSED,
    MP_CMD_REJECT_LIMIT_OUT_OF_RANGE,
    MP_CMD_REJECT_GAME_NOT_STARTED
} mp_command_reject_reason;

typedef struct {
    int route_id;
    int city_id;
} mp_cmd_open_trade_route;

typedef struct {
    int route_id;
    int city_id;
} mp_cmd_close_trade_route;

typedef struct {
    int route_id;
    int resource;
    int enabled;  /* 1 = import on, 0 = import off */
} mp_cmd_set_import_policy;

typedef struct {
    int route_id;
    int resource;
    int enabled;  /* 1 = export on, 0 = export off */
} mp_cmd_set_export_policy;

typedef struct {
    int route_id;
    int resource;
    int buying;   /* 1 = buying limit, 0 = selling limit */
    int amount;
} mp_cmd_set_route_limit;

typedef struct {
    uint8_t speed;
} mp_cmd_request_speed;

/* Command envelope used in the command bus */
typedef struct {
    uint32_t sequence_id;
    uint16_t command_type;
    uint8_t player_id;
    uint32_t target_tick;
    mp_command_status status;
    uint8_t reject_reason;

    /* Payload - union of all command data */
    union {
        mp_cmd_open_trade_route open_route;
        mp_cmd_close_trade_route close_route;
        mp_cmd_set_import_policy import_policy;
        mp_cmd_set_export_policy export_policy;
        mp_cmd_set_route_limit route_limit;
        mp_cmd_request_speed speed;
    } data;
} mp_command;

/* Serialization helpers for commands */
void mp_command_serialize(const mp_command *cmd, uint8_t *buffer, uint32_t *size);
void mp_command_deserialize(mp_command *cmd, const uint8_t *buffer, uint32_t size);

const char *mp_command_type_name(mp_command_type type);

#endif /* ENABLE_MULTIPLAYER */

#endif /* MULTIPLAYER_COMMAND_TYPES_H */

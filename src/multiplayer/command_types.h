#ifndef MULTIPLAYER_COMMAND_TYPES_H
#define MULTIPLAYER_COMMAND_TYPES_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>

/**
 * Command types for the host-authoritative multiplayer model.
 *
 * Clients never modify game state directly — they submit commands (intents)
 * to the host through the command bus. The host validates, applies, and
 * broadcasts the result.
 *
 * Route lifecycle:
 *   CREATE  → host validates ownership, costs, caps, creates route_id
 *   DELETE  → host removes route entirely
 *   ENABLE  → reactivate a disabled route
 *   DISABLE → temporarily stop trade on a route
 *   SET_POLICY → change import/export flags per resource
 *   SET_LIMIT  → change buy/sell quantity limits per resource
 */

typedef enum {
    MP_CMD_NONE = 0,

    /* Session control */
    MP_CMD_SET_READY,
    MP_CMD_REQUEST_PAUSE,
    MP_CMD_REQUEST_RESUME,
    MP_CMD_REQUEST_SPEED,

    /* Route lifecycle (Phase 3) */
    MP_CMD_CREATE_TRADE_ROUTE,    /* Client: "I want a route from my city to city X" */
    MP_CMD_DELETE_TRADE_ROUTE,    /* Client: "Remove this route entirely" */
    MP_CMD_ENABLE_TRADE_ROUTE,    /* Client: "Reactivate this disabled route" */
    MP_CMD_DISABLE_TRADE_ROUTE,   /* Client: "Temporarily stop this route" */
    MP_CMD_SET_ROUTE_POLICY,      /* Client: "Change import/export flag for resource on route" */
    MP_CMD_SET_ROUTE_LIMIT,       /* Client: "Change buy/sell limit for resource on route" */

    /* Legacy compat (map to new lifecycle internally) */
    MP_CMD_OPEN_TRADE_ROUTE,      /* Opens city route (AI cities) */
    MP_CMD_CLOSE_TRADE_ROUTE,     /* Closes city route (AI cities) */

    /* City resource settings (import/export/stockpile) */
    MP_CMD_SET_RESOURCE_SETTING,

    /* Chat */
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
    MP_CMD_REJECT_NONE = 0,
    MP_CMD_REJECT_INVALID,
    MP_CMD_REJECT_NOT_OWNER,
    MP_CMD_REJECT_INSUFFICIENT_FUNDS,
    MP_CMD_REJECT_ROUTE_NOT_FOUND,
    MP_CMD_REJECT_ALREADY_OPEN,
    MP_CMD_REJECT_ALREADY_CLOSED,
    MP_CMD_REJECT_LIMIT_OUT_OF_RANGE,
    MP_CMD_REJECT_GAME_NOT_STARTED,
    MP_CMD_REJECT_CITY_NOT_FOUND,
    MP_CMD_REJECT_CITY_NOT_OWNED,
    MP_CMD_REJECT_CITY_OFFLINE,
    MP_CMD_REJECT_DUPLICATE_ROUTE,
    MP_CMD_REJECT_ROUTE_CAP_REACHED,
    MP_CMD_REJECT_NO_CONNECTIVITY,
    MP_CMD_REJECT_RESOURCE_INVALID,
    MP_CMD_REJECT_ROUTE_DISABLED,
    MP_CMD_REJECT_ROUTE_ALREADY_ENABLED,
    MP_CMD_REJECT_ROUTE_ALREADY_DISABLED,
    MP_CMD_REJECT_PLAYER_DISCONNECTED
} mp_command_reject_reason;

/* ---- Command payloads ---- */

/**
 * Create a new P2P trade route.
 * Client sends intent: origin city, destination city, preferred mode.
 * Client does NOT send route_id — the host assigns it.
 */
typedef struct {
    int origin_city_id;   /* Must be owned by the requesting player */
    int dest_city_id;     /* Target city (may be AI or another player) */
    uint8_t prefer_sea;   /* 1 = prefer sea route, 0 = prefer land */
} mp_cmd_create_trade_route;

/**
 * Delete an existing route entirely. Only the route owner can delete.
 */
typedef struct {
    int route_id;
    uint32_t network_route_id;  /* For verification */
} mp_cmd_delete_trade_route;

/**
 * Enable/disable a route. Disabling pauses trade without deleting the route.
 */
typedef struct {
    int route_id;
    uint32_t network_route_id;
} mp_cmd_enable_trade_route;

typedef struct {
    int route_id;
    uint32_t network_route_id;
} mp_cmd_disable_trade_route;

/**
 * Set import/export policy for a resource on a route.
 */
typedef struct {
    int route_id;
    uint32_t network_route_id;
    int resource;         /* Resource enum */
    int is_export;        /* 1 = export policy, 0 = import policy */
    int enabled;          /* 1 = enable, 0 = disable */
} mp_cmd_set_route_policy;

/**
 * Set buy/sell quantity limit for a resource on a route.
 */
typedef struct {
    int route_id;
    uint32_t network_route_id;
    int resource;
    int is_buying;        /* 1 = buying limit, 0 = selling limit */
    int amount;           /* New limit amount (0 = unlimited) */
} mp_cmd_set_route_limit;

/* Legacy open/close (for AI cities) */
typedef struct {
    int route_id;
    int city_id;
} mp_cmd_open_trade_route;

typedef struct {
    int route_id;
    int city_id;
} mp_cmd_close_trade_route;

typedef struct {
    uint8_t speed;
} mp_cmd_request_speed;

/**
 * Change a city resource setting (import/export toggle or stockpile threshold).
 * Setting types: 0=export, 1=import, 2=stockpile threshold.
 */
typedef struct {
    int resource;
    uint8_t setting_type;   /* MP_TRADE_SETTING_EXPORT/IMPORT/STOCKPILE */
    int value;              /* 0/1 for toggles, threshold amount for stockpile */
} mp_cmd_set_resource_setting;

typedef struct {
    char message[128];
    uint8_t sender_id;
} mp_cmd_chat_message;

/* ---- Command envelope ---- */

typedef struct {
    uint32_t sequence_id;
    uint16_t command_type;
    uint8_t player_id;
    uint32_t target_tick;
    mp_command_status status;
    uint8_t reject_reason;

    union {
        mp_cmd_create_trade_route create_route;
        mp_cmd_delete_trade_route delete_route;
        mp_cmd_enable_trade_route enable_route;
        mp_cmd_disable_trade_route disable_route;
        mp_cmd_set_route_policy route_policy;
        mp_cmd_set_route_limit route_limit;
        mp_cmd_open_trade_route open_route;
        mp_cmd_close_trade_route close_route;
        mp_cmd_set_resource_setting resource_setting;
        mp_cmd_request_speed speed;
        mp_cmd_chat_message chat;
    } data;
} mp_command;

/* Serialization helpers */
void mp_command_serialize(const mp_command *cmd, uint8_t *buffer, uint32_t *size);
void mp_command_deserialize(mp_command *cmd, const uint8_t *buffer, uint32_t size);

const char *mp_command_type_name(mp_command_type type);

#endif /* ENABLE_MULTIPLAYER */

#endif /* MULTIPLAYER_COMMAND_TYPES_H */

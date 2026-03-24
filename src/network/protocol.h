#ifndef NETWORK_PROTOCOL_H
#define NETWORK_PROTOCOL_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>

#define NET_PROTOCOL_VERSION        1
#define NET_MAX_PAYLOAD_SIZE        65536
#define NET_MAX_PLAYER_NAME         32
#define NET_MAX_PLAYERS             8
#define NET_MAGIC                   0x41554755  /* "AUGU" */
#define NET_CHECKSUM_INTERVAL_TICKS 500
#define NET_HEARTBEAT_INTERVAL_MS   1000
#define NET_TIMEOUT_MS              5000

typedef enum {
    NET_MSG_HELLO = 1,
    NET_MSG_JOIN_REQUEST,
    NET_MSG_JOIN_ACCEPT,
    NET_MSG_JOIN_REJECT,
    NET_MSG_READY_STATE,
    NET_MSG_START_GAME,
    NET_MSG_CLIENT_COMMAND,
    NET_MSG_HOST_COMMAND_ACK,
    NET_MSG_FULL_SNAPSHOT,
    NET_MSG_DELTA_SNAPSHOT,
    NET_MSG_HOST_EVENT,
    NET_MSG_CHECKSUM_REQUEST,
    NET_MSG_CHECKSUM_RESPONSE,
    NET_MSG_RESYNC_REQUEST,
    NET_MSG_RESYNC_GRANTED,
    NET_MSG_DISCONNECT_NOTICE,
    NET_MSG_HEARTBEAT,
    NET_MSG_CHAT,
    NET_MSG_GAME_PREPARE,       /* Host → Client: manifest with scenario info */
    NET_MSG_GAME_LOAD_COMPLETE, /* Client → Host: scenario loaded locally */
    NET_MSG_GAME_START_FINAL,   /* Host → Client: all loaded, begin gameplay */
    NET_MSG_COUNT
} net_message_type;

typedef enum {
    NET_REJECT_VERSION_MISMATCH = 1,
    NET_REJECT_SESSION_FULL,
    NET_REJECT_GAME_IN_PROGRESS,
    NET_REJECT_NAME_TAKEN,
    NET_REJECT_BANNED
} net_reject_reason;

typedef enum {
    NET_EVENT_TRADER_SPAWNED = 1,
    NET_EVENT_TRADER_REACHED_STORAGE,
    NET_EVENT_TRADER_TRADE_EXECUTED,
    NET_EVENT_TRADER_ABORTED,
    NET_EVENT_TRADER_RETURNING,
    NET_EVENT_TRADER_DESPAWNED,
    NET_EVENT_ROUTE_OPENED,
    NET_EVENT_ROUTE_CLOSED,
    NET_EVENT_TRADE_POLICY_CHANGED,
    NET_EVENT_CITY_VIEW_UPDATED,
    NET_EVENT_PLAYER_JOINED,
    NET_EVENT_PLAYER_LEFT,
    NET_EVENT_GAME_PAUSED,
    NET_EVENT_GAME_RESUMED,
    NET_EVENT_SPEED_CHANGED,
    NET_EVENT_ROUTE_CREATED,
    NET_EVENT_ROUTE_DELETED,
    NET_EVENT_ROUTE_ENABLED,
    NET_EVENT_ROUTE_DISABLED,
    NET_EVENT_ROUTE_POLICY_SET,
    NET_EVENT_ROUTE_LIMIT_SET,
    NET_EVENT_PLAYER_RECONNECTED,
    NET_EVENT_PLAYER_CITY_OFFLINE,
    NET_EVENT_PLAYER_CITY_ONLINE,
    NET_EVENT_SPAWN_TABLE_UPDATED,
    NET_EVENT_CITY_RESOURCE_SETTING,   /* Player changed import/export/stockpile for a resource */
    NET_EVENT_STORAGE_STATE_CHANGED,   /* Player changed warehouse/granary storage state */
    NET_EVENT_STORAGE_PERMISSION_CHANGED, /* Player toggled warehouse/granary permission */
    NET_EVENT_COUNT
} net_host_event_type;

typedef struct {
    uint16_t protocol_version;
    uint16_t message_type;
    uint32_t sequence_id;
    uint32_t ack_sequence_id;
    uint32_t session_id;
    uint32_t game_tick;
    uint32_t payload_size;
} net_packet_header;

#define NET_PACKET_HEADER_SIZE 24

typedef struct {
    uint32_t magic;
    uint16_t protocol_version;
    uint32_t save_version;       /* savegame version for compat check */
    uint32_t map_hash;           /* Hash of empire map for desync prevention */
    uint32_t scenario_hash;      /* Hash of scenario data */
    uint32_t feature_flags;      /* Bitmask of enabled features */
    char player_name[NET_MAX_PLAYER_NAME];
    uint8_t player_uuid[16];     /* For reconnect identification */
    uint8_t reconnect_token[16]; /* Optional: for rejoin after disconnect */
} net_msg_hello;

typedef struct {
    uint8_t player_id;
    uint8_t slot_id;
    uint32_t session_id;
    uint32_t session_seed;
    uint8_t player_count;
    uint8_t player_uuid[16];       /* Assigned UUID */
    uint8_t reconnect_token[16];   /* For future reconnect */
} net_msg_join_accept;

typedef struct {
    uint8_t reason;
} net_msg_join_reject;

typedef struct {
    uint8_t player_id;
    uint8_t is_ready;
} net_msg_ready_state;

typedef struct {
    uint32_t start_tick;
    uint8_t game_speed;
} net_msg_start_game;

typedef struct {
    uint16_t command_type;
    uint8_t player_id;
    uint32_t target_tick;
    uint32_t param_size;
    /* param data follows */
} net_msg_client_command;

typedef struct {
    uint32_t command_sequence;
    uint8_t accepted;
    uint8_t reason;
} net_msg_host_command_ack;

typedef struct {
    uint16_t event_type;
    uint32_t event_tick;
    uint32_t param_size;
    /* param data follows */
} net_msg_host_event;

typedef struct {
    uint32_t tick;
    uint32_t checksum;
} net_msg_checksum_request;

typedef struct {
    uint32_t tick;
    uint32_t checksum;
    uint8_t player_id;
} net_msg_checksum_response;

typedef struct {
    uint8_t player_id;
    uint8_t reason;
} net_msg_disconnect_notice;

typedef struct {
    uint32_t timestamp_ms;
} net_msg_heartbeat;

int net_protocol_validate_header(const net_packet_header *header);
int net_protocol_check_version(uint16_t remote_version);
const char *net_protocol_message_name(net_message_type type);

#endif /* ENABLE_MULTIPLAYER */

#endif /* NETWORK_PROTOCOL_H */

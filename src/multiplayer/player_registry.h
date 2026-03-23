#ifndef MULTIPLAYER_PLAYER_REGISTRY_H
#define MULTIPLAYER_PLAYER_REGISTRY_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>

#define MP_MAX_PLAYERS          8
#define MP_PLAYER_UUID_SIZE     16  /* 128-bit UUID stored as bytes */
#define MP_PLAYER_NAME_SIZE     32
#define MP_RECONNECT_TOKEN_SIZE 16  /* Random reconnect token */

typedef enum {
    MP_PLAYER_STATUS_NONE = 0,
    MP_PLAYER_LOBBY,
    MP_PLAYER_READY,
    MP_PLAYER_IN_GAME,
    MP_PLAYER_DISCONNECTED,
    MP_PLAYER_DESYNCED,
    MP_PLAYER_AWAITING_RECONNECT
} mp_player_status;

typedef enum {
    MP_CONNECTION_NONE = 0,
    MP_CONNECTION_CONNECTING,
    MP_CONNECTION_CONNECTED,
    MP_CONNECTION_DISCONNECTED,
    MP_CONNECTION_RECONNECTING,
    MP_CONNECTION_TIMED_OUT
} mp_connection_state;

/**
 * Player identity with stable fields for reconnect and save/load.
 *
 * player_id:   session-scoped index (0..MP_MAX_PLAYERS-1), changes between sessions
 * slot_id:     persistent slot tied to spawn position, survives save/load/reconnect
 * player_uuid: globally unique identity across sessions, used for rejoin after save/load
 */
typedef struct {
    int active;

    /* Session-scoped identity */
    uint8_t player_id;
    uint8_t slot_id;                           /* Stable spawn slot (0..MP_MAX_PLAYERS-1) */
    uint8_t player_uuid[MP_PLAYER_UUID_SIZE];  /* 128-bit persistent identity */
    char name[MP_PLAYER_NAME_SIZE];
    char display_name[MP_PLAYER_NAME_SIZE];

    /* Session role */
    int is_local;
    int is_host;

    /* Connection state */
    uint8_t peer_id;
    mp_player_status status;
    mp_connection_state connection_state;
    uint32_t disconnect_tick;          /* Tick when player disconnected */
    uint32_t reconnect_deadline_tick;  /* After this tick, slot is freed */
    uint8_t reconnect_token[MP_RECONNECT_TOKEN_SIZE];

    /* City binding */
    int empire_city_id;       /* City ID in the empire map (-1 = unassigned) */
    int scenario_city_slot;   /* Slot in the scenario/XML (-1 = unassigned) */
    int assigned_city_id;     /* Authoritative city assignment from spawn_table */

    /* Checksum tracking */
    uint32_t last_checksum;
    uint32_t last_checksum_tick;

    /* Statistics */
    uint32_t commands_sent;
    uint32_t commands_rejected;
    uint32_t resyncs_requested;
} mp_player;

void mp_player_registry_init(void);
void mp_player_registry_clear(void);

/* Add player - generates UUID and reconnect token */
int mp_player_registry_add(uint8_t player_id, const char *name, int is_local, int is_host);

/* Add player with specific UUID (used on load/rejoin) */
int mp_player_registry_add_with_uuid(uint8_t player_id, const char *name,
                                      const uint8_t *uuid, int is_local, int is_host);

void mp_player_registry_remove(uint8_t player_id);

/* Lookups */
mp_player *mp_player_registry_get(uint8_t player_id);
mp_player *mp_player_registry_get_local(void);
mp_player *mp_player_registry_get_host(void);
mp_player *mp_player_registry_get_by_uuid(const uint8_t *uuid);
mp_player *mp_player_registry_get_by_slot(uint8_t slot_id);
int mp_player_registry_get_count(void);

/* Setters */
void mp_player_registry_set_status(uint8_t player_id, mp_player_status status);
void mp_player_registry_set_city(uint8_t player_id, int empire_city_id);
void mp_player_registry_set_slot(uint8_t player_id, int scenario_city_slot);
void mp_player_registry_set_connection_state(uint8_t player_id, mp_connection_state state);
void mp_player_registry_set_assigned_city(uint8_t player_id, int city_id);

/* Slot management */
int mp_player_registry_assign_slot(uint8_t player_id);  /* Returns assigned slot_id */
int mp_player_registry_find_free_slot(void);

/* Reconnect support */
void mp_player_registry_generate_reconnect_token(uint8_t player_id);
int mp_player_registry_validate_reconnect(const uint8_t *uuid,
                                           const uint8_t *token);
void mp_player_registry_mark_disconnected(uint8_t player_id, uint32_t current_tick,
                                           uint32_t timeout_ticks);
int mp_player_registry_handle_reconnect(const uint8_t *uuid, uint8_t peer_id);
void mp_player_registry_cleanup_expired(uint32_t current_tick);

/* Iteration */
int mp_player_registry_get_first_active_id(void);
int mp_player_registry_get_next_active_id(int after_id);

/* UUID utilities */
void mp_player_registry_generate_uuid(uint8_t *out_uuid);
int mp_player_registry_uuid_equals(const uint8_t *a, const uint8_t *b);
void mp_player_registry_uuid_to_string(const uint8_t *uuid, char *out, int out_size);

/* Serialization for snapshots and save/load */
void mp_player_registry_serialize(uint8_t *buffer, uint32_t *size);
void mp_player_registry_deserialize(const uint8_t *buffer, uint32_t size);

#endif /* ENABLE_MULTIPLAYER */

#endif /* MULTIPLAYER_PLAYER_REGISTRY_H */

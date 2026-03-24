#ifndef MULTIPLAYER_MP_TRADE_ROUTE_H
#define MULTIPLAYER_MP_TRADE_ROUTE_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>
#include "game/resource.h"

/**
 * Independent P2P trade route entity for multiplayer.
 *
 * Decouples multiplayer trade from the legacy single-route-per-city model.
 * Each mp_trade_route_instance represents a bidirectional trade relationship
 * between two cities (player-to-player or player-to-AI) with its own quotas,
 * limits, traded counters, and lifecycle state.
 *
 * The host is the single source of truth for all route mutations.
 * Clients mirror the state via events and snapshots.
 */

#define MP_TRADE_ROUTE_MAX 64
#define MP_TRADE_ROUTE_INVALID_ID 0

typedef enum {
    MP_TROUTE_INACTIVE = 0,
    MP_TROUTE_PENDING,
    MP_TROUTE_ACTIVE,
    MP_TROUTE_DISABLED,
    MP_TROUTE_OFFLINE,
    MP_TROUTE_DELETED
} mp_trade_route_status;

typedef enum {
    MP_TROUTE_LAND = 0,
    MP_TROUTE_SEA,
    MP_TROUTE_AUTO
} mp_trade_route_transport;

/**
 * Per-resource limits and progress within a route instance.
 * Tracked separately for buy (import into local city) and sell (export from local city).
 */
typedef struct {
    int export_limit;           /* max units exportable per year (0 = no limit) */
    int import_limit;           /* max units importable per year (0 = no limit) */
    int exported_this_year;     /* units exported in current game year */
    int imported_this_year;     /* units imported in current game year */
    uint8_t export_enabled;     /* 1 = resource can be exported on this route */
    uint8_t import_enabled;     /* 1 = resource can be imported on this route */
} mp_trade_route_resource;

/**
 * Core P2P trade route instance.
 * One per active trade relationship. Fully independent of empire_city.route_id.
 */
typedef struct {
    int in_use;
    uint32_t instance_id;           /* unique route ID (host-allocated, monotonic) */
    uint32_t network_route_id;      /* maps to ownership network_route_id for replication */

    /* endpoints */
    uint8_t origin_player_id;
    int origin_city_id;
    uint8_t dest_player_id;         /* 0xFF if AI city */
    int dest_city_id;

    /* underlying Augustus route_id (from trade_route_new() or empire_city.route_id) */
    int augustus_route_id;

    /* mode and transport */
    uint8_t is_player_to_player;    /* 1 if both endpoints are player cities */
    mp_trade_route_transport transport;

    /* lifecycle */
    mp_trade_route_status status;
    uint32_t created_tick;
    uint32_t last_trade_tick;
    uint32_t state_version;         /* incremented on any mutation */

    /* per-resource quota tracking */
    mp_trade_route_resource resources[RESOURCE_MAX];

    /* aggregate counters */
    int total_exported;
    int total_imported;
    int total_transactions;
} mp_trade_route_instance;

/* ---- Lifecycle ---- */
void mp_trade_route_init(void);
void mp_trade_route_clear(void);

/* Host-only: create a new route instance, returns instance_id or 0 on failure */
uint32_t mp_trade_route_create(uint8_t origin_player_id, int origin_city_id,
                                uint8_t dest_player_id, int dest_city_id,
                                int augustus_route_id, uint32_t network_route_id,
                                mp_trade_route_transport transport);

/* Delete / disable / enable */
int mp_trade_route_delete(uint32_t instance_id);
int mp_trade_route_set_status(uint32_t instance_id, mp_trade_route_status status);

/* ---- Queries ---- */
mp_trade_route_instance *mp_trade_route_get(uint32_t instance_id);
mp_trade_route_instance *mp_trade_route_find_by_endpoints(int origin_city_id, int dest_city_id);
mp_trade_route_instance *mp_trade_route_find_by_augustus_route(int augustus_route_id);
int mp_trade_route_count_player_routes(uint8_t player_id);
int mp_trade_route_count_active(void);

/* Iterate all routes: callback returns 0 to continue, non-zero to stop */
typedef int (*mp_trade_route_callback)(mp_trade_route_instance *route, void *userdata);
void mp_trade_route_foreach(mp_trade_route_callback cb, void *userdata);
void mp_trade_route_foreach_active(mp_trade_route_callback cb, void *userdata);

/* ---- Resource policy ---- */
int mp_trade_route_set_resource_export(uint32_t instance_id, int resource, int enabled, int limit);
int mp_trade_route_set_resource_import(uint32_t instance_id, int resource, int enabled, int limit);
int mp_trade_route_can_export(uint32_t instance_id, int resource);
int mp_trade_route_can_import(uint32_t instance_id, int resource);
int mp_trade_route_export_remaining(uint32_t instance_id, int resource);
int mp_trade_route_import_remaining(uint32_t instance_id, int resource);

/* ---- Trade execution (host-only) ---- */
int mp_trade_route_record_export(uint32_t instance_id, int resource, int amount);
int mp_trade_route_record_import(uint32_t instance_id, int resource, int amount);

/* ---- Annual reset ---- */
void mp_trade_route_reset_annual_counters(void);

/* ---- Serialization ---- */
void mp_trade_route_serialize(uint8_t *buffer, uint32_t *out_size, uint32_t max_size);
void mp_trade_route_deserialize(const uint8_t *buffer, uint32_t size);

/* ---- Version tracking ---- */
uint32_t mp_trade_route_get_version(uint32_t instance_id);
void mp_trade_route_increment_version(uint32_t instance_id);

#endif /* ENABLE_MULTIPLAYER */
#endif /* MULTIPLAYER_MP_TRADE_ROUTE_H */

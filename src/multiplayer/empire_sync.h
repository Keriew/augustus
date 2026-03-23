#ifndef MULTIPLAYER_EMPIRE_SYNC_H
#define MULTIPLAYER_EMPIRE_SYNC_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>
#include "game/resource.h"

/**
 * Synchronization of the empire map between host and clients.
 * Manages remote player city visibility and trade view replication.
 */

#define MP_MAX_TRADE_VIEW_CITIES 32

/**
 * Trade view: replicated snapshot of a remote city's trade capabilities.
 * This is what clients see instead of computing trade eligibility locally.
 */
typedef struct {
    int city_id;
    int player_id;
    int online;
    int exportable[RESOURCE_MAX];     /* Can this city export resource X? */
    int importable[RESOURCE_MAX];     /* Can this city import resource X? */
    int dock_available;                /* Has working dock */
    int land_route_available;          /* Has land trade access */
    int stock_level[RESOURCE_MAX];     /* Approximate stock levels (for UI) */
} mp_city_trade_view;

void mp_empire_sync_init(void);
void mp_empire_sync_clear(void);

/* Host: register a player's city on the empire map */
void mp_empire_sync_register_player_city(int city_id, uint8_t player_id,
                                          int empire_object_id);

/* Host: unregister (disconnect) */
void mp_empire_sync_unregister_player_city(uint8_t player_id);

/* Host: update trade view for a player's city (called periodically) */
void mp_empire_sync_update_trade_views(void);

/* Host: broadcast updated trade views to all clients */
void mp_empire_sync_broadcast_views(void);

/* Query trade views */
const mp_city_trade_view *mp_empire_sync_get_trade_view(int city_id);
int mp_empire_sync_get_city_id_for_player(uint8_t player_id);

/* Check if a resource can be exported/imported for a remote player city */
int mp_empire_sync_can_export_to_remote(int city_id, int resource);
int mp_empire_sync_can_import_from_remote(int city_id, int resource);

/* Serialization for snapshots */
void mp_empire_sync_serialize(uint8_t *buffer, uint32_t *size);
void mp_empire_sync_deserialize(const uint8_t *buffer, uint32_t size);

/* Called from session.c */
void multiplayer_empire_sync_receive_event(const uint8_t *data, uint32_t size);

#endif /* ENABLE_MULTIPLAYER */

#endif /* MULTIPLAYER_EMPIRE_SYNC_H */

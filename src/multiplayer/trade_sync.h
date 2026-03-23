#ifndef MULTIPLAYER_TRADE_SYNC_H
#define MULTIPLAYER_TRADE_SYNC_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>

/**
 * Synchronization of trade routes, quotas, and trader entities.
 * The host is authoritative for all trade outcomes.
 */

void mp_trade_sync_init(void);
void mp_trade_sync_clear(void);

/* Host: emit trader events for replication */
void mp_trade_sync_emit_trader_spawned(int figure_id, int city_id, int route_id);
void mp_trade_sync_emit_trader_trade_executed(int figure_id, int resource,
                                               int amount, int buying);
void mp_trade_sync_emit_trader_despawned(int figure_id);

/* Host: broadcast route state changes */
void mp_trade_sync_broadcast_route_state(int route_id);

/* Client: handle events from host */
void mp_trade_sync_handle_event(uint16_t event_type,
                                 const uint8_t *data, uint32_t size);

/* Serialization for snapshots */
void mp_trade_sync_serialize_routes(uint8_t *buffer, uint32_t *size);
void mp_trade_sync_deserialize_routes(const uint8_t *buffer, uint32_t size);
void mp_trade_sync_serialize_traders(uint8_t *buffer, uint32_t *size);
void mp_trade_sync_deserialize_traders(const uint8_t *buffer, uint32_t size);

/* Query: is this figure's trade authoritative on this machine? */
int mp_trade_sync_is_authoritative(int figure_id);

#endif /* ENABLE_MULTIPLAYER */

#endif /* MULTIPLAYER_TRADE_SYNC_H */

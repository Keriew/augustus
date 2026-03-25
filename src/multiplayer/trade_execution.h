#ifndef MULTIPLAYER_TRADE_EXECUTION_H
#define MULTIPLAYER_TRADE_EXECUTION_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>

/**
 * Host-authoritative trade execution layer.
 *
 * This is the single orchestration point for all multiplayer trade transactions.
 * It sits above the Claudius core economic engine and enforces:
 *   - Validation before any stock mutation
 *   - Atomic commit (remove from origin + add to destination)
 *   - Route quota tracking
 *   - Event emission for client sync
 *   - Deterministic per-tick ordering
 *
 * Only the host calls execute functions. Clients receive results via events.
 */

#define MP_TRADE_EXEC_MAX_PER_TICK 32

/* Transaction result codes */
typedef enum {
    MP_TRADE_OK = 0,
    MP_TRADE_ERR_ROUTE_INVALID,
    MP_TRADE_ERR_ROUTE_INACTIVE,
    MP_TRADE_ERR_RESOURCE_INVALID,
    MP_TRADE_ERR_EXPORT_DISABLED,
    MP_TRADE_ERR_IMPORT_DISABLED,
    MP_TRADE_ERR_NO_STOCK,
    MP_TRADE_ERR_NO_CAPACITY,
    MP_TRADE_ERR_QUOTA_EXCEEDED,
    MP_TRADE_ERR_CITY_OFFLINE,
    MP_TRADE_ERR_STORAGE_NOT_FOUND,
    MP_TRADE_ERR_INTERNAL
} mp_trade_exec_result;

/**
 * A completed transaction record, used for logging and event emission.
 */
typedef struct {
    uint32_t transaction_id;
    uint32_t route_instance_id;
    uint8_t origin_player_id;
    int origin_city_id;
    uint8_t dest_player_id;
    int dest_city_id;
    int resource;
    int amount_requested;
    int amount_committed;
    int source_storage_id;      /* building_id of source warehouse/granary */
    int dest_storage_id;        /* building_id of destination warehouse/granary */
    uint32_t tick;
    uint8_t transport_type;     /* 0=land, 1=sea */
    int figure_id;              /* trader figure that executed this, or -1 */
    mp_trade_exec_result result;
} mp_trade_transaction;

/* ---- Initialization ---- */
void mp_trade_execution_init(void);

/* ---- Host-only: validate and execute ---- */

/**
 * Validate whether an export is possible from origin city.
 * Checks: resource policy, stock availability, quota, route state.
 */
mp_trade_exec_result mp_trade_validate_export(uint32_t route_instance_id,
                                               int resource, int amount,
                                               int source_building_id);

/**
 * Validate whether an import is possible to destination city.
 * Checks: resource policy, capacity, quota, route state.
 */
mp_trade_exec_result mp_trade_validate_import(uint32_t route_instance_id,
                                               int resource, int amount,
                                               int dest_building_id);

/**
 * Execute a complete trade transaction atomically.
 * 1. Validates export from source
 * 2. Validates import to destination
 * 3. Removes from source warehouse/granary
 * 4. Adds to destination warehouse/granary
 * 5. Updates route quotas
 * 6. Emits transaction event
 * 7. Logs the transaction
 *
 * Returns MP_TRADE_OK on success. On failure, no state is mutated.
 */
mp_trade_exec_result mp_trade_commit_transaction(uint32_t route_instance_id,
                                                   int resource, int amount,
                                                   int source_building_id,
                                                   int dest_building_id,
                                                   int figure_id);

/**
 * Process all eligible routes for the current tick.
 * Called once per tick on the host.
 * Routes are processed in deterministic order (by instance_id).
 * Each transaction sees updated stock from previous transactions.
 */
void mp_trade_execute_tick(uint32_t current_tick);

/* ---- Transaction history ---- */
const mp_trade_transaction *mp_trade_get_last_transaction(void);
uint32_t mp_trade_get_transaction_count(void);

/* ---- Trader cargo recovery (host-only) ---- */
void mp_trade_recover_trader_cargo(int figure_id);

/* ---- Annual reset hook ---- */
void mp_trade_execution_on_year_change(void);

#endif /* ENABLE_MULTIPLAYER */
#endif /* MULTIPLAYER_TRADE_EXECUTION_H */

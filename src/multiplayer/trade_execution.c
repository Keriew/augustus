#include "trade_execution.h"

#ifdef ENABLE_MULTIPLAYER

#include "mp_trade_route.h"
#include "trade_log.h"
#include "mp_debug_log.h"
#include "ownership.h"
#include "trade_sync.h"
#include "empire_sync.h"
#include "network/session.h"
#include "network/serialize.h"
#include "network/protocol.h"
#include "building/building.h"
#include "building/warehouse.h"
#include "building/granary.h"
#include "city/resource.h"
#include "empire/city.h"
#include "empire/trade_route.h"
#include "empire/trade_prices.h"
#include "game/resource.h"
#include "core/log.h"

#include <string.h>

static struct {
    uint32_t next_transaction_id;
    mp_trade_transaction last_transaction;
    uint32_t total_transactions;
} exec_data;

void mp_trade_execution_init(void)
{
    memset(&exec_data, 0, sizeof(exec_data));
    exec_data.next_transaction_id = 1;
    mp_trade_log_init();
}

/* ---- Validation ---- */

mp_trade_exec_result mp_trade_validate_export(uint32_t route_instance_id,
                                               int resource, int amount,
                                               int source_building_id)
{
    if (resource < RESOURCE_MIN || resource >= RESOURCE_MAX) {
        return MP_TRADE_ERR_RESOURCE_INVALID;
    }

    mp_trade_route_instance *route = mp_trade_route_get(route_instance_id);
    if (!route) {
        return MP_TRADE_ERR_ROUTE_INVALID;
    }
    if (route->status != MP_TROUTE_ACTIVE) {
        return MP_TRADE_ERR_ROUTE_INACTIVE;
    }

    /* Check export enabled on this route for this resource */
    if (!mp_trade_route_can_export(route_instance_id, resource)) {
        return MP_TRADE_ERR_EXPORT_DISABLED;
    }

    /* Check quota remaining */
    if (mp_trade_route_export_remaining(route_instance_id, resource) < amount) {
        return MP_TRADE_ERR_QUOTA_EXCEEDED;
    }

    /* Check source building exists and has stock */
    if (source_building_id > 0) {
        building *b = building_get(source_building_id);
        if (!b || b->state != BUILDING_STATE_IN_USE) {
            return MP_TRADE_ERR_STORAGE_NOT_FOUND;
        }
        int available = 0;
        if (b->type == BUILDING_GRANARY) {
            available = building_granary_count_available_resource(b, resource, 1);
        } else if (b->type == BUILDING_WAREHOUSE) {
            available = building_warehouse_get_available_amount(b, resource);
        } else {
            return MP_TRADE_ERR_STORAGE_NOT_FOUND;
        }
        if (available < amount) {
            return MP_TRADE_ERR_NO_STOCK;
        }
    }

    return MP_TRADE_OK;
}

mp_trade_exec_result mp_trade_validate_import(uint32_t route_instance_id,
                                               int resource, int amount,
                                               int dest_building_id)
{
    if (resource < RESOURCE_MIN || resource >= RESOURCE_MAX) {
        return MP_TRADE_ERR_RESOURCE_INVALID;
    }

    mp_trade_route_instance *route = mp_trade_route_get(route_instance_id);
    if (!route) {
        return MP_TRADE_ERR_ROUTE_INVALID;
    }
    if (route->status != MP_TROUTE_ACTIVE) {
        return MP_TRADE_ERR_ROUTE_INACTIVE;
    }

    /* Check import enabled on this route for this resource */
    if (!mp_trade_route_can_import(route_instance_id, resource)) {
        return MP_TRADE_ERR_IMPORT_DISABLED;
    }

    /* Check quota remaining */
    if (mp_trade_route_import_remaining(route_instance_id, resource) < amount) {
        return MP_TRADE_ERR_QUOTA_EXCEEDED;
    }

    /* Check destination building can accept */
    if (dest_building_id > 0) {
        building *b = building_get(dest_building_id);
        if (!b || b->state != BUILDING_STATE_IN_USE) {
            return MP_TRADE_ERR_STORAGE_NOT_FOUND;
        }
        int capacity = 0;
        if (b->type == BUILDING_GRANARY) {
            capacity = building_granary_maximum_receptible_amount(b, resource);
        } else if (b->type == BUILDING_WAREHOUSE) {
            capacity = building_warehouse_maximum_receptible_amount(b, resource);
        } else {
            return MP_TRADE_ERR_STORAGE_NOT_FOUND;
        }
        if (capacity < amount) {
            return MP_TRADE_ERR_NO_CAPACITY;
        }
    }

    return MP_TRADE_OK;
}

/* ---- Emit transaction event ---- */

static void emit_transaction_event(const mp_trade_transaction *tx)
{
    if (!net_session_is_host()) {
        return;
    }

    uint8_t buf[96];
    net_serializer s;
    net_serializer_init(&s, buf, sizeof(buf));

    net_write_u16(&s, NET_EVENT_TRADER_TRADE_EXECUTED);
    net_write_u32(&s, tx->tick);
    net_write_u32(&s, tx->transaction_id);
    net_write_u32(&s, tx->route_instance_id);
    net_write_u8(&s, tx->origin_player_id);
    net_write_i32(&s, tx->origin_city_id);
    net_write_u8(&s, tx->dest_player_id);
    net_write_i32(&s, tx->dest_city_id);
    net_write_i32(&s, tx->resource);
    net_write_i32(&s, tx->amount_committed);
    net_write_i32(&s, tx->source_storage_id);
    net_write_i32(&s, tx->dest_storage_id);
    net_write_u8(&s, tx->transport_type);
    net_write_i32(&s, tx->figure_id);

    net_session_broadcast(NET_MSG_HOST_EVENT, buf, (uint32_t)net_serializer_position(&s));
}

/* ---- Atomic Commit ---- */

mp_trade_exec_result mp_trade_commit_transaction(uint32_t route_instance_id,
                                                   int resource, int amount,
                                                   int source_building_id,
                                                   int dest_building_id,
                                                   int figure_id)
{
    if (!net_session_is_host() && net_session_is_active()) {
        MP_LOG_ERROR("TRADE_EXEC", "Only host can commit transactions");
        return MP_TRADE_ERR_INTERNAL;
    }

    mp_trade_route_instance *route = mp_trade_route_get(route_instance_id);
    if (!route) {
        return MP_TRADE_ERR_ROUTE_INVALID;
    }

    /* 1. Validate export */
    mp_trade_exec_result export_result = mp_trade_validate_export(
        route_instance_id, resource, amount, source_building_id);
    if (export_result != MP_TRADE_OK) {
        MP_LOG_WARN("TRADE_EXEC", "Export validation failed: route=%u res=%d err=%d",
                    route_instance_id, resource, export_result);
        return export_result;
    }

    /* 2. Validate import */
    mp_trade_exec_result import_result = mp_trade_validate_import(
        route_instance_id, resource, amount, dest_building_id);
    if (import_result != MP_TRADE_OK) {
        MP_LOG_WARN("TRADE_EXEC", "Import validation failed: route=%u res=%d err=%d",
                    route_instance_id, resource, import_result);
        return import_result;
    }

    /* 3. Remove from source (atomic: if this fails, abort) */
    int removed = 0;
    if (source_building_id > 0) {
        building *src = building_get(source_building_id);
        if (!src) {
            return MP_TRADE_ERR_STORAGE_NOT_FOUND;
        }
        int trader_type = (route->transport == MP_TROUTE_SEA) ? 0 : 1;
        if (src->type == BUILDING_GRANARY) {
            removed = building_granary_remove_export(src, resource, amount, trader_type);
        } else {
            removed = building_warehouse_remove_export(src, resource, amount, trader_type);
        }
        if (removed <= 0) {
            MP_LOG_WARN("TRADE_EXEC", "Remove from source failed: bld=%d res=%d amt=%d",
                        source_building_id, resource, amount);
            return MP_TRADE_ERR_NO_STOCK;
        }
    } else {
        removed = amount; /* virtual export (e.g., remote player city) */
    }

    /* 4. Add to destination */
    int added = 0;
    if (dest_building_id > 0) {
        building *dst = building_get(dest_building_id);
        if (!dst) {
            /* Rollback: re-add to source if we already removed */
            if (source_building_id > 0 && removed > 0) {
                building *src = building_get(source_building_id);
                if (src) {
                    int trader_type = (route->transport == MP_TROUTE_SEA) ? 0 : 1;
                    if (src->type == BUILDING_GRANARY) {
                        building_granary_add_import(src, resource, removed, trader_type);
                    } else {
                        building_warehouse_add_import(src, resource, removed, trader_type);
                    }
                }
            }
            return MP_TRADE_ERR_STORAGE_NOT_FOUND;
        }
        int trader_type = (route->transport == MP_TROUTE_SEA) ? 0 : 1;
        if (dst->type == BUILDING_GRANARY) {
            added = building_granary_add_import(dst, resource, removed, trader_type);
        } else {
            added = building_warehouse_add_import(dst, resource, removed, trader_type);
        }
        if (added <= 0) {
            /* Rollback: re-add to source */
            if (source_building_id > 0 && removed > 0) {
                building *src = building_get(source_building_id);
                if (src) {
                    if (src->type == BUILDING_GRANARY) {
                        building_granary_add_import(src, resource, removed, trader_type);
                    } else {
                        building_warehouse_add_import(src, resource, removed, trader_type);
                    }
                }
            }
            MP_LOG_WARN("TRADE_EXEC", "Add to dest failed: bld=%d res=%d amt=%d",
                        dest_building_id, resource, removed);
            return MP_TRADE_ERR_NO_CAPACITY;
        }
    } else {
        added = removed; /* virtual import (e.g., remote player city) */
    }

    /* 5. Update route quotas */
    mp_trade_route_record_export(route_instance_id, resource, removed);
    mp_trade_route_record_import(route_instance_id, resource, added);

    /* Also update the underlying Augustus trade route if it exists */
    if (route->augustus_route_id >= 0 && trade_route_is_valid(route->augustus_route_id)) {
        trade_route_increase_traded(route->augustus_route_id, resource, 1); /* buying */
        trade_route_increase_traded(route->augustus_route_id, resource, 0); /* selling */
    }

    route->last_trade_tick = net_session_get_authoritative_tick();

    /* 6. Build transaction record */
    mp_trade_transaction tx;
    memset(&tx, 0, sizeof(tx));
    tx.transaction_id = exec_data.next_transaction_id++;
    tx.route_instance_id = route_instance_id;
    tx.origin_player_id = route->origin_player_id;
    tx.origin_city_id = route->origin_city_id;
    tx.dest_player_id = route->dest_player_id;
    tx.dest_city_id = route->dest_city_id;
    tx.resource = resource;
    tx.amount_requested = amount;
    tx.amount_committed = added;
    tx.source_storage_id = source_building_id;
    tx.dest_storage_id = dest_building_id;
    tx.tick = net_session_get_authoritative_tick();
    tx.transport_type = (route->transport == MP_TROUTE_SEA) ? 1 : 0;
    tx.figure_id = figure_id;
    tx.result = MP_TRADE_OK;

    exec_data.last_transaction = tx;
    exec_data.total_transactions++;

    /* 7. Emit event to all clients */
    emit_transaction_event(&tx);

    /* 8. Structured audit log */
    {
        mp_trade_log_entry log_entry;
        memset(&log_entry, 0, sizeof(log_entry));
        log_entry.tick = tx.tick;
        log_entry.route_instance_id = route_instance_id;
        log_entry.origin_player_id = route->origin_player_id;
        log_entry.origin_city_id = route->origin_city_id;
        log_entry.dest_player_id = route->dest_player_id;
        log_entry.dest_city_id = route->dest_city_id;
        log_entry.resource = resource;
        log_entry.amount_requested = amount;
        log_entry.amount_committed = added;
        log_entry.source_storage_id = source_building_id;
        log_entry.dest_storage_id = dest_building_id;
        log_entry.figure_id = figure_id;
        log_entry.export_quota_after = route->resources[resource].exported_this_year;
        log_entry.import_quota_after = route->resources[resource].imported_this_year;
        log_entry.outcome = MP_TLOG_SUCCESS;
        mp_trade_log_record(&log_entry);
    }

    return MP_TRADE_OK;
}

/* ---- Per-tick processing ---- */

/**
 * Called once per tick on the host.
 * Iterates all active routes in deterministic order (by instance_id).
 * For routes whose traders are at storage, this triggers the trade execution.
 *
 * Note: The actual per-tick trader execution still happens through the
 * Augustus figure action system (trader.c). This function handles the
 * mp_trade_route_instance bookkeeping and ensures deterministic ordering.
 */
void mp_trade_execute_tick(uint32_t current_tick)
{
    if (!net_session_is_host() && net_session_is_active()) {
        return;
    }

    /* Routes are processed in instance_id order for determinism.
     * The alloc_route function allocates linearly, so iterating
     * the array in index order gives deterministic instance_id order. */
    (void)current_tick;
    /* Actual trade execution is driven by figure actions (trader.c).
     * This function provides the deterministic ordering hook.
     * The mp_trade_commit_transaction calls happen when traders
     * reach storage buildings. */
}

/* ---- Year change ---- */

void mp_trade_execution_on_year_change(void)
{
    if (!net_session_is_active()) {
        return;
    }

    /* Only host resets and broadcasts */
    if (net_session_is_host()) {
        mp_trade_route_reset_annual_counters();

        /* Broadcast the route snapshot so clients get the reset state */
        uint8_t buf[8192];
        uint32_t size = 0;
        mp_trade_route_serialize(buf, &size, sizeof(buf));

        if (size > 0) {
            uint8_t event_buf[8200];
            net_serializer s;
            net_serializer_init(&s, event_buf, sizeof(event_buf));
            net_write_u16(&s, NET_EVENT_TRADE_POLICY_CHANGED);
            net_write_u32(&s, net_session_get_authoritative_tick());
            net_write_u32(&s, size);
            /* Copy serialized route data after header */
            uint32_t header_size = (uint32_t)net_serializer_position(&s);
            if (header_size + size <= sizeof(event_buf)) {
                memcpy(event_buf + header_size, buf, size);
                net_session_broadcast(NET_MSG_HOST_EVENT, event_buf, header_size + size);
            }
        }

        MP_LOG_INFO("TRADE_EXEC", "Year change: annual trade counters reset and broadcast");
    }
}

/* ---- Queries ---- */

const mp_trade_transaction *mp_trade_get_last_transaction(void)
{
    return &exec_data.last_transaction;
}

uint32_t mp_trade_get_transaction_count(void)
{
    return exec_data.total_transactions;
}

#endif /* ENABLE_MULTIPLAYER */

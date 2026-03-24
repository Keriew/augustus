#ifndef MULTIPLAYER_TRADE_LOG_H
#define MULTIPLAYER_TRADE_LOG_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>

/**
 * Structured trade transaction log for multiplayer debugging.
 * Stores a ring buffer of recent transactions with full audit trail.
 * Each entry records before/after stock and quota state.
 */

#define MP_TRADE_LOG_MAX_ENTRIES 256

typedef enum {
    MP_TLOG_SUCCESS = 0,
    MP_TLOG_EXPORT_BLOCKED,
    MP_TLOG_IMPORT_BLOCKED,
    MP_TLOG_NO_STOCK,
    MP_TLOG_NO_CAPACITY,
    MP_TLOG_QUOTA_EXCEEDED,
    MP_TLOG_ROUTE_INACTIVE,
    MP_TLOG_STORAGE_MISSING,
    MP_TLOG_ROLLBACK
} mp_trade_log_outcome;

typedef struct {
    uint32_t entry_id;
    uint32_t tick;
    uint32_t route_instance_id;
    uint8_t origin_player_id;
    int origin_city_id;
    uint8_t dest_player_id;
    int dest_city_id;
    int resource;
    int amount_requested;
    int amount_committed;
    int source_storage_id;
    int dest_storage_id;
    int figure_id;
    /* before/after stock */
    int origin_stock_before;
    int origin_stock_after;
    int dest_stock_before;
    int dest_stock_after;
    /* before/after quota */
    int export_quota_before;
    int export_quota_after;
    int import_quota_before;
    int import_quota_after;
    mp_trade_log_outcome outcome;
} mp_trade_log_entry;

void mp_trade_log_init(void);

/* Record a transaction attempt (success or failure) */
void mp_trade_log_record(const mp_trade_log_entry *entry);

/* Query: get the N most recent entries */
int mp_trade_log_get_recent(mp_trade_log_entry *out, int max_entries);

/* Query: total entries ever recorded */
uint32_t mp_trade_log_total_count(void);

/* Query: total successful / failed */
uint32_t mp_trade_log_success_count(void);
uint32_t mp_trade_log_failure_count(void);

#endif /* ENABLE_MULTIPLAYER */
#endif /* MULTIPLAYER_TRADE_LOG_H */

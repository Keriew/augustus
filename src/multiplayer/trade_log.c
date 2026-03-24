#include "trade_log.h"

#ifdef ENABLE_MULTIPLAYER

#include "mp_debug_log.h"
#include <string.h>

static struct {
    mp_trade_log_entry entries[MP_TRADE_LOG_MAX_ENTRIES];
    int head;       /* next write position */
    int count;      /* entries currently in buffer */
    uint32_t next_entry_id;
    uint32_t total_count;
    uint32_t success_count;
    uint32_t failure_count;
} log_data;

void mp_trade_log_init(void)
{
    memset(&log_data, 0, sizeof(log_data));
    log_data.next_entry_id = 1;
}

void mp_trade_log_record(const mp_trade_log_entry *entry)
{
    mp_trade_log_entry *slot = &log_data.entries[log_data.head];
    *slot = *entry;
    slot->entry_id = log_data.next_entry_id++;

    log_data.head = (log_data.head + 1) % MP_TRADE_LOG_MAX_ENTRIES;
    if (log_data.count < MP_TRADE_LOG_MAX_ENTRIES) {
        log_data.count++;
    }

    log_data.total_count++;
    if (entry->outcome == MP_TLOG_SUCCESS) {
        log_data.success_count++;
    } else {
        log_data.failure_count++;
    }

    /* Structured log output */
    MP_LOG_INFO("TRADE_LOG",
        "[%s] #%u tick=%u route=%u P%d->P%d res=%d req=%d got=%d "
        "stock[%d->%d] dest[%d->%d] quota_exp[%d->%d] quota_imp[%d->%d] "
        "src_bld=%d dst_bld=%d fig=%d",
        entry->outcome == MP_TLOG_SUCCESS ? "OK" : "FAIL",
        slot->entry_id, entry->tick, entry->route_instance_id,
        entry->origin_player_id, entry->dest_player_id,
        entry->resource, entry->amount_requested, entry->amount_committed,
        entry->origin_stock_before, entry->origin_stock_after,
        entry->dest_stock_before, entry->dest_stock_after,
        entry->export_quota_before, entry->export_quota_after,
        entry->import_quota_before, entry->import_quota_after,
        entry->source_storage_id, entry->dest_storage_id, entry->figure_id);
}

int mp_trade_log_get_recent(mp_trade_log_entry *out, int max_entries)
{
    int to_return = max_entries < log_data.count ? max_entries : log_data.count;
    int read_pos = (log_data.head - to_return + MP_TRADE_LOG_MAX_ENTRIES) % MP_TRADE_LOG_MAX_ENTRIES;
    for (int i = 0; i < to_return; i++) {
        out[i] = log_data.entries[read_pos];
        read_pos = (read_pos + 1) % MP_TRADE_LOG_MAX_ENTRIES;
    }
    return to_return;
}

uint32_t mp_trade_log_total_count(void) { return log_data.total_count; }
uint32_t mp_trade_log_success_count(void) { return log_data.success_count; }
uint32_t mp_trade_log_failure_count(void) { return log_data.failure_count; }

#endif /* ENABLE_MULTIPLAYER */

#include "snapshot.h"

#ifdef ENABLE_MULTIPLAYER

#include "player_registry.h"
#include "ownership.h"
#include "trade_sync.h"
#include "empire_sync.h"
#include "time_sync.h"
#include "network/serialize.h"
#include "network/session.h"
#include "core/log.h"

#include <string.h>

static uint32_t last_snapshot_tick = 0;

static void write_snapshot_header(net_serializer *s, const mp_snapshot_header *header)
{
    net_write_u32(s, header->tick);
    net_write_u32(s, header->checksum);
    for (int i = 0; i < MP_SNAPSHOT_COUNT; i++) {
        net_write_u32(s, header->domain_offsets[i]);
        net_write_u32(s, header->domain_sizes[i]);
    }
    net_write_u32(s, header->total_size);
}

static void read_snapshot_header(net_serializer *s, mp_snapshot_header *header)
{
    header->tick = net_read_u32(s);
    header->checksum = net_read_u32(s);
    for (int i = 0; i < MP_SNAPSHOT_COUNT; i++) {
        header->domain_offsets[i] = net_read_u32(s);
        header->domain_sizes[i] = net_read_u32(s);
    }
    header->total_size = net_read_u32(s);
}

int mp_snapshot_build_full(uint8_t *buffer, uint32_t buffer_size, uint32_t *out_size)
{
    if (buffer_size < MP_SNAPSHOT_MAX_SIZE) {
        log_error("Snapshot buffer too small", 0, 0);
        return 0;
    }

    mp_snapshot_header header;
    memset(&header, 0, sizeof(header));
    header.tick = net_session_get_authoritative_tick();

    /* Reserve space for header (will fill offsets after writing domains) */
    uint32_t header_size = 12 + MP_SNAPSHOT_COUNT * 8; /* tick + checksum + total_size + domains */
    uint32_t write_pos = header_size;

    /* Domain: Player Registry */
    header.domain_offsets[MP_SNAPSHOT_PLAYER_REGISTRY] = write_pos;
    uint32_t domain_size = 0;
    mp_player_registry_serialize(buffer + write_pos, &domain_size);
    header.domain_sizes[MP_SNAPSHOT_PLAYER_REGISTRY] = domain_size;
    write_pos += domain_size;

    /* Domain: Ownership */
    header.domain_offsets[MP_SNAPSHOT_OWNERSHIP] = write_pos;
    mp_ownership_serialize(buffer + write_pos, &domain_size);
    header.domain_sizes[MP_SNAPSHOT_OWNERSHIP] = domain_size;
    write_pos += domain_size;

    /* Domain: Empire (city states, trade views) */
    header.domain_offsets[MP_SNAPSHOT_EMPIRE] = write_pos;
    mp_empire_sync_serialize(buffer + write_pos, &domain_size);
    header.domain_sizes[MP_SNAPSHOT_EMPIRE] = domain_size;
    write_pos += domain_size;

    /* Domain: Trade Routes */
    header.domain_offsets[MP_SNAPSHOT_TRADE_ROUTES] = write_pos;
    mp_trade_sync_serialize_routes(buffer + write_pos, &domain_size);
    header.domain_sizes[MP_SNAPSHOT_TRADE_ROUTES] = domain_size;
    write_pos += domain_size;

    /* Domain: Trader Entities */
    header.domain_offsets[MP_SNAPSHOT_TRADER_ENTITIES] = write_pos;
    mp_trade_sync_serialize_traders(buffer + write_pos, &domain_size);
    header.domain_sizes[MP_SNAPSHOT_TRADER_ENTITIES] = domain_size;
    write_pos += domain_size;

    /* Domain: Time */
    header.domain_offsets[MP_SNAPSHOT_TIME] = write_pos;
    mp_time_sync_serialize(buffer + write_pos, &domain_size);
    header.domain_sizes[MP_SNAPSHOT_TIME] = domain_size;
    write_pos += domain_size;

    header.total_size = write_pos;

    /* Write header at the beginning */
    net_serializer hs;
    net_serializer_init(&hs, buffer, header_size);
    write_snapshot_header(&hs, &header);

    *out_size = write_pos;
    last_snapshot_tick = header.tick;

    log_info("Full snapshot built", 0, (int)write_pos);
    return 1;
}

int mp_snapshot_apply_full(const uint8_t *buffer, uint32_t size)
{
    mp_snapshot_header header;
    net_serializer hs;
    net_serializer_init(&hs, (uint8_t *)buffer, size);
    read_snapshot_header(&hs, &header);

    if (net_serializer_has_overflow(&hs)) {
        log_error("Invalid snapshot header", 0, 0);
        return 0;
    }

    /* Apply each domain */
    if (header.domain_sizes[MP_SNAPSHOT_PLAYER_REGISTRY] > 0) {
        mp_player_registry_deserialize(
            buffer + header.domain_offsets[MP_SNAPSHOT_PLAYER_REGISTRY],
            header.domain_sizes[MP_SNAPSHOT_PLAYER_REGISTRY]);
    }

    if (header.domain_sizes[MP_SNAPSHOT_OWNERSHIP] > 0) {
        mp_ownership_deserialize(
            buffer + header.domain_offsets[MP_SNAPSHOT_OWNERSHIP],
            header.domain_sizes[MP_SNAPSHOT_OWNERSHIP]);
    }

    if (header.domain_sizes[MP_SNAPSHOT_EMPIRE] > 0) {
        mp_empire_sync_deserialize(
            buffer + header.domain_offsets[MP_SNAPSHOT_EMPIRE],
            header.domain_sizes[MP_SNAPSHOT_EMPIRE]);
    }

    if (header.domain_sizes[MP_SNAPSHOT_TRADE_ROUTES] > 0) {
        mp_trade_sync_deserialize_routes(
            buffer + header.domain_offsets[MP_SNAPSHOT_TRADE_ROUTES],
            header.domain_sizes[MP_SNAPSHOT_TRADE_ROUTES]);
    }

    if (header.domain_sizes[MP_SNAPSHOT_TRADER_ENTITIES] > 0) {
        mp_trade_sync_deserialize_traders(
            buffer + header.domain_offsets[MP_SNAPSHOT_TRADER_ENTITIES],
            header.domain_sizes[MP_SNAPSHOT_TRADER_ENTITIES]);
    }

    if (header.domain_sizes[MP_SNAPSHOT_TIME] > 0) {
        mp_time_sync_deserialize(
            buffer + header.domain_offsets[MP_SNAPSHOT_TIME],
            header.domain_sizes[MP_SNAPSHOT_TIME]);
    }

    last_snapshot_tick = header.tick;
    log_info("Full snapshot applied at tick", 0, (int)header.tick);
    return 1;
}

int mp_snapshot_build_delta(uint8_t *buffer, uint32_t buffer_size,
                            uint32_t *out_size, uint32_t since_tick)
{
    /* For MVP, delta snapshots use the same format as full but only include
       domains that have changed. The trade_sync and empire_sync modules
       track dirty flags to determine what needs to be sent. */

    /* In practice, trade views and route states change most frequently,
       so we always include those. Other domains only if explicitly dirty. */
    return mp_snapshot_build_full(buffer, buffer_size, out_size);
}

int mp_snapshot_apply_delta(const uint8_t *buffer, uint32_t size)
{
    return mp_snapshot_apply_full(buffer, size);
}

uint32_t mp_snapshot_get_last_tick(void)
{
    return last_snapshot_tick;
}

void multiplayer_snapshot_receive_full(const uint8_t *data, uint32_t size)
{
    mp_snapshot_apply_full(data, size);
}

void multiplayer_snapshot_receive_delta(const uint8_t *data, uint32_t size)
{
    mp_snapshot_apply_delta(data, size);
}

#endif /* ENABLE_MULTIPLAYER */

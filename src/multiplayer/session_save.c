#include "session_save.h"

#ifdef ENABLE_MULTIPLAYER

#include "player_registry.h"
#include "ownership.h"
#include "empire_sync.h"
#include "trade_sync.h"
#include "time_sync.h"
#include "checksum.h"
#include "network/session.h"
#include "network/serialize.h"
#include "network/protocol.h"
#include "core/log.h"

#include <string.h>

#define DOMAIN_BUFFER_SIZE 32768

int mp_session_save_to_buffer(uint8_t *buffer, uint32_t buffer_size, uint32_t *out_size)
{
    if (!net_session_is_host()) {
        log_error("Only host can save multiplayer session", 0, 0);
        return 0;
    }

    mp_save_header header;
    memset(&header, 0, sizeof(header));
    header.magic = MP_SAVE_MAGIC;
    header.version = MP_SAVE_VERSION;
    header.protocol_version = NET_PROTOCOL_VERSION;
    header.session_id = net_session_get()->session_id;
    header.host_player_id = net_session_get_local_player_id();
    header.player_count = (uint8_t)mp_player_registry_get_count();
    header.snapshot_tick = net_session_get_authoritative_tick();
    header.checksum = mp_checksum_compute();

    /* Serialize each domain into temporary buffers */
    uint8_t *temp = (uint8_t *)malloc(DOMAIN_BUFFER_SIZE * 6);
    if (!temp) {
        log_error("Failed to allocate save buffer", 0, 0);
        return 0;
    }

    uint8_t *player_buf = temp;
    uint8_t *ownership_buf = temp + DOMAIN_BUFFER_SIZE;
    uint8_t *empire_buf = temp + DOMAIN_BUFFER_SIZE * 2;
    uint8_t *routes_buf = temp + DOMAIN_BUFFER_SIZE * 3;
    uint8_t *traders_buf = temp + DOMAIN_BUFFER_SIZE * 4;
    uint8_t *time_buf = temp + DOMAIN_BUFFER_SIZE * 5;

    mp_player_registry_serialize(player_buf, &header.player_registry_size);
    mp_ownership_serialize(ownership_buf, &header.ownership_size);
    mp_empire_sync_serialize(empire_buf, &header.empire_sync_size);
    mp_trade_sync_serialize_routes(routes_buf, &header.trade_sync_routes_size);
    mp_trade_sync_serialize_traders(traders_buf, &header.trade_sync_traders_size);
    mp_time_sync_serialize(time_buf, &header.time_sync_size);

    /* Calculate total size */
    uint32_t total = sizeof(mp_save_header)
                   + header.player_registry_size
                   + header.ownership_size
                   + header.empire_sync_size
                   + header.trade_sync_routes_size
                   + header.trade_sync_traders_size
                   + header.time_sync_size;

    if (total > buffer_size) {
        log_error("Save buffer too small", 0, (int)total);
        free(temp);
        return 0;
    }

    /* Write header */
    net_serializer s;
    net_serializer_init(&s, buffer, buffer_size);
    net_write_u32(&s, header.magic);
    net_write_u32(&s, header.version);
    net_write_u32(&s, header.protocol_version);
    net_write_u32(&s, header.session_id);
    net_write_u8(&s, header.host_player_id);
    net_write_u8(&s, header.player_count);
    net_write_u32(&s, header.snapshot_tick);
    net_write_u32(&s, header.checksum);
    net_write_u32(&s, header.player_registry_size);
    net_write_u32(&s, header.ownership_size);
    net_write_u32(&s, header.empire_sync_size);
    net_write_u32(&s, header.trade_sync_routes_size);
    net_write_u32(&s, header.trade_sync_traders_size);
    net_write_u32(&s, header.time_sync_size);

    /* Write domains */
    net_write_raw(&s, player_buf, header.player_registry_size);
    net_write_raw(&s, ownership_buf, header.ownership_size);
    net_write_raw(&s, empire_buf, header.empire_sync_size);
    net_write_raw(&s, routes_buf, header.trade_sync_routes_size);
    net_write_raw(&s, traders_buf, header.trade_sync_traders_size);
    net_write_raw(&s, time_buf, header.time_sync_size);

    *out_size = (uint32_t)net_serializer_position(&s);

    free(temp);
    log_info("Multiplayer session saved", 0, (int)*out_size);
    return 1;
}

int mp_session_load_from_buffer(const uint8_t *buffer, uint32_t size)
{
    mp_save_header header;
    if (!mp_session_save_read_header(buffer, size, &header)) {
        return 0;
    }

    net_serializer s;
    net_serializer_init(&s, (uint8_t *)buffer, size);

    /* Skip past header (we already read it) */
    size_t header_bytes = 4 + 4 + 4 + 4 + 1 + 1 + 4 + 4 + 4 + 4 + 4 + 4 + 4 + 4;
    s.position = header_bytes;

    /* Read each domain */
    if (header.player_registry_size > 0) {
        const uint8_t *data = buffer + s.position;
        mp_player_registry_deserialize(data, header.player_registry_size);
        s.position += header.player_registry_size;
    }

    if (header.ownership_size > 0) {
        const uint8_t *data = buffer + s.position;
        mp_ownership_deserialize(data, header.ownership_size);
        s.position += header.ownership_size;
    }

    if (header.empire_sync_size > 0) {
        const uint8_t *data = buffer + s.position;
        mp_empire_sync_deserialize(data, header.empire_sync_size);
        s.position += header.empire_sync_size;
    }

    if (header.trade_sync_routes_size > 0) {
        const uint8_t *data = buffer + s.position;
        mp_trade_sync_deserialize_routes(data, header.trade_sync_routes_size);
        s.position += header.trade_sync_routes_size;
    }

    if (header.trade_sync_traders_size > 0) {
        const uint8_t *data = buffer + s.position;
        mp_trade_sync_deserialize_traders(data, header.trade_sync_traders_size);
        s.position += header.trade_sync_traders_size;
    }

    if (header.time_sync_size > 0) {
        const uint8_t *data = buffer + s.position;
        mp_time_sync_deserialize(data, header.time_sync_size);
        s.position += header.time_sync_size;
    }

    log_info("Multiplayer session loaded", 0, 0);
    return 1;
}

int mp_session_save_is_multiplayer(const uint8_t *buffer, uint32_t size)
{
    if (size < 4) {
        return 0;
    }
    uint32_t magic = (uint32_t)buffer[0]
                   | ((uint32_t)buffer[1] << 8)
                   | ((uint32_t)buffer[2] << 16)
                   | ((uint32_t)buffer[3] << 24);
    return magic == MP_SAVE_MAGIC;
}

int mp_session_save_read_header(const uint8_t *buffer, uint32_t size, mp_save_header *header)
{
    if (size < sizeof(mp_save_header)) {
        return 0;
    }

    net_serializer s;
    net_serializer_init(&s, (uint8_t *)buffer, size);

    header->magic = net_read_u32(&s);
    header->version = net_read_u32(&s);
    header->protocol_version = net_read_u32(&s);
    header->session_id = net_read_u32(&s);
    header->host_player_id = net_read_u8(&s);
    header->player_count = net_read_u8(&s);
    header->snapshot_tick = net_read_u32(&s);
    header->checksum = net_read_u32(&s);
    header->player_registry_size = net_read_u32(&s);
    header->ownership_size = net_read_u32(&s);
    header->empire_sync_size = net_read_u32(&s);
    header->trade_sync_routes_size = net_read_u32(&s);
    header->trade_sync_traders_size = net_read_u32(&s);
    header->time_sync_size = net_read_u32(&s);

    if (header->magic != MP_SAVE_MAGIC) {
        return 0;
    }
    if (header->version > MP_SAVE_VERSION) {
        log_error("Unsupported multiplayer save version", 0, (int)header->version);
        return 0;
    }

    return 1;
}

#endif /* ENABLE_MULTIPLAYER */

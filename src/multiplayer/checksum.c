#include "checksum.h"

#ifdef ENABLE_MULTIPLAYER

#include "ownership.h"
#include "player_registry.h"
#include "network/session.h"
#include "network/serialize.h"
#include "network/protocol.h"
#include "empire/city.h"
#include "empire/trade_route.h"
#include "game/time.h"
#include "game/resource.h"
#include "core/log.h"

#include <string.h>

static struct {
    uint32_t last_check_tick;
    uint32_t host_checksum;
    int has_desync;
    uint8_t desynced_player;
} checksum_data;

void mp_checksum_init(void)
{
    memset(&checksum_data, 0, sizeof(checksum_data));
}

/* FNV-1a hash */
static uint32_t fnv1a_init(void)
{
    return 2166136261u;
}

static uint32_t fnv1a_update(uint32_t hash, const void *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    for (size_t i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

static uint32_t fnv1a_u32(uint32_t hash, uint32_t value)
{
    return fnv1a_update(hash, &value, sizeof(value));
}

static uint32_t fnv1a_i32(uint32_t hash, int32_t value)
{
    return fnv1a_update(hash, &value, sizeof(value));
}

uint32_t mp_checksum_compute(void)
{
    uint32_t hash = fnv1a_init();

    /* Hash game tick */
    hash = fnv1a_u32(hash, net_session_get_authoritative_tick());

    /* Hash game time */
    hash = fnv1a_i32(hash, game_time_year());
    hash = fnv1a_i32(hash, game_time_month());
    hash = fnv1a_i32(hash, game_time_day());

    /* Hash empire city states */
    int array_size = empire_city_get_array_size();
    for (int i = 0; i < array_size; i++) {
        empire_city *city = empire_city_get(i);
        if (!city || !city->in_use) {
            continue;
        }
        hash = fnv1a_i32(hash, city->is_open);
        hash = fnv1a_i32(hash, city->route_id);
        hash = fnv1a_i32(hash, (int32_t)city->type);

        /* Hash city ownership */
        hash = fnv1a_i32(hash, (int32_t)mp_ownership_get_city_owner_type(i));
        hash = fnv1a_u32(hash, mp_ownership_get_city_player_id(i));
    }

    /* Hash trade routes */
    int route_count = trade_route_count();
    for (int r = 0; r < route_count; r++) {
        if (!trade_route_is_valid(r)) {
            continue;
        }
        for (int res = RESOURCE_MIN; res < RESOURCE_MAX; res++) {
            /* Buys */
            hash = fnv1a_i32(hash, trade_route_limit(r, res, 0));
            hash = fnv1a_i32(hash, trade_route_traded(r, res, 0));
            /* Sells */
            hash = fnv1a_i32(hash, trade_route_limit(r, res, 1));
            hash = fnv1a_i32(hash, trade_route_traded(r, res, 1));
        }
    }

    return hash;
}

void mp_checksum_request_from_clients(uint32_t tick)
{
    checksum_data.host_checksum = mp_checksum_compute();
    checksum_data.last_check_tick = tick;
    checksum_data.has_desync = 0;

    uint8_t buf[8];
    net_serializer s;
    net_serializer_init(&s, buf, sizeof(buf));
    net_write_u32(&s, tick);
    net_write_u32(&s, checksum_data.host_checksum);

    net_session_broadcast(NET_MSG_CHECKSUM_REQUEST, buf, (uint32_t)net_serializer_position(&s));
}

void multiplayer_checksum_receive_response(uint8_t player_id,
                                            const uint8_t *data, uint32_t size)
{
    net_serializer s;
    net_serializer_init(&s, (uint8_t *)data, size);

    uint32_t tick = net_read_u32(&s);
    uint32_t client_checksum = net_read_u32(&s);
    net_read_u8(&s); /* player_id in payload - use parameter instead */

    if (tick != checksum_data.last_check_tick) {
        return; /* Stale response */
    }

    if (client_checksum != checksum_data.host_checksum) {
        checksum_data.has_desync = 1;
        checksum_data.desynced_player = player_id;
        log_error("DESYNC detected for player", 0, player_id);
        log_error("Host checksum vs client", 0,
                 (int)(checksum_data.host_checksum ^ client_checksum));
    } else {
        mp_player *p = mp_player_registry_get(player_id);
        if (p) {
            p->last_checksum = client_checksum;
            p->last_checksum_tick = tick;
        }
    }
}

void multiplayer_checksum_handle_request(const uint8_t *data, uint32_t size)
{
    net_serializer s;
    net_serializer_init(&s, (uint8_t *)data, size);

    uint32_t tick = net_read_u32(&s);
    uint32_t host_checksum = net_read_u32(&s);
    (void)host_checksum;

    uint32_t local_checksum = mp_checksum_compute();

    uint8_t resp[16];
    net_serializer rs;
    net_serializer_init(&rs, resp, sizeof(resp));
    net_write_u32(&rs, tick);
    net_write_u32(&rs, local_checksum);
    net_write_u8(&rs, net_session_get_local_player_id());

    net_session_send_to_host(NET_MSG_CHECKSUM_RESPONSE, resp,
                            (uint32_t)net_serializer_position(&rs));
}

int mp_checksum_should_check(uint32_t current_tick)
{
    return (current_tick > 0) &&
           (current_tick - checksum_data.last_check_tick >= NET_CHECKSUM_INTERVAL_TICKS);
}

int mp_checksum_has_desync(void)
{
    return checksum_data.has_desync;
}

uint8_t mp_checksum_desynced_player(void)
{
    return checksum_data.desynced_player;
}

#endif /* ENABLE_MULTIPLAYER */

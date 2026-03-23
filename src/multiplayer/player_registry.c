#include "player_registry.h"

#ifdef ENABLE_MULTIPLAYER

#include "network/serialize.h"
#include "core/log.h"

#include <string.h>

static struct {
    mp_player players[MP_MAX_PLAYERS];
    int count;
} registry;

void mp_player_registry_init(void)
{
    memset(&registry, 0, sizeof(registry));
}

void mp_player_registry_clear(void)
{
    mp_player_registry_init();
}

int mp_player_registry_add(uint8_t player_id, const char *name, int is_local, int is_host)
{
    if (player_id >= MP_MAX_PLAYERS) {
        log_error("Player ID out of range", 0, player_id);
        return 0;
    }
    if (registry.players[player_id].active) {
        log_error("Player slot already occupied", 0, player_id);
        return 0;
    }

    mp_player *p = &registry.players[player_id];
    memset(p, 0, sizeof(mp_player));
    p->active = 1;
    p->player_id = player_id;
    p->is_local = is_local;
    p->is_host = is_host;
    p->status = MP_PLAYER_STATUS_LOBBY;
    p->empire_city_id = -1;
    p->scenario_city_slot = -1;

    if (name) {
        strncpy(p->name, name, sizeof(p->name) - 1);
        p->name[sizeof(p->name) - 1] = '\0';
    }

    registry.count++;
    log_info("Player registered", name, player_id);
    return 1;
}

void mp_player_registry_remove(uint8_t player_id)
{
    if (player_id >= MP_MAX_PLAYERS) {
        return;
    }
    if (registry.players[player_id].active) {
        log_info("Player removed", registry.players[player_id].name, player_id);
        registry.players[player_id].active = 0;
        registry.count--;
    }
}

mp_player *mp_player_registry_get(uint8_t player_id)
{
    if (player_id >= MP_MAX_PLAYERS) {
        return NULL;
    }
    if (!registry.players[player_id].active) {
        return NULL;
    }
    return &registry.players[player_id];
}

mp_player *mp_player_registry_get_local(void)
{
    for (int i = 0; i < MP_MAX_PLAYERS; i++) {
        if (registry.players[i].active && registry.players[i].is_local) {
            return &registry.players[i];
        }
    }
    return NULL;
}

mp_player *mp_player_registry_get_host(void)
{
    for (int i = 0; i < MP_MAX_PLAYERS; i++) {
        if (registry.players[i].active && registry.players[i].is_host) {
            return &registry.players[i];
        }
    }
    return NULL;
}

int mp_player_registry_get_count(void)
{
    return registry.count;
}

void mp_player_registry_set_status(uint8_t player_id, mp_player_status status)
{
    mp_player *p = mp_player_registry_get(player_id);
    if (p) {
        p->status = status;
    }
}

void mp_player_registry_set_city(uint8_t player_id, int empire_city_id)
{
    mp_player *p = mp_player_registry_get(player_id);
    if (p) {
        p->empire_city_id = empire_city_id;
    }
}

void mp_player_registry_set_slot(uint8_t player_id, int scenario_city_slot)
{
    mp_player *p = mp_player_registry_get(player_id);
    if (p) {
        p->scenario_city_slot = scenario_city_slot;
    }
}

int mp_player_registry_get_first_active_id(void)
{
    for (int i = 0; i < MP_MAX_PLAYERS; i++) {
        if (registry.players[i].active) {
            return i;
        }
    }
    return -1;
}

int mp_player_registry_get_next_active_id(int after_id)
{
    for (int i = after_id + 1; i < MP_MAX_PLAYERS; i++) {
        if (registry.players[i].active) {
            return i;
        }
    }
    return -1;
}

void mp_player_registry_serialize(uint8_t *buffer, uint32_t *size)
{
    net_serializer s;
    net_serializer_init(&s, buffer, 4096);

    net_write_u8(&s, (uint8_t)registry.count);

    for (int i = 0; i < MP_MAX_PLAYERS; i++) {
        mp_player *p = &registry.players[i];
        net_write_u8(&s, (uint8_t)p->active);
        if (!p->active) {
            continue;
        }
        net_write_u8(&s, p->player_id);
        net_write_raw(&s, p->name, 32);
        net_write_u8(&s, p->peer_id);
        net_write_u8(&s, (uint8_t)p->status);
        net_write_i32(&s, p->empire_city_id);
        net_write_i32(&s, p->scenario_city_slot);
        net_write_u8(&s, (uint8_t)p->is_host);
    }

    *size = (uint32_t)net_serializer_position(&s);
}

void mp_player_registry_deserialize(const uint8_t *buffer, uint32_t size)
{
    mp_player_registry_clear();

    net_serializer s;
    net_serializer_init(&s, (uint8_t *)buffer, size);

    uint8_t count = net_read_u8(&s);
    (void)count;

    for (int i = 0; i < MP_MAX_PLAYERS; i++) {
        uint8_t active = net_read_u8(&s);
        if (!active) {
            continue;
        }

        mp_player *p = &registry.players[i];
        p->active = 1;
        p->player_id = net_read_u8(&s);
        net_read_raw(&s, p->name, 32);
        p->name[31] = '\0';
        p->peer_id = net_read_u8(&s);
        p->status = (mp_player_status)net_read_u8(&s);
        p->empire_city_id = net_read_i32(&s);
        p->scenario_city_slot = net_read_i32(&s);
        p->is_host = net_read_u8(&s);
        p->is_local = 0; /* Will be set by the receiver */
        registry.count++;
    }
}

#endif /* ENABLE_MULTIPLAYER */

#include "player_registry.h"

#ifdef ENABLE_MULTIPLAYER

#include "network/serialize.h"
#include "core/log.h"
#include "core/random.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static struct {
    mp_player players[MP_MAX_PLAYERS];
    int count;
    uint8_t slot_assigned[MP_MAX_PLAYERS]; /* 1 if slot is taken */
} registry;

/* ---- UUID generation ---- */

void mp_player_registry_generate_uuid(uint8_t *out_uuid)
{
    /* Generate 128-bit pseudo-random UUID (version 4 variant 1).
     * Uses core/random pool combined with time-based entropy. */
    for (int i = 0; i < MP_PLAYER_UUID_SIZE; i++) {
        out_uuid[i] = (uint8_t)(random_from_pool(i) ^ (uint8_t)(rand() & 0xFF));
    }
    /* Set version 4 (random) */
    out_uuid[6] = (out_uuid[6] & 0x0F) | 0x40;
    /* Set variant 1 */
    out_uuid[8] = (out_uuid[8] & 0x3F) | 0x80;
}

int mp_player_registry_uuid_equals(const uint8_t *a, const uint8_t *b)
{
    return memcmp(a, b, MP_PLAYER_UUID_SIZE) == 0;
}

void mp_player_registry_uuid_to_string(const uint8_t *uuid, char *out, int out_size)
{
    if (out_size < 37) {
        if (out_size > 0) {
            out[0] = '\0';
        }
        return;
    }
    snprintf(out, out_size,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid[0], uuid[1], uuid[2], uuid[3],
        uuid[4], uuid[5], uuid[6], uuid[7],
        uuid[8], uuid[9], uuid[10], uuid[11],
        uuid[12], uuid[13], uuid[14], uuid[15]);
}

static void generate_reconnect_token(uint8_t *token)
{
    for (int i = 0; i < MP_RECONNECT_TOKEN_SIZE; i++) {
        token[i] = (uint8_t)(random_from_pool(i + 16) ^ (uint8_t)(rand() & 0xFF));
    }
}

/* ---- Init / Clear ---- */

void mp_player_registry_init(void)
{
    memset(&registry, 0, sizeof(registry));
    for (int i = 0; i < MP_MAX_PLAYERS; i++) {
        registry.players[i].empire_city_id = -1;
        registry.players[i].scenario_city_slot = -1;
        registry.players[i].assigned_city_id = -1;
    }
}

void mp_player_registry_clear(void)
{
    mp_player_registry_init();
}

/* ---- Add / Remove ---- */

int mp_player_registry_add(uint8_t player_id, const char *name, int is_local, int is_host)
{
    uint8_t uuid[MP_PLAYER_UUID_SIZE];
    mp_player_registry_generate_uuid(uuid);
    return mp_player_registry_add_with_uuid(player_id, name, uuid, is_local, is_host);
}

int mp_player_registry_add_with_uuid(uint8_t player_id, const char *name,
                                      const uint8_t *uuid, int is_local, int is_host)
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
    p->slot_id = 0xFF; /* Unassigned until explicitly set */
    p->is_local = is_local;
    p->is_host = is_host;
    p->status = MP_PLAYER_LOBBY;
    p->connection_state = MP_CONNECTION_CONNECTED;
    p->empire_city_id = -1;
    p->scenario_city_slot = -1;
    p->assigned_city_id = -1;

    memcpy(p->player_uuid, uuid, MP_PLAYER_UUID_SIZE);
    generate_reconnect_token(p->reconnect_token);

    if (name) {
        strncpy(p->name, name, MP_PLAYER_NAME_SIZE - 1);
        p->name[MP_PLAYER_NAME_SIZE - 1] = '\0';
        strncpy(p->display_name, name, MP_PLAYER_NAME_SIZE - 1);
        p->display_name[MP_PLAYER_NAME_SIZE - 1] = '\0';
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
    mp_player *p = &registry.players[player_id];
    if (p->active) {
        log_info("Player removed", p->name, player_id);
        /* Free slot if assigned */
        if (p->slot_id < MP_MAX_PLAYERS) {
            registry.slot_assigned[p->slot_id] = 0;
        }
        memset(p, 0, sizeof(mp_player));
        p->empire_city_id = -1;
        p->scenario_city_slot = -1;
        p->assigned_city_id = -1;
        registry.count--;
    }
}

/* ---- Lookups ---- */

mp_player *mp_player_registry_get(uint8_t player_id)
{
    if (player_id >= MP_MAX_PLAYERS) {
        return 0;
    }
    if (!registry.players[player_id].active) {
        return 0;
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
    return 0;
}

mp_player *mp_player_registry_get_host(void)
{
    for (int i = 0; i < MP_MAX_PLAYERS; i++) {
        if (registry.players[i].active && registry.players[i].is_host) {
            return &registry.players[i];
        }
    }
    return 0;
}

mp_player *mp_player_registry_get_by_uuid(const uint8_t *uuid)
{
    for (int i = 0; i < MP_MAX_PLAYERS; i++) {
        if (registry.players[i].active &&
            mp_player_registry_uuid_equals(registry.players[i].player_uuid, uuid)) {
            return &registry.players[i];
        }
    }
    /* Also check disconnected/awaiting slots */
    for (int i = 0; i < MP_MAX_PLAYERS; i++) {
        mp_player *p = &registry.players[i];
        if (p->status == MP_PLAYER_AWAITING_RECONNECT &&
            mp_player_registry_uuid_equals(p->player_uuid, uuid)) {
            return p;
        }
    }
    return 0;
}

mp_player *mp_player_registry_get_by_slot(uint8_t slot_id)
{
    for (int i = 0; i < MP_MAX_PLAYERS; i++) {
        if (registry.players[i].active && registry.players[i].slot_id == slot_id) {
            return &registry.players[i];
        }
    }
    return 0;
}

int mp_player_registry_get_count(void)
{
    return registry.count;
}

/* ---- Setters ---- */

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

void mp_player_registry_set_connection_state(uint8_t player_id, mp_connection_state state)
{
    mp_player *p = mp_player_registry_get(player_id);
    if (p) {
        p->connection_state = state;
    }
}

void mp_player_registry_set_assigned_city(uint8_t player_id, int city_id)
{
    mp_player *p = mp_player_registry_get(player_id);
    if (p) {
        p->assigned_city_id = city_id;
    }
}

/* ---- Slot Management ---- */

int mp_player_registry_find_free_slot(void)
{
    for (int i = 0; i < MP_MAX_PLAYERS; i++) {
        if (!registry.slot_assigned[i]) {
            return i;
        }
    }
    return -1;
}

int mp_player_registry_assign_slot(uint8_t player_id)
{
    mp_player *p = mp_player_registry_get(player_id);
    if (!p) {
        return -1;
    }

    /* Already has a slot */
    if (p->slot_id < MP_MAX_PLAYERS) {
        return p->slot_id;
    }

    int slot = mp_player_registry_find_free_slot();
    if (slot < 0) {
        log_error("No free player slots available", 0, 0);
        return -1;
    }

    p->slot_id = (uint8_t)slot;
    registry.slot_assigned[slot] = 1;
    return slot;
}

/* ---- Reconnect Support ---- */

void mp_player_registry_generate_reconnect_token(uint8_t player_id)
{
    mp_player *p = mp_player_registry_get(player_id);
    if (p) {
        generate_reconnect_token(p->reconnect_token);
    }
}

int mp_player_registry_validate_reconnect(const uint8_t *uuid, const uint8_t *token)
{
    mp_player *p = mp_player_registry_get_by_uuid(uuid);
    if (!p) {
        return 0;
    }
    if (p->status != MP_PLAYER_DISCONNECTED &&
        p->status != MP_PLAYER_AWAITING_RECONNECT) {
        return 0;
    }
    return memcmp(p->reconnect_token, token, MP_RECONNECT_TOKEN_SIZE) == 0;
}

void mp_player_registry_mark_disconnected(uint8_t player_id, uint32_t current_tick,
                                           uint32_t timeout_ticks)
{
    mp_player *p = mp_player_registry_get(player_id);
    if (!p) {
        return;
    }

    p->status = MP_PLAYER_AWAITING_RECONNECT;
    p->connection_state = MP_CONNECTION_DISCONNECTED;
    p->disconnect_tick = current_tick;
    p->reconnect_deadline_tick = current_tick + timeout_ticks;

    char uuid_str[40];
    mp_player_registry_uuid_to_string(p->player_uuid, uuid_str, sizeof(uuid_str));
    log_info("Player marked for reconnect", uuid_str, player_id);
}

int mp_player_registry_handle_reconnect(const uint8_t *uuid, uint8_t peer_id)
{
    mp_player *p = mp_player_registry_get_by_uuid(uuid);
    if (!p) {
        return -1;
    }
    if (p->status != MP_PLAYER_DISCONNECTED &&
        p->status != MP_PLAYER_AWAITING_RECONNECT) {
        return -1;
    }

    p->peer_id = peer_id;
    p->connection_state = MP_CONNECTION_RECONNECTING;
    p->status = MP_PLAYER_IN_GAME;

    char uuid_str[40];
    mp_player_registry_uuid_to_string(p->player_uuid, uuid_str, sizeof(uuid_str));
    log_info("Player reconnected", uuid_str, p->player_id);
    return p->player_id;
}

void mp_player_registry_cleanup_expired(uint32_t current_tick)
{
    for (int i = 0; i < MP_MAX_PLAYERS; i++) {
        mp_player *p = &registry.players[i];
        if (!p->active) {
            continue;
        }
        if (p->status != MP_PLAYER_AWAITING_RECONNECT) {
            continue;
        }
        if (current_tick >= p->reconnect_deadline_tick) {
            log_info("Reconnect timeout, removing player", p->name, p->player_id);
            mp_player_registry_remove(p->player_id);
        }
    }
}

/* ---- Iteration ---- */

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

/* ---- Serialization ---- */

void mp_player_registry_serialize(uint8_t *buffer, uint32_t *size)
{
    net_serializer s;
    net_serializer_init(&s, buffer, 8192);

    net_write_u8(&s, (uint8_t)registry.count);

    for (int i = 0; i < MP_MAX_PLAYERS; i++) {
        mp_player *p = &registry.players[i];
        net_write_u8(&s, (uint8_t)p->active);
        if (!p->active) {
            continue;
        }

        /* Core identity */
        net_write_u8(&s, p->player_id);
        net_write_u8(&s, p->slot_id);
        net_write_raw(&s, p->player_uuid, MP_PLAYER_UUID_SIZE);
        net_write_raw(&s, p->name, MP_PLAYER_NAME_SIZE);
        net_write_raw(&s, p->display_name, MP_PLAYER_NAME_SIZE);

        /* Role */
        net_write_u8(&s, (uint8_t)p->is_host);

        /* State */
        net_write_u8(&s, p->peer_id);
        net_write_u8(&s, (uint8_t)p->status);
        net_write_u8(&s, (uint8_t)p->connection_state);
        net_write_u32(&s, p->disconnect_tick);
        net_write_u32(&s, p->reconnect_deadline_tick);
        net_write_raw(&s, p->reconnect_token, MP_RECONNECT_TOKEN_SIZE);

        /* City binding */
        net_write_i32(&s, p->empire_city_id);
        net_write_i32(&s, p->scenario_city_slot);
        net_write_i32(&s, p->assigned_city_id);

        /* Checksum */
        net_write_u32(&s, p->last_checksum);
        net_write_u32(&s, p->last_checksum_tick);

        /* Stats */
        net_write_u32(&s, p->commands_sent);
        net_write_u32(&s, p->commands_rejected);
        net_write_u32(&s, p->resyncs_requested);
    }

    /* Slot assignment table */
    for (int i = 0; i < MP_MAX_PLAYERS; i++) {
        net_write_u8(&s, registry.slot_assigned[i]);
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

        /* Core identity */
        p->player_id = net_read_u8(&s);
        p->slot_id = net_read_u8(&s);
        net_read_raw(&s, p->player_uuid, MP_PLAYER_UUID_SIZE);
        net_read_raw(&s, p->name, MP_PLAYER_NAME_SIZE);
        p->name[MP_PLAYER_NAME_SIZE - 1] = '\0';
        net_read_raw(&s, p->display_name, MP_PLAYER_NAME_SIZE);
        p->display_name[MP_PLAYER_NAME_SIZE - 1] = '\0';

        /* Role */
        p->is_host = net_read_u8(&s);
        p->is_local = 0; /* Will be set by receiver */

        /* State */
        p->peer_id = net_read_u8(&s);
        p->status = (mp_player_status)net_read_u8(&s);
        p->connection_state = (mp_connection_state)net_read_u8(&s);
        p->disconnect_tick = net_read_u32(&s);
        p->reconnect_deadline_tick = net_read_u32(&s);
        net_read_raw(&s, p->reconnect_token, MP_RECONNECT_TOKEN_SIZE);

        /* City binding */
        p->empire_city_id = net_read_i32(&s);
        p->scenario_city_slot = net_read_i32(&s);
        p->assigned_city_id = net_read_i32(&s);

        /* Checksum */
        p->last_checksum = net_read_u32(&s);
        p->last_checksum_tick = net_read_u32(&s);

        /* Stats */
        p->commands_sent = net_read_u32(&s);
        p->commands_rejected = net_read_u32(&s);
        p->resyncs_requested = net_read_u32(&s);

        registry.count++;
    }

    /* Slot assignment table */
    for (int i = 0; i < MP_MAX_PLAYERS; i++) {
        registry.slot_assigned[i] = net_read_u8(&s);
    }
}

#endif /* ENABLE_MULTIPLAYER */

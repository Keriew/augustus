#include "session.h"

#ifdef ENABLE_MULTIPLAYER

#include "transport_tcp.h"
#include "transport_udp.h"
#include "serialize.h"
#include "discovery_lan.h"
#include "multiplayer/player_registry.h"
#include "multiplayer/ownership.h"
#include "multiplayer/empire_sync.h"
#include "multiplayer/worldgen.h"
#include "multiplayer/mp_debug_log.h"
#include "core/config.h"
#include "core/log.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>

static net_session session;

/* Persistent player name — survives session init/shutdown cycles */
static char persistent_player_name[NET_MAX_PLAYER_NAME] = "Player";
static int persistent_name_set;

/* Join status tracking for UI feedback */
static net_join_status join_status;
static uint8_t join_reject_reason;
static uint32_t join_start_ms;
static uint32_t join_correlation_id; /* Unique ID per join attempt for log correlation */

#define NET_JOIN_TIMEOUT_MS 10000 /* 10 seconds to complete handshake */

/* Chat message ring buffer for client-side display */
#define MP_CHAT_HISTORY_SIZE 64
#define MP_CHAT_MESSAGE_MAX 128

typedef struct {
    uint8_t sender_id;
    char message[MP_CHAT_MESSAGE_MAX];
    uint32_t timestamp_tick;
} mp_chat_entry;

static struct {
    mp_chat_entry entries[MP_CHAT_HISTORY_SIZE];
    int write_index;
    int count;
    int unread;
} chat_history;

static uint32_t generate_session_id(void)
{
    srand((unsigned int)time(NULL));
    return (uint32_t)rand() ^ ((uint32_t)rand() << 16);
}

static int find_free_peer_slot(void)
{
    for (int i = 0; i < NET_MAX_PEERS; i++) {
        if (!session.peers[i].active) {
            return i;
        }
    }
    return -1;
}

static void close_peer(int index)
{
    if (index < 0 || index >= NET_MAX_PEERS) {
        return;
    }
    net_peer *peer = &session.peers[index];
    if (!peer->active) {
        return;
    }
    net_tcp_close(peer->socket_fd);
    net_peer_reset(peer);
    session.peer_count--;
}

static void handle_peer_disconnect(int peer_index)
{
    net_peer *peer = &session.peers[peer_index];
    if (!peer->active) {
        return;
    }

    uint8_t player_id = peer->player_id;

    /* Mark player as disconnected in registry */
    mp_player *player = mp_player_registry_get(player_id);
    if (player && player->active) {
        uint32_t timeout_ticks = 3000; /* ~5 minutes at normal speed */
        mp_player_registry_mark_disconnected(player_id,
            session.authoritative_tick, timeout_ticks);

        /* Set all routes involving this player to offline */
        mp_ownership_set_player_routes_offline(player_id);

        /* Mark city offline in empire sync */
        mp_empire_sync_unregister_player_city(player_id);
        mp_ownership_set_city_online(player->assigned_city_id, 0);

        /* Broadcast player disconnected event to remaining peers */
        uint8_t event_buf[32];
        net_serializer es;
        net_serializer_init(&es, event_buf, sizeof(event_buf));
        net_write_u16(&es, NET_EVENT_PLAYER_LEFT);
        net_write_u32(&es, session.authoritative_tick);
        net_write_u8(&es, player_id);
        net_session_broadcast(NET_MSG_HOST_EVENT, event_buf,
                              (uint32_t)net_serializer_position(&es));

        log_info("Player disconnected, city frozen", peer->name, (int)player_id);
    }

    close_peer(peer_index);
}

static void send_raw_to_peer(net_peer *peer, uint16_t message_type,
                             const uint8_t *payload, uint32_t size)
{
    size_t encoded = net_codec_encode(&peer->codec,
                                      message_type,
                                      session.session_id,
                                      session.authoritative_tick,
                                      payload, size,
                                      peer->send_buffer,
                                      sizeof(peer->send_buffer));
    if (encoded == 0) {
        log_error("Failed to encode packet", net_protocol_message_name(message_type), 0);
        return;
    }

    int sent = net_tcp_send(peer->socket_fd, peer->send_buffer, encoded);
    if (sent < 0) {
        log_error("Failed to send to peer", peer->name, 0);
        return;
    }
    peer->bytes_sent += sent;
    peer->packets_sent++;
}

static int try_reconnect_player(net_peer *peer, const uint8_t *uuid,
                                const uint8_t *reconnect_token)
{
    /* Check if a player with this UUID is awaiting reconnect */
    mp_player *existing = mp_player_registry_get_by_uuid(uuid);
    if (!existing || !existing->active) {
        return 0;
    }
    if (existing->status != MP_PLAYER_AWAITING_RECONNECT &&
        existing->connection_state != MP_CONNECTION_DISCONNECTED) {
        return 0;
    }

    /* Validate reconnect token */
    if (!mp_player_registry_validate_reconnect(uuid, reconnect_token)) {
        log_error("Invalid reconnect token", 0, 0);
        return 0;
    }

    /* Reconnect the player */
    uint8_t old_player_id = existing->player_id;
    int peer_index = -1;
    for (int i = 0; i < NET_MAX_PEERS; i++) {
        if (&session.peers[i] == peer) {
            peer_index = i;
            break;
        }
    }
    if (peer_index < 0) {
        return 0;
    }

    mp_player_registry_handle_reconnect(uuid, (uint8_t)peer_index);

    /* Restore routes */
    mp_ownership_set_player_routes_online(old_player_id);

    /* Mark city back online */
    if (existing->assigned_city_id >= 0) {
        mp_ownership_set_city_online(existing->assigned_city_id, 1);
    }

    /* Update peer state */
    strncpy(peer->name, existing->name, NET_MAX_PLAYER_NAME - 1);
    peer->name[NET_MAX_PLAYER_NAME - 1] = '\0';
    net_peer_set_player_id(peer, old_player_id);
    peer->state = PEER_STATE_IN_GAME;

    log_info("Player reconnected", existing->name, (int)old_player_id);

    /* Send JOIN_ACCEPT with reconnect info */
    uint8_t accept_buf[128];
    net_serializer as;
    net_serializer_init(&as, accept_buf, sizeof(accept_buf));
    net_write_u8(&as, old_player_id);
    net_write_u8(&as, existing->slot_id);
    net_write_u32(&as, session.session_id);
    net_write_u32(&as, mp_worldgen_get_spawn_table_mutable()->session_seed);
    net_write_u8(&as, (uint8_t)(session.peer_count + 1));
    net_write_raw(&as, existing->player_uuid, MP_PLAYER_UUID_SIZE);
    net_write_raw(&as, existing->reconnect_token, MP_RECONNECT_TOKEN_SIZE);
    send_raw_to_peer(peer, NET_MSG_JOIN_ACCEPT, accept_buf,
                     (uint32_t)net_serializer_position(&as));

    /* Broadcast reconnect event */
    uint8_t event_buf[32];
    net_serializer es;
    net_serializer_init(&es, event_buf, sizeof(event_buf));
    net_write_u16(&es, NET_EVENT_PLAYER_RECONNECTED);
    net_write_u32(&es, session.authoritative_tick);
    net_write_u8(&es, old_player_id);
    net_session_broadcast(NET_MSG_HOST_EVENT, event_buf,
                          (uint32_t)net_serializer_position(&es));

    /* Send full snapshot to reconnecting player so they can resync */
    extern void multiplayer_resync_handle_request(uint8_t player_id);
    multiplayer_resync_handle_request(old_player_id);

    return 1;
}

static void handle_hello(net_peer *peer, const uint8_t *payload, uint32_t size)
{
    /* Minimum HELLO size: magic(4) + version(2) + name(32) = 38 bytes */
    if (size < 38) {
        MP_LOG_ERROR("HANDSHAKE", "HELLO too small: %u bytes (minimum 38)", size);
        log_error("HELLO packet too small", 0, (int)size);
        return;
    }

    net_serializer s;
    net_serializer_init(&s, (uint8_t *)payload, size);

    /* Read extended hello */
    uint32_t magic = net_read_u32(&s);
    uint16_t version = net_read_u16(&s);
    uint32_t save_version = net_read_u32(&s);
    uint32_t map_hash = net_read_u32(&s);
    uint32_t scenario_hash = net_read_u32(&s);
    uint32_t feature_flags = net_read_u32(&s);
    char name[NET_MAX_PLAYER_NAME];
    net_read_raw(&s, name, NET_MAX_PLAYER_NAME);
    name[NET_MAX_PLAYER_NAME - 1] = '\0';
    uint8_t player_uuid[16];
    net_read_raw(&s, player_uuid, 16);
    uint8_t reconnect_token[16];
    net_read_raw(&s, reconnect_token, 16);

    /* Suppress unused variable warnings for forward compatibility */
    (void)save_version;
    (void)map_hash;
    (void)scenario_hash;
    (void)feature_flags;

    if (net_serializer_has_overflow(&s)) {
        /* Fall back to legacy hello (magic + version + name only) */
        net_serializer_init(&s, (uint8_t *)payload, size);
        magic = net_read_u32(&s);
        version = net_read_u16(&s);
        net_read_raw(&s, name, NET_MAX_PLAYER_NAME);
        name[NET_MAX_PLAYER_NAME - 1] = '\0';
        memset(player_uuid, 0, 16);
        memset(reconnect_token, 0, 16);

        if (net_serializer_has_overflow(&s)) {
            MP_LOG_ERROR("HANDSHAKE", "Malformed HELLO: payload size=%u, overflow after legacy parse", size);
            log_error("Malformed HELLO from peer", 0, 0);
            return;
        }
    }

    /* Defensive: trim trailing spaces and ensure valid name */
    {
        int len = (int)strlen(name);
        while (len > 0 && (name[len - 1] == ' ' || name[len - 1] == '\0')) {
            name[--len] = '\0';
        }
        if (len == 0) {
            strncpy(name, "Unknown", NET_MAX_PLAYER_NAME - 1);
        }
    }

    MP_LOG_INFO("HANDSHAKE", "HELLO received: name='%s' magic=0x%08x version=%d "
                "save_ver=%u map_hash=0x%08x scenario_hash=0x%08x flags=0x%08x",
                name, magic, (int)version, save_version, map_hash, scenario_hash, feature_flags);

    if (magic != NET_MAGIC) {
        MP_LOG_ERROR("HANDSHAKE", "REJECT: invalid magic 0x%08x (expected 0x%08x)", magic, NET_MAGIC);
        log_error("Invalid magic in HELLO", 0, 0);
        uint8_t reject_buf[2];
        net_serializer rs;
        net_serializer_init(&rs, reject_buf, sizeof(reject_buf));
        net_write_u8(&rs, NET_REJECT_VERSION_MISMATCH);
        send_raw_to_peer(peer, NET_MSG_JOIN_REJECT, reject_buf,
                         (uint32_t)net_serializer_position(&rs));
        return;
    }

    if (!net_protocol_check_version(version)) {
        MP_LOG_ERROR("HANDSHAKE", "REJECT: version mismatch, remote=%d local=%d",
                     (int)version, NET_PROTOCOL_VERSION);
        log_error("Protocol version mismatch", 0, (int)version);
        uint8_t reject_buf[2];
        net_serializer rs;
        net_serializer_init(&rs, reject_buf, sizeof(reject_buf));
        net_write_u8(&rs, NET_REJECT_VERSION_MISMATCH);
        send_raw_to_peer(peer, NET_MSG_JOIN_REJECT, reject_buf,
                         (uint32_t)net_serializer_position(&rs));
        return;
    }

    /* If game is in progress, try reconnect by UUID */
    if (session.state == NET_SESSION_HOSTING_GAME) {
        /* Check for non-zero UUID */
        int has_uuid = 0;
        for (int i = 0; i < 16; i++) {
            if (player_uuid[i] != 0) {
                has_uuid = 1;
                break;
            }
        }

        MP_LOG_INFO("HANDSHAKE", "Game in progress, has_uuid=%d — attempting reconnect for '%s'",
                    has_uuid, name);

        if (has_uuid && try_reconnect_player(peer, player_uuid, reconnect_token)) {
            MP_LOG_INFO("HANDSHAKE", "Reconnect succeeded for '%s'", name);
            return; /* Reconnect succeeded */
        }

        /* No reconnect possible — reject */
        MP_LOG_WARN("HANDSHAKE", "REJECT: game in progress, no valid UUID for reconnect (name='%s')", name);
        uint8_t reject_buf[2];
        net_serializer rs;
        net_serializer_init(&rs, reject_buf, sizeof(reject_buf));
        net_write_u8(&rs, NET_REJECT_GAME_IN_PROGRESS);
        send_raw_to_peer(peer, NET_MSG_JOIN_REJECT, reject_buf,
                         (uint32_t)net_serializer_position(&rs));
        return;
    }

    /* Check for duplicate name */
    for (int i = 0; i < NET_MAX_PEERS; i++) {
        if (session.peers[i].active && session.peers[i].player_id != peer->player_id) {
            if (strncmp(session.peers[i].name, name, NET_MAX_PLAYER_NAME - 1) == 0) {
                uint8_t reject_buf[2];
                net_serializer rs;
                net_serializer_init(&rs, reject_buf, sizeof(reject_buf));
                net_write_u8(&rs, NET_REJECT_NAME_TAKEN);
                send_raw_to_peer(peer, NET_MSG_JOIN_REJECT, reject_buf,
                                 (uint32_t)net_serializer_position(&rs));
                return;
            }
        }
    }
    /* Also check against host name */
    if (strncmp(session.local_player_name, name, NET_MAX_PLAYER_NAME - 1) == 0) {
        MP_LOG_WARN("HANDSHAKE", "REJECT: name '%s' conflicts with host name", name);
        uint8_t reject_buf[2];
        net_serializer rs;
        net_serializer_init(&rs, reject_buf, sizeof(reject_buf));
        net_write_u8(&rs, NET_REJECT_NAME_TAKEN);
        send_raw_to_peer(peer, NET_MSG_JOIN_REJECT, reject_buf,
                         (uint32_t)net_serializer_position(&rs));
        return;
    }

    /* Assign player ID */
    uint8_t new_player_id = 0;
    for (int i = 0; i < NET_MAX_PEERS; i++) {
        if (session.peers[i].active && session.peers[i].player_id >= new_player_id) {
            new_player_id = session.peers[i].player_id + 1;
        }
    }
    /* Player 0 is host */
    if (new_player_id == 0) {
        new_player_id = 1;
    }

    strncpy(peer->name, name, NET_MAX_PLAYER_NAME - 1);
    peer->name[NET_MAX_PLAYER_NAME - 1] = '\0';
    net_peer_set_player_id(peer, new_player_id);
    peer->state = PEER_STATE_JOINED;

    uint32_t now = net_tcp_get_timestamp_ms();
    peer->last_heartbeat_recv_ms = now;

    /* Register in player registry */
    mp_player_registry_add(new_player_id, name, 0, 0);
    mp_player_registry_set_status(new_player_id, MP_PLAYER_LOBBY);
    mp_player_registry_set_connection_state(new_player_id, MP_CONNECTION_CONNECTED);
    int slot = mp_player_registry_assign_slot(new_player_id);

    log_info("Player joined", name, (int)new_player_id);

    /* Build extended JOIN_ACCEPT */
    mp_player *new_player = mp_player_registry_get(new_player_id);
    uint32_t session_seed = mp_worldgen_get_spawn_table_mutable()->session_seed;

    uint8_t accept_buf[128];
    net_serializer as;
    net_serializer_init(&as, accept_buf, sizeof(accept_buf));
    net_write_u8(&as, new_player_id);
    net_write_u8(&as, (uint8_t)slot);
    net_write_u32(&as, session.session_id);
    net_write_u32(&as, session_seed);
    net_write_u8(&as, (uint8_t)(session.peer_count + 1)); /* +1 includes host */
    if (new_player) {
        net_write_raw(&as, new_player->player_uuid, MP_PLAYER_UUID_SIZE);
        net_write_raw(&as, new_player->reconnect_token, MP_RECONNECT_TOKEN_SIZE);
    } else {
        uint8_t zeros[16] = {0};
        net_write_raw(&as, zeros, 16);
        net_write_raw(&as, zeros, 16);
    }
    send_raw_to_peer(peer, NET_MSG_JOIN_ACCEPT, accept_buf,
                     (uint32_t)net_serializer_position(&as));

    MP_LOG_INFO("HANDSHAKE", "JOIN_ACCEPT sent: player_id=%d slot=%d session=0x%08x seed=%u",
                (int)new_player_id, slot, session.session_id, session_seed);

    /* Broadcast player joined event to other peers */
    uint8_t event_buf[64];
    net_serializer es;
    net_serializer_init(&es, event_buf, sizeof(event_buf));
    net_write_u16(&es, NET_EVENT_PLAYER_JOINED);
    net_write_u32(&es, session.authoritative_tick);
    net_write_u8(&es, new_player_id);
    net_write_raw(&es, name, NET_MAX_PLAYER_NAME);
    net_session_broadcast(NET_MSG_HOST_EVENT, event_buf,
                          (uint32_t)net_serializer_position(&es));
}

static void handle_chat_from_client(net_peer *peer, const uint8_t *payload, uint32_t size)
{
    /* Host received a chat message from client — relay to all */
    uint8_t relay_buf[256];
    net_serializer s;
    net_serializer_init(&s, relay_buf, sizeof(relay_buf));

    /* Override sender_id with peer's verified player_id */
    net_write_u8(&s, peer->player_id);

    /* Read original message */
    net_serializer rs;
    net_serializer_init(&rs, (uint8_t *)payload, size);
    net_read_u8(&rs); /* skip client-provided sender_id */

    char message[MP_CHAT_MESSAGE_MAX];
    net_read_string(&rs, message, sizeof(message));
    net_write_string(&s, message, sizeof(message));

    /* Broadcast to all peers (including sender for confirmation) */
    net_session_broadcast(NET_MSG_CHAT, relay_buf,
                          (uint32_t)net_serializer_position(&s));

    /* Also store in host's chat history */
    mp_chat_entry *entry = &chat_history.entries[chat_history.write_index];
    entry->sender_id = peer->player_id;
    strncpy(entry->message, message, MP_CHAT_MESSAGE_MAX - 1);
    entry->message[MP_CHAT_MESSAGE_MAX - 1] = '\0';
    entry->timestamp_tick = session.authoritative_tick;
    chat_history.write_index = (chat_history.write_index + 1) % MP_CHAT_HISTORY_SIZE;
    if (chat_history.count < MP_CHAT_HISTORY_SIZE) {
        chat_history.count++;
    }
    chat_history.unread++;
}

static void handle_client_message(net_peer *peer, const net_packet_header *header,
                                  const uint8_t *payload, uint32_t size)
{
    /* Validate payload size */
    if (size > NET_MAX_PAYLOAD_SIZE) {
        MP_LOG_ERROR("NET", "Payload too large from peer '%s': %u bytes (max %u)",
                     peer->name, size, NET_MAX_PAYLOAD_SIZE);
        return;
    }
    if (size != header->payload_size) {
        MP_LOG_ERROR("NET", "Payload size mismatch from peer '%s': header says %u, got %u",
                     peer->name, header->payload_size, size);
        return;
    }

    MP_LOG_TRACE("NET", "Message from peer '%s': type=%s(%d) size=%u seq=%u",
                 peer->name, net_protocol_message_name(header->message_type),
                 (int)header->message_type, size, header->sequence_id);

    switch (header->message_type) {
        case NET_MSG_HELLO:
            handle_hello(peer, payload, size);
            break;
        case NET_MSG_READY_STATE: {
            if (size >= 2) {
                net_serializer s;
                net_serializer_init(&s, (uint8_t *)payload, size);
                net_read_u8(&s); /* player_id - use peer's known id instead */
                uint8_t ready = net_read_u8(&s);
                peer->state = ready ? PEER_STATE_READY : PEER_STATE_JOINED;
                mp_player_registry_set_status(peer->player_id,
                    ready ? MP_PLAYER_READY : MP_PLAYER_LOBBY);
                log_info("Player ready state changed", peer->name, ready);
            }
            break;
        }
        case NET_MSG_CLIENT_COMMAND: {
            extern void multiplayer_command_bus_receive(uint8_t player_id,
                                                       const uint8_t *data, uint32_t size);
            multiplayer_command_bus_receive(peer->player_id, payload, size);
            break;
        }
        case NET_MSG_CHAT: {
            handle_chat_from_client(peer, payload, size);
            break;
        }
        case NET_MSG_HEARTBEAT: {
            uint32_t now = net_tcp_get_timestamp_ms();
            net_peer_update_heartbeat_recv(peer, now);
            net_peer_update_quality(peer);
            /* Echo heartbeat back */
            uint8_t hb_buf[4];
            net_serializer hs;
            net_serializer_init(&hs, hb_buf, sizeof(hb_buf));
            net_write_u32(&hs, now);
            send_raw_to_peer(peer, NET_MSG_HEARTBEAT, hb_buf, 4);
            break;
        }
        case NET_MSG_CHECKSUM_RESPONSE: {
            extern void multiplayer_checksum_receive_response(uint8_t player_id,
                                                              const uint8_t *data, uint32_t size);
            multiplayer_checksum_receive_response(peer->player_id, payload, size);
            break;
        }
        case NET_MSG_RESYNC_REQUEST: {
            extern void multiplayer_resync_handle_request(uint8_t player_id);
            multiplayer_resync_handle_request(peer->player_id);
            mp_player *p = mp_player_registry_get(peer->player_id);
            if (p) {
                p->resyncs_requested++;
            }
            break;
        }
        case NET_MSG_DISCONNECT_NOTICE: {
            log_info("Peer disconnecting gracefully", peer->name, 0);
            for (int i = 0; i < NET_MAX_PEERS; i++) {
                if (&session.peers[i] == peer) {
                    handle_peer_disconnect(i);
                    break;
                }
            }
            break;
        }
        default:
            if (header->message_type >= NET_MSG_COUNT) {
                log_error("Invalid message type from peer", peer->name,
                         header->message_type);
            } else {
                log_error("Unhandled message from peer",
                         net_protocol_message_name(header->message_type),
                         header->message_type);
            }
            break;
    }
}

static void handle_chat_from_host(const uint8_t *payload, uint32_t size)
{
    net_serializer s;
    net_serializer_init(&s, (uint8_t *)payload, size);

    uint8_t sender_id = net_read_u8(&s);
    char message[MP_CHAT_MESSAGE_MAX];
    net_read_string(&s, message, sizeof(message));

    if (net_serializer_has_overflow(&s)) {
        return;
    }

    /* Store in chat history */
    mp_chat_entry *entry = &chat_history.entries[chat_history.write_index];
    entry->sender_id = sender_id;
    strncpy(entry->message, message, MP_CHAT_MESSAGE_MAX - 1);
    entry->message[MP_CHAT_MESSAGE_MAX - 1] = '\0';
    entry->timestamp_tick = session.authoritative_tick;
    chat_history.write_index = (chat_history.write_index + 1) % MP_CHAT_HISTORY_SIZE;
    if (chat_history.count < MP_CHAT_HISTORY_SIZE) {
        chat_history.count++;
    }
    chat_history.unread++;
}

static void handle_host_message(const net_packet_header *header,
                                const uint8_t *payload, uint32_t size)
{
    /* Validate payload size */
    if (size > NET_MAX_PAYLOAD_SIZE) {
        MP_LOG_ERROR("NET", "Payload too large from host: %u bytes (max %u)",
                     size, NET_MAX_PAYLOAD_SIZE);
        return;
    }
    if (size != header->payload_size) {
        MP_LOG_ERROR("NET", "Payload size mismatch from host: header says %u, got %u",
                     header->payload_size, size);
        return;
    }

    MP_LOG_TRACE("NET", "Message from host: type=%s(%d) size=%u seq=%u",
                 net_protocol_message_name(header->message_type),
                 (int)header->message_type, size, header->sequence_id);

    switch (header->message_type) {
        case NET_MSG_JOIN_ACCEPT: {
            net_serializer s;
            net_serializer_init(&s, (uint8_t *)payload, size);
            session.local_player_id = net_read_u8(&s);
            uint8_t slot_id = net_read_u8(&s);
            session.session_id = net_read_u32(&s);
            uint32_t session_seed = net_read_u32(&s);
            uint8_t player_count = net_read_u8(&s);
            uint8_t assigned_uuid[16];
            uint8_t assigned_token[16];
            net_read_raw(&s, assigned_uuid, 16);
            net_read_raw(&s, assigned_token, 16);

            session.host_peer.state = PEER_STATE_JOINED;
            session.state = NET_SESSION_CLIENT_LOBBY;
            join_status = NET_JOIN_STATUS_CONNECTED;

            MP_LOG_INFO("HANDSHAKE", "[join:%04x] JOIN_ACCEPT received: player_id=%d slot=%d "
                        "session=0x%08x seed=%u player_count=%d",
                        join_correlation_id, (int)session.local_player_id, (int)slot_id,
                        session.session_id, session_seed, (int)player_count);

            /* Register self in player registry with assigned UUID */
            mp_player_registry_add_with_uuid(session.local_player_id,
                session.local_player_name, assigned_uuid, 1, 0);
            mp_player *self = mp_player_registry_get(session.local_player_id);
            if (self) {
                self->slot_id = slot_id;
                memcpy(self->reconnect_token, assigned_token, MP_RECONNECT_TOKEN_SIZE);
                self->status = MP_PLAYER_LOBBY;
                self->connection_state = MP_CONNECTION_CONNECTED;
            }

            /* Store session seed for worldgen */
            mp_spawn_table *table = mp_worldgen_get_spawn_table_mutable();
            table->session_seed = session_seed;

            log_info("Joined session as player", 0, session.local_player_id);
            (void)player_count;
            break;
        }
        case NET_MSG_JOIN_REJECT: {
            if (size >= 1) {
                net_serializer s;
                net_serializer_init(&s, (uint8_t *)payload, size);
                uint8_t reason = net_read_u8(&s);
                const char *reason_str = "unknown";
                switch (reason) {
                    case NET_REJECT_VERSION_MISMATCH: reason_str = "VERSION_MISMATCH"; break;
                    case NET_REJECT_SESSION_FULL: reason_str = "SESSION_FULL"; break;
                    case NET_REJECT_GAME_IN_PROGRESS: reason_str = "GAME_IN_PROGRESS"; break;
                    case NET_REJECT_NAME_TAKEN: reason_str = "NAME_TAKEN"; break;
                    case NET_REJECT_BANNED: reason_str = "BANNED"; break;
                }
                MP_LOG_ERROR("HANDSHAKE", "[join:%04x] JOIN_REJECT received: reason=%s (%d)",
                             join_correlation_id, reason_str, (int)reason);
                log_error("Join rejected", reason_str, reason);
                join_reject_reason = reason;
            }
            join_status = NET_JOIN_STATUS_REJECTED;
            session.state = NET_SESSION_DISCONNECTING;
            break;
        }
        case NET_MSG_START_GAME: {
            net_serializer s;
            net_serializer_init(&s, (uint8_t *)payload, size);
            session.authoritative_tick = net_read_u32(&s);
            session.game_speed = net_read_u8(&s);
            session.state = NET_SESSION_CLIENT_GAME;
            session.host_peer.state = PEER_STATE_IN_GAME;

            /* Update local player status */
            mp_player_registry_set_status(session.local_player_id, MP_PLAYER_IN_GAME);

            log_info("Game started by host", 0, 0);
            break;
        }
        case NET_MSG_HOST_COMMAND_ACK: {
            extern void multiplayer_command_bus_receive_ack(const uint8_t *data, uint32_t size);
            multiplayer_command_bus_receive_ack(payload, size);
            break;
        }
        case NET_MSG_FULL_SNAPSHOT: {
            extern void multiplayer_snapshot_receive_full(const uint8_t *data, uint32_t size);
            multiplayer_snapshot_receive_full(payload, size);
            break;
        }
        case NET_MSG_DELTA_SNAPSHOT: {
            extern void multiplayer_snapshot_receive_delta(const uint8_t *data, uint32_t size);
            multiplayer_snapshot_receive_delta(payload, size);
            break;
        }
        case NET_MSG_HOST_EVENT: {
            extern void multiplayer_empire_sync_receive_event(const uint8_t *data, uint32_t size);
            multiplayer_empire_sync_receive_event(payload, size);
            break;
        }
        case NET_MSG_CHECKSUM_REQUEST: {
            extern void multiplayer_checksum_handle_request(const uint8_t *data, uint32_t size);
            multiplayer_checksum_handle_request(payload, size);
            break;
        }
        case NET_MSG_RESYNC_GRANTED: {
            extern void multiplayer_resync_apply_full_snapshot(const uint8_t *data, uint32_t size);
            multiplayer_resync_apply_full_snapshot(payload, size);
            break;
        }
        case NET_MSG_CHAT: {
            handle_chat_from_host(payload, size);
            break;
        }
        case NET_MSG_HEARTBEAT: {
            uint32_t now = net_tcp_get_timestamp_ms();
            net_peer_update_heartbeat_recv(&session.host_peer, now);
            net_peer_update_quality(&session.host_peer);
            break;
        }
        case NET_MSG_DISCONNECT_NOTICE: {
            log_info("Host disconnected", 0, 0);
            session.state = NET_SESSION_DISCONNECTING;
            break;
        }
        default:
            if (header->message_type >= NET_MSG_COUNT) {
                log_error("Invalid message type from host", 0, header->message_type);
            } else {
                log_error("Unhandled message from host",
                         net_protocol_message_name(header->message_type),
                         header->message_type);
            }
            break;
    }
}

static void host_accept_connections(void)
{
    if (session.listen_fd < 0) {
        return;
    }

    int client_fd = net_tcp_accept(session.listen_fd);
    if (client_fd < 0) {
        return;
    }

    int slot = find_free_peer_slot();
    if (slot < 0) {
        /* Session full */
        log_error("Session full, rejecting connection", 0, 0);
        net_tcp_close(client_fd);
        return;
    }

    net_peer_init(&session.peers[slot]);
    net_peer_set_connected(&session.peers[slot], client_fd, "");
    session.peer_count++;

    log_info("New connection accepted in slot", 0, slot);
    MP_LOG_INFO("SESSION", "New TCP connection accepted: slot=%d fd=%d total_peers=%d",
                slot, client_fd, session.peer_count);
}

static void host_process_peers(void)
{
    uint32_t now = net_tcp_get_timestamp_ms();
    uint8_t recv_buf[4096];

    for (int i = 0; i < NET_MAX_PEERS; i++) {
        net_peer *peer = &session.peers[i];
        if (!peer->active) {
            continue;
        }

        /* Check timeout */
        if (peer->state != PEER_STATE_CONNECTING && net_peer_is_timed_out(peer, now)) {
            log_error("Peer timed out", peer->name, i);
            handle_peer_disconnect(i);
            continue;
        }

        /* Receive data */
        int received = net_tcp_recv(peer->socket_fd, recv_buf, sizeof(recv_buf));
        if (received < 0) {
            log_error("Peer connection lost", peer->name, i);
            handle_peer_disconnect(i);
            continue;
        }
        if (received > 0) {
            peer->bytes_received += received;
            net_codec_feed(&peer->codec, recv_buf, (size_t)received);

            /* Process all complete packets */
            net_packet_header header;
            const uint8_t *payload;
            uint32_t payload_size;

            while (net_codec_decode(&peer->codec, &header, &payload, &payload_size) == CODEC_OK) {
                peer->packets_received++;
                handle_client_message(peer, &header, payload, payload_size);
            }
        }

        /* Send heartbeats periodically */
        if (now - peer->last_heartbeat_sent_ms > NET_HEARTBEAT_INTERVAL_MS) {
            uint8_t hb_buf[4];
            net_serializer hs;
            net_serializer_init(&hs, hb_buf, sizeof(hb_buf));
            net_write_u32(&hs, now);
            send_raw_to_peer(peer, NET_MSG_HEARTBEAT, hb_buf, 4);
            net_peer_update_heartbeat_sent(peer, now);
        }
    }

    /* Periodic cleanup of expired reconnect slots */
    if (session.state == NET_SESSION_HOSTING_GAME) {
        mp_player_registry_cleanup_expired(session.authoritative_tick);
    }
}

static void client_process_host(void)
{
    net_peer *peer = &session.host_peer;
    if (!peer->active) {
        return;
    }

    uint32_t now = net_tcp_get_timestamp_ms();
    uint8_t recv_buf[4096];

    /* Check handshake timeout while joining */
    if (session.state == NET_SESSION_JOINING &&
        join_start_ms > 0 && (now - join_start_ms) > NET_JOIN_TIMEOUT_MS) {
        MP_LOG_ERROR("HANDSHAKE", "[join:%04x] Handshake timed out after %u ms",
                     join_correlation_id, now - join_start_ms);
        log_error("Join handshake timed out", 0, 0);
        join_status = NET_JOIN_STATUS_TIMEOUT;
        session.state = NET_SESSION_DISCONNECTING;
        return;
    }

    /* Check timeout for established connections */
    if (peer->state != PEER_STATE_CONNECTING &&
        peer->state != PEER_STATE_HELLO_SENT &&
        net_peer_is_timed_out(peer, now)) {
        log_error("Host connection timed out", 0, 0);
        session.state = NET_SESSION_DISCONNECTING;
        return;
    }

    /* Receive data */
    int received = net_tcp_recv(peer->socket_fd, recv_buf, sizeof(recv_buf));
    if (received < 0) {
        MP_LOG_ERROR("NET", "Host connection lost (recv returned %d)", received);
        log_error("Host connection lost", 0, 0);
        if (session.state == NET_SESSION_JOINING) {
            join_status = NET_JOIN_STATUS_FAILED;
        }
        session.state = NET_SESSION_DISCONNECTING;
        return;
    }
    if (received > 0) {
        peer->bytes_received += received;
        net_codec_feed(&peer->codec, recv_buf, (size_t)received);

        net_packet_header header;
        const uint8_t *payload;
        uint32_t payload_size;

        while (net_codec_decode(&peer->codec, &header, &payload, &payload_size) == CODEC_OK) {
            peer->packets_received++;
            handle_host_message(&header, payload, payload_size);
        }
    }

    /* Send heartbeats */
    if (now - peer->last_heartbeat_sent_ms > NET_HEARTBEAT_INTERVAL_MS) {
        uint8_t hb_buf[4];
        net_serializer hs;
        net_serializer_init(&hs, hb_buf, sizeof(hb_buf));
        net_write_u32(&hs, now);
        net_session_send_to_host(NET_MSG_HEARTBEAT, hb_buf, 4);
        net_peer_update_heartbeat_sent(peer, now);
    }
}

/* ---- Public API ---- */

int net_session_init(void)
{
    memset(&session, 0, sizeof(net_session));
    memset(&chat_history, 0, sizeof(chat_history));
    session.listen_fd = -1;
    session.udp_fd = -1;

    for (int i = 0; i < NET_MAX_PEERS; i++) {
        net_peer_init(&session.peers[i]);
    }
    net_peer_init(&session.host_peer);

    if (!net_tcp_init()) {
        return 0;
    }
    net_udp_init();
    net_discovery_init();

    mp_debug_log_init();

    /* Load persistent player name from config.
     * If the user has never set a name, the default "Player" is used. */
    if (!persistent_name_set) {
        const char *saved_name = config_get_string(CONFIG_STRING_MP_PLAYER_NAME);
        if (saved_name && saved_name[0]) {
            strncpy(persistent_player_name, saved_name, NET_MAX_PLAYER_NAME - 1);
            persistent_player_name[NET_MAX_PLAYER_NAME - 1] = '\0';
        }
        persistent_name_set = 1;
    }

    log_info("Network session system initialized", 0, 0);
    MP_LOG_INFO("SESSION", "Network session initialized, player_name='%s'", persistent_player_name);
    return 1;
}

void net_session_shutdown(void)
{
    MP_LOG_INFO("SESSION", "Session shutdown");
    net_session_disconnect();
    net_discovery_shutdown();
    net_tcp_shutdown();
    net_udp_shutdown();
    mp_debug_log_shutdown();
}

int net_session_host(const char *player_name, uint16_t port)
{
    if (session.state != NET_SESSION_IDLE) {
        log_error("Cannot host: session not idle", 0, 0);
        return 0;
    }

    session.listen_fd = net_tcp_listen(port);
    if (session.listen_fd < 0) {
        return 0;
    }

    session.udp_fd = net_udp_create(port);
    if (session.udp_fd >= 0) {
        net_udp_enable_broadcast(session.udp_fd);
    }

    session.port = port;
    session.role = NET_ROLE_HOST;
    session.state = NET_SESSION_HOSTING_LOBBY;
    session.session_id = generate_session_id();
    session.local_player_id = 0; /* Host is always player 0 */

    strncpy(session.local_player_name, player_name, NET_MAX_PLAYER_NAME - 1);
    session.local_player_name[NET_MAX_PLAYER_NAME - 1] = '\0';

    /* Register host in player registry */
    mp_player_registry_init();
    mp_player_registry_add(0, player_name, 1, 1);
    mp_player_registry_set_status(0, MP_PLAYER_LOBBY);
    mp_player_registry_set_connection_state(0, MP_CONNECTION_CONNECTED);
    mp_player_registry_assign_slot(0);

    /* Start LAN discovery broadcasting so clients can find this host */
    net_discovery_start_announcing(player_name, port, 1, NET_MAX_PLAYERS, session.session_id);

    log_info("Hosting session", player_name, (int)port);
    MP_LOG_INFO("SESSION", "Hosting session: name='%s' port=%d session_id=0x%08x",
                player_name, (int)port, session.session_id);
    return 1;
}

void net_session_kick_peer(uint8_t player_id)
{
    for (int i = 0; i < NET_MAX_PEERS; i++) {
        if (session.peers[i].active && session.peers[i].player_id == player_id) {
            /* Send disconnect notice */
            uint8_t buf[2];
            net_serializer s;
            net_serializer_init(&s, buf, sizeof(buf));
            net_write_u8(&s, player_id);
            net_write_u8(&s, 0); /* reason: kicked */
            send_raw_to_peer(&session.peers[i], NET_MSG_DISCONNECT_NOTICE,
                           buf, (uint32_t)net_serializer_position(&s));

            /* Remove from registry (kicked players don't get reconnect) */
            mp_player_registry_remove(player_id);

            close_peer(i);
            break;
        }
    }
}

int net_session_join(const char *player_name, const char *host_address, uint16_t port)
{
    if (session.state != NET_SESSION_IDLE) {
        log_error("Cannot join: session not idle", 0, 0);
        return 0;
    }

    int fd = net_tcp_connect(host_address, port);
    if (fd < 0) {
        return 0;
    }

    session.role = NET_ROLE_CLIENT;
    session.state = NET_SESSION_JOINING;

    strncpy(session.local_player_name, player_name, NET_MAX_PLAYER_NAME - 1);
    session.local_player_name[NET_MAX_PLAYER_NAME - 1] = '\0';

    net_peer_init(&session.host_peer);
    net_peer_set_connected(&session.host_peer, fd, "Host");
    session.host_peer.state = PEER_STATE_HELLO_SENT;
    session.host_peer.last_heartbeat_recv_ms = net_tcp_get_timestamp_ms();

    /* Track join status for UI feedback */
    join_status = NET_JOIN_STATUS_CONNECTING;
    join_reject_reason = 0;
    join_start_ms = net_tcp_get_timestamp_ms();
    join_correlation_id = (uint32_t)(join_start_ms ^ (uint32_t)(size_t)&session) & 0xFFFF;

    /* Send extended HELLO with UUID (if reconnecting) and protocol info.
     * Use a zeroed buffer for the player name to avoid reading past
     * the source string if it's shorter than NET_MAX_PLAYER_NAME. */
    uint8_t hello_buf[128];
    net_serializer s;
    net_serializer_init(&s, hello_buf, sizeof(hello_buf));
    net_write_u32(&s, NET_MAGIC);
    net_write_u16(&s, NET_PROTOCOL_VERSION);
    net_write_u32(&s, 0); /* save_version: filled by caller if needed */
    net_write_u32(&s, 0); /* map_hash */
    net_write_u32(&s, 0); /* scenario_hash */
    net_write_u32(&s, 0); /* feature_flags */

    /* Fixed-size name field: zero-padded to NET_MAX_PLAYER_NAME bytes */
    char name_buf[NET_MAX_PLAYER_NAME];
    memset(name_buf, 0, sizeof(name_buf));
    strncpy(name_buf, player_name, NET_MAX_PLAYER_NAME - 1);
    net_write_raw(&s, name_buf, NET_MAX_PLAYER_NAME);

    /* If we have a previous UUID (for reconnect), send it */
    mp_player *local = mp_player_registry_get_local();
    if (local && local->active) {
        net_write_raw(&s, local->player_uuid, 16);
        net_write_raw(&s, local->reconnect_token, 16);
    } else {
        uint8_t zeros[16] = {0};
        net_write_raw(&s, zeros, 16);
        net_write_raw(&s, zeros, 16);
    }

    net_session_send_to_host(NET_MSG_HELLO, hello_buf, (uint32_t)net_serializer_position(&s));

    log_info("Joining session at", host_address, (int)port);
    MP_LOG_INFO("SESSION", "[join:%04x] Joining session: host='%s' port=%d name='%s' hello_size=%d",
                join_correlation_id, host_address, (int)port, player_name,
                (int)net_serializer_position(&s));
    MP_LOG_INFO("HANDSHAKE", "[join:%04x] HELLO sent to host (magic=0x%08x version=%d)",
                join_correlation_id, NET_MAGIC, NET_PROTOCOL_VERSION);
    return 1;
}

void net_session_disconnect(void)
{
    if (session.state == NET_SESSION_IDLE) {
        return;
    }

    MP_LOG_INFO("SESSION", "Disconnecting: state=%s role=%s join_status=%d",
                net_session_state_name(session.state),
                session.role == NET_ROLE_HOST ? "HOST" : "CLIENT",
                (int)join_status);

    if (session.role == NET_ROLE_HOST) {
        /* Notify all peers */
        uint8_t buf[2];
        net_serializer s;
        net_serializer_init(&s, buf, sizeof(buf));
        net_write_u8(&s, session.local_player_id);
        net_write_u8(&s, 0);

        for (int i = 0; i < NET_MAX_PEERS; i++) {
            if (session.peers[i].active) {
                send_raw_to_peer(&session.peers[i], NET_MSG_DISCONNECT_NOTICE,
                               buf, (uint32_t)net_serializer_position(&s));
                close_peer(i);
            }
        }
        net_tcp_close(session.listen_fd);
        session.listen_fd = -1;
    } else if (session.role == NET_ROLE_CLIENT) {
        if (session.host_peer.active) {
            uint8_t buf[2];
            net_serializer s;
            net_serializer_init(&s, buf, sizeof(buf));
            net_write_u8(&s, session.local_player_id);
            net_write_u8(&s, 0);
            net_session_send_to_host(NET_MSG_DISCONNECT_NOTICE, buf,
                                    (uint32_t)net_serializer_position(&s));
            net_tcp_close(session.host_peer.socket_fd);
            net_peer_reset(&session.host_peer);
        }
    }

    /* Stop LAN discovery */
    net_discovery_stop_announcing();
    net_discovery_stop_listening();

    if (session.udp_fd >= 0) {
        net_udp_close(session.udp_fd);
        session.udp_fd = -1;
    }

    session.state = NET_SESSION_IDLE;
    session.role = NET_ROLE_NONE;
    session.peer_count = 0;

    log_info("Session disconnected", 0, 0);
    MP_LOG_INFO("SESSION", "Session disconnected — state reset to IDLE");
}

void net_session_update(void)
{
    if (session.state == NET_SESSION_IDLE) {
        return;
    }

    if (session.state == NET_SESSION_DISCONNECTING) {
        net_session_disconnect();
        return;
    }

    if (session.role == NET_ROLE_HOST) {
        host_accept_connections();
        host_process_peers();
    } else if (session.role == NET_ROLE_CLIENT) {
        client_process_host();
    }
}

int net_session_is_active(void)
{
    return session.state != NET_SESSION_IDLE;
}

int net_session_is_host(void)
{
    return session.role == NET_ROLE_HOST;
}

int net_session_is_client(void)
{
    return session.role == NET_ROLE_CLIENT;
}

int net_session_is_in_game(void)
{
    return session.state == NET_SESSION_HOSTING_GAME ||
           session.state == NET_SESSION_CLIENT_GAME;
}

int net_session_is_in_lobby(void)
{
    return session.state == NET_SESSION_HOSTING_LOBBY ||
           session.state == NET_SESSION_CLIENT_LOBBY;
}

net_session_state net_session_get_state(void)
{
    return session.state;
}

uint8_t net_session_get_local_player_id(void)
{
    return session.local_player_id;
}

uint32_t net_session_get_authoritative_tick(void)
{
    return session.authoritative_tick;
}

static const char *SESSION_STATE_NAMES[] = {
    "IDLE", "HOSTING_LOBBY", "HOSTING_GAME", "JOINING",
    "CLIENT_LOBBY", "CLIENT_GAME", "DISCONNECTING"
};

const char *net_session_state_name(net_session_state state)
{
    if (state < 0 || state > NET_SESSION_DISCONNECTING) {
        return "UNKNOWN";
    }
    return SESSION_STATE_NAMES[state];
}

int net_session_get_peer_count(void)
{
    return session.peer_count;
}

const net_peer *net_session_get_peer(int index)
{
    if (index < 0 || index >= NET_MAX_PEERS) {
        return NULL;
    }
    return &session.peers[index];
}

const net_peer *net_session_get_host_peer(void)
{
    return &session.host_peer;
}

int net_session_start_game(void)
{
    if (session.role != NET_ROLE_HOST || session.state != NET_SESSION_HOSTING_LOBBY) {
        return 0;
    }

    session.state = NET_SESSION_HOSTING_GAME;
    session.authoritative_tick = 0;
    session.game_speed = 2; /* Normal speed */

    /* Update all player statuses */
    mp_player_registry_set_status(0, MP_PLAYER_IN_GAME);

    /* Send START_GAME to all peers */
    uint8_t buf[8];
    net_serializer s;
    net_serializer_init(&s, buf, sizeof(buf));
    net_write_u32(&s, session.authoritative_tick);
    net_write_u8(&s, session.game_speed);

    for (int i = 0; i < NET_MAX_PEERS; i++) {
        if (session.peers[i].active &&
            (session.peers[i].state == PEER_STATE_READY ||
             session.peers[i].state == PEER_STATE_JOINED)) {
            session.peers[i].state = PEER_STATE_IN_GAME;
            mp_player_registry_set_status(session.peers[i].player_id, MP_PLAYER_IN_GAME);
            send_raw_to_peer(&session.peers[i], NET_MSG_START_GAME,
                           buf, (uint32_t)net_serializer_position(&s));
        }
    }

    log_info("Game started", 0, 0);
    MP_LOG_INFO("SESSION", "Game started: tick=%u speed=%d peers=%d",
                session.authoritative_tick, (int)session.game_speed, session.peer_count);
    return 1;
}

void net_session_set_game_speed(uint8_t speed)
{
    if (speed > 3) {
        speed = 3;
    }
    session.game_speed = speed;

    /* Broadcast speed change event */
    if (session.role == NET_ROLE_HOST) {
        uint8_t event_buf[16];
        net_serializer s;
        net_serializer_init(&s, event_buf, sizeof(event_buf));
        net_write_u16(&s, NET_EVENT_SPEED_CHANGED);
        net_write_u32(&s, session.authoritative_tick);
        net_write_u8(&s, speed);
        net_session_broadcast(NET_MSG_HOST_EVENT, event_buf,
                              (uint32_t)net_serializer_position(&s));
    }
}

void net_session_set_paused(int paused)
{
    session.game_paused = paused;

    /* Broadcast pause/resume event */
    if (session.role == NET_ROLE_HOST) {
        uint8_t event_buf[16];
        net_serializer s;
        net_serializer_init(&s, event_buf, sizeof(event_buf));
        net_write_u16(&s, paused ? NET_EVENT_GAME_PAUSED : NET_EVENT_GAME_RESUMED);
        net_write_u32(&s, session.authoritative_tick);
        net_session_broadcast(NET_MSG_HOST_EVENT, event_buf,
                              (uint32_t)net_serializer_position(&s));
    }
}

void net_session_advance_tick(void)
{
    session.authoritative_tick++;
}

int net_session_send_to_host(uint16_t message_type, const uint8_t *payload, uint32_t size)
{
    if (session.role != NET_ROLE_CLIENT || !session.host_peer.active) {
        return 0;
    }
    send_raw_to_peer(&session.host_peer, message_type, payload, size);
    return 1;
}

int net_session_send_to_peer(int peer_index, uint16_t message_type,
                             const uint8_t *payload, uint32_t size)
{
    if (peer_index < 0 || peer_index >= NET_MAX_PEERS) {
        return 0;
    }
    if (!session.peers[peer_index].active) {
        return 0;
    }
    send_raw_to_peer(&session.peers[peer_index], message_type, payload, size);
    return 1;
}

int net_session_broadcast(uint16_t message_type, const uint8_t *payload, uint32_t size)
{
    if (session.role != NET_ROLE_HOST) {
        return 0;
    }
    int sent_count = 0;
    for (int i = 0; i < NET_MAX_PEERS; i++) {
        if (session.peers[i].active && session.peers[i].state == PEER_STATE_IN_GAME) {
            send_raw_to_peer(&session.peers[i], message_type, payload, size);
            sent_count++;
        }
    }
    return sent_count;
}

void net_session_set_ready(int is_ready)
{
    if (session.role == NET_ROLE_CLIENT) {
        uint8_t buf[2];
        net_serializer s;
        net_serializer_init(&s, buf, sizeof(buf));
        net_write_u8(&s, session.local_player_id);
        net_write_u8(&s, (uint8_t)is_ready);
        net_session_send_to_host(NET_MSG_READY_STATE, buf, 2);
    }
}

int net_session_all_peers_ready(void)
{
    if (session.role != NET_ROLE_HOST) {
        return 0;
    }
    for (int i = 0; i < NET_MAX_PEERS; i++) {
        if (session.peers[i].active && session.peers[i].state != PEER_STATE_READY) {
            return 0;
        }
    }
    return session.peer_count > 0;
}

net_session *net_session_get(void)
{
    return &session;
}

/* ---- Chat API ---- */

int net_session_chat_get_count(void)
{
    return chat_history.count;
}

const char *net_session_chat_get_message(int index, uint8_t *out_sender_id)
{
    if (index < 0 || index >= chat_history.count) {
        return NULL;
    }
    /* Convert from logical index to ring buffer index */
    int ring_index;
    if (chat_history.count >= MP_CHAT_HISTORY_SIZE) {
        ring_index = (chat_history.write_index + index) % MP_CHAT_HISTORY_SIZE;
    } else {
        ring_index = index;
    }
    if (out_sender_id) {
        *out_sender_id = chat_history.entries[ring_index].sender_id;
    }
    return chat_history.entries[ring_index].message;
}

int net_session_chat_get_unread(void)
{
    return chat_history.unread;
}

void net_session_chat_mark_read(void)
{
    chat_history.unread = 0;
}

int net_session_send_chat(const char *message)
{
    if (!net_session_is_active()) {
        return 0;
    }

    uint8_t buf[256];
    net_serializer s;
    net_serializer_init(&s, buf, sizeof(buf));
    net_write_u8(&s, session.local_player_id);
    net_write_string(&s, message, MP_CHAT_MESSAGE_MAX);

    if (net_session_is_host()) {
        /* Host: store locally and broadcast */
        mp_chat_entry *entry = &chat_history.entries[chat_history.write_index];
        entry->sender_id = session.local_player_id;
        strncpy(entry->message, message, MP_CHAT_MESSAGE_MAX - 1);
        entry->message[MP_CHAT_MESSAGE_MAX - 1] = '\0';
        entry->timestamp_tick = session.authoritative_tick;
        chat_history.write_index = (chat_history.write_index + 1) % MP_CHAT_HISTORY_SIZE;
        if (chat_history.count < MP_CHAT_HISTORY_SIZE) {
            chat_history.count++;
        }
        net_session_broadcast(NET_MSG_CHAT, buf, (uint32_t)net_serializer_position(&s));
    } else {
        /* Client: send to host for relay */
        net_session_send_to_host(NET_MSG_CHAT, buf, (uint32_t)net_serializer_position(&s));
    }
    return 1;
}

/* ---- Player name management ---- */

void net_session_set_local_name(const char *name)
{
    if (!name || !name[0]) {
        return;
    }

    /* Trim leading/trailing spaces */
    const char *start = name;
    while (*start == ' ') {
        start++;
    }
    if (!*start) {
        return; /* All spaces — reject */
    }

    strncpy(persistent_player_name, start, NET_MAX_PLAYER_NAME - 1);
    persistent_player_name[NET_MAX_PLAYER_NAME - 1] = '\0';

    /* Trim trailing spaces */
    int len = (int)strlen(persistent_player_name);
    while (len > 0 && persistent_player_name[len - 1] == ' ') {
        persistent_player_name[--len] = '\0';
    }

    persistent_name_set = 1;

    /* Persist to config file */
    config_set_string(CONFIG_STRING_MP_PLAYER_NAME, persistent_player_name);

    /* Also update the active session name if one exists */
    if (session.state != NET_SESSION_IDLE) {
        strncpy(session.local_player_name, persistent_player_name, NET_MAX_PLAYER_NAME - 1);
        session.local_player_name[NET_MAX_PLAYER_NAME - 1] = '\0';
    }

    MP_LOG_INFO("SESSION", "Player name set to '%s'", persistent_player_name);
}

const char *net_session_get_local_name(void)
{
    return persistent_player_name;
}

/* ---- Join status tracking ---- */

net_join_status net_session_get_join_status(void)
{
    return join_status;
}

uint8_t net_session_get_reject_reason(void)
{
    return join_reject_reason;
}

void net_session_clear_join_status(void)
{
    join_status = NET_JOIN_STATUS_NONE;
    join_reject_reason = 0;
    join_start_ms = 0;
}

#endif /* ENABLE_MULTIPLAYER */

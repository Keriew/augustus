#include "session.h"

#ifdef ENABLE_MULTIPLAYER

#include "transport_tcp.h"
#include "transport_udp.h"
#include "core/log.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>

static net_session session;

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

static void handle_hello(net_peer *peer, const uint8_t *payload, uint32_t size)
{
    net_serializer s;
    net_serializer_init(&s, (uint8_t *)payload, size);

    uint32_t magic = net_read_u32(&s);
    uint16_t version = net_read_u16(&s);
    char name[NET_MAX_PLAYER_NAME];
    net_read_raw(&s, name, NET_MAX_PLAYER_NAME);
    name[NET_MAX_PLAYER_NAME - 1] = '\0';

    if (net_serializer_has_overflow(&s)) {
        log_error("Malformed HELLO from peer", 0, 0);
        return;
    }

    if (magic != NET_MAGIC) {
        log_error("Invalid magic in HELLO", 0, 0);
        uint8_t reject_buf[2];
        net_serializer rs;
        net_serializer_init(&rs, reject_buf, sizeof(reject_buf));
        net_write_u8(&rs, NET_REJECT_VERSION_MISMATCH);
        send_raw_to_peer(peer, NET_MSG_JOIN_REJECT, reject_buf, (uint32_t)net_serializer_position(&rs));
        return;
    }

    if (!net_protocol_check_version(version)) {
        log_error("Protocol version mismatch", 0, (int)version);
        uint8_t reject_buf[2];
        net_serializer rs;
        net_serializer_init(&rs, reject_buf, sizeof(reject_buf));
        net_write_u8(&rs, NET_REJECT_VERSION_MISMATCH);
        send_raw_to_peer(peer, NET_MSG_JOIN_REJECT, reject_buf, (uint32_t)net_serializer_position(&rs));
        return;
    }

    if (session.state == NET_SESSION_HOSTING_GAME) {
        uint8_t reject_buf[2];
        net_serializer rs;
        net_serializer_init(&rs, reject_buf, sizeof(reject_buf));
        net_write_u8(&rs, NET_REJECT_GAME_IN_PROGRESS);
        send_raw_to_peer(peer, NET_MSG_JOIN_REJECT, reject_buf, (uint32_t)net_serializer_position(&rs));
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

    log_info("Player joined", name, (int)new_player_id);

    /* Send JOIN_ACCEPT */
    uint8_t accept_buf[16];
    net_serializer as;
    net_serializer_init(&as, accept_buf, sizeof(accept_buf));
    net_write_u8(&as, new_player_id);
    net_write_u32(&as, session.session_id);
    net_write_u8(&as, (uint8_t)(session.peer_count + 1)); /* +1 includes host */
    send_raw_to_peer(peer, NET_MSG_JOIN_ACCEPT, accept_buf, (uint32_t)net_serializer_position(&as));
}

static void handle_client_message(net_peer *peer, const net_packet_header *header,
                                  const uint8_t *payload, uint32_t size)
{
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
                log_info("Player ready state changed", peer->name, ready);
            }
            break;
        }
        case NET_MSG_CLIENT_COMMAND: {
            /* Forward to multiplayer command bus for validation and application */
            /* The command_bus module will handle this via its own interface */
            extern void multiplayer_command_bus_receive(uint8_t player_id,
                                                       const uint8_t *data, uint32_t size);
            multiplayer_command_bus_receive(peer->player_id, payload, size);
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
            break;
        }
        case NET_MSG_DISCONNECT_NOTICE: {
            log_info("Peer disconnecting", peer->name, 0);
            /* Find peer index */
            for (int i = 0; i < NET_MAX_PEERS; i++) {
                if (&session.peers[i] == peer) {
                    close_peer(i);
                    break;
                }
            }
            break;
        }
        default:
            log_error("Unknown message from peer",
                     net_protocol_message_name(header->message_type), header->message_type);
            break;
    }
}

static void handle_host_message(const net_packet_header *header,
                                const uint8_t *payload, uint32_t size)
{
    switch (header->message_type) {
        case NET_MSG_JOIN_ACCEPT: {
            net_serializer s;
            net_serializer_init(&s, (uint8_t *)payload, size);
            session.local_player_id = net_read_u8(&s);
            session.session_id = net_read_u32(&s);
            uint8_t player_count = net_read_u8(&s);
            session.host_peer.state = PEER_STATE_JOINED;
            session.state = NET_SESSION_CLIENT_LOBBY;
            log_info("Joined session as player", 0, session.local_player_id);
            (void)player_count;
            break;
        }
        case NET_MSG_JOIN_REJECT: {
            if (size >= 1) {
                net_serializer s;
                net_serializer_init(&s, (uint8_t *)payload, size);
                uint8_t reason = net_read_u8(&s);
                log_error("Join rejected", 0, reason);
            }
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
            log_error("Unknown message from host",
                     net_protocol_message_name(header->message_type), header->message_type);
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
            close_peer(i);
            continue;
        }

        /* Receive data */
        int received = net_tcp_recv(peer->socket_fd, recv_buf, sizeof(recv_buf));
        if (received < 0) {
            log_error("Peer connection lost", peer->name, i);
            close_peer(i);
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
}

static void client_process_host(void)
{
    net_peer *peer = &session.host_peer;
    if (!peer->active) {
        return;
    }

    uint32_t now = net_tcp_get_timestamp_ms();
    uint8_t recv_buf[4096];

    /* Check timeout */
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
        log_error("Host connection lost", 0, 0);
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

    log_info("Network session system initialized", 0, 0);
    return 1;
}

void net_session_shutdown(void)
{
    net_session_disconnect();
    net_tcp_shutdown();
    net_udp_shutdown();
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

    log_info("Hosting session", player_name, (int)port);
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

    /* Send HELLO */
    uint8_t hello_buf[4 + 2 + NET_MAX_PLAYER_NAME];
    net_serializer s;
    net_serializer_init(&s, hello_buf, sizeof(hello_buf));
    net_write_u32(&s, NET_MAGIC);
    net_write_u16(&s, NET_PROTOCOL_VERSION);
    net_write_raw(&s, player_name, NET_MAX_PLAYER_NAME);

    net_session_send_to_host(NET_MSG_HELLO, hello_buf, (uint32_t)net_serializer_position(&s));

    log_info("Joining session at", host_address, (int)port);
    return 1;
}

void net_session_disconnect(void)
{
    if (session.state == NET_SESSION_IDLE) {
        return;
    }

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

    if (session.udp_fd >= 0) {
        net_udp_close(session.udp_fd);
        session.udp_fd = -1;
    }

    session.state = NET_SESSION_IDLE;
    session.role = NET_ROLE_NONE;
    session.peer_count = 0;

    log_info("Session disconnected", 0, 0);
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
            send_raw_to_peer(&session.peers[i], NET_MSG_START_GAME,
                           buf, (uint32_t)net_serializer_position(&s));
        }
    }

    log_info("Game started", 0, 0);
    return 1;
}

void net_session_set_game_speed(uint8_t speed)
{
    session.game_speed = speed;
}

void net_session_set_paused(int paused)
{
    session.game_paused = paused;
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

#endif /* ENABLE_MULTIPLAYER */

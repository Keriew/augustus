#include "discovery_lan.h"

#ifdef ENABLE_MULTIPLAYER

#include "transport_tcp.h"
#include "transport_udp.h"
#include "serialize.h"
#include "session.h"
#include "core/log.h"
#include "multiplayer/mp_debug_log.h"

#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

#define ANNOUNCE_PACKET_SIZE 64

/*
 * HOST_EXPIRY_MS must be > 2 * NET_DISCOVERY_BROADCAST_MS to tolerate
 * at least one dropped packet. With broadcast at 2000ms, 8000ms gives
 * a 4:1 ratio — 3 consecutive missed packets before expiry.
 */
#define HOST_EXPIRY_MS 8000

typedef struct {
    int announcing;
    int listening;
    int udp_fd;
    uint32_t last_broadcast_ms;

    /* Announce data */
    char host_name[32];
    uint16_t game_port;
    uint8_t player_count;
    uint8_t max_players;
    uint32_t session_id;

    /* Discovered hosts */
    net_discovered_host hosts[NET_MAX_DISCOVERED_HOSTS];
    int host_count;
} discovery_data;

static discovery_data data;

void net_discovery_init(void)
{
    memset(&data, 0, sizeof(discovery_data));
    data.udp_fd = -1;
    MP_LOG_INFO("DISCOVERY", "Discovery system initialized (port=%d, broadcast_ms=%d, expiry_ms=%d)",
                NET_DISCOVERY_PORT, NET_DISCOVERY_BROADCAST_MS, HOST_EXPIRY_MS);
}

void net_discovery_shutdown(void)
{
    net_discovery_stop_announcing();
    net_discovery_stop_listening();
    MP_LOG_INFO("DISCOVERY", "Discovery system shutdown");
}

int net_discovery_start_announcing(const char *host_name, uint16_t game_port,
                                   uint8_t player_count, uint8_t max_players,
                                   uint32_t session_id)
{
    if (data.announcing) {
        return 1;
    }

    if (data.udp_fd < 0) {
        data.udp_fd = net_udp_create(0); /* Ephemeral port for sending */
        if (data.udp_fd < 0) {
            log_error("Failed to create discovery broadcast socket", 0, 0);
            MP_LOG_ERROR("DISCOVERY", "Failed to create broadcast socket (ephemeral)");
            return 0;
        }
        if (!net_udp_enable_broadcast(data.udp_fd)) {
            MP_LOG_ERROR("DISCOVERY", "Failed to enable broadcast on discovery socket — "
                         "host will not be visible on LAN");
        }
    }

    strncpy(data.host_name, host_name, sizeof(data.host_name) - 1);
    data.host_name[sizeof(data.host_name) - 1] = '\0';
    data.game_port = game_port;
    data.player_count = player_count;
    data.max_players = max_players;
    data.session_id = session_id;
    data.announcing = 1;
    data.last_broadcast_ms = 0;

    log_info("LAN discovery announcing started", host_name, 0);
    MP_LOG_INFO("DISCOVERY", "Start announcing: name='%s' port=%d session=0x%08x players=%d/%d",
                host_name, (int)game_port, session_id, player_count, max_players);
    return 1;
}

void net_discovery_stop_announcing(void)
{
    if (!data.announcing) {
        return;
    }
    data.announcing = 0;
    MP_LOG_INFO("DISCOVERY", "Stop announcing");
    if (!data.listening && data.udp_fd >= 0) {
        net_udp_close(data.udp_fd);
        data.udp_fd = -1;
    }
}

void net_discovery_update_announcing(uint8_t player_count)
{
    data.player_count = player_count;
}

int net_discovery_start_listening(void)
{
    if (data.listening) {
        return 1;
    }

    if (data.udp_fd < 0) {
        data.udp_fd = net_udp_create(NET_DISCOVERY_PORT);
        if (data.udp_fd < 0) {
            log_error("Failed to create discovery listen socket", 0, 0);
            MP_LOG_ERROR("DISCOVERY", "Failed to bind listen socket on port %d — "
                         "another instance may be running or port is blocked by firewall",
                         NET_DISCOVERY_PORT);
            return 0;
        }
        if (!net_udp_enable_broadcast(data.udp_fd)) {
            MP_LOG_WARN("DISCOVERY", "Failed to enable broadcast on listen socket");
        }
    }

    data.listening = 1;
    data.host_count = 0;
    memset(data.hosts, 0, sizeof(data.hosts));

    log_info("LAN discovery listening started", 0, 0);
    MP_LOG_INFO("DISCOVERY", "Start listening on UDP port %d", NET_DISCOVERY_PORT);
    return 1;
}

void net_discovery_stop_listening(void)
{
    if (!data.listening) {
        return;
    }
    data.listening = 0;
    MP_LOG_INFO("DISCOVERY", "Stop listening");
    if (!data.announcing && data.udp_fd >= 0) {
        net_udp_close(data.udp_fd);
        data.udp_fd = -1;
    }
}

static void send_announcement(void)
{
    uint8_t buf[ANNOUNCE_PACKET_SIZE];
    net_serializer s;
    net_serializer_init(&s, buf, sizeof(buf));

    net_write_u32(&s, NET_DISCOVERY_MAGIC);
    net_write_u32(&s, data.session_id);
    net_write_u16(&s, data.game_port);
    net_write_u8(&s, data.player_count);
    net_write_u8(&s, data.max_players);
    net_write_raw(&s, data.host_name, 32);

    if (!net_serializer_has_overflow(&s)) {
        int sent = net_udp_send_broadcast(data.udp_fd, NET_DISCOVERY_PORT,
                                          buf, (size_t)net_serializer_position(&s));
        MP_LOG_TRACE("DISCOVERY", "Broadcast announcement sent (%d bytes, result=%d)",
                     (int)net_serializer_position(&s), sent);
    }
}

static void process_announcement(const uint8_t *buf, size_t size, const net_udp_addr *from)
{
    if (size < 44) {
        MP_LOG_WARN("DISCOVERY", "Received undersized announcement: %d bytes (need >= 44)", (int)size);
        return;
    }

    net_serializer s;
    net_serializer_init(&s, (uint8_t *)buf, size);

    uint32_t magic = net_read_u32(&s);
    if (magic != NET_DISCOVERY_MAGIC) {
        MP_LOG_DEBUG("DISCOVERY", "Received packet with wrong magic: 0x%08x (expected 0x%08x)",
                     magic, NET_DISCOVERY_MAGIC);
        return;
    }

    uint32_t sess_id = net_read_u32(&s);
    uint16_t game_port = net_read_u16(&s);
    uint8_t player_count = net_read_u8(&s);
    uint8_t max_players = net_read_u8(&s);
    char host_name[32];
    net_read_raw(&s, host_name, 32);
    host_name[31] = '\0';

    /* Extract sender IP directly from the UDP address using inet_ntop.
     * This avoids the old colon-stripping hack which would break IPv6. */
    char sender_ip[INET_ADDRSTRLEN];
    struct in_addr sender_in;
    sender_in.s_addr = from->addr;
    inet_ntop(AF_INET, &sender_in, sender_ip, sizeof(sender_ip));

    uint32_t now = net_tcp_get_timestamp_ms();

    /* Ignore our own announcements */
    if (data.announcing && data.session_id == sess_id) {
        return;
    }

    /* Check if we already know this host */
    for (int i = 0; i < NET_MAX_DISCOVERED_HOSTS; i++) {
        if (data.hosts[i].active && data.hosts[i].session_id == sess_id) {
            data.hosts[i].player_count = player_count;
            data.hosts[i].last_seen_ms = now;
            MP_LOG_TRACE("DISCOVERY", "Updated known host '%s' at %s (players=%d/%d)",
                         host_name, sender_ip, player_count, max_players);
            return;
        }
    }

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < NET_MAX_DISCOVERED_HOSTS; i++) {
        if (!data.hosts[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        /* Replace oldest */
        uint32_t oldest_time = now;
        for (int i = 0; i < NET_MAX_DISCOVERED_HOSTS; i++) {
            if (data.hosts[i].last_seen_ms < oldest_time) {
                oldest_time = data.hosts[i].last_seen_ms;
                slot = i;
            }
        }
    }

    if (slot >= 0) {
        net_discovered_host *h = &data.hosts[slot];
        h->active = 1;
        strncpy(h->host_name, host_name, sizeof(h->host_name) - 1);
        h->host_name[sizeof(h->host_name) - 1] = '\0';
        /* Store clean IP (no port suffix) from inet_ntop */
        strncpy(h->host_ip, sender_ip, sizeof(h->host_ip) - 1);
        h->host_ip[sizeof(h->host_ip) - 1] = '\0';
        h->port = game_port;
        h->player_count = player_count;
        h->max_players = max_players;
        h->session_id = sess_id;
        h->last_seen_ms = now;
        if (slot >= data.host_count) {
            data.host_count = slot + 1;
        }

        MP_LOG_INFO("DISCOVERY", "New host discovered: '%s' at %s:%d (session=0x%08x, players=%d/%d)",
                    host_name, sender_ip, (int)game_port, sess_id, player_count, max_players);
    }
}

static void expire_old_hosts(uint32_t now)
{
    for (int i = 0; i < NET_MAX_DISCOVERED_HOSTS; i++) {
        if (data.hosts[i].active && (now - data.hosts[i].last_seen_ms) > HOST_EXPIRY_MS) {
            MP_LOG_INFO("DISCOVERY", "Host expired: '%s' at %s (last seen %ums ago)",
                        data.hosts[i].host_name, data.hosts[i].host_ip,
                        now - data.hosts[i].last_seen_ms);
            data.hosts[i].active = 0;
        }
    }
    /* Recalculate host_count */
    data.host_count = 0;
    for (int i = 0; i < NET_MAX_DISCOVERED_HOSTS; i++) {
        if (data.hosts[i].active && i >= data.host_count) {
            data.host_count = i + 1;
        }
    }
}

void net_discovery_update(void)
{
    if (data.udp_fd < 0) {
        return;
    }

    uint32_t now = net_tcp_get_timestamp_ms();

    /* Announce if hosting */
    if (data.announcing) {
        if (now - data.last_broadcast_ms > NET_DISCOVERY_BROADCAST_MS) {
            send_announcement();
            data.last_broadcast_ms = now;
        }
    }

    /* Receive if listening */
    if (data.listening) {
        uint8_t recv_buf[ANNOUNCE_PACKET_SIZE + 16];
        net_udp_addr from;

        int received = net_udp_recv(data.udp_fd, &from, recv_buf, sizeof(recv_buf));
        while (received > 0) {
            process_announcement(recv_buf, (size_t)received, &from);
            received = net_udp_recv(data.udp_fd, &from, recv_buf, sizeof(recv_buf));
        }

        expire_old_hosts(now);
    }
}

int net_discovery_get_host_count(void)
{
    int count = 0;
    for (int i = 0; i < NET_MAX_DISCOVERED_HOSTS; i++) {
        if (data.hosts[i].active) {
            count++;
        }
    }
    return count;
}

const net_discovered_host *net_discovery_get_host(int index)
{
    if (index < 0 || index >= NET_MAX_DISCOVERED_HOSTS) {
        return NULL;
    }
    return &data.hosts[index];
}

void net_discovery_clear_hosts(void)
{
    memset(data.hosts, 0, sizeof(data.hosts));
    data.host_count = 0;
}

#endif /* ENABLE_MULTIPLAYER */

#include "peer.h"

#ifdef ENABLE_MULTIPLAYER

#include <string.h>

void net_peer_init(net_peer *peer)
{
    memset(peer, 0, sizeof(net_peer));
    peer->socket_fd = -1;
    peer->quality = PEER_QUALITY_UNKNOWN;
    net_codec_init(&peer->codec);
}

void net_peer_reset(net_peer *peer)
{
    int fd = peer->socket_fd;
    net_peer_init(peer);
    /* Socket should be closed by caller before reset */
    (void)fd;
}

void net_peer_set_connected(net_peer *peer, int socket_fd, const char *name)
{
    peer->active = 1;
    peer->socket_fd = socket_fd;
    peer->state = PEER_STATE_CONNECTING;
    if (name) {
        strncpy(peer->name, name, NET_MAX_PLAYER_NAME - 1);
        peer->name[NET_MAX_PLAYER_NAME - 1] = '\0';
    }
}

void net_peer_set_player_id(net_peer *peer, uint8_t player_id)
{
    peer->player_id = player_id;
}

void net_peer_update_heartbeat_sent(net_peer *peer, uint32_t timestamp_ms)
{
    peer->last_heartbeat_sent_ms = timestamp_ms;
}

void net_peer_update_heartbeat_recv(net_peer *peer, uint32_t timestamp_ms)
{
    if (peer->last_heartbeat_sent_ms > 0 && timestamp_ms >= peer->last_heartbeat_sent_ms) {
        uint32_t rtt = timestamp_ms - peer->last_heartbeat_sent_ms;
        if (peer->rtt_smoothed_ms == 0) {
            peer->rtt_smoothed_ms = rtt;
        } else {
            /* Exponential moving average: 7/8 old + 1/8 new */
            peer->rtt_smoothed_ms = (peer->rtt_smoothed_ms * 7 + rtt) / 8;
        }
        peer->rtt_ms = rtt;
    }
    peer->last_heartbeat_recv_ms = timestamp_ms;
}

void net_peer_update_quality(net_peer *peer)
{
    if (peer->rtt_smoothed_ms == 0) {
        peer->quality = PEER_QUALITY_UNKNOWN;
    } else if (peer->rtt_smoothed_ms < 50) {
        peer->quality = PEER_QUALITY_GOOD;
    } else if (peer->rtt_smoothed_ms < 150) {
        peer->quality = PEER_QUALITY_DEGRADED;
    } else if (peer->rtt_smoothed_ms < 400) {
        peer->quality = PEER_QUALITY_POOR;
    } else {
        peer->quality = PEER_QUALITY_CRITICAL;
    }
}

int net_peer_is_timed_out(const net_peer *peer, uint32_t current_ms)
{
    if (!peer->active || peer->state == PEER_STATE_DISCONNECTED) {
        return 0;
    }
    if (peer->last_heartbeat_recv_ms == 0) {
        /* Never received a heartbeat - use connection time approximation */
        return 0;
    }
    return (current_ms - peer->last_heartbeat_recv_ms) > NET_TIMEOUT_MS;
}

static const char *PEER_STATE_NAMES[] = {
    "DISCONNECTED",
    "CONNECTING",
    "HELLO_SENT",
    "JOINED",
    "READY",
    "IN_GAME",
    "DESYNCED",
    "DISCONNECTING"
};

const char *net_peer_state_name(net_peer_state state)
{
    if (state < 0 || state > PEER_STATE_DISCONNECTING) {
        return "UNKNOWN";
    }
    return PEER_STATE_NAMES[state];
}

static const char *PEER_QUALITY_NAMES[] = {
    "UNKNOWN",
    "GOOD",
    "DEGRADED",
    "POOR",
    "CRITICAL"
};

const char *net_peer_quality_name(net_peer_quality quality)
{
    if (quality < 0 || quality > PEER_QUALITY_CRITICAL) {
        return "UNKNOWN";
    }
    return PEER_QUALITY_NAMES[quality];
}

#endif /* ENABLE_MULTIPLAYER */

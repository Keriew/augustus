#ifndef NETWORK_PEER_H
#define NETWORK_PEER_H

#ifdef ENABLE_MULTIPLAYER

#include "protocol.h"
#include "packet_codec.h"

#include <stdint.h>

typedef enum {
    PEER_STATE_DISCONNECTED = 0,
    PEER_STATE_CONNECTING,
    PEER_STATE_HELLO_SENT,
    PEER_STATE_JOINED,
    PEER_STATE_READY,
    PEER_STATE_IN_GAME,
    PEER_STATE_DESYNCED,
    PEER_STATE_DISCONNECTING
} net_peer_state;

typedef enum {
    PEER_QUALITY_UNKNOWN = 0,
    PEER_QUALITY_GOOD,
    PEER_QUALITY_DEGRADED,
    PEER_QUALITY_POOR,
    PEER_QUALITY_CRITICAL
} net_peer_quality;

typedef struct {
    int active;
    int socket_fd;
    uint8_t player_id;
    net_peer_state state;
    char name[NET_MAX_PLAYER_NAME];

    /* Codec per peer for framing/sequencing */
    net_packet_codec codec;

    /* Connection quality tracking */
    uint32_t last_heartbeat_sent_ms;
    uint32_t last_heartbeat_recv_ms;
    uint32_t rtt_ms;
    uint32_t rtt_smoothed_ms;
    net_peer_quality quality;

    /* Statistics */
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint32_t packets_sent;
    uint32_t packets_received;
    uint32_t packets_lost;

    /* Send buffer for outgoing data */
    uint8_t send_buffer[NET_FRAME_PREFIX_SIZE + NET_MAX_FRAME_SIZE];
} net_peer;

#define NET_MAX_PEERS NET_MAX_PLAYERS

void net_peer_init(net_peer *peer);
void net_peer_reset(net_peer *peer);
void net_peer_set_connected(net_peer *peer, int socket_fd, const char *name);
void net_peer_set_player_id(net_peer *peer, uint8_t player_id);
void net_peer_update_heartbeat_sent(net_peer *peer, uint32_t timestamp_ms);
void net_peer_update_heartbeat_recv(net_peer *peer, uint32_t timestamp_ms);
void net_peer_update_quality(net_peer *peer);
int net_peer_is_timed_out(const net_peer *peer, uint32_t current_ms);
const char *net_peer_state_name(net_peer_state state);
const char *net_peer_quality_name(net_peer_quality quality);

#endif /* ENABLE_MULTIPLAYER */

#endif /* NETWORK_PEER_H */

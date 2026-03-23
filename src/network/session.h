#ifndef NETWORK_SESSION_H
#define NETWORK_SESSION_H

#ifdef ENABLE_MULTIPLAYER

#include "peer.h"
#include "protocol.h"

#include <stdint.h>

#define NET_DEFAULT_PORT    29073
#define NET_DISCOVERY_PORT  29074

typedef enum {
    NET_SESSION_IDLE = 0,
    NET_SESSION_HOSTING_LOBBY,
    NET_SESSION_HOSTING_GAME,
    NET_SESSION_JOINING,
    NET_SESSION_CLIENT_LOBBY,
    NET_SESSION_CLIENT_GAME,
    NET_SESSION_DISCONNECTING
} net_session_state;

typedef enum {
    NET_ROLE_NONE = 0,
    NET_ROLE_HOST,
    NET_ROLE_CLIENT
} net_session_role;

typedef struct {
    net_session_state state;
    net_session_role role;
    uint32_t session_id;
    uint8_t local_player_id;

    /* Network */
    int listen_fd;
    int udp_fd;
    uint16_t port;

    /* Host: connected peers */
    net_peer peers[NET_MAX_PEERS];
    int peer_count;

    /* Client: connection to host */
    net_peer host_peer;

    /* Local player info */
    char local_player_name[NET_MAX_PLAYER_NAME];

    /* Game state */
    uint32_t authoritative_tick;
    uint32_t local_tick;
    uint8_t game_speed;
    int game_paused;

    /* Timing */
    uint32_t last_heartbeat_ms;
    uint32_t last_checksum_tick;
} net_session;

/* Lifecycle */
int net_session_init(void);
void net_session_shutdown(void);

/* Host operations */
int net_session_host(const char *player_name, uint16_t port);
void net_session_kick_peer(uint8_t player_id);

/* Client operations */
int net_session_join(const char *player_name, const char *host_address, uint16_t port);

/* Common operations */
void net_session_disconnect(void);
void net_session_update(void);

/* State queries */
int net_session_is_active(void);
int net_session_is_host(void);
int net_session_is_client(void);
int net_session_is_in_game(void);
int net_session_is_in_lobby(void);
net_session_state net_session_get_state(void);
uint8_t net_session_get_local_player_id(void);
uint32_t net_session_get_authoritative_tick(void);
const char *net_session_state_name(net_session_state state);

/* Peer queries */
int net_session_get_peer_count(void);
const net_peer *net_session_get_peer(int index);
const net_peer *net_session_get_host_peer(void);

/* Game control (host only) */
int net_session_start_game(void);
void net_session_set_game_speed(uint8_t speed);
void net_session_set_paused(int paused);
void net_session_advance_tick(void);

/* Message sending */
int net_session_send_to_host(uint16_t message_type, const uint8_t *payload, uint32_t size);
int net_session_send_to_peer(int peer_index, uint16_t message_type,
                             const uint8_t *payload, uint32_t size);
int net_session_broadcast(uint16_t message_type, const uint8_t *payload, uint32_t size);

/* Ready state */
void net_session_set_ready(int is_ready);
int net_session_all_peers_ready(void);

/* Access to session for multiplayer integration */
net_session *net_session_get(void);

#endif /* ENABLE_MULTIPLAYER */

#endif /* NETWORK_SESSION_H */

#ifndef NETWORK_DISCOVERY_LAN_H
#define NETWORK_DISCOVERY_LAN_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>

#define NET_DISCOVERY_MAGIC         0x41554744  /* "AUGD" */
#define NET_MAX_DISCOVERED_HOSTS    16
#define NET_DISCOVERY_BROADCAST_MS  2000

typedef struct {
    int active;
    char host_name[32];
    char host_ip[48];
    uint16_t port;
    uint8_t player_count;
    uint8_t max_players;
    uint32_t session_id;
    uint32_t last_seen_ms;
} net_discovered_host;

void net_discovery_init(void);
void net_discovery_shutdown(void);

/* Host: start announcing on LAN */
int net_discovery_start_announcing(const char *host_name, uint16_t game_port,
                                   uint8_t player_count, uint8_t max_players,
                                   uint32_t session_id);
void net_discovery_stop_announcing(void);
void net_discovery_update_announcing(uint8_t player_count);

/* Client: start listening for announcements */
int net_discovery_start_listening(void);
void net_discovery_stop_listening(void);

/* Update must be called each frame */
void net_discovery_update(void);

/* Query discovered hosts */
int net_discovery_get_host_count(void);
const net_discovered_host *net_discovery_get_host(int index);
void net_discovery_clear_hosts(void);

#endif /* ENABLE_MULTIPLAYER */

#endif /* NETWORK_DISCOVERY_LAN_H */

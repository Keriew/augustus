#ifndef NETWORK_TRANSPORT_UDP_H
#define NETWORK_TRANSPORT_UDP_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>
#include <stddef.h>

/**
 * UDP transport layer.
 * Used for heartbeats, delta snapshots, and low-latency events.
 * Also used for LAN discovery broadcasts.
 */

typedef struct {
    uint32_t addr;    /* network byte order */
    uint16_t port;    /* network byte order */
} net_udp_addr;

int net_udp_init(void);
void net_udp_shutdown(void);

int net_udp_create(uint16_t port);
void net_udp_close(int socket_fd);

int net_udp_enable_broadcast(int socket_fd);

int net_udp_send(int socket_fd, const net_udp_addr *dest,
                 const uint8_t *data, size_t size);
int net_udp_recv(int socket_fd, net_udp_addr *from,
                 uint8_t *buffer, size_t buffer_size);

int net_udp_send_broadcast(int socket_fd, uint16_t port,
                           const uint8_t *data, size_t size);

void net_udp_addr_set(net_udp_addr *addr, const char *host, uint16_t port);
int net_udp_addr_equal(const net_udp_addr *a, const net_udp_addr *b);
void net_udp_addr_to_string(const net_udp_addr *addr, char *buffer, size_t buffer_size);

#endif /* ENABLE_MULTIPLAYER */

#endif /* NETWORK_TRANSPORT_UDP_H */

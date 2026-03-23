#ifndef NETWORK_TRANSPORT_TCP_H
#define NETWORK_TRANSPORT_TCP_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>
#include <stddef.h>

/**
 * TCP transport layer.
 * Handles socket creation, connection, sending and receiving of raw bytes.
 * Platform differences (winsock vs posix) are abstracted here.
 */

/* Must be called once at startup on Windows (WSAStartup) */
int net_tcp_init(void);
void net_tcp_shutdown(void);

/* Server operations */
int net_tcp_listen(uint16_t port);
int net_tcp_accept(int listen_fd);
void net_tcp_set_nonblocking(int socket_fd);

/* Client operations */
int net_tcp_connect(const char *host, uint16_t port);

/* Data transfer - non-blocking */
int net_tcp_send(int socket_fd, const uint8_t *data, size_t size);
int net_tcp_recv(int socket_fd, uint8_t *buffer, size_t buffer_size);

/* Utility */
void net_tcp_close(int socket_fd);
int net_tcp_is_valid(int socket_fd);
uint32_t net_tcp_get_timestamp_ms(void);

/* Address info */
int net_tcp_get_local_ip(char *buffer, size_t buffer_size);

#endif /* ENABLE_MULTIPLAYER */

#endif /* NETWORK_TRANSPORT_TCP_H */

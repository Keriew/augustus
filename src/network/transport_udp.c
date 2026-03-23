#include "transport_udp.h"

#ifdef ENABLE_MULTIPLAYER

#include "core/log.h"

#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#define NET_WOULD_BLOCK (WSAGetLastError() == WSAEWOULDBLOCK)
#define NET_CLOSE_SOCKET closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#define NET_WOULD_BLOCK (errno == EAGAIN || errno == EWOULDBLOCK)
#define NET_CLOSE_SOCKET close
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

int net_udp_init(void)
{
    /* WSAStartup handled by net_tcp_init - only need to call once */
    return 1;
}

void net_udp_shutdown(void)
{
    /* Cleanup handled by net_tcp_shutdown */
}

int net_udp_create(uint16_t port)
{
    int fd = (int)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd == INVALID_SOCKET) {
        log_error("Failed to create UDP socket", 0, 0);
        return -1;
    }

    /* Allow address reuse */
    int opt = 1;
#ifdef _WIN32
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
#else
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    if (port > 0) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
            log_error("Failed to bind UDP socket", 0, (int)port);
            NET_CLOSE_SOCKET(fd);
            return -1;
        }
    }

    /* Set non-blocking */
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(fd, FIONBIO, &mode);
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
#endif

    return fd;
}

void net_udp_close(int socket_fd)
{
    if (socket_fd >= 0) {
        NET_CLOSE_SOCKET(socket_fd);
    }
}

int net_udp_enable_broadcast(int socket_fd)
{
    int opt = 1;
#ifdef _WIN32
    int result = setsockopt(socket_fd, SOL_SOCKET, SO_BROADCAST, (const char *)&opt, sizeof(opt));
#else
    int result = setsockopt(socket_fd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
#endif
    if (result == SOCKET_ERROR) {
        log_error("Failed to enable UDP broadcast", 0, 0);
        return 0;
    }
    return 1;
}

int net_udp_send(int socket_fd, const net_udp_addr *dest,
                 const uint8_t *data, size_t size)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = dest->addr;
    addr.sin_port = dest->port;

    int sent = sendto(socket_fd, (const char *)data, (int)size, 0,
                      (struct sockaddr *)&addr, sizeof(addr));
    if (sent == SOCKET_ERROR) {
        if (NET_WOULD_BLOCK) {
            return 0;
        }
        return -1;
    }
    return sent;
}

int net_udp_recv(int socket_fd, net_udp_addr *from,
                 uint8_t *buffer, size_t buffer_size)
{
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    int received = recvfrom(socket_fd, (char *)buffer, (int)buffer_size, 0,
                            (struct sockaddr *)&addr, &addr_len);
    if (received == SOCKET_ERROR) {
        if (NET_WOULD_BLOCK) {
            return 0;
        }
        return -1;
    }

    if (from) {
        from->addr = addr.sin_addr.s_addr;
        from->port = addr.sin_port;
    }

    return received;
}

int net_udp_send_broadcast(int socket_fd, uint16_t port,
                           const uint8_t *data, size_t size)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_BROADCAST;
    addr.sin_port = htons(port);

    int sent = sendto(socket_fd, (const char *)data, (int)size, 0,
                      (struct sockaddr *)&addr, sizeof(addr));
    if (sent == SOCKET_ERROR) {
        if (NET_WOULD_BLOCK) {
            return 0;
        }
        return -1;
    }
    return sent;
}

void net_udp_addr_set(net_udp_addr *addr, const char *host, uint16_t port)
{
    addr->addr = inet_addr(host);
    addr->port = htons(port);
}

int net_udp_addr_equal(const net_udp_addr *a, const net_udp_addr *b)
{
    return a->addr == b->addr && a->port == b->port;
}

void net_udp_addr_to_string(const net_udp_addr *addr, char *buffer, size_t buffer_size)
{
    struct in_addr in;
    in.s_addr = addr->addr;
    const char *ip = inet_ntoa(in);
    snprintf(buffer, buffer_size, "%s:%u", ip, ntohs(addr->port));
}

#endif /* ENABLE_MULTIPLAYER */

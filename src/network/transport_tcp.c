#include "transport_tcp.h"

#ifdef ENABLE_MULTIPLAYER

#include "core/log.h"
#include "protocol.h"
#include "multiplayer/mp_debug_log.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#define NET_WOULD_BLOCK (WSAGetLastError() == WSAEWOULDBLOCK)
#define NET_CLOSE_SOCKET closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#define NET_WOULD_BLOCK (errno == EAGAIN || errno == EWOULDBLOCK)
#define NET_CLOSE_SOCKET close
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

#include <time.h>

#ifdef _WIN32
#include <sysinfoapi.h>
#endif

static int tcp_initialized = 0;

int net_tcp_init(void)
{
    if (tcp_initialized) {
        return 1;
    }
#ifdef _WIN32
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
        log_error("WSAStartup failed", 0, result);
        return 0;
    }
#endif
    tcp_initialized = 1;
    return 1;
}

void net_tcp_shutdown(void)
{
    if (!tcp_initialized) {
        return;
    }
#ifdef _WIN32
    WSACleanup();
#endif
    tcp_initialized = 0;
}

int net_tcp_listen(uint16_t port)
{
    int fd = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == INVALID_SOCKET) {
        log_error("Failed to create listen socket", 0, 0);
        return -1;
    }

    /* Allow address reuse */
    int opt = 1;
#ifdef _WIN32
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
#else
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
#ifdef _WIN32
        MP_LOG_ERROR("NET", "TCP bind failed on port %d, WSAError=%d", (int)port, WSAGetLastError());
#else
        MP_LOG_ERROR("NET", "TCP bind failed on port %d, errno=%d", (int)port, errno);
#endif
        log_error("Failed to bind listen socket", 0, (int)port);
        NET_CLOSE_SOCKET(fd);
        return -1;
    }

    if (listen(fd, NET_MAX_PLAYERS) == SOCKET_ERROR) {
#ifdef _WIN32
        MP_LOG_ERROR("NET", "TCP listen failed on port %d, WSAError=%d", (int)port, WSAGetLastError());
#else
        MP_LOG_ERROR("NET", "TCP listen failed on port %d, errno=%d", (int)port, errno);
#endif
        log_error("Failed to listen on socket", 0, (int)port);
        NET_CLOSE_SOCKET(fd);
        return -1;
    }

    net_tcp_set_nonblocking(fd);

    log_info("TCP listening on port", 0, (int)port);
    MP_LOG_INFO("NET", "TCP listening on port %d (fd=%d, backlog=%d)",
                (int)port, fd, NET_MAX_PLAYERS);
    return fd;
}

int net_tcp_accept(int listen_fd)
{
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int client_fd = (int)accept(listen_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd == INVALID_SOCKET) {
        if (NET_WOULD_BLOCK) {
            return -1; /* No pending connections */
        }
        log_error("Accept failed", 0, 0);
        return -1;
    }

    net_tcp_set_nonblocking(client_fd);

    /* Disable Nagle's algorithm for lower latency */
    int flag = 1;
#ifdef _WIN32
    setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&flag, sizeof(flag));
#else
    setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
#endif

    return client_fd;
}

void net_tcp_set_nonblocking(int socket_fd)
{
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(socket_fd, FIONBIO, &mode);
#else
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
    }
#endif
}

int net_tcp_connect(const char *host, uint16_t port)
{
    struct addrinfo hints, *result, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%u", port);

    int status = getaddrinfo(host, port_str, &hints, &result);
    if (status != 0) {
        log_error("Failed to resolve host", host, 0);
        return -1;
    }

    int fd = -1;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        fd = (int)socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd == INVALID_SOCKET) {
            continue;
        }
        if (connect(fd, rp->ai_addr, (int)rp->ai_addrlen) != SOCKET_ERROR) {
            break; /* Success */
        }
        NET_CLOSE_SOCKET(fd);
        fd = -1;
    }

    freeaddrinfo(result);

    if (fd < 0) {
        log_error("Failed to connect to host", host, (int)port);
        MP_LOG_ERROR("NET", "TCP connect failed: host='%s' port=%d — all addresses exhausted", host, (int)port);
        return -1;
    }

    net_tcp_set_nonblocking(fd);

    /* Disable Nagle's algorithm */
    int flag = 1;
#ifdef _WIN32
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&flag, sizeof(flag));
#else
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
#endif

    log_info("TCP connected to host", host, (int)port);
    MP_LOG_INFO("NET", "TCP connected: host='%s' port=%d fd=%d (TCP_NODELAY enabled)", host, (int)port, fd);
    return fd;
}

int net_tcp_send(int socket_fd, const uint8_t *data, size_t size)
{
    int sent = send(socket_fd, (const char *)data, (int)size, 0);
    if (sent == SOCKET_ERROR) {
        if (NET_WOULD_BLOCK) {
            return 0; /* Would block - try again later */
        }
        return -1; /* Error */
    }
    return sent;
}

int net_tcp_recv(int socket_fd, uint8_t *buffer, size_t buffer_size)
{
    int received = recv(socket_fd, (char *)buffer, (int)buffer_size, 0);
    if (received == SOCKET_ERROR) {
        if (NET_WOULD_BLOCK) {
            return 0; /* No data available */
        }
        return -1; /* Error */
    }
    if (received == 0) {
        return -1; /* Connection closed */
    }
    return received;
}

void net_tcp_close(int socket_fd)
{
    if (socket_fd >= 0) {
        NET_CLOSE_SOCKET(socket_fd);
    }
}

int net_tcp_is_valid(int socket_fd)
{
    return socket_fd >= 0;
}

uint32_t net_tcp_get_timestamp_ms(void)
{
#ifdef _WIN32
    return (uint32_t)GetTickCount();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#endif
}

int net_tcp_get_local_ip(char *buffer, size_t buffer_size)
{
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(buffer, "127.0.0.1", buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
        return 0;
    }

    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(hostname, NULL, &hints, &result) != 0) {
        strncpy(buffer, "127.0.0.1", buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
        return 0;
    }

    struct sockaddr_in *addr = (struct sockaddr_in *)result->ai_addr;
    const char *ip = inet_ntoa(addr->sin_addr);
    strncpy(buffer, ip, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';

    freeaddrinfo(result);
    return 1;
}

#endif /* ENABLE_MULTIPLAYER */

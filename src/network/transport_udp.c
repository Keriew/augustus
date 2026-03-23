#include "transport_udp.h"

#ifdef ENABLE_MULTIPLAYER

#include "core/log.h"
#include "multiplayer/mp_debug_log.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
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
#include <ifaddrs.h>
#include <net/if.h>
#define NET_WOULD_BLOCK (errno == EAGAIN || errno == EWOULDBLOCK)
#define NET_CLOSE_SOCKET close
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

/* Cached subnet broadcast addresses */
#define MAX_BROADCAST_ADDRS 8
static struct {
    uint32_t addrs[MAX_BROADCAST_ADDRS]; /* network byte order */
    int count;
    int initialized;
} broadcast_cache;

int net_udp_init(void)
{
    /* WSAStartup handled by net_tcp_init - only need to call once */
    return 1;
}

void net_udp_shutdown(void)
{
    broadcast_cache.initialized = 0;
    broadcast_cache.count = 0;
}

int net_udp_create(uint16_t port)
{
    int fd = (int)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd == INVALID_SOCKET) {
        log_error("Failed to create UDP socket", 0, 0);
        MP_LOG_ERROR("NET", "Failed to create UDP socket (DGRAM)");
        return -1;
    }

    /* Allow address reuse */
    int opt = 1;
#ifdef _WIN32
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt)) == SOCKET_ERROR) {
        MP_LOG_WARN("NET", "setsockopt SO_REUSEADDR failed on UDP socket fd=%d, err=%d", fd, WSAGetLastError());
    }
#else
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        MP_LOG_WARN("NET", "setsockopt SO_REUSEADDR failed on UDP socket fd=%d, errno=%d", fd, errno);
    }
#endif

    if (port > 0) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
#ifdef _WIN32
            MP_LOG_ERROR("NET", "Failed to bind UDP socket to port %d, WSAError=%d", (int)port, WSAGetLastError());
#else
            MP_LOG_ERROR("NET", "Failed to bind UDP socket to port %d, errno=%d", (int)port, errno);
#endif
            log_error("Failed to bind UDP socket", 0, (int)port);
            NET_CLOSE_SOCKET(fd);
            return -1;
        }
        MP_LOG_INFO("NET", "UDP socket bound to port %d (fd=%d)", (int)port, fd);
    } else {
        MP_LOG_DEBUG("NET", "UDP socket created on ephemeral port (fd=%d)", fd);
    }

    /* Set non-blocking */
#ifdef _WIN32
    u_long mode = 1;
    if (ioctlsocket(fd, FIONBIO, &mode) == SOCKET_ERROR) {
        MP_LOG_WARN("NET", "ioctlsocket FIONBIO failed on UDP fd=%d, err=%d", fd, WSAGetLastError());
    }
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
            MP_LOG_WARN("NET", "fcntl O_NONBLOCK failed on UDP fd=%d, errno=%d", fd, errno);
        }
    }
#endif

    return fd;
}

void net_udp_close(int socket_fd)
{
    if (socket_fd >= 0) {
        MP_LOG_DEBUG("NET", "Closing UDP socket fd=%d", socket_fd);
        NET_CLOSE_SOCKET(socket_fd);
    }
}

int net_udp_enable_broadcast(int socket_fd)
{
    int opt = 1;
#ifdef _WIN32
    int result = setsockopt(socket_fd, SOL_SOCKET, SO_BROADCAST, (const char *)&opt, sizeof(opt));
    if (result == SOCKET_ERROR) {
        int err = WSAGetLastError();
        log_error("Failed to enable UDP broadcast", 0, err);
        MP_LOG_ERROR("NET", "setsockopt SO_BROADCAST failed fd=%d, WSAError=%d", socket_fd, err);
        return 0;
    }
#else
    int result = setsockopt(socket_fd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
    if (result == SOCKET_ERROR) {
        log_error("Failed to enable UDP broadcast", 0, errno);
        MP_LOG_ERROR("NET", "setsockopt SO_BROADCAST failed fd=%d, errno=%d", socket_fd, errno);
        return 0;
    }
#endif
    MP_LOG_INFO("NET", "Broadcast enabled on UDP socket fd=%d", socket_fd);
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

/**
 * Enumerate local network interfaces and cache subnet broadcast addresses.
 * Uses GetAdaptersInfo on Windows and getifaddrs on Linux/macOS.
 * Falls back to 255.255.255.255 if enumeration fails.
 */
static void compute_subnet_broadcasts(void)
{
    broadcast_cache.count = 0;
    broadcast_cache.initialized = 1;

#ifdef _WIN32
    ULONG buf_size = 0;
    GetAdaptersInfo(NULL, &buf_size);
    if (buf_size == 0) {
        MP_LOG_WARN("DISCOVERY", "GetAdaptersInfo returned 0 size, using global broadcast");
        broadcast_cache.addrs[broadcast_cache.count++] = INADDR_BROADCAST;
        return;
    }

    IP_ADAPTER_INFO *adapters = (IP_ADAPTER_INFO *)malloc(buf_size);
    if (!adapters) {
        broadcast_cache.addrs[broadcast_cache.count++] = INADDR_BROADCAST;
        return;
    }

    DWORD result = GetAdaptersInfo(adapters, &buf_size);
    if (result != ERROR_SUCCESS) {
        MP_LOG_WARN("DISCOVERY", "GetAdaptersInfo failed with error %lu", (unsigned long)result);
        free(adapters);
        broadcast_cache.addrs[broadcast_cache.count++] = INADDR_BROADCAST;
        return;
    }

    IP_ADAPTER_INFO *adapter = adapters;
    while (adapter && broadcast_cache.count < MAX_BROADCAST_ADDRS) {
        /* Skip loopback and non-operational adapters */
        if (adapter->Type == MIB_IF_TYPE_LOOPBACK) {
            adapter = adapter->Next;
            continue;
        }

        uint32_t ip = inet_addr(adapter->IpAddressList.IpAddress.String);
        uint32_t mask = inet_addr(adapter->IpAddressList.IpMask.String);

        /* Skip 0.0.0.0 addresses */
        if (ip == 0 || ip == INADDR_NONE) {
            adapter = adapter->Next;
            continue;
        }

        uint32_t bcast = (ip & mask) | (~mask);
        broadcast_cache.addrs[broadcast_cache.count] = bcast;

        char ip_str[INET_ADDRSTRLEN];
        char bcast_str[INET_ADDRSTRLEN];
        struct in_addr ip_in, bcast_in;
        ip_in.s_addr = ip;
        bcast_in.s_addr = bcast;
        inet_ntop(AF_INET, &ip_in, ip_str, sizeof(ip_str));
        inet_ntop(AF_INET, &bcast_in, bcast_str, sizeof(bcast_str));
        MP_LOG_INFO("DISCOVERY", "Interface %s: IP=%s broadcast=%s",
                    adapter->Description, ip_str, bcast_str);

        broadcast_cache.count++;
        adapter = adapter->Next;
    }
    free(adapters);
#else
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        MP_LOG_WARN("DISCOVERY", "getifaddrs failed, errno=%d, using global broadcast", errno);
        broadcast_cache.addrs[broadcast_cache.count++] = INADDR_BROADCAST;
        return;
    }

    for (ifa = ifaddr; ifa != NULL && broadcast_cache.count < MAX_BROADCAST_ADDRS; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        if (!ifa->ifa_netmask || !ifa->ifa_broadaddr) {
            continue;
        }
        /* Skip loopback */
        if (ifa->ifa_flags & IFF_LOOPBACK) {
            continue;
        }

        struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
        struct sockaddr_in *bcast_sa = (struct sockaddr_in *)ifa->ifa_broadaddr;

        uint32_t ip = sa->sin_addr.s_addr;
        uint32_t bcast = bcast_sa->sin_addr.s_addr;

        if (ip == 0 || ip == htonl(INADDR_LOOPBACK)) {
            continue;
        }

        broadcast_cache.addrs[broadcast_cache.count] = bcast;

        char ip_str[INET_ADDRSTRLEN];
        char bcast_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sa->sin_addr, ip_str, sizeof(ip_str));
        inet_ntop(AF_INET, &bcast_sa->sin_addr, bcast_str, sizeof(bcast_str));
        MP_LOG_INFO("DISCOVERY", "Interface %s: IP=%s broadcast=%s",
                    ifa->ifa_name, ip_str, bcast_str);

        broadcast_cache.count++;
    }
    freeifaddrs(ifaddr);
#endif

    /* Always include global broadcast as fallback */
    if (broadcast_cache.count == 0) {
        MP_LOG_WARN("DISCOVERY", "No subnet broadcasts found, using global 255.255.255.255");
        broadcast_cache.addrs[broadcast_cache.count++] = INADDR_BROADCAST;
    }
}

int net_udp_send_broadcast(int socket_fd, uint16_t port,
                           const uint8_t *data, size_t size)
{
    /* Compute subnet broadcasts on first call */
    if (!broadcast_cache.initialized) {
        compute_subnet_broadcasts();
    }

    int total_sent = 0;
    for (int i = 0; i < broadcast_cache.count; i++) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = broadcast_cache.addrs[i];
        addr.sin_port = htons(port);

        int sent = sendto(socket_fd, (const char *)data, (int)size, 0,
                          (struct sockaddr *)&addr, sizeof(addr));
        if (sent == SOCKET_ERROR) {
            if (!NET_WOULD_BLOCK) {
                char bcast_str[INET_ADDRSTRLEN];
                struct in_addr bcast_in;
                bcast_in.s_addr = broadcast_cache.addrs[i];
                inet_ntop(AF_INET, &bcast_in, bcast_str, sizeof(bcast_str));
                MP_LOG_WARN("DISCOVERY", "sendto broadcast %s:%d failed", bcast_str, (int)port);
            }
        } else {
            total_sent += sent;
        }
    }
    return total_sent > 0 ? total_sent : -1;
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
    /* Use inet_ntop instead of deprecated inet_ntoa (thread-safe) */
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &in, ip_str, sizeof(ip_str));
    snprintf(buffer, buffer_size, "%s:%u", ip_str, ntohs(addr->port));
}

#endif /* ENABLE_MULTIPLAYER */

#include "protocol.h"

#ifdef ENABLE_MULTIPLAYER

static const char *MESSAGE_NAMES[] = {
    "UNKNOWN",
    "HELLO",
    "JOIN_REQUEST",
    "JOIN_ACCEPT",
    "JOIN_REJECT",
    "READY_STATE",
    "START_GAME",
    "CLIENT_COMMAND",
    "HOST_COMMAND_ACK",
    "FULL_SNAPSHOT",
    "DELTA_SNAPSHOT",
    "HOST_EVENT",
    "CHECKSUM_REQUEST",
    "CHECKSUM_RESPONSE",
    "RESYNC_REQUEST",
    "RESYNC_GRANTED",
    "DISCONNECT_NOTICE",
    "HEARTBEAT",
    "CHAT",
    "GAME_PREPARE",
    "GAME_LOAD_COMPLETE",
    "GAME_START_FINAL"
};

int net_protocol_validate_header(const net_packet_header *header)
{
    if (!header) {
        return 0;
    }
    if (header->protocol_version != NET_PROTOCOL_VERSION) {
        return 0;
    }
    if (header->message_type == 0 || header->message_type >= NET_MSG_COUNT) {
        return 0;
    }
    if (header->payload_size > NET_MAX_PAYLOAD_SIZE) {
        return 0;
    }
    return 1;
}

int net_protocol_check_version(uint16_t remote_version)
{
    return remote_version == NET_PROTOCOL_VERSION;
}

const char *net_protocol_message_name(net_message_type type)
{
    if (type <= 0 || type >= NET_MSG_COUNT) {
        return MESSAGE_NAMES[0];
    }
    return MESSAGE_NAMES[type];
}

#endif /* ENABLE_MULTIPLAYER */

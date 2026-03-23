#ifndef NETWORK_PACKET_CODEC_H
#define NETWORK_PACKET_CODEC_H

#ifdef ENABLE_MULTIPLAYER

#include "protocol.h"
#include "serialize.h"

#include <stdint.h>
#include <stddef.h>

/**
 * Packet framing and codec.
 *
 * Wire format:
 *   [4 bytes frame_magic] [4 bytes frame_size] [NET_PACKET_HEADER_SIZE header] [payload]
 *
 * frame_size includes header + payload (not the 8-byte frame prefix).
 */

#define NET_FRAME_MAGIC         0x46524D45  /* "FRME" */
#define NET_FRAME_PREFIX_SIZE   8
#define NET_MAX_FRAME_SIZE      (NET_PACKET_HEADER_SIZE + NET_MAX_PAYLOAD_SIZE)

typedef enum {
    CODEC_OK = 0,
    CODEC_NEED_MORE_DATA,
    CODEC_INVALID_FRAME,
    CODEC_OVERFLOW,
    CODEC_ERROR
} net_codec_result;

typedef struct {
    uint8_t recv_buffer[NET_FRAME_PREFIX_SIZE + NET_MAX_FRAME_SIZE];
    size_t recv_fill;
    uint32_t next_send_sequence;
    uint32_t last_recv_sequence;
} net_packet_codec;

void net_codec_init(net_packet_codec *codec);
void net_codec_reset(net_packet_codec *codec);

/**
 * Encode a message into a send buffer.
 * Returns total bytes written (frame prefix + header + payload), or 0 on error.
 */
size_t net_codec_encode(net_packet_codec *codec,
                        uint16_t message_type,
                        uint32_t session_id,
                        uint32_t game_tick,
                        const uint8_t *payload,
                        uint32_t payload_size,
                        uint8_t *out_buffer,
                        size_t out_capacity);

/**
 * Feed received data into the codec's internal buffer.
 * Returns bytes consumed (may be less than available if buffer is full).
 */
size_t net_codec_feed(net_packet_codec *codec, const uint8_t *data, size_t size);

/**
 * Try to extract a complete packet from the codec's internal buffer.
 * On success, fills header and sets payload_out to point within recv_buffer.
 * Returns CODEC_OK if a packet was extracted, CODEC_NEED_MORE_DATA if incomplete.
 */
net_codec_result net_codec_decode(net_packet_codec *codec,
                                  net_packet_header *header,
                                  const uint8_t **payload_out,
                                  uint32_t *payload_size_out);

#endif /* ENABLE_MULTIPLAYER */

#endif /* NETWORK_PACKET_CODEC_H */

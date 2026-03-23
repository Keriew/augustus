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

    /**
     * Stable decode buffer: payload is copied here BEFORE the recv_buffer
     * is compacted by memmove. This guarantees that the pointer returned
     * by net_codec_decode() remains valid until the next decode call on
     * the same codec instance. Ownership: codec owns this buffer.
     * Lifetime: valid until the next call to net_codec_decode() or reset.
     */
    uint8_t decode_buffer[NET_MAX_PAYLOAD_SIZE];
    uint32_t decode_payload_size;
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
 * On CODEC_OK: header is filled; payload_out points to a stable internal
 * buffer (decode_buffer) that remains valid until the next call to
 * net_codec_decode() or net_codec_reset() on the same codec instance.
 * Returns CODEC_OK if a packet was extracted, CODEC_NEED_MORE_DATA if incomplete.
 *
 * OWNERSHIP CONTRACT:
 * - The codec owns the payload memory.
 * - The pointer is valid until the next decode/reset call on this codec.
 * - The caller must NOT store or cache the pointer across decode calls.
 * - Each peer has its own codec, so payloads from different peers don't conflict.
 */
net_codec_result net_codec_decode(net_packet_codec *codec,
                                  net_packet_header *header,
                                  const uint8_t **payload_out,
                                  uint32_t *payload_size_out);

#endif /* ENABLE_MULTIPLAYER */

#endif /* NETWORK_PACKET_CODEC_H */

#include "packet_codec.h"

#ifdef ENABLE_MULTIPLAYER

#include "multiplayer/mp_debug_log.h"

#include <string.h>

void net_codec_init(net_packet_codec *codec)
{
    memset(codec, 0, sizeof(net_packet_codec));
    codec->next_send_sequence = 1;
}

void net_codec_reset(net_packet_codec *codec)
{
    codec->recv_fill = 0;
    codec->decode_payload_size = 0;
}

size_t net_codec_encode(net_packet_codec *codec,
                        uint16_t message_type,
                        uint32_t session_id,
                        uint32_t game_tick,
                        const uint8_t *payload,
                        uint32_t payload_size,
                        uint8_t *out_buffer,
                        size_t out_capacity)
{
    uint32_t frame_content_size = NET_PACKET_HEADER_SIZE + payload_size;
    size_t total_size = NET_FRAME_PREFIX_SIZE + frame_content_size;

    if (total_size > out_capacity) {
        return 0;
    }
    if (payload_size > NET_MAX_PAYLOAD_SIZE) {
        return 0;
    }

    net_serializer s;
    net_serializer_init(&s, out_buffer, out_capacity);

    /* Frame prefix */
    net_write_u32(&s, NET_FRAME_MAGIC);
    net_write_u32(&s, frame_content_size);

    /* Packet header */
    net_packet_header header;
    header.protocol_version = NET_PROTOCOL_VERSION;
    header.message_type = message_type;
    header.sequence_id = codec->next_send_sequence++;
    header.ack_sequence_id = codec->last_recv_sequence;
    header.session_id = session_id;
    header.game_tick = game_tick;
    header.payload_size = payload_size;

    net_write_packet_header(&s, &header);

    /* Payload */
    if (payload && payload_size > 0) {
        net_write_raw(&s, payload, payload_size);
    }

    if (net_serializer_has_overflow(&s)) {
        return 0;
    }

    return total_size;
}

size_t net_codec_feed(net_packet_codec *codec, const uint8_t *data, size_t size)
{
    size_t buffer_capacity = sizeof(codec->recv_buffer);
    size_t space = buffer_capacity - codec->recv_fill;
    size_t to_copy = size;
    if (to_copy > space) {
        to_copy = space;
    }
    if (to_copy > 0) {
        memcpy(codec->recv_buffer + codec->recv_fill, data, to_copy);
        codec->recv_fill += to_copy;
    }
    return to_copy;
}

net_codec_result net_codec_decode(net_packet_codec *codec,
                                  net_packet_header *header,
                                  const uint8_t **payload_out,
                                  uint32_t *payload_size_out)
{
    *payload_out = NULL;
    *payload_size_out = 0;

    /* Need at least the frame prefix */
    if (codec->recv_fill < NET_FRAME_PREFIX_SIZE) {
        return CODEC_NEED_MORE_DATA;
    }

    net_serializer s;
    net_serializer_init(&s, codec->recv_buffer, codec->recv_fill);

    uint32_t magic = net_read_u32(&s);
    uint32_t frame_size = net_read_u32(&s);

    if (magic != NET_FRAME_MAGIC) {
        MP_LOG_WARN("NET", "Invalid frame magic: 0x%08x at recv_fill=%u (expected 0x%08x)",
                    magic, (unsigned)codec->recv_fill, NET_FRAME_MAGIC);
        /* Try to find the next valid frame magic by scanning forward */
        size_t scan = 1;
        int found = 0;
        while (scan + 4 <= codec->recv_fill) {
            uint32_t test_magic = (uint32_t)codec->recv_buffer[scan]
                                | ((uint32_t)codec->recv_buffer[scan + 1] << 8)
                                | ((uint32_t)codec->recv_buffer[scan + 2] << 16)
                                | ((uint32_t)codec->recv_buffer[scan + 3] << 24);
            if (test_magic == NET_FRAME_MAGIC) {
                /* Shift buffer to align on this frame */
                size_t remaining = codec->recv_fill - scan;
                memmove(codec->recv_buffer, codec->recv_buffer + scan, remaining);
                codec->recv_fill = remaining;
                found = 1;
                break;
            }
            scan++;
        }
        if (!found) {
            /* No valid magic found - discard all but last 3 bytes */
            if (codec->recv_fill > 3) {
                size_t keep = 3;
                memmove(codec->recv_buffer, codec->recv_buffer + codec->recv_fill - keep, keep);
                codec->recv_fill = keep;
            }
        }
        return CODEC_INVALID_FRAME;
    }

    if (frame_size > NET_MAX_FRAME_SIZE) {
        MP_LOG_ERROR("NET", "Frame size too large: %u bytes (max %u)", frame_size, NET_MAX_FRAME_SIZE);
        /* Corrupt frame - skip past this magic */
        size_t remaining = codec->recv_fill - 4;
        memmove(codec->recv_buffer, codec->recv_buffer + 4, remaining);
        codec->recv_fill = remaining;
        return CODEC_INVALID_FRAME;
    }

    /* Check if we have the complete frame */
    size_t total_needed = NET_FRAME_PREFIX_SIZE + frame_size;
    if (codec->recv_fill < total_needed) {
        return CODEC_NEED_MORE_DATA;
    }

    /* Parse the header */
    net_read_packet_header(&s, header);
    if (net_serializer_has_overflow(&s)) {
        /* Discard this frame */
        size_t remaining = codec->recv_fill - total_needed;
        memmove(codec->recv_buffer, codec->recv_buffer + total_needed, remaining);
        codec->recv_fill = remaining;
        return CODEC_ERROR;
    }

    /* Validate header */
    if (!net_protocol_validate_header(header)) {
        size_t remaining = codec->recv_fill - total_needed;
        memmove(codec->recv_buffer, codec->recv_buffer + total_needed, remaining);
        codec->recv_fill = remaining;
        return CODEC_INVALID_FRAME;
    }

    /* Copy payload to stable decode buffer BEFORE consuming the frame.
     * This is the critical fix: the recv_buffer will be compacted by memmove
     * below, which would invalidate any pointer into it. By copying first,
     * the caller gets a pointer that remains valid until the next decode call. */
    uint32_t actual_payload_size = header->payload_size;
    if (actual_payload_size > 0) {
        if (actual_payload_size > NET_MAX_PAYLOAD_SIZE) {
            /* Payload claims to be larger than protocol allows — reject */
            size_t remaining = codec->recv_fill - total_needed;
            memmove(codec->recv_buffer, codec->recv_buffer + total_needed, remaining);
            codec->recv_fill = remaining;
            return CODEC_INVALID_FRAME;
        }
        memcpy(codec->decode_buffer,
               codec->recv_buffer + NET_FRAME_PREFIX_SIZE + NET_PACKET_HEADER_SIZE,
               actual_payload_size);
        *payload_out = codec->decode_buffer;
        *payload_size_out = actual_payload_size;
    }
    codec->decode_payload_size = actual_payload_size;

    /* Update tracking */
    codec->last_recv_sequence = header->sequence_id;

    /* NOW consume the frame — the payload is safely in decode_buffer */
    size_t remaining = codec->recv_fill - total_needed;
    if (remaining > 0) {
        memmove(codec->recv_buffer, codec->recv_buffer + total_needed, remaining);
    }
    codec->recv_fill = remaining;

    return CODEC_OK;
}

#endif /* ENABLE_MULTIPLAYER */

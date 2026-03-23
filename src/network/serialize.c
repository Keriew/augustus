#include "serialize.h"

#ifdef ENABLE_MULTIPLAYER

#include <string.h>

void net_serializer_init(net_serializer *s, uint8_t *buffer, size_t capacity)
{
    s->data = buffer;
    s->capacity = capacity;
    s->position = 0;
    s->overflow = 0;
}

void net_serializer_reset(net_serializer *s)
{
    s->position = 0;
    s->overflow = 0;
}

int net_serializer_has_overflow(const net_serializer *s)
{
    return s->overflow;
}

size_t net_serializer_position(const net_serializer *s)
{
    return s->position;
}

size_t net_serializer_remaining(const net_serializer *s)
{
    if (s->position >= s->capacity) {
        return 0;
    }
    return s->capacity - s->position;
}

void net_write_u8(net_serializer *s, uint8_t value)
{
    if (s->position + 1 > s->capacity) {
        s->overflow = 1;
        return;
    }
    s->data[s->position++] = value;
}

void net_write_u16(net_serializer *s, uint16_t value)
{
    if (s->position + 2 > s->capacity) {
        s->overflow = 1;
        return;
    }
    s->data[s->position++] = (uint8_t)(value & 0xFF);
    s->data[s->position++] = (uint8_t)((value >> 8) & 0xFF);
}

void net_write_u32(net_serializer *s, uint32_t value)
{
    if (s->position + 4 > s->capacity) {
        s->overflow = 1;
        return;
    }
    s->data[s->position++] = (uint8_t)(value & 0xFF);
    s->data[s->position++] = (uint8_t)((value >> 8) & 0xFF);
    s->data[s->position++] = (uint8_t)((value >> 16) & 0xFF);
    s->data[s->position++] = (uint8_t)((value >> 24) & 0xFF);
}

void net_write_i8(net_serializer *s, int8_t value)
{
    net_write_u8(s, (uint8_t)value);
}

void net_write_i16(net_serializer *s, int16_t value)
{
    net_write_u16(s, (uint16_t)value);
}

void net_write_i32(net_serializer *s, int32_t value)
{
    net_write_u32(s, (uint32_t)value);
}

void net_write_raw(net_serializer *s, const void *data, size_t size)
{
    if (s->position + size > s->capacity) {
        s->overflow = 1;
        return;
    }
    memcpy(s->data + s->position, data, size);
    s->position += size;
}

void net_write_string(net_serializer *s, const char *str, size_t max_len)
{
    size_t len = 0;
    if (str) {
        len = strlen(str);
        if (len >= max_len) {
            len = max_len - 1;
        }
    }
    net_write_u16(s, (uint16_t)len);
    if (len > 0) {
        net_write_raw(s, str, len);
    }
}

uint8_t net_read_u8(net_serializer *s)
{
    if (s->position + 1 > s->capacity) {
        s->overflow = 1;
        return 0;
    }
    return s->data[s->position++];
}

uint16_t net_read_u16(net_serializer *s)
{
    if (s->position + 2 > s->capacity) {
        s->overflow = 1;
        return 0;
    }
    uint16_t value = (uint16_t)s->data[s->position]
                   | ((uint16_t)s->data[s->position + 1] << 8);
    s->position += 2;
    return value;
}

uint32_t net_read_u32(net_serializer *s)
{
    if (s->position + 4 > s->capacity) {
        s->overflow = 1;
        return 0;
    }
    uint32_t value = (uint32_t)s->data[s->position]
                   | ((uint32_t)s->data[s->position + 1] << 8)
                   | ((uint32_t)s->data[s->position + 2] << 16)
                   | ((uint32_t)s->data[s->position + 3] << 24);
    s->position += 4;
    return value;
}

int8_t net_read_i8(net_serializer *s)
{
    return (int8_t)net_read_u8(s);
}

int16_t net_read_i16(net_serializer *s)
{
    return (int16_t)net_read_u16(s);
}

int32_t net_read_i32(net_serializer *s)
{
    return (int32_t)net_read_u32(s);
}

void net_read_raw(net_serializer *s, void *dest, size_t size)
{
    if (s->position + size > s->capacity) {
        s->overflow = 1;
        return;
    }
    memcpy(dest, s->data + s->position, size);
    s->position += size;
}

void net_read_string(net_serializer *s, char *dest, size_t max_len)
{
    uint16_t len = net_read_u16(s);
    if (s->overflow) {
        dest[0] = '\0';
        return;
    }
    if (len >= max_len) {
        /* read and discard excess */
        size_t to_read = max_len - 1;
        net_read_raw(s, dest, to_read);
        dest[to_read] = '\0';
        size_t skip = len - to_read;
        if (s->position + skip <= s->capacity) {
            s->position += skip;
        } else {
            s->overflow = 1;
        }
    } else {
        if (len > 0) {
            net_read_raw(s, dest, len);
        }
        dest[len] = '\0';
    }
}

void net_write_packet_header(net_serializer *s, const net_packet_header *header)
{
    net_write_u16(s, header->protocol_version);
    net_write_u16(s, header->message_type);
    net_write_u32(s, header->sequence_id);
    net_write_u32(s, header->ack_sequence_id);
    net_write_u32(s, header->session_id);
    net_write_u32(s, header->game_tick);
    net_write_u32(s, header->payload_size);
}

void net_read_packet_header(net_serializer *s, net_packet_header *header)
{
    header->protocol_version = net_read_u16(s);
    header->message_type = net_read_u16(s);
    header->sequence_id = net_read_u32(s);
    header->ack_sequence_id = net_read_u32(s);
    header->session_id = net_read_u32(s);
    header->game_tick = net_read_u32(s);
    header->payload_size = net_read_u32(s);
}

#endif /* ENABLE_MULTIPLAYER */

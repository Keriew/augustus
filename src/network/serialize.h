#ifndef NETWORK_SERIALIZE_H
#define NETWORK_SERIALIZE_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>
#include <stddef.h>

#include "protocol.h"

/**
 * Network serialization with explicit endianness (little-endian wire format).
 * Never dumps raw structs - all fields are written individually with known sizes.
 */

typedef struct {
    uint8_t *data;
    size_t capacity;
    size_t position;
    int overflow;
} net_serializer;

void net_serializer_init(net_serializer *s, uint8_t *buffer, size_t capacity);
void net_serializer_reset(net_serializer *s);
int net_serializer_has_overflow(const net_serializer *s);
size_t net_serializer_position(const net_serializer *s);
size_t net_serializer_remaining(const net_serializer *s);

/* Write functions - little endian */
void net_write_u8(net_serializer *s, uint8_t value);
void net_write_u16(net_serializer *s, uint16_t value);
void net_write_u32(net_serializer *s, uint32_t value);
void net_write_i8(net_serializer *s, int8_t value);
void net_write_i16(net_serializer *s, int16_t value);
void net_write_i32(net_serializer *s, int32_t value);
void net_write_raw(net_serializer *s, const void *data, size_t size);
void net_write_string(net_serializer *s, const char *str, size_t max_len);

/* Read functions - little endian */
uint8_t net_read_u8(net_serializer *s);
uint16_t net_read_u16(net_serializer *s);
uint32_t net_read_u32(net_serializer *s);
int8_t net_read_i8(net_serializer *s);
int16_t net_read_i16(net_serializer *s);
int32_t net_read_i32(net_serializer *s);
void net_read_raw(net_serializer *s, void *dest, size_t size);
void net_read_string(net_serializer *s, char *dest, size_t max_len);

/* Packet header serialization */
void net_write_packet_header(net_serializer *s, const net_packet_header *header);
void net_read_packet_header(net_serializer *s, net_packet_header *header);

#endif /* ENABLE_MULTIPLAYER */

#endif /* NETWORK_SERIALIZE_H */

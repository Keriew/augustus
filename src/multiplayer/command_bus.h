#ifndef MULTIPLAYER_COMMAND_BUS_H
#define MULTIPLAYER_COMMAND_BUS_H

#ifdef ENABLE_MULTIPLAYER

#include "command_types.h"

#include <stdint.h>

#define MP_COMMAND_QUEUE_SIZE 256

/**
 * The command bus is the single entry point for all multiplayer gameplay commands.
 * Clients submit commands through the bus; the host validates and applies them.
 */

void mp_command_bus_init(void);
void mp_command_bus_init_from_save(uint32_t next_sequence_id);
void mp_command_bus_clear(void);

/**
 * Submit a command locally. If we're the host, validate and apply immediately.
 * If we're a client, serialize and send to host.
 */
int mp_command_bus_submit(mp_command *cmd);

/**
 * Called on the host when a command arrives from a client peer.
 * Validates, applies if accepted, and sends acknowledgment.
 */
void multiplayer_command_bus_receive(uint8_t player_id,
                                    const uint8_t *data, uint32_t size);

/**
 * Called on the client when the host acknowledges a command.
 */
void multiplayer_command_bus_receive_ack(const uint8_t *data, uint32_t size);

/**
 * Process pending commands. Called once per tick on the host.
 */
void mp_command_bus_process_pending(uint32_t current_tick);

/**
 * Get the last command status for UI feedback.
 */
mp_command_status mp_command_bus_get_last_status(void);
uint8_t mp_command_bus_get_last_reject_reason(void);

/**
 * Get/set the next sequence ID (for save/restore).
 */
uint32_t mp_command_bus_get_next_sequence_id(void);
void mp_command_bus_set_next_sequence_id(uint32_t id);

#endif /* ENABLE_MULTIPLAYER */

#endif /* MULTIPLAYER_COMMAND_BUS_H */

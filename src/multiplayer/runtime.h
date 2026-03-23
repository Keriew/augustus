#ifndef MULTIPLAYER_RUNTIME_H
#define MULTIPLAYER_RUNTIME_H

#ifdef ENABLE_MULTIPLAYER

/**
 * Multiplayer runtime: per-frame network processing decoupled from simulation ticks.
 *
 * The key architectural insight is that network I/O (accepting connections,
 * receiving packets, processing handshakes, discovery broadcasts) must happen
 * every rendered frame, not just on simulation ticks. When the game is in
 * lobby/connect/menu windows, simulation ticks do NOT run, but the network
 * state machine still needs to advance for handshakes to complete.
 *
 * This module is called once per frame from game_run(), before ticks.
 */

/**
 * Initialize the multiplayer runtime subsystem.
 * Called once at startup (or lazily on first multiplayer use).
 */
void multiplayer_runtime_init(void);

/**
 * Per-frame update: processes network I/O, discovery, and session state.
 * Must be called every frame from game_run(), regardless of game state.
 * Safe to call when multiplayer is not active (early-returns).
 */
void multiplayer_runtime_update(void);

/**
 * Shutdown the multiplayer runtime subsystem.
 * Called on game exit.
 */
void multiplayer_runtime_shutdown(void);

#endif /* ENABLE_MULTIPLAYER */

#endif /* MULTIPLAYER_RUNTIME_H */

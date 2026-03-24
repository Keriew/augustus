#ifndef MULTIPLAYER_SCENARIO_SELECTION_H
#define MULTIPLAYER_SCENARIO_SELECTION_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>

/**
 * Scenario validation and hash computation for multiplayer.
 *
 * Responsibilities:
 * - Validate that a loaded scenario supports multiplayer with N players
 * - Count eligible trade cities in current empire state
 * - Compute FNV-1a hash of scenario file for manifest validation
 *
 * A city is eligible for player spawn if:
 * - It is in_use
 * - It has type TRADE, FUTURE_TRADE, or DISTANT_ROMAN
 * - It has a valid route_id (>= 0)
 * - It buys or sells at least one resource (trade viability)
 * - It has a valid empire_object_id (for map position)
 */

#define MP_SCENARIO_MIN_TRADE_CITIES 2

/**
 * Count trade cities in the currently loaded empire that could serve as
 * player spawns. Must be called AFTER scenario load (empire populated).
 * @return Number of eligible cities
 */
int mp_scenario_count_eligible_cities(void);

/**
 * Validate that the currently loaded scenario supports multiplayer
 * with the given number of players.
 * @param player_count Number of players (2-8)
 * @return 1 if valid, 0 if not enough eligible cities
 */
int mp_scenario_validate_for_multiplayer(int player_count);

/**
 * Compute FNV-1a hash of a scenario file on disk.
 * Reads the file in chunks, no large allocation needed.
 * @param scenario_name Scenario filename without extension (ASCII)
 * @param out_hash Output hash value
 * @return 1 on success, 0 if file not found
 */
int mp_scenario_compute_file_hash(const char *scenario_name, uint32_t *out_hash);

/**
 * Check if two hashes match. Used by client to validate against manifest.
 * @param local_hash Hash computed locally
 * @param manifest_hash Hash from the game manifest
 * @return 1 if match, 0 if mismatch
 */
int mp_scenario_hashes_match(uint32_t local_hash, uint32_t manifest_hash);

#endif /* ENABLE_MULTIPLAYER */

#endif /* MULTIPLAYER_SCENARIO_SELECTION_H */

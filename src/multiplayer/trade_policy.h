#ifndef MULTIPLAYER_TRADE_POLICY_H
#define MULTIPLAYER_TRADE_POLICY_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>

/**
 * Unified trade policy — the SINGLE SOURCE OF TRUTH for multiplayer trade.
 *
 * In multiplayer, trade eligibility depends on multiple factors:
 * - City type (AI vs player-owned)
 * - Route state (active/disabled/offline)
 * - Resource settings (import/export toggles, stockpile thresholds)
 * - Trade view data (for remote player cities)
 *
 * This module provides a canonical query API so no caller needs to
 * assemble the answer from multiple scattered sources.
 *
 * Mutation flow:
 *   1. Player changes resource setting (import/export/stockpile)
 *   2. Change is applied locally AND sent to host via command_bus
 *   3. Host validates and broadcasts to all peers
 *   4. Peers update their trade views for that player's city
 *   5. Next trade eligibility query reflects the change
 */

/**
 * Can a resource be exported TO the given city (i.e., the city buys it)?
 * Single entry point — handles AI, local player, and remote player cities.
 * @param city_id Target empire city
 * @param resource Resource enum
 * @return 1 if export is allowed, 0 otherwise
 */
int mp_trade_policy_can_export_to(int city_id, int resource);

/**
 * Can a resource be imported FROM the given city (i.e., the city sells it)?
 * Single entry point — handles AI, local player, and remote player cities.
 * @param city_id Source empire city
 * @param resource Resource enum
 * @return 1 if import is allowed, 0 otherwise
 */
int mp_trade_policy_can_import_from(int city_id, int resource);

/**
 * Notify the host that the local player changed a city resource setting.
 * Called from resource_settings.c when import/export or stockpile changes.
 * @param resource Resource enum
 * @param setting_type 0=export toggle, 1=import toggle, 2=stockpile threshold
 * @param value New value (0/1 for toggles, threshold amount for stockpile)
 */
void mp_trade_policy_notify_setting_changed(int resource, int setting_type, int value);

/**
 * Host: apply a city resource setting change from a remote player.
 * Updates the trade view immediately so other players see the change.
 * @param player_id Player who changed the setting
 * @param resource Resource enum
 * @param setting_type 0=export toggle, 1=import toggle, 2=stockpile threshold
 * @param value New value
 */
void mp_trade_policy_apply_remote_setting(uint8_t player_id, int resource,
                                           int setting_type, int value);

/**
 * Host: force an immediate trade view update for a player's city.
 * Called when a significant policy change occurs that shouldn't wait
 * for the periodic 50-tick sync.
 * @param player_id Player whose trade view should be refreshed
 */
void mp_trade_policy_force_view_update(uint8_t player_id);

#define MP_TRADE_SETTING_EXPORT  0
#define MP_TRADE_SETTING_IMPORT  1
#define MP_TRADE_SETTING_STOCKPILE 2

#endif /* ENABLE_MULTIPLAYER */

#endif /* MULTIPLAYER_TRADE_POLICY_H */

#include "trade_policy.h"

#ifdef ENABLE_MULTIPLAYER

#include "command_bus.h"
#include "command_types.h"
#include "empire_sync.h"
#include "ownership.h"
#include "mp_debug_log.h"
#include "network/session.h"
#include "network/serialize.h"
#include "network/protocol.h"
#include "empire/city.h"
#include "empire/trade_route.h"
#include "city/resource.h"
#include "city/constants.h"
#include "game/resource.h"
#include "core/config.h"

#include <string.h>

/**
 * Unified trade policy — canonical answer for "can this trade happen?"
 *
 * Architecture:
 * - AI cities: use empire_city.buys_resource/sells_resource (scenario data)
 * - Remote player cities: use mp_city_trade_view (host-replicated snapshots)
 * - Local player city: use city_resource_trade_status (authoritative locally)
 * - All paths also check trade_route quotas and local settings
 *
 * When a player changes their city's resource settings:
 * 1. Change is applied locally immediately (responsive UI)
 * 2. A notification is sent to the host
 * 3. Host updates the trade view for that player's city
 * 4. Host broadcasts the updated view to all other clients
 */

int mp_trade_policy_can_export_to(int city_id, int resource)
{
    if (!resource_is_storable(resource)) {
        return 0;
    }
    if (resource < RESOURCE_MIN || resource >= RESOURCE_MAX) {
        return 0;
    }

    /* Remote player city — use replicated trade view */
    if (empire_city_is_remote_player_city(city_id)) {
        const mp_city_trade_view *view = mp_empire_sync_get_trade_view(city_id);
        if (!view || !view->online) {
            return 0;
        }
        if (!view->importable[resource]) {
            return 0;
        }
        /* Check our own local export capability */
        int in_stock;
        if (!resource_is_food(resource) || config_get(CONFIG_GP_CH_ALLOW_EXPORTING_FROM_GRANARIES)) {
            in_stock = city_resource_get_total_amount(resource, 1);
        } else {
            in_stock = city_resource_count_warehouses_amount(resource);
        }
        if (in_stock <= city_resource_export_over(resource)) {
            return 0;
        }
        return (city_resource_trade_status((resource_type)resource) & TRADE_STATUS_EXPORT) == TRADE_STATUS_EXPORT;
    }

    /* AI or local city — use standard empire_city data */
    empire_city *city = empire_city_get(city_id);
    if (!city || !city->in_use) {
        return 0;
    }

    /* Check route quota */
    if (city_id && city->route_id >= 0 &&
        trade_route_limit_reached(city->route_id, resource, 1)) {
        return 0;
    }

    /* Check local stock */
    int in_stock;
    if (!resource_is_food(resource) || config_get(CONFIG_GP_CH_ALLOW_EXPORTING_FROM_GRANARIES)) {
        in_stock = city_resource_get_total_amount(resource, 1);
    } else {
        in_stock = city_resource_count_warehouses_amount(resource);
    }
    if (in_stock <= city_resource_export_over(resource)) {
        return 0;
    }

    /* Check city accepts this resource AND we have export enabled */
    if (city_id == 0 || city->buys_resource[resource]) {
        return (city_resource_trade_status((resource_type)resource) & TRADE_STATUS_EXPORT) == TRADE_STATUS_EXPORT;
    }
    return 0;
}

int mp_trade_policy_can_import_from(int city_id, int resource)
{
    if (!resource_is_storable(resource)) {
        return 0;
    }
    if (resource < RESOURCE_MIN || resource >= RESOURCE_MAX) {
        return 0;
    }

    /* Remote player city — use replicated trade view */
    if (empire_city_is_remote_player_city(city_id)) {
        const mp_city_trade_view *view = mp_empire_sync_get_trade_view(city_id);
        if (!view || !view->online) {
            return 0;
        }
        if (!view->exportable[resource]) {
            return 0;
        }
        /* Check our local import settings */
        return (city_resource_trade_status((resource_type)resource) & TRADE_STATUS_IMPORT) == TRADE_STATUS_IMPORT;
    }

    /* AI or local city — use standard data */
    empire_city *city = empire_city_get(city_id);
    if (!city || !city->in_use) {
        return 0;
    }

    if (!city->sells_resource[resource]) {
        return 0;
    }
    if (!(city_resource_trade_status(resource) & TRADE_STATUS_IMPORT)) {
        return 0;
    }
    if (city->route_id >= 0 && trade_route_limit_reached(city->route_id, resource, 0)) {
        return 0;
    }
    return 1;
}

void mp_trade_policy_notify_setting_changed(int resource, int setting_type, int value)
{
    if (!net_session_is_active()) {
        return;
    }

    MP_LOG_INFO("TRADE_POLICY", "Resource setting changed: res=%d type=%d val=%d",
                resource, setting_type, value);

    /* Submit through the command bus — works for both host and client.
     * Host: validates + applies + broadcasts immediately.
     * Client: serializes as mp_command, sends to host via CLIENT_COMMAND. */
    mp_command cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.command_type = MP_CMD_SET_RESOURCE_SETTING;
    cmd.player_id = net_session_get_local_player_id();
    cmd.target_tick = net_session_get_authoritative_tick();
    cmd.data.resource_setting.resource = resource;
    cmd.data.resource_setting.setting_type = (uint8_t)setting_type;
    cmd.data.resource_setting.value = value;

    mp_command_bus_submit(&cmd);
}

void mp_trade_policy_apply_remote_setting(uint8_t player_id, int resource,
                                           int setting_type, int value)
{
    MP_LOG_INFO("TRADE_POLICY", "Remote setting from player %d: res=%d type=%d val=%d",
                (int)player_id, resource, setting_type, value);

    /* The remote player's city settings changed. We need to update the
     * trade view so other clients see the change immediately. */
    mp_trade_policy_force_view_update(player_id);
}

void mp_trade_policy_force_view_update(uint8_t player_id)
{
    if (!net_session_is_host()) {
        return;
    }

    /* Force immediate update and broadcast of trade views */
    mp_empire_sync_update_trade_views();
    mp_empire_sync_broadcast_views();

    MP_LOG_INFO("TRADE_POLICY", "Forced trade view update for player %d",
                (int)player_id);
}

#endif /* ENABLE_MULTIPLAYER */

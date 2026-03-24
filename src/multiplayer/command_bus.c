#include "command_bus.h"

#ifdef ENABLE_MULTIPLAYER

#include "ownership.h"
#include "player_registry.h"
#include "trade_policy.h"
#include "trade_sync.h"
#include "network/session.h"
#include "network/serialize.h"
#include "network/protocol.h"
#include "building/building.h"
#include "building/storage.h"
#include "empire/city.h"
#include "empire/trade_route.h"
#include "game/resource.h"
#include "core/log.h"

#include <string.h>

#define MAX_PLAYER_ROUTES 10  /* Max active routes per player */

static struct {
    mp_command queue[MP_COMMAND_QUEUE_SIZE];
    int queue_head;
    int queue_tail;
    int queue_count;
    uint32_t next_sequence_id;
    mp_command_status last_status;
    uint8_t last_reject_reason;
} bus;

void mp_command_bus_init(void)
{
    memset(&bus, 0, sizeof(bus));
    bus.next_sequence_id = 1;
}

void mp_command_bus_clear(void)
{
    mp_command_bus_init();
}

static int enqueue_command(const mp_command *cmd)
{
    if (bus.queue_count >= MP_COMMAND_QUEUE_SIZE) {
        log_error("Command queue full", 0, 0);
        return 0;
    }
    bus.queue[bus.queue_tail] = *cmd;
    bus.queue_tail = (bus.queue_tail + 1) % MP_COMMAND_QUEUE_SIZE;
    bus.queue_count++;
    return 1;
}

static mp_command *dequeue_command(void)
{
    if (bus.queue_count == 0) {
        return 0;
    }
    mp_command *cmd = &bus.queue[bus.queue_head];
    bus.queue_head = (bus.queue_head + 1) % MP_COMMAND_QUEUE_SIZE;
    bus.queue_count--;
    return cmd;
}

/* ---- Validation (host-side only) ---- */

static int validate_create_trade_route(const mp_command *cmd)
{
    const mp_cmd_create_trade_route *data = &cmd->data.create_route;

    /* Validate origin city exists and is owned by requesting player */
    empire_city *origin = empire_city_get(data->origin_city_id);
    if (!origin || !origin->in_use) {
        return MP_CMD_REJECT_CITY_NOT_FOUND;
    }

    uint8_t origin_owner = mp_ownership_get_city_player_id(data->origin_city_id);
    if (origin_owner != cmd->player_id) {
        return MP_CMD_REJECT_CITY_NOT_OWNED;
    }

    /* Validate destination city exists */
    empire_city *dest = empire_city_get(data->dest_city_id);
    if (!dest || !dest->in_use) {
        return MP_CMD_REJECT_CITY_NOT_FOUND;
    }

    /* Check if destination is a player city and is online */
    if (mp_ownership_is_city_player_owned(data->dest_city_id)) {
        if (!mp_ownership_is_city_online(data->dest_city_id)) {
            return MP_CMD_REJECT_CITY_OFFLINE;
        }
    }

    /* Check for duplicate route */
    int existing = mp_ownership_find_route_between(data->origin_city_id, data->dest_city_id);
    if (existing >= 0) {
        return MP_CMD_REJECT_DUPLICATE_ROUTE;
    }

    /* Check route cap per player */
    int player_routes = mp_ownership_count_player_routes(cmd->player_id);
    if (player_routes >= MAX_PLAYER_ROUTES) {
        return MP_CMD_REJECT_ROUTE_CAP_REACHED;
    }

    return MP_CMD_REJECT_NONE;
}

static int validate_delete_trade_route(const mp_command *cmd)
{
    const mp_cmd_delete_trade_route *data = &cmd->data.delete_route;

    mp_route_state state = mp_ownership_get_route_state(data->route_id);
    if (state == MP_ROUTE_STATE_INACTIVE || state == MP_ROUTE_STATE_DELETED) {
        return MP_CMD_REJECT_ROUTE_NOT_FOUND;
    }

    /* Only the origin player can delete */
    uint8_t origin = mp_ownership_get_route_origin_player(data->route_id);
    if (origin != cmd->player_id) {
        return MP_CMD_REJECT_NOT_OWNER;
    }

    /* Verify network route ID matches */
    uint32_t net_id = mp_ownership_get_network_route_id(data->route_id);
    if (net_id != data->network_route_id) {
        return MP_CMD_REJECT_ROUTE_NOT_FOUND;
    }

    return MP_CMD_REJECT_NONE;
}

static int validate_enable_trade_route(const mp_command *cmd)
{
    const mp_cmd_enable_trade_route *data = &cmd->data.enable_route;

    mp_route_state state = mp_ownership_get_route_state(data->route_id);
    if (state == MP_ROUTE_STATE_INACTIVE || state == MP_ROUTE_STATE_DELETED) {
        return MP_CMD_REJECT_ROUTE_NOT_FOUND;
    }
    if (state == MP_ROUTE_STATE_ACTIVE) {
        return MP_CMD_REJECT_ROUTE_ALREADY_ENABLED;
    }
    if (state != MP_ROUTE_STATE_DISABLED) {
        return MP_CMD_REJECT_INVALID;
    }

    uint8_t origin = mp_ownership_get_route_origin_player(data->route_id);
    if (origin != cmd->player_id) {
        return MP_CMD_REJECT_NOT_OWNER;
    }

    return MP_CMD_REJECT_NONE;
}

static int validate_disable_trade_route(const mp_command *cmd)
{
    const mp_cmd_disable_trade_route *data = &cmd->data.disable_route;

    mp_route_state state = mp_ownership_get_route_state(data->route_id);
    if (state == MP_ROUTE_STATE_INACTIVE || state == MP_ROUTE_STATE_DELETED) {
        return MP_CMD_REJECT_ROUTE_NOT_FOUND;
    }
    if (state == MP_ROUTE_STATE_DISABLED) {
        return MP_CMD_REJECT_ROUTE_ALREADY_DISABLED;
    }
    if (state != MP_ROUTE_STATE_ACTIVE) {
        return MP_CMD_REJECT_INVALID;
    }

    uint8_t origin = mp_ownership_get_route_origin_player(data->route_id);
    if (origin != cmd->player_id) {
        return MP_CMD_REJECT_NOT_OWNER;
    }

    return MP_CMD_REJECT_NONE;
}

static int validate_set_route_policy(const mp_command *cmd)
{
    const mp_cmd_set_route_policy *data = &cmd->data.route_policy;

    mp_route_state state = mp_ownership_get_route_state(data->route_id);
    if (state == MP_ROUTE_STATE_INACTIVE || state == MP_ROUTE_STATE_DELETED) {
        return MP_CMD_REJECT_ROUTE_NOT_FOUND;
    }

    if (!trade_route_is_valid(data->route_id)) {
        return MP_CMD_REJECT_ROUTE_NOT_FOUND;
    }

    if (data->resource < 0 || data->resource >= RESOURCE_MAX) {
        return MP_CMD_REJECT_RESOURCE_INVALID;
    }

    /* The player must be either origin or destination */
    uint8_t origin = mp_ownership_get_route_origin_player(data->route_id);
    uint8_t dest = mp_ownership_get_route_dest_player(data->route_id);
    if (origin != cmd->player_id && dest != cmd->player_id) {
        return MP_CMD_REJECT_NOT_OWNER;
    }

    return MP_CMD_REJECT_NONE;
}

static int validate_set_route_limit(const mp_command *cmd)
{
    const mp_cmd_set_route_limit *data = &cmd->data.route_limit;

    if (!trade_route_is_valid(data->route_id)) {
        return MP_CMD_REJECT_ROUTE_NOT_FOUND;
    }

    if (data->resource < 0 || data->resource >= RESOURCE_MAX) {
        return MP_CMD_REJECT_RESOURCE_INVALID;
    }

    if (data->amount < 0) {
        return MP_CMD_REJECT_LIMIT_OUT_OF_RANGE;
    }

    uint8_t origin = mp_ownership_get_route_origin_player(data->route_id);
    uint8_t dest = mp_ownership_get_route_dest_player(data->route_id);
    if (origin != cmd->player_id && dest != cmd->player_id) {
        return MP_CMD_REJECT_NOT_OWNER;
    }

    return MP_CMD_REJECT_NONE;
}

static int validate_command(const mp_command *cmd)
{
    switch (cmd->command_type) {
        case MP_CMD_CREATE_TRADE_ROUTE:
            return validate_create_trade_route(cmd);

        case MP_CMD_DELETE_TRADE_ROUTE:
            return validate_delete_trade_route(cmd);

        case MP_CMD_ENABLE_TRADE_ROUTE:
            return validate_enable_trade_route(cmd);

        case MP_CMD_DISABLE_TRADE_ROUTE:
            return validate_disable_trade_route(cmd);

        case MP_CMD_SET_ROUTE_POLICY:
            return validate_set_route_policy(cmd);

        case MP_CMD_SET_ROUTE_LIMIT:
            return validate_set_route_limit(cmd);

        case MP_CMD_OPEN_TRADE_ROUTE: {
            int city_id = cmd->data.open_route.city_id;
            empire_city *city = empire_city_get(city_id);
            if (!city || !city->in_use) {
                return MP_CMD_REJECT_CITY_NOT_FOUND;
            }
            if (city->is_open) {
                return MP_CMD_REJECT_ALREADY_OPEN;
            }
            mp_player *player = mp_player_registry_get(cmd->player_id);
            if (!player) {
                return MP_CMD_REJECT_NOT_OWNER;
            }
            return MP_CMD_REJECT_NONE;
        }

        case MP_CMD_CLOSE_TRADE_ROUTE: {
            int city_id = cmd->data.close_route.city_id;
            empire_city *city = empire_city_get(city_id);
            if (!city || !city->in_use) {
                return MP_CMD_REJECT_CITY_NOT_FOUND;
            }
            if (!city->is_open) {
                return MP_CMD_REJECT_ALREADY_CLOSED;
            }
            return MP_CMD_REJECT_NONE;
        }

        case MP_CMD_SET_RESOURCE_SETTING: {
            int res = cmd->data.resource_setting.resource;
            if (res < RESOURCE_MIN || res >= RESOURCE_MAX) {
                return MP_CMD_REJECT_RESOURCE_INVALID;
            }
            uint8_t st = cmd->data.resource_setting.setting_type;
            if (st > 2) { /* MP_TRADE_SETTING_STOCKPILE = 2 */
                return MP_CMD_REJECT_INVALID;
            }
            return MP_CMD_REJECT_NONE;
        }

        case MP_CMD_SET_STORAGE_STATE: {
            int bid = cmd->data.storage_state.building_id;
            building *b = building_get(bid);
            if (!b || b->state != BUILDING_STATE_IN_USE) {
                return MP_CMD_REJECT_CITY_NOT_FOUND;
            }
            if (b->type != BUILDING_WAREHOUSE && b->type != BUILDING_GRANARY) {
                return MP_CMD_REJECT_INVALID;
            }
            int res = cmd->data.storage_state.resource;
            if (res < RESOURCE_MIN || res >= RESOURCE_MAX) {
                return MP_CMD_REJECT_RESOURCE_INVALID;
            }
            if (cmd->data.storage_state.new_state >= BUILDING_STORAGE_STATE_MAX) {
                return MP_CMD_REJECT_INVALID;
            }
            return MP_CMD_REJECT_NONE;
        }

        case MP_CMD_SET_STORAGE_PERMISSION: {
            int bid = cmd->data.storage_permission.building_id;
            building *b = building_get(bid);
            if (!b || b->state != BUILDING_STATE_IN_USE) {
                return MP_CMD_REJECT_CITY_NOT_FOUND;
            }
            if (b->type != BUILDING_WAREHOUSE && b->type != BUILDING_GRANARY) {
                return MP_CMD_REJECT_INVALID;
            }
            return MP_CMD_REJECT_NONE;
        }

        case MP_CMD_REQUEST_PAUSE:
        case MP_CMD_REQUEST_RESUME:
        case MP_CMD_REQUEST_SPEED:
        case MP_CMD_SET_READY:
        case MP_CMD_CHAT_MESSAGE:
            return MP_CMD_REJECT_NONE;

        default:
            return MP_CMD_REJECT_INVALID;
    }
}

/* ---- Application (host-side only) ---- */

static void broadcast_route_event(uint16_t event_type, int route_id,
                                    uint8_t player_id, uint32_t network_route_id)
{
    uint8_t event_buf[64];
    net_serializer s;
    net_serializer_init(&s, event_buf, sizeof(event_buf));
    net_write_u16(&s, event_type);
    net_write_u32(&s, net_session_get_authoritative_tick());
    net_write_i32(&s, route_id);
    net_write_u8(&s, player_id);
    net_write_u32(&s, network_route_id);
    net_session_broadcast(NET_MSG_HOST_EVENT, event_buf,
                          (uint32_t)net_serializer_position(&s));
}

static void broadcast_route_created_event(int route_id, uint8_t player_id,
                                            uint32_t network_route_id,
                                            int dest_city_id, uint8_t dest_player_id,
                                            mp_route_owner_mode mode)
{
    uint8_t event_buf[80];
    net_serializer s;
    net_serializer_init(&s, event_buf, sizeof(event_buf));
    net_write_u16(&s, NET_EVENT_ROUTE_CREATED);
    net_write_u32(&s, net_session_get_authoritative_tick());
    net_write_i32(&s, route_id);
    net_write_u8(&s, player_id);
    net_write_u32(&s, network_route_id);
    net_write_i32(&s, dest_city_id);
    net_write_u8(&s, dest_player_id);
    net_write_u8(&s, (uint8_t)mode);
    net_session_broadcast(NET_MSG_HOST_EVENT, event_buf,
                          (uint32_t)net_serializer_position(&s));
}

static void apply_command(mp_command *cmd)
{
    uint32_t tick = net_session_get_authoritative_tick();

    switch (cmd->command_type) {
        case MP_CMD_CREATE_TRADE_ROUTE: {
            const mp_cmd_create_trade_route *data = &cmd->data.create_route;

            /* Determine route owner mode */
            int dest_is_player = mp_ownership_is_city_player_owned(data->dest_city_id);
            uint8_t dest_player = mp_ownership_get_city_player_id(data->dest_city_id);
            mp_route_owner_mode mode;

            if (dest_is_player) {
                mode = MP_ROUTE_PLAYER_TO_PLAYER;
            } else {
                mode = MP_ROUTE_PLAYER_TO_AI;
            }

            /* Allocate network route ID */
            uint32_t network_id = mp_ownership_allocate_network_route_id();

            /* Find or create a trade route in the Augustus system.
             * AI cities already have a route_id from the scenario data.
             * Player-to-player routes need a freshly allocated route entry. */
            int route_id = empire_city_get_route_id(data->dest_city_id);
            if (route_id < 0) {
                route_id = trade_route_new();
                if (route_id < 0) {
                    log_error("Failed to allocate trade route for P2P", 0, 0);
                    cmd->status = MP_CMD_STATUS_REJECTED;
                    return;
                }
            }

            /* Register in ownership */
            if (mp_ownership_create_route(route_id, mode, cmd->player_id,
                    dest_is_player ? dest_player : 0, network_id) < 0) {
                cmd->status = MP_CMD_STATUS_REJECTED;
                return;
            }

            mp_ownership_set_route_state(route_id, MP_ROUTE_STATE_ACTIVE);
            mp_ownership_set_route_open_tick(route_id, tick);

            /* Open the trade city if it's an AI city */
            if (!dest_is_player) {
                empire_city_open_trade(data->dest_city_id, 1);
            }

            /* Bind the route to both players */
            trade_route_set_player_binding(route_id, cmd->player_id,
                dest_is_player ? dest_player : 0xFF);

            log_info("Trade route created", 0, route_id);
            broadcast_route_created_event(route_id, cmd->player_id, network_id,
                data->dest_city_id,
                dest_is_player ? dest_player : 0xFF, mode);

            /* Also broadcast full route state so clients get initial policy */
            mp_trade_sync_broadcast_route_state(route_id);
            break;
        }

        case MP_CMD_DELETE_TRADE_ROUTE: {
            int route_id = cmd->data.delete_route.route_id;
            mp_ownership_delete_route(route_id);
            trade_route_clear_player_binding(route_id);
            log_info("Trade route deleted", 0, route_id);
            broadcast_route_event(NET_EVENT_ROUTE_DELETED, route_id,
                                  cmd->player_id, cmd->data.delete_route.network_route_id);
            break;
        }

        case MP_CMD_ENABLE_TRADE_ROUTE: {
            int route_id = cmd->data.enable_route.route_id;
            mp_ownership_set_route_state(route_id, MP_ROUTE_STATE_ACTIVE);
            log_info("Trade route enabled", 0, route_id);
            broadcast_route_event(NET_EVENT_ROUTE_ENABLED, route_id,
                                  cmd->player_id, cmd->data.enable_route.network_route_id);
            break;
        }

        case MP_CMD_DISABLE_TRADE_ROUTE: {
            int route_id = cmd->data.disable_route.route_id;
            mp_ownership_set_route_state(route_id, MP_ROUTE_STATE_DISABLED);
            log_info("Trade route disabled", 0, route_id);
            broadcast_route_event(NET_EVENT_ROUTE_DISABLED, route_id,
                                  cmd->player_id, cmd->data.disable_route.network_route_id);
            break;
        }

        case MP_CMD_SET_ROUTE_POLICY: {
            const mp_cmd_set_route_policy *data = &cmd->data.route_policy;
            if (data->is_export) {
                trade_route_set_export_enabled(data->route_id, data->resource, data->enabled);
            } else {
                trade_route_set_import_enabled(data->route_id, data->resource, data->enabled);
            }
            mp_ownership_increment_route_version(data->route_id);
            mp_trade_sync_broadcast_route_policy(data->route_id, data->resource,
                                                  data->is_export, data->enabled);
            break;
        }

        case MP_CMD_SET_ROUTE_LIMIT: {
            const mp_cmd_set_route_limit *data = &cmd->data.route_limit;
            trade_route_set_limit(data->route_id, data->resource,
                                  data->amount, data->is_buying);
            mp_ownership_increment_route_version(data->route_id);
            mp_trade_sync_broadcast_route_limit(data->route_id, data->resource,
                                                 data->is_buying, data->amount);
            break;
        }

        case MP_CMD_OPEN_TRADE_ROUTE: {
            int city_id = cmd->data.open_route.city_id;
            empire_city_open_trade(city_id, 1);
            log_info("AI trade route opened by player", 0, cmd->player_id);

            uint8_t event_buf[32];
            net_serializer s;
            net_serializer_init(&s, event_buf, sizeof(event_buf));
            net_write_u16(&s, NET_EVENT_ROUTE_OPENED);
            net_write_u32(&s, tick);
            net_write_i32(&s, city_id);
            net_write_u8(&s, cmd->player_id);
            net_session_broadcast(NET_MSG_HOST_EVENT, event_buf,
                                  (uint32_t)net_serializer_position(&s));
            break;
        }

        case MP_CMD_CLOSE_TRADE_ROUTE: {
            int city_id = cmd->data.close_route.city_id;
            empire_city *city = empire_city_get(city_id);
            if (city) {
                city->is_open = 0;
            }
            log_info("AI trade route closed by player", 0, cmd->player_id);

            uint8_t event_buf[32];
            net_serializer s;
            net_serializer_init(&s, event_buf, sizeof(event_buf));
            net_write_u16(&s, NET_EVENT_ROUTE_CLOSED);
            net_write_u32(&s, tick);
            net_write_i32(&s, city_id);
            net_write_u8(&s, cmd->player_id);
            net_session_broadcast(NET_MSG_HOST_EVENT, event_buf,
                                  (uint32_t)net_serializer_position(&s));
            break;
        }

        case MP_CMD_REQUEST_PAUSE:
            net_session_set_paused(1);
            break;

        case MP_CMD_REQUEST_RESUME:
            net_session_set_paused(0);
            break;

        case MP_CMD_REQUEST_SPEED:
            net_session_set_game_speed(cmd->data.speed.speed);
            break;

        case MP_CMD_SET_RESOURCE_SETTING: {
            const mp_cmd_set_resource_setting *data = &cmd->data.resource_setting;
            mp_trade_policy_apply_remote_setting(cmd->player_id, data->resource,
                                                   data->setting_type, data->value);

            /* Broadcast as host event so all clients update their trade views */
            uint8_t event_buf[32];
            net_serializer es;
            net_serializer_init(&es, event_buf, sizeof(event_buf));
            net_write_u16(&es, NET_EVENT_CITY_RESOURCE_SETTING);
            net_write_u32(&es, tick);
            net_write_u8(&es, cmd->player_id);
            net_write_i32(&es, data->resource);
            net_write_u8(&es, data->setting_type);
            net_write_i32(&es, data->value);
            net_session_broadcast(NET_MSG_HOST_EVENT, event_buf,
                                  (uint32_t)net_serializer_position(&es));
            break;
        }

        case MP_CMD_SET_STORAGE_STATE: {
            const mp_cmd_set_storage_state *data = &cmd->data.storage_state;
            building *b = building_get(data->building_id);
            if (b && b->storage_id) {
                const building_storage *current = building_storage_get(b->storage_id);
                if (current) {
                    building_storage new_data = *current;
                    new_data.resource_state[data->resource].state = data->new_state;
                    building_storage_set_data(b->storage_id, new_data);
                }
            }
            /* Broadcast to all clients */
            uint8_t event_buf[32];
            net_serializer es;
            net_serializer_init(&es, event_buf, sizeof(event_buf));
            net_write_u16(&es, NET_EVENT_STORAGE_STATE_CHANGED);
            net_write_u32(&es, tick);
            net_write_u8(&es, cmd->player_id);
            net_write_i32(&es, data->building_id);
            net_write_i32(&es, data->resource);
            net_write_u8(&es, data->new_state);
            net_session_broadcast(NET_MSG_HOST_EVENT, event_buf,
                                  (uint32_t)net_serializer_position(&es));
            break;
        }

        case MP_CMD_SET_STORAGE_PERMISSION: {
            const mp_cmd_set_storage_permission *data = &cmd->data.storage_permission;
            building *b = building_get(data->building_id);
            if (b) {
                building_storage_toggle_permission(data->permission, b);
            }
            /* Broadcast to all clients */
            uint8_t event_buf[32];
            net_serializer es;
            net_serializer_init(&es, event_buf, sizeof(event_buf));
            net_write_u16(&es, NET_EVENT_STORAGE_PERMISSION_CHANGED);
            net_write_u32(&es, tick);
            net_write_u8(&es, cmd->player_id);
            net_write_i32(&es, data->building_id);
            net_write_u8(&es, data->permission);
            net_session_broadcast(NET_MSG_HOST_EVENT, event_buf,
                                  (uint32_t)net_serializer_position(&es));
            break;
        }

        case MP_CMD_CHAT_MESSAGE:
            /* Chat is handled directly by the session layer (net_session_send_chat).
             * If a chat command reaches here from a client, relay through session broadcast. */
            {
                uint8_t chat_buf[256];
                net_serializer s;
                net_serializer_init(&s, chat_buf, sizeof(chat_buf));
                net_write_u8(&s, cmd->player_id); /* Use verified player_id */
                net_write_string(&s, cmd->data.chat.message,
                                 sizeof(cmd->data.chat.message));
                net_session_broadcast(NET_MSG_CHAT, chat_buf,
                                      (uint32_t)net_serializer_position(&s));
            }
            break;

        default:
            break;
    }

    cmd->status = MP_CMD_STATUS_APPLIED;
}

static void send_ack(uint8_t player_id, uint32_t sequence_id, int accepted, uint8_t reason)
{
    uint8_t buf[8];
    net_serializer s;
    net_serializer_init(&s, buf, sizeof(buf));
    net_write_u32(&s, sequence_id);
    net_write_u8(&s, (uint8_t)accepted);
    net_write_u8(&s, reason);

    net_session *sess = net_session_get();
    for (int i = 0; i < NET_MAX_PEERS; i++) {
        if (sess->peers[i].active && sess->peers[i].player_id == player_id) {
            net_session_send_to_peer(i, NET_MSG_HOST_COMMAND_ACK,
                                     buf, (uint32_t)net_serializer_position(&s));
            break;
        }
    }
}

int mp_command_bus_submit(mp_command *cmd)
{
    cmd->sequence_id = bus.next_sequence_id++;

    /* Track stats */
    mp_player *player = mp_player_registry_get(cmd->player_id);

    if (net_session_is_host()) {
        int reject = validate_command(cmd);
        if (reject) {
            cmd->status = MP_CMD_STATUS_REJECTED;
            cmd->reject_reason = (uint8_t)reject;
            bus.last_status = MP_CMD_STATUS_REJECTED;
            bus.last_reject_reason = (uint8_t)reject;
            if (player) player->commands_rejected++;
            log_error("Command rejected", mp_command_type_name(cmd->command_type), reject);
            return 0;
        }
        cmd->status = MP_CMD_STATUS_ACCEPTED;
        apply_command(cmd);
        bus.last_status = MP_CMD_STATUS_APPLIED;
        if (player) player->commands_sent++;
        return 1;
    } else {
        uint8_t buf[512];
        uint32_t size;
        mp_command_serialize(cmd, buf, &size);
        net_session_send_to_host(NET_MSG_CLIENT_COMMAND, buf, size);
        cmd->status = MP_CMD_STATUS_PENDING;
        bus.last_status = MP_CMD_STATUS_PENDING;
        if (player) player->commands_sent++;
        return 1;
    }
}

void multiplayer_command_bus_receive(uint8_t player_id,
                                     const uint8_t *data, uint32_t size)
{
    mp_command cmd;
    memset(&cmd, 0, sizeof(cmd));
    mp_command_deserialize(&cmd, data, size);
    cmd.player_id = player_id; /* Override with verified peer player_id */

    /* Check player is connected */
    mp_player *player = mp_player_registry_get(player_id);
    if (!player || player->connection_state == MP_CONNECTION_DISCONNECTED) {
        send_ack(player_id, cmd.sequence_id, 0, MP_CMD_REJECT_PLAYER_DISCONNECTED);
        return;
    }

    int reject = validate_command(&cmd);
    if (reject) {
        cmd.status = MP_CMD_STATUS_REJECTED;
        cmd.reject_reason = (uint8_t)reject;
        send_ack(player_id, cmd.sequence_id, 0, (uint8_t)reject);
        if (player) player->commands_rejected++;
        log_error("Command from peer rejected",
                  mp_command_type_name(cmd.command_type), reject);
        return;
    }

    cmd.status = MP_CMD_STATUS_ACCEPTED;
    cmd.target_tick = net_session_get_authoritative_tick() + 1;
    if (!enqueue_command(&cmd)) {
        send_ack(player_id, cmd.sequence_id, 0, MP_CMD_REJECT_INVALID);
        return;
    }

    send_ack(player_id, cmd.sequence_id, 1, 0);
}

void multiplayer_command_bus_receive_ack(const uint8_t *data, uint32_t size)
{
    net_serializer s;
    net_serializer_init(&s, (uint8_t *)data, size);

    uint32_t sequence = net_read_u32(&s);
    uint8_t accepted = net_read_u8(&s);
    uint8_t reason = net_read_u8(&s);

    if (accepted) {
        bus.last_status = MP_CMD_STATUS_ACCEPTED;
        log_info("Command accepted by host", 0, (int)sequence);
    } else {
        bus.last_status = MP_CMD_STATUS_REJECTED;
        bus.last_reject_reason = reason;
        log_error("Command rejected by host", 0, (int)reason);
    }
}

void mp_command_bus_process_pending(uint32_t current_tick)
{
    int processed = 0;
    int count = bus.queue_count;

    for (int i = 0; i < count; i++) {
        mp_command *cmd = dequeue_command();
        if (!cmd) {
            break;
        }

        if (cmd->target_tick <= current_tick) {
            apply_command(cmd);
            processed++;
        } else {
            enqueue_command(cmd);
        }
    }

    if (processed > 0) {
        log_info("Commands applied this tick", 0, processed);
    }
}

mp_command_status mp_command_bus_get_last_status(void)
{
    return bus.last_status;
}

uint8_t mp_command_bus_get_last_reject_reason(void)
{
    return bus.last_reject_reason;
}

#endif /* ENABLE_MULTIPLAYER */

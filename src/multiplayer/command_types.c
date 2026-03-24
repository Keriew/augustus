#include "command_types.h"

#ifdef ENABLE_MULTIPLAYER

#include "network/serialize.h"

#include <string.h>

static const char *COMMAND_NAMES[] = {
    "NONE",
    "SET_READY",
    "REQUEST_PAUSE",
    "REQUEST_RESUME",
    "REQUEST_SPEED",
    "CREATE_TRADE_ROUTE",
    "DELETE_TRADE_ROUTE",
    "ENABLE_TRADE_ROUTE",
    "DISABLE_TRADE_ROUTE",
    "SET_ROUTE_POLICY",
    "SET_ROUTE_LIMIT",
    "OPEN_TRADE_ROUTE",
    "CLOSE_TRADE_ROUTE",
    "SET_RESOURCE_SETTING",
    "SET_STORAGE_STATE",
    "SET_STORAGE_PERMISSION",
    "CHAT_MESSAGE"
};

void mp_command_serialize(const mp_command *cmd, uint8_t *buffer, uint32_t *size)
{
    net_serializer s;
    net_serializer_init(&s, buffer, 512);

    net_write_u32(&s, cmd->sequence_id);
    net_write_u16(&s, cmd->command_type);
    net_write_u8(&s, cmd->player_id);
    net_write_u32(&s, cmd->target_tick);

    switch (cmd->command_type) {
        case MP_CMD_CREATE_TRADE_ROUTE:
            net_write_i32(&s, cmd->data.create_route.origin_city_id);
            net_write_i32(&s, cmd->data.create_route.dest_city_id);
            net_write_u8(&s, cmd->data.create_route.prefer_sea);
            break;

        case MP_CMD_DELETE_TRADE_ROUTE:
            net_write_i32(&s, cmd->data.delete_route.route_id);
            net_write_u32(&s, cmd->data.delete_route.network_route_id);
            break;

        case MP_CMD_ENABLE_TRADE_ROUTE:
            net_write_i32(&s, cmd->data.enable_route.route_id);
            net_write_u32(&s, cmd->data.enable_route.network_route_id);
            break;

        case MP_CMD_DISABLE_TRADE_ROUTE:
            net_write_i32(&s, cmd->data.disable_route.route_id);
            net_write_u32(&s, cmd->data.disable_route.network_route_id);
            break;

        case MP_CMD_SET_ROUTE_POLICY:
            net_write_i32(&s, cmd->data.route_policy.route_id);
            net_write_u32(&s, cmd->data.route_policy.network_route_id);
            net_write_i32(&s, cmd->data.route_policy.resource);
            net_write_u8(&s, (uint8_t)cmd->data.route_policy.is_export);
            net_write_u8(&s, (uint8_t)cmd->data.route_policy.enabled);
            break;

        case MP_CMD_SET_ROUTE_LIMIT:
            net_write_i32(&s, cmd->data.route_limit.route_id);
            net_write_u32(&s, cmd->data.route_limit.network_route_id);
            net_write_i32(&s, cmd->data.route_limit.resource);
            net_write_u8(&s, (uint8_t)cmd->data.route_limit.is_buying);
            net_write_i32(&s, cmd->data.route_limit.amount);
            break;

        case MP_CMD_OPEN_TRADE_ROUTE:
            net_write_i32(&s, cmd->data.open_route.route_id);
            net_write_i32(&s, cmd->data.open_route.city_id);
            break;

        case MP_CMD_CLOSE_TRADE_ROUTE:
            net_write_i32(&s, cmd->data.close_route.route_id);
            net_write_i32(&s, cmd->data.close_route.city_id);
            break;

        case MP_CMD_REQUEST_SPEED:
            net_write_u8(&s, cmd->data.speed.speed);
            break;

        case MP_CMD_SET_RESOURCE_SETTING:
            net_write_i32(&s, cmd->data.resource_setting.resource);
            net_write_u8(&s, cmd->data.resource_setting.setting_type);
            net_write_i32(&s, cmd->data.resource_setting.value);
            break;

        case MP_CMD_SET_STORAGE_STATE:
            net_write_i32(&s, cmd->data.storage_state.building_id);
            net_write_i32(&s, cmd->data.storage_state.resource);
            net_write_u8(&s, cmd->data.storage_state.new_state);
            break;

        case MP_CMD_SET_STORAGE_PERMISSION:
            net_write_i32(&s, cmd->data.storage_permission.building_id);
            net_write_u8(&s, cmd->data.storage_permission.permission);
            break;

        case MP_CMD_CHAT_MESSAGE:
            net_write_u8(&s, cmd->data.chat.sender_id);
            net_write_string(&s, cmd->data.chat.message, sizeof(cmd->data.chat.message));
            break;

        case MP_CMD_REQUEST_PAUSE:
        case MP_CMD_REQUEST_RESUME:
        case MP_CMD_SET_READY:
            /* No additional data */
            break;

        default:
            break;
    }

    *size = (uint32_t)net_serializer_position(&s);
}

void mp_command_deserialize(mp_command *cmd, const uint8_t *buffer, uint32_t size)
{
    net_serializer s;
    net_serializer_init(&s, (uint8_t *)buffer, size);

    cmd->sequence_id = net_read_u32(&s);
    cmd->command_type = net_read_u16(&s);
    cmd->player_id = net_read_u8(&s);
    cmd->target_tick = net_read_u32(&s);
    cmd->status = MP_CMD_STATUS_PENDING;
    cmd->reject_reason = 0;

    switch (cmd->command_type) {
        case MP_CMD_CREATE_TRADE_ROUTE:
            cmd->data.create_route.origin_city_id = net_read_i32(&s);
            cmd->data.create_route.dest_city_id = net_read_i32(&s);
            cmd->data.create_route.prefer_sea = net_read_u8(&s);
            break;

        case MP_CMD_DELETE_TRADE_ROUTE:
            cmd->data.delete_route.route_id = net_read_i32(&s);
            cmd->data.delete_route.network_route_id = net_read_u32(&s);
            break;

        case MP_CMD_ENABLE_TRADE_ROUTE:
            cmd->data.enable_route.route_id = net_read_i32(&s);
            cmd->data.enable_route.network_route_id = net_read_u32(&s);
            break;

        case MP_CMD_DISABLE_TRADE_ROUTE:
            cmd->data.disable_route.route_id = net_read_i32(&s);
            cmd->data.disable_route.network_route_id = net_read_u32(&s);
            break;

        case MP_CMD_SET_ROUTE_POLICY:
            cmd->data.route_policy.route_id = net_read_i32(&s);
            cmd->data.route_policy.network_route_id = net_read_u32(&s);
            cmd->data.route_policy.resource = net_read_i32(&s);
            cmd->data.route_policy.is_export = net_read_u8(&s);
            cmd->data.route_policy.enabled = net_read_u8(&s);
            break;

        case MP_CMD_SET_ROUTE_LIMIT:
            cmd->data.route_limit.route_id = net_read_i32(&s);
            cmd->data.route_limit.network_route_id = net_read_u32(&s);
            cmd->data.route_limit.resource = net_read_i32(&s);
            cmd->data.route_limit.is_buying = net_read_u8(&s);
            cmd->data.route_limit.amount = net_read_i32(&s);
            break;

        case MP_CMD_OPEN_TRADE_ROUTE:
            cmd->data.open_route.route_id = net_read_i32(&s);
            cmd->data.open_route.city_id = net_read_i32(&s);
            break;

        case MP_CMD_CLOSE_TRADE_ROUTE:
            cmd->data.close_route.route_id = net_read_i32(&s);
            cmd->data.close_route.city_id = net_read_i32(&s);
            break;

        case MP_CMD_REQUEST_SPEED:
            cmd->data.speed.speed = net_read_u8(&s);
            break;

        case MP_CMD_SET_RESOURCE_SETTING:
            cmd->data.resource_setting.resource = net_read_i32(&s);
            cmd->data.resource_setting.setting_type = net_read_u8(&s);
            cmd->data.resource_setting.value = net_read_i32(&s);
            break;

        case MP_CMD_SET_STORAGE_STATE:
            cmd->data.storage_state.building_id = net_read_i32(&s);
            cmd->data.storage_state.resource = net_read_i32(&s);
            cmd->data.storage_state.new_state = net_read_u8(&s);
            break;

        case MP_CMD_SET_STORAGE_PERMISSION:
            cmd->data.storage_permission.building_id = net_read_i32(&s);
            cmd->data.storage_permission.permission = net_read_u8(&s);
            break;

        case MP_CMD_CHAT_MESSAGE:
            cmd->data.chat.sender_id = net_read_u8(&s);
            net_read_string(&s, cmd->data.chat.message, sizeof(cmd->data.chat.message));
            break;

        default:
            break;
    }
}

const char *mp_command_type_name(mp_command_type type)
{
    if (type <= 0 || type >= MP_CMD_COUNT) {
        return COMMAND_NAMES[0];
    }
    return COMMAND_NAMES[type];
}

#endif /* ENABLE_MULTIPLAYER */

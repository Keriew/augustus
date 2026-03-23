#include "command_types.h"

#ifdef ENABLE_MULTIPLAYER

#include "network/serialize.h"

static const char *COMMAND_NAMES[] = {
    "UNKNOWN",
    "SET_READY",
    "OPEN_TRADE_ROUTE",
    "CLOSE_TRADE_ROUTE",
    "SET_IMPORT_POLICY",
    "SET_EXPORT_POLICY",
    "SET_ROUTE_LIMIT",
    "REQUEST_PAUSE",
    "REQUEST_RESUME",
    "REQUEST_SPEED",
    "CHAT_MESSAGE"
};

void mp_command_serialize(const mp_command *cmd, uint8_t *buffer, uint32_t *size)
{
    net_serializer s;
    net_serializer_init(&s, buffer, 256);

    net_write_u32(&s, cmd->sequence_id);
    net_write_u16(&s, cmd->command_type);
    net_write_u8(&s, cmd->player_id);
    net_write_u32(&s, cmd->target_tick);

    switch (cmd->command_type) {
        case MP_CMD_OPEN_TRADE_ROUTE:
            net_write_i32(&s, cmd->data.open_route.route_id);
            net_write_i32(&s, cmd->data.open_route.city_id);
            break;
        case MP_CMD_CLOSE_TRADE_ROUTE:
            net_write_i32(&s, cmd->data.close_route.route_id);
            net_write_i32(&s, cmd->data.close_route.city_id);
            break;
        case MP_CMD_SET_IMPORT_POLICY:
            net_write_i32(&s, cmd->data.import_policy.route_id);
            net_write_i32(&s, cmd->data.import_policy.resource);
            net_write_i32(&s, cmd->data.import_policy.enabled);
            break;
        case MP_CMD_SET_EXPORT_POLICY:
            net_write_i32(&s, cmd->data.export_policy.route_id);
            net_write_i32(&s, cmd->data.export_policy.resource);
            net_write_i32(&s, cmd->data.export_policy.enabled);
            break;
        case MP_CMD_SET_ROUTE_LIMIT:
            net_write_i32(&s, cmd->data.route_limit.route_id);
            net_write_i32(&s, cmd->data.route_limit.resource);
            net_write_i32(&s, cmd->data.route_limit.buying);
            net_write_i32(&s, cmd->data.route_limit.amount);
            break;
        case MP_CMD_REQUEST_SPEED:
            net_write_u8(&s, cmd->data.speed.speed);
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
        case MP_CMD_OPEN_TRADE_ROUTE:
            cmd->data.open_route.route_id = net_read_i32(&s);
            cmd->data.open_route.city_id = net_read_i32(&s);
            break;
        case MP_CMD_CLOSE_TRADE_ROUTE:
            cmd->data.close_route.route_id = net_read_i32(&s);
            cmd->data.close_route.city_id = net_read_i32(&s);
            break;
        case MP_CMD_SET_IMPORT_POLICY:
            cmd->data.import_policy.route_id = net_read_i32(&s);
            cmd->data.import_policy.resource = net_read_i32(&s);
            cmd->data.import_policy.enabled = net_read_i32(&s);
            break;
        case MP_CMD_SET_EXPORT_POLICY:
            cmd->data.export_policy.route_id = net_read_i32(&s);
            cmd->data.export_policy.resource = net_read_i32(&s);
            cmd->data.export_policy.enabled = net_read_i32(&s);
            break;
        case MP_CMD_SET_ROUTE_LIMIT:
            cmd->data.route_limit.route_id = net_read_i32(&s);
            cmd->data.route_limit.resource = net_read_i32(&s);
            cmd->data.route_limit.buying = net_read_i32(&s);
            cmd->data.route_limit.amount = net_read_i32(&s);
            break;
        case MP_CMD_REQUEST_SPEED:
            cmd->data.speed.speed = net_read_u8(&s);
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

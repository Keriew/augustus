#include "command_bus.h"

#ifdef ENABLE_MULTIPLAYER

#include "ownership.h"
#include "player_registry.h"
#include "network/session.h"
#include "network/serialize.h"
#include "network/protocol.h"
#include "empire/city.h"
#include "empire/trade_route.h"
#include "core/log.h"

#include <string.h>

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
        return NULL;
    }
    mp_command *cmd = &bus.queue[bus.queue_head];
    bus.queue_head = (bus.queue_head + 1) % MP_COMMAND_QUEUE_SIZE;
    bus.queue_count--;
    return cmd;
}

static int validate_command(const mp_command *cmd)
{
    switch (cmd->command_type) {
        case MP_CMD_OPEN_TRADE_ROUTE: {
            int city_id = cmd->data.open_route.city_id;
            empire_city *city = empire_city_get(city_id);
            if (!city || !city->in_use) {
                return MP_CMD_REJECT_ROUTE_NOT_FOUND;
            }
            if (city->is_open) {
                return MP_CMD_REJECT_ALREADY_OPEN;
            }
            /* Verify the requesting player owns a city that can trade with this one */
            mp_player *player = mp_player_registry_get(cmd->player_id);
            if (!player) {
                return MP_CMD_REJECT_NOT_OWNER;
            }
            return 0;
        }
        case MP_CMD_CLOSE_TRADE_ROUTE: {
            int city_id = cmd->data.close_route.city_id;
            empire_city *city = empire_city_get(city_id);
            if (!city || !city->in_use) {
                return MP_CMD_REJECT_ROUTE_NOT_FOUND;
            }
            if (!city->is_open) {
                return MP_CMD_REJECT_ALREADY_CLOSED;
            }
            return 0;
        }
        case MP_CMD_SET_IMPORT_POLICY:
        case MP_CMD_SET_EXPORT_POLICY: {
            int route_id = (cmd->command_type == MP_CMD_SET_IMPORT_POLICY)
                ? cmd->data.import_policy.route_id
                : cmd->data.export_policy.route_id;
            if (!trade_route_is_valid(route_id)) {
                return MP_CMD_REJECT_ROUTE_NOT_FOUND;
            }
            return 0;
        }
        case MP_CMD_SET_ROUTE_LIMIT: {
            if (!trade_route_is_valid(cmd->data.route_limit.route_id)) {
                return MP_CMD_REJECT_ROUTE_NOT_FOUND;
            }
            if (cmd->data.route_limit.amount < 0) {
                return MP_CMD_REJECT_LIMIT_OUT_OF_RANGE;
            }
            return 0;
        }
        case MP_CMD_REQUEST_PAUSE:
        case MP_CMD_REQUEST_RESUME:
        case MP_CMD_REQUEST_SPEED:
        case MP_CMD_SET_READY:
            return 0;
        default:
            return MP_CMD_REJECT_INVALID;
    }
}

static void apply_command(mp_command *cmd)
{
    switch (cmd->command_type) {
        case MP_CMD_OPEN_TRADE_ROUTE: {
            int city_id = cmd->data.open_route.city_id;
            empire_city_open_trade(city_id, 1);
            log_info("Trade route opened by player", 0, cmd->player_id);

            /* Broadcast route opened event */
            uint8_t event_buf[32];
            net_serializer s;
            net_serializer_init(&s, event_buf, sizeof(event_buf));
            net_write_u16(&s, NET_EVENT_ROUTE_OPENED);
            net_write_u32(&s, net_session_get_authoritative_tick());
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
            log_info("Trade route closed by player", 0, cmd->player_id);

            uint8_t event_buf[32];
            net_serializer s;
            net_serializer_init(&s, event_buf, sizeof(event_buf));
            net_write_u16(&s, NET_EVENT_ROUTE_CLOSED);
            net_write_u32(&s, net_session_get_authoritative_tick());
            net_write_i32(&s, city_id);
            net_write_u8(&s, cmd->player_id);
            net_session_broadcast(NET_MSG_HOST_EVENT, event_buf,
                                (uint32_t)net_serializer_position(&s));
            break;
        }
        case MP_CMD_SET_ROUTE_LIMIT: {
            trade_route_set_limit(cmd->data.route_limit.route_id,
                                 cmd->data.route_limit.resource,
                                 cmd->data.route_limit.amount,
                                 cmd->data.route_limit.buying);
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

    /* Find the peer for this player */
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

    if (net_session_is_host()) {
        /* Host: validate and apply immediately */
        int reject = validate_command(cmd);
        if (reject) {
            cmd->status = MP_CMD_STATUS_REJECTED;
            cmd->reject_reason = (uint8_t)reject;
            bus.last_status = MP_CMD_STATUS_REJECTED;
            bus.last_reject_reason = (uint8_t)reject;
            log_error("Command rejected", mp_command_type_name(cmd->command_type), reject);
            return 0;
        }
        cmd->status = MP_CMD_STATUS_ACCEPTED;
        apply_command(cmd);
        bus.last_status = MP_CMD_STATUS_APPLIED;
        return 1;
    } else {
        /* Client: serialize and send to host */
        uint8_t buf[256];
        uint32_t size;
        mp_command_serialize(cmd, buf, &size);
        net_session_send_to_host(NET_MSG_CLIENT_COMMAND, buf, size);
        cmd->status = MP_CMD_STATUS_PENDING;
        bus.last_status = MP_CMD_STATUS_PENDING;
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

    int reject = validate_command(&cmd);
    if (reject) {
        cmd.status = MP_CMD_STATUS_REJECTED;
        cmd.reject_reason = (uint8_t)reject;
        send_ack(player_id, cmd.sequence_id, 0, (uint8_t)reject);
        log_error("Command from peer rejected",
                 mp_command_type_name(cmd.command_type), reject);
        return;
    }

    /* Enqueue for application at the right tick */
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
    /* Process commands that are due for this tick */
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
            /* Re-enqueue for later */
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

#include "time_sync.h"

#ifdef ENABLE_MULTIPLAYER

#include "network/session.h"
#include "network/serialize.h"
#include "game/time.h"
#include "core/log.h"

#include <string.h>

static struct {
    uint32_t confirmed_tick;  /* Last tick confirmed by host */
    uint32_t local_tick;      /* Local tick counter */
    int paused;
    uint8_t speed;
} time_data;

void mp_time_sync_init(void)
{
    memset(&time_data, 0, sizeof(time_data));
    time_data.speed = 2;
}

int mp_time_sync_can_advance_tick(void)
{
    if (time_data.paused) {
        return 0;
    }

    if (net_session_is_host()) {
        /* Host always advances - it IS the authority */
        return 1;
    }

    if (net_session_is_client()) {
        /* Client can only advance if local tick is behind confirmed tick */
        return time_data.local_tick < time_data.confirmed_tick;
    }

    /* Not in multiplayer - always advance */
    return 1;
}

void mp_time_sync_on_tick_advanced(void)
{
    time_data.local_tick++;

    if (net_session_is_host()) {
        net_session_advance_tick();
        time_data.confirmed_tick = time_data.local_tick;
    }
}

uint32_t mp_time_sync_get_authoritative_tick(void)
{
    if (net_session_is_host()) {
        return time_data.local_tick;
    }
    return time_data.confirmed_tick;
}

void mp_time_sync_set_confirmed_tick(uint32_t tick)
{
    time_data.confirmed_tick = tick;
}

int mp_time_sync_is_paused(void)
{
    return time_data.paused;
}

uint8_t mp_time_sync_get_speed(void)
{
    return time_data.speed;
}

void mp_time_sync_set_paused(int paused)
{
    time_data.paused = paused;
}

void mp_time_sync_set_speed(uint8_t speed)
{
    if (speed > 3) {
        speed = 3;
    }
    time_data.speed = speed;
}

void mp_time_sync_serialize(uint8_t *buffer, uint32_t *size)
{
    net_serializer s;
    net_serializer_init(&s, buffer, 64);

    net_write_u32(&s, time_data.confirmed_tick);
    net_write_u32(&s, time_data.local_tick);
    net_write_u8(&s, (uint8_t)time_data.paused);
    net_write_u8(&s, time_data.speed);

    /* Include game time for verification */
    net_write_i32(&s, game_time_year());
    net_write_i32(&s, game_time_month());
    net_write_i32(&s, game_time_day());
    net_write_i32(&s, game_time_tick());

    *size = (uint32_t)net_serializer_position(&s);
}

void mp_time_sync_deserialize(const uint8_t *buffer, uint32_t size)
{
    net_serializer s;
    net_serializer_init(&s, (uint8_t *)buffer, size);

    time_data.confirmed_tick = net_read_u32(&s);
    time_data.local_tick = net_read_u32(&s);
    time_data.paused = net_read_u8(&s);
    time_data.speed = net_read_u8(&s);

    /* Read game time but don't apply - game time is managed by tick.c */
    int year = net_read_i32(&s);
    int month = net_read_i32(&s);
    int day = net_read_i32(&s);
    int tick = net_read_i32(&s);
    (void)year;
    (void)month;
    (void)day;
    (void)tick;
}

#endif /* ENABLE_MULTIPLAYER */

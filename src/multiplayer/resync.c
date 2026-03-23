#include "resync.h"

#ifdef ENABLE_MULTIPLAYER

#include "snapshot.h"
#include "network/session.h"
#include "network/protocol.h"
#include "core/log.h"

#include <string.h>
#include <stdlib.h>

static struct {
    int in_progress;
    int attempt_count;
} resync_data;

void mp_resync_init(void)
{
    memset(&resync_data, 0, sizeof(resync_data));
}

void mp_resync_request_full_snapshot(void)
{
    if (!net_session_is_client()) {
        return;
    }
    if (resync_data.in_progress) {
        return;
    }

    resync_data.in_progress = 1;
    resync_data.attempt_count++;

    log_info("Requesting full resync from host", 0, resync_data.attempt_count);

    net_session_send_to_host(NET_MSG_RESYNC_REQUEST, NULL, 0);
}

void multiplayer_resync_handle_request(uint8_t player_id)
{
    if (!net_session_is_host()) {
        return;
    }

    log_info("Resync requested by player", 0, player_id);

    /* Build a full snapshot */
    uint8_t *snapshot_buffer = (uint8_t *)malloc(MP_SNAPSHOT_MAX_SIZE);
    if (!snapshot_buffer) {
        log_error("Failed to allocate resync snapshot buffer", 0, 0);
        return;
    }

    uint32_t snapshot_size = 0;
    if (mp_snapshot_build_full(snapshot_buffer, MP_SNAPSHOT_MAX_SIZE, &snapshot_size)) {
        /* Send to the requesting player */
        net_session *sess = net_session_get();
        for (int i = 0; i < NET_MAX_PEERS; i++) {
            if (sess->peers[i].active && sess->peers[i].player_id == player_id) {
                net_session_send_to_peer(i, NET_MSG_RESYNC_GRANTED,
                                        snapshot_buffer, snapshot_size);
                break;
            }
        }
    }

    free(snapshot_buffer);
}

void multiplayer_resync_apply_full_snapshot(const uint8_t *data, uint32_t size)
{
    log_info("Applying resync snapshot", 0, (int)size);

    if (mp_snapshot_apply_full(data, size)) {
        resync_data.in_progress = 0;
        log_info("Resync completed successfully", 0, 0);
    } else {
        log_error("Resync snapshot application failed", 0, 0);
        resync_data.in_progress = 0;
    }
}

int mp_resync_is_in_progress(void)
{
    return resync_data.in_progress;
}

int mp_resync_get_attempt_count(void)
{
    return resync_data.attempt_count;
}

#endif /* ENABLE_MULTIPLAYER */

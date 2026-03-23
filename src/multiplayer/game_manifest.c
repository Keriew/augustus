#include "game_manifest.h"

#ifdef ENABLE_MULTIPLAYER

#include "network/serialize.h"
#include "mp_debug_log.h"

#include <string.h>

static mp_game_manifest manifest;

void mp_game_manifest_init(void)
{
    memset(&manifest, 0, sizeof(mp_game_manifest));
}

void mp_game_manifest_clear(void)
{
    memset(&manifest, 0, sizeof(mp_game_manifest));
}

void mp_game_manifest_set(mp_game_mode mode, const char *scenario_name,
                          uint32_t map_hash, uint32_t scenario_hash,
                          uint32_t save_version, uint32_t session_seed,
                          uint8_t max_players)
{
    manifest.mode = mode;
    memset(manifest.scenario_name, 0, MP_MANIFEST_SCENARIO_NAME_MAX);
    if (scenario_name) {
        strncpy(manifest.scenario_name, scenario_name, MP_MANIFEST_SCENARIO_NAME_MAX - 1);
    }
    manifest.map_hash = map_hash;
    manifest.scenario_hash = scenario_hash;
    manifest.save_version = save_version;
    manifest.session_seed = session_seed;
    manifest.max_players = max_players;
    manifest.player_count = 1; /* Host is already counted */
    manifest.feature_flags = 0;
    manifest.valid = 1;

    MP_LOG_INFO("GAME", "Manifest set: mode=%d scenario='%s' map_hash=0x%08x "
                "scenario_hash=0x%08x seed=%u max_players=%d",
                (int)mode, manifest.scenario_name, map_hash,
                scenario_hash, session_seed, (int)max_players);
}

const mp_game_manifest *mp_game_manifest_get(void)
{
    return &manifest;
}

mp_game_manifest *mp_game_manifest_get_mutable(void)
{
    return &manifest;
}

void mp_game_manifest_set_player_count(uint8_t count)
{
    manifest.player_count = count;
}

void mp_game_manifest_serialize(uint8_t *buffer, uint32_t *out_size)
{
    net_serializer s;
    net_serializer_init(&s, buffer, 256);

    net_write_u8(&s, (uint8_t)manifest.mode);
    net_write_raw(&s, manifest.scenario_name, MP_MANIFEST_SCENARIO_NAME_MAX);
    net_write_u32(&s, manifest.map_hash);
    net_write_u32(&s, manifest.scenario_hash);
    net_write_u32(&s, manifest.save_version);
    net_write_u32(&s, manifest.session_seed);
    net_write_u8(&s, manifest.max_players);
    net_write_u8(&s, manifest.player_count);
    net_write_u32(&s, manifest.feature_flags);

    *out_size = (uint32_t)net_serializer_position(&s);
}

int mp_game_manifest_deserialize(const uint8_t *buffer, uint32_t size)
{
    net_serializer s;
    net_serializer_init(&s, (uint8_t *)buffer, size);

    manifest.mode = (mp_game_mode)net_read_u8(&s);
    net_read_raw(&s, manifest.scenario_name, MP_MANIFEST_SCENARIO_NAME_MAX);
    manifest.scenario_name[MP_MANIFEST_SCENARIO_NAME_MAX - 1] = '\0';
    manifest.map_hash = net_read_u32(&s);
    manifest.scenario_hash = net_read_u32(&s);
    manifest.save_version = net_read_u32(&s);
    manifest.session_seed = net_read_u32(&s);
    manifest.max_players = net_read_u8(&s);
    manifest.player_count = net_read_u8(&s);
    manifest.feature_flags = net_read_u32(&s);

    if (net_serializer_has_overflow(&s)) {
        MP_LOG_ERROR("GAME", "Manifest deserialize overflow: size=%u", size);
        memset(&manifest, 0, sizeof(mp_game_manifest));
        return 0;
    }

    manifest.valid = 1;

    MP_LOG_INFO("GAME", "Manifest received: mode=%d scenario='%s' map_hash=0x%08x "
                "seed=%u max_players=%d",
                (int)manifest.mode, manifest.scenario_name, manifest.map_hash,
                manifest.session_seed, (int)manifest.max_players);
    return 1;
}

int mp_game_manifest_validate_local(void)
{
    if (!manifest.valid) {
        return 0;
    }
    /* For MVP, just check that the scenario name is non-empty.
     * Hash validation will be added when scenario hashing is implemented. */
    if (manifest.scenario_name[0] == '\0') {
        MP_LOG_ERROR("GAME", "Manifest validation failed: empty scenario name");
        return 0;
    }
    return 1;
}

#endif /* ENABLE_MULTIPLAYER */

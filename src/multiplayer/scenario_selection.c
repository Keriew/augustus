#include "scenario_selection.h"

#ifdef ENABLE_MULTIPLAYER

#include "mp_debug_log.h"
#include "empire/city.h"
#include "empire/object.h"
#include "empire/type.h"
#include "core/dir.h"
#include "core/file.h"
#include "game/resource.h"

#include <string.h>
#include <stdio.h>

/* FNV-1a 32-bit constants */
#define FNV_OFFSET 2166136261u
#define FNV_PRIME  16777619u

/* Hash chunk size — small stack allocation, iterated over file */
#define HASH_CHUNK_SIZE 4096

static int city_has_trade_resources(int city_id)
{
    empire_city *city = empire_city_get(city_id);
    if (!city) {
        return 0;
    }

    for (int r = 0; r < RESOURCE_MAX; r++) {
        if (city->buys_resource[r] || city->sells_resource[r]) {
            return 1;
        }
    }
    return 0;
}

int mp_scenario_count_eligible_cities(void)
{
    int count = 0;
    int num_cities = empire_city_get_array_size();

    for (int i = 0; i < num_cities; i++) {
        empire_city *city = empire_city_get(i);
        if (!city || !city->in_use) {
            continue;
        }

        /* Only trade-capable city types are valid spawn targets */
        if (city->type != EMPIRE_CITY_TRADE &&
            city->type != EMPIRE_CITY_FUTURE_TRADE &&
            city->type != EMPIRE_CITY_DISTANT_ROMAN) {
            continue;
        }

        /* Must have a valid route_id for trade mechanics */
        if (city->route_id < 0) {
            continue;
        }

        /* Must buy or sell at least one resource (trade viability) */
        if (!city_has_trade_resources(i)) {
            continue;
        }

        /* Must have a valid empire object for map position */
        if (city->empire_object_id < 0) {
            continue;
        }
        const empire_object *obj = empire_object_get(city->empire_object_id);
        if (!obj) {
            continue;
        }

        count++;
    }

    MP_LOG_INFO("SCENARIO", "Eligible trade cities for multiplayer: %d", count);
    return count;
}

int mp_scenario_validate_for_multiplayer(int player_count)
{
    if (player_count < 2 || player_count > 8) {
        MP_LOG_ERROR("SCENARIO", "Invalid player count: %d (must be 2-8)", player_count);
        return 0;
    }

    int eligible = mp_scenario_count_eligible_cities();

    if (eligible < player_count) {
        MP_LOG_ERROR("SCENARIO", "Scenario not viable for %d players: only %d eligible cities",
                     player_count, eligible);
        return 0;
    }

    MP_LOG_INFO("SCENARIO", "Scenario validated: %d eligible cities for %d players",
                eligible, player_count);
    return 1;
}

/**
 * Try to find and open a scenario file with the given base name.
 * Tries .map then .mapx extensions.
 * @param scenario_name Base name (no extension)
 * @return FILE pointer or NULL
 */
static FILE *open_scenario_file(const char *scenario_name)
{
    char filename[FILE_NAME_MAX];
    const char *resolved;

    /* Try .map */
    snprintf(filename, FILE_NAME_MAX, "%s.map", scenario_name);
    resolved = dir_get_file_at_location(filename, PATH_LOCATION_SCENARIO);
    if (resolved) {
        FILE *fp = file_open(resolved, "rb");
        if (fp) {
            return fp;
        }
    }

    /* Try .mapx */
    snprintf(filename, FILE_NAME_MAX, "%s.mapx", scenario_name);
    resolved = dir_get_file_at_location(filename, PATH_LOCATION_SCENARIO);
    if (resolved) {
        FILE *fp = file_open(resolved, "rb");
        if (fp) {
            return fp;
        }
    }

    return NULL;
}

int mp_scenario_compute_file_hash(const char *scenario_name, uint32_t *out_hash)
{
    if (!scenario_name || !scenario_name[0] || !out_hash) {
        return 0;
    }

    FILE *fp = open_scenario_file(scenario_name);
    if (!fp) {
        MP_LOG_ERROR("SCENARIO", "Cannot find scenario file for hash: '%s'", scenario_name);
        return 0;
    }

    /* Compute FNV-1a hash in chunks — no large allocation needed */
    uint32_t hash = FNV_OFFSET;
    uint8_t chunk[HASH_CHUNK_SIZE];
    size_t bytes_read;
    size_t total_bytes = 0;

    while ((bytes_read = fread(chunk, 1, sizeof(chunk), fp)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            hash ^= chunk[i];
            hash *= FNV_PRIME;
        }
        total_bytes += bytes_read;
    }

    file_close(fp);

    *out_hash = hash;
    MP_LOG_INFO("SCENARIO", "Hash for '%s': 0x%08x (%u bytes)",
                scenario_name, hash, (unsigned int)total_bytes);
    return 1;
}

int mp_scenario_hashes_match(uint32_t local_hash, uint32_t manifest_hash)
{
    if (manifest_hash == 0) {
        /* Legacy manifest without hash — skip validation */
        MP_LOG_WARN("SCENARIO", "Manifest has no scenario hash — skipping validation");
        return 1;
    }
    if (local_hash != manifest_hash) {
        MP_LOG_ERROR("SCENARIO", "Hash mismatch: local=0x%08x manifest=0x%08x",
                     local_hash, manifest_hash);
        return 0;
    }
    MP_LOG_INFO("SCENARIO", "Hash match confirmed: 0x%08x", local_hash);
    return 1;
}

#endif /* ENABLE_MULTIPLAYER */

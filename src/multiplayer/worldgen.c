#include "worldgen.h"

#ifdef ENABLE_MULTIPLAYER

#include "player_registry.h"
#include "ownership.h"
#include "empire/city.h"
#include "empire/object.h"
#include "empire/type.h"
#include "empire/empire.h"
#include "empire/trade_route.h"
#include "network/serialize.h"
#include "core/log.h"

#include <string.h>
#include <math.h>

/* Worldgen parameters */
#define MIN_PLAYER_DISTANCE     80   /* Minimum pixel distance between player spawns */
#define MIN_AI_DISTANCE         40   /* Minimum pixel distance from AI trade cities */
#define MAX_CLUSTER_RADIUS      400  /* Maximum distance from cluster center */
#define FAIRNESS_TOLERANCE      50   /* Max fairness score difference between spawns */

/* FNV-1a 32-bit for deterministic hashing */
#define FNV_OFFSET 2166136261u
#define FNV_PRIME  16777619u

static mp_spawn_table spawn_table;

typedef struct {
    int city_id;
    int object_id;
    int x;
    int y;
    int is_sea;
    int is_land;
    int route_cost;
    int is_xml_slot;       /* From XML <multiplayer> element */
    int xml_slot_id;       /* Assigned slot from XML, or -1 */
    int ai_distance;       /* Distance to nearest AI trade city */
} spawn_candidate;

/* ---- Deterministic PRNG seeded from session_seed ---- */

static uint32_t prng_state;

static void prng_seed(uint32_t seed)
{
    prng_state = seed ^ FNV_OFFSET;
    if (prng_state == 0) {
        prng_state = FNV_OFFSET;
    }
}

static uint32_t prng_next(void)
{
    /* xorshift32 */
    prng_state ^= prng_state << 13;
    prng_state ^= prng_state >> 17;
    prng_state ^= prng_state << 5;
    return prng_state;
}

static int prng_range(int min_val, int max_val)
{
    if (min_val >= max_val) {
        return min_val;
    }
    uint32_t range = (uint32_t)(max_val - min_val + 1);
    return min_val + (int)(prng_next() % range);
}

/* ---- Distance calculation ---- */

static int distance_sq(int x1, int y1, int x2, int y2)
{
    int dx = x1 - x2;
    int dy = y1 - y2;
    return dx * dx + dy * dy;
}

static int distance(int x1, int y1, int x2, int y2)
{
    int dsq = distance_sq(x1, y1, x2, y2);
    if (dsq <= 0) {
        return 0;
    }
    /* Integer square root approximation */
    int result = 1;
    int temp = dsq;
    while (temp > 1) {
        temp >>= 2;
        result <<= 1;
    }
    /* Newton's method refinement */
    for (int i = 0; i < 8; i++) {
        if (result == 0) break;
        result = (result + dsq / result) / 2;
    }
    return result;
}

/* ---- Candidate Collection ---- */

static int collect_xml_slot_candidates(spawn_candidate *candidates, int max_candidates)
{
    int count = 0;
    int num_cities = empire_city_get_array_size();

    for (int i = 0; i < num_cities && count < max_candidates; i++) {
        empire_city *city = empire_city_get(i);
        if (!city || !city->in_use) {
            continue;
        }
        /* Check for XML multiplayer metadata in replicated_flags.
         * Bits 0-7 = slot, bit 8 = host_start, bit 9 = allow_human.
         * A city with bit 9 set is an explicit multiplayer slot. */
#ifdef ENABLE_MULTIPLAYER
        if (city->replicated_flags & 0x200) {  /* bit 9 = allow_human */
            int xml_slot = city->replicated_flags & 0xFF;
            candidates[count].city_id = i;
            candidates[count].object_id = city->empire_object_id;

            /* Get position from empire object */
            const empire_object *obj = empire_object_get(city->empire_object_id);
            if (obj) {
                candidates[count].x = obj->x;
                candidates[count].y = obj->y;
            }

            candidates[count].is_sea = city->is_sea_trade;
            candidates[count].is_land = !city->is_sea_trade;
            candidates[count].route_cost = city->cost_to_open;
            candidates[count].is_xml_slot = 1;
            candidates[count].xml_slot_id = xml_slot;
            candidates[count].ai_distance = 0;
            count++;
        }
#endif
    }
    return count;
}

static int collect_ai_trade_cities(int *ai_xs, int *ai_ys, int max_ai)
{
    int count = 0;
    int num_cities = empire_city_get_array_size();

    for (int i = 0; i < num_cities && count < max_ai; i++) {
        empire_city *city = empire_city_get(i);
        if (!city || !city->in_use) {
            continue;
        }
        if (city->type != EMPIRE_CITY_TRADE && city->type != EMPIRE_CITY_FUTURE_TRADE) {
            continue;
        }

        const empire_object *obj = empire_object_get(city->empire_object_id);
        if (obj) {
            ai_xs[count] = obj->x;
            ai_ys[count] = obj->y;
            count++;
        }
    }
    return count;
}

static int compute_ai_min_distance(int x, int y, const int *ai_xs, const int *ai_ys, int ai_count)
{
    int min_dist = 999999;
    for (int i = 0; i < ai_count; i++) {
        int d = distance(x, y, ai_xs[i], ai_ys[i]);
        if (d < min_dist) {
            min_dist = d;
        }
    }
    return min_dist;
}

static int collect_procedural_candidates(spawn_candidate *candidates, int max_candidates,
                                          const int *ai_xs, const int *ai_ys, int ai_count)
{
    int count = 0;
    int num_cities = empire_city_get_array_size();

    /* Look for trade cities (AI) that could be repurposed or cities near good positions */
    for (int i = 0; i < num_cities && count < max_candidates; i++) {
        empire_city *city = empire_city_get(i);
        if (!city || !city->in_use) {
            continue;
        }
        /* Only consider cities that are trade-capable and not "ours" */
        if (city->type != EMPIRE_CITY_TRADE &&
            city->type != EMPIRE_CITY_FUTURE_TRADE &&
            city->type != EMPIRE_CITY_DISTANT_ROMAN) {
            continue;
        }

        const empire_object *obj = empire_object_get(city->empire_object_id);
        if (!obj) {
            continue;
        }

        int ai_dist = compute_ai_min_distance(obj->x, obj->y, ai_xs, ai_ys, ai_count);

        candidates[count].city_id = i;
        candidates[count].object_id = city->empire_object_id;
        candidates[count].x = obj->x;
        candidates[count].y = obj->y;
        candidates[count].is_sea = city->is_sea_trade;
        candidates[count].is_land = !city->is_sea_trade;
        candidates[count].route_cost = city->cost_to_open;
        candidates[count].is_xml_slot = 0;
        candidates[count].xml_slot_id = -1;
        candidates[count].ai_distance = ai_dist;
        count++;
    }

    return count;
}

/* ---- Cluster center computation ---- */

static void compute_cluster_center(const spawn_candidate *candidates, int count,
                                    int *center_x, int *center_y)
{
    if (count == 0) {
        *center_x = 500;
        *center_y = 500;
        return;
    }

    long sum_x = 0, sum_y = 0;
    for (int i = 0; i < count; i++) {
        sum_x += candidates[i].x;
        sum_y += candidates[i].y;
    }
    *center_x = (int)(sum_x / count);
    *center_y = (int)(sum_y / count);
}

/* ---- Fairness scoring ---- */

static int compute_fairness_score(const spawn_candidate *c,
                                   int center_x, int center_y,
                                   int avg_route_cost)
{
    int score = 0;

    /* Distance from cluster center - prefer closer */
    int dist_from_center = distance(c->x, c->y, center_x, center_y);
    if (dist_from_center > MAX_CLUSTER_RADIUS) {
        score += (dist_from_center - MAX_CLUSTER_RADIUS) / 10;
    }

    /* Route cost deviation from average */
    int cost_diff = c->route_cost - avg_route_cost;
    if (cost_diff < 0) cost_diff = -cost_diff;
    score += cost_diff / 100;

    /* Penalty for no sea access (in maps where sea trade exists) */
    if (!c->is_sea && !c->is_land) {
        score += 100;
    }

    return score;
}

/* ---- Selection with constraints ---- */

static int check_min_distance(const mp_spawn_entry *spawns, int spawn_count,
                               int x, int y, int min_dist)
{
    for (int i = 0; i < spawn_count; i++) {
        if (!spawns[i].valid) {
            continue;
        }
        int d = distance(x, y, spawns[i].x, spawns[i].y);
        if (d < min_dist) {
            return 0;
        }
    }
    return 1;
}

static int select_spawns_from_candidates(spawn_candidate *candidates, int candidate_count,
                                          mp_spawn_entry *spawns, int player_count,
                                          const int *ai_xs, const int *ai_ys, int ai_count)
{
    if (candidate_count < player_count) {
        log_error("Not enough spawn candidates", 0, candidate_count);
        return 0;
    }

    /* Compute cluster center and average route cost */
    int center_x, center_y;
    compute_cluster_center(candidates, candidate_count, &center_x, &center_y);

    int total_cost = 0;
    for (int i = 0; i < candidate_count; i++) {
        total_cost += candidates[i].route_cost;
    }
    int avg_cost = candidate_count > 0 ? total_cost / candidate_count : 0;

    /* Score all candidates */
    int scores[MP_WORLDGEN_MAX_CANDIDATES];
    for (int i = 0; i < candidate_count; i++) {
        scores[i] = compute_fairness_score(&candidates[i], center_x, center_y, avg_cost);
    }

    /* Fisher-Yates shuffle with deterministic PRNG, then sort by score */
    int indices[MP_WORLDGEN_MAX_CANDIDATES];
    for (int i = 0; i < candidate_count; i++) {
        indices[i] = i;
    }
    for (int i = candidate_count - 1; i > 0; i--) {
        int j = prng_range(0, i);
        int tmp = indices[i];
        indices[i] = indices[j];
        indices[j] = tmp;
    }

    /* Stable insertion sort by score (preserves random order for equal scores) */
    for (int i = 1; i < candidate_count; i++) {
        int key = indices[i];
        int key_score = scores[key];
        int j = i - 1;
        while (j >= 0 && scores[indices[j]] > key_score) {
            indices[j + 1] = indices[j];
            j--;
        }
        indices[j + 1] = key;
    }

    /* Greedy selection with constraint checking */
    int selected = 0;
    for (int i = 0; i < candidate_count && selected < player_count; i++) {
        int ci = indices[i];
        spawn_candidate *c = &candidates[ci];

        /* Check minimum distance from already-selected spawns */
        if (!check_min_distance(spawns, selected, c->x, c->y, MIN_PLAYER_DISTANCE)) {
            continue;
        }

        /* Check minimum distance from AI cities */
        int ai_dist = compute_ai_min_distance(c->x, c->y, ai_xs, ai_ys, ai_count);
        if (ai_dist < MIN_AI_DISTANCE) {
            continue;
        }

        /* Accept this candidate */
        mp_spawn_entry *entry = &spawns[selected];
        entry->valid = 1;
        entry->slot_id = (uint8_t)selected;
        entry->empire_city_id = c->city_id;
        entry->empire_object_id = c->object_id;
        entry->x = c->x;
        entry->y = c->y;
        entry->is_sea_trade = c->is_sea;
        entry->is_land_trade = c->is_land;
        entry->nearest_ai_distance = ai_dist;
        entry->nearest_player_distance = 0; /* Will compute after */
        entry->fairness_score = scores[ci];
        selected++;
    }

    /* Compute nearest-player distances */
    for (int i = 0; i < selected; i++) {
        int min_pd = 999999;
        for (int j = 0; j < selected; j++) {
            if (i == j) continue;
            int d = distance(spawns[i].x, spawns[i].y, spawns[j].x, spawns[j].y);
            if (d < min_pd) {
                min_pd = d;
            }
        }
        spawns[i].nearest_player_distance = min_pd;
    }

    return selected;
}

/* ---- Public API ---- */

void mp_worldgen_init(void)
{
    memset(&spawn_table, 0, sizeof(spawn_table));
}

void mp_worldgen_clear(void)
{
    mp_worldgen_init();
}

int mp_worldgen_generate_player_spawns(uint32_t session_seed, int player_count,
                                        int use_xml_slots)
{
    if (player_count < 2 || player_count > MP_MAX_PLAYERS) {
        log_error("Invalid player count for worldgen", 0, player_count);
        return 0;
    }

    memset(&spawn_table, 0, sizeof(spawn_table));
    spawn_table.session_seed = session_seed;
    spawn_table.player_count = (uint8_t)player_count;

    prng_seed(session_seed);

    /* Collect AI city positions for distance checks */
    int ai_xs[200], ai_ys[200];
    int ai_count = collect_ai_trade_cities(ai_xs, ai_ys, 200);

    spawn_candidate candidates[MP_WORLDGEN_MAX_CANDIDATES];
    int candidate_count = 0;

    /* Phase 1: Try XML-defined slots first */
    if (use_xml_slots) {
        spawn_candidate xml_candidates[MP_WORLDGEN_MAX_SPAWNS];
        int xml_count = collect_xml_slot_candidates(xml_candidates, MP_WORLDGEN_MAX_SPAWNS);

        if (xml_count >= player_count) {
            /* Enough XML slots - use them directly */
            int selected = select_spawns_from_candidates(xml_candidates, xml_count,
                spawn_table.spawns, player_count, ai_xs, ai_ys, ai_count);
            if (selected >= player_count) {
                spawn_table.spawn_count = (uint8_t)selected;
                log_info("Worldgen: used XML slots", 0, selected);
                return 1;
            }
        }

        /* Partial XML slots - add to candidate pool */
        for (int i = 0; i < xml_count && candidate_count < MP_WORLDGEN_MAX_CANDIDATES; i++) {
            candidates[candidate_count++] = xml_candidates[i];
        }
    }

    /* Phase 2: Collect procedural candidates */
    int proc_count = collect_procedural_candidates(
        candidates + candidate_count,
        MP_WORLDGEN_MAX_CANDIDATES - candidate_count,
        ai_xs, ai_ys, ai_count);
    candidate_count += proc_count;

    /* Phase 3: Select with constraints */
    int selected = select_spawns_from_candidates(candidates, candidate_count,
        spawn_table.spawns, player_count, ai_xs, ai_ys, ai_count);

    if (selected < player_count) {
        /* Retry with relaxed constraints if strict selection failed */
        log_info("Worldgen: relaxing constraints, only found", 0, selected);

        /* Reset and try without minimum distance constraints */
        memset(spawn_table.spawns, 0, sizeof(spawn_table.spawns));
        int relaxed_selected = 0;

        /* Just take the best-scored candidates in order */
        for (int i = 0; i < candidate_count && relaxed_selected < player_count; i++) {
            spawn_candidate *c = &candidates[i];
            mp_spawn_entry *entry = &spawn_table.spawns[relaxed_selected];
            entry->valid = 1;
            entry->slot_id = (uint8_t)relaxed_selected;
            entry->empire_city_id = c->city_id;
            entry->empire_object_id = c->object_id;
            entry->x = c->x;
            entry->y = c->y;
            entry->is_sea_trade = c->is_sea;
            entry->is_land_trade = c->is_land;
            entry->nearest_ai_distance = c->ai_distance;
            entry->nearest_player_distance = 0;
            entry->fairness_score = 0;
            relaxed_selected++;
        }
        selected = relaxed_selected;
    }

    if (selected < 2) {
        log_error("Worldgen: cannot find enough spawn positions", 0, selected);
        return 0;
    }

    spawn_table.spawn_count = (uint8_t)selected;
    log_info("Worldgen: spawns generated", 0, selected);
    return 1;
}

const mp_spawn_table *mp_worldgen_get_spawn_table(void)
{
    return &spawn_table;
}

mp_spawn_table *mp_worldgen_get_spawn_table_mutable(void)
{
    return &spawn_table;
}

int mp_worldgen_reroll(uint32_t new_seed, int player_count)
{
    if (spawn_table.locked) {
        log_error("Cannot reroll: spawn table is locked", 0, 0);
        return 0;
    }
    return mp_worldgen_generate_player_spawns(new_seed, player_count, 1);
}

void mp_worldgen_lock(void)
{
    spawn_table.locked = 1;
    log_info("Worldgen: spawn table locked", 0, 0);
}

const mp_spawn_entry *mp_worldgen_get_spawn_for_slot(uint8_t slot_id)
{
    for (int i = 0; i < spawn_table.spawn_count; i++) {
        if (spawn_table.spawns[i].valid && spawn_table.spawns[i].slot_id == slot_id) {
            return &spawn_table.spawns[i];
        }
    }
    return 0;
}

int mp_worldgen_apply_spawns(void)
{
    if (!spawn_table.locked) {
        log_error("Cannot apply spawns: table not locked", 0, 0);
        return 0;
    }

    int applied = 0;
    for (int i = 0; i < spawn_table.spawn_count; i++) {
        mp_spawn_entry *entry = &spawn_table.spawns[i];
        if (!entry->valid) {
            continue;
        }

        /* Configure the empire city for this player slot */
        empire_city *city = empire_city_get(entry->empire_city_id);
        if (!city || !city->in_use) {
            log_error("Worldgen: invalid city for slot", 0, entry->slot_id);
            continue;
        }

        /* Mark the city as player-owned in ownership system */
        mp_player *player = mp_player_registry_get_by_slot(entry->slot_id);
        if (player) {
            mp_ownership_set_city(entry->empire_city_id,
                player->is_local ? MP_OWNER_LOCAL_PLAYER : MP_OWNER_REMOTE_PLAYER,
                player->player_id);
            mp_player_registry_set_city(player->player_id, entry->empire_city_id);
            mp_player_registry_set_assigned_city(player->player_id, entry->empire_city_id);

#ifdef ENABLE_MULTIPLAYER
            city->owner_type = player->is_local ? CITY_OWNER_LOCAL : CITY_OWNER_REMOTE;
            city->owner_player_id = player->player_id;
            city->authoritative_city_id = entry->empire_city_id;
#endif
            applied++;
            log_info("Worldgen: applied spawn for slot", player->name, entry->slot_id);
        }
    }

    return applied;
}

/* ---- Serialization ---- */

void mp_worldgen_serialize(uint8_t *buffer, uint32_t *size)
{
    net_serializer s;
    net_serializer_init(&s, buffer, 4096);

    net_write_u32(&s, spawn_table.session_seed);
    net_write_u8(&s, spawn_table.player_count);
    net_write_u8(&s, spawn_table.spawn_count);
    net_write_u32(&s, spawn_table.generation_tick);
    net_write_u8(&s, (uint8_t)spawn_table.locked);

    for (int i = 0; i < spawn_table.spawn_count; i++) {
        mp_spawn_entry *e = &spawn_table.spawns[i];
        net_write_u8(&s, (uint8_t)e->valid);
        net_write_u8(&s, e->slot_id);
        net_write_i32(&s, e->empire_city_id);
        net_write_i32(&s, e->empire_object_id);
        net_write_i32(&s, e->x);
        net_write_i32(&s, e->y);
        net_write_u8(&s, (uint8_t)e->is_sea_trade);
        net_write_u8(&s, (uint8_t)e->is_land_trade);
        net_write_i32(&s, e->nearest_ai_distance);
        net_write_i32(&s, e->nearest_player_distance);
        net_write_i32(&s, e->fairness_score);
    }

    *size = (uint32_t)net_serializer_position(&s);
}

void mp_worldgen_deserialize(const uint8_t *buffer, uint32_t size)
{
    mp_worldgen_clear();

    net_serializer s;
    net_serializer_init(&s, (uint8_t *)buffer, size);

    spawn_table.session_seed = net_read_u32(&s);
    spawn_table.player_count = net_read_u8(&s);
    spawn_table.spawn_count = net_read_u8(&s);
    spawn_table.generation_tick = net_read_u32(&s);
    spawn_table.locked = net_read_u8(&s);

    for (int i = 0; i < spawn_table.spawn_count && !net_serializer_has_overflow(&s); i++) {
        mp_spawn_entry *e = &spawn_table.spawns[i];
        e->valid = net_read_u8(&s);
        e->slot_id = net_read_u8(&s);
        e->empire_city_id = net_read_i32(&s);
        e->empire_object_id = net_read_i32(&s);
        e->x = net_read_i32(&s);
        e->y = net_read_i32(&s);
        e->is_sea_trade = net_read_u8(&s);
        e->is_land_trade = net_read_u8(&s);
        e->nearest_ai_distance = net_read_i32(&s);
        e->nearest_player_distance = net_read_i32(&s);
        e->fairness_score = net_read_i32(&s);
    }
}

#endif /* ENABLE_MULTIPLAYER */

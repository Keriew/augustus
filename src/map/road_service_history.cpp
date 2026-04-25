#include "map/road_service_history.h"

#include "core/crash_context.h"

extern "C" {
#include "game/time.h"
#include "map/grid.h"
}

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

constexpr uint32_t kSaveFormatVersion = 1;
constexpr size_t kSaveHeaderSize = 2 * sizeof(uint32_t);
constexpr size_t kEffectGridCells = GRID_SIZE * GRID_SIZE;
constexpr uint32_t kLastPreReligionEffectCount = ROAD_SERVICE_EFFECT_RELIGION_CERES;
std::array<grid_u32, ROAD_SERVICE_EFFECT_MAX> g_history;

bool is_valid_effect(road_service_effect effect)
{
    return effect > ROAD_SERVICE_EFFECT_NONE && effect < ROAD_SERVICE_EFFECT_MAX;
}

uint32_t current_visit_stamp()
{
    // Reserve zero for "never visited" so old saves and new roads sort as stale.
    const int ticks_per_day = game_time_ticks_per_day();
    const int day_ticks = ticks_per_day > 0 ? ticks_per_day : 1;
    const int current_month_ticks = game_time_ticks_per_month(game_time_month());
    const int fallback_month_ticks = game_time_days_in_current_month() * day_ticks;
    const int month_ticks = current_month_ticks > 0 ?
        current_month_ticks :
        (fallback_month_ticks > 0 ? fallback_month_ticks : day_ticks);
    const int total_months = game_time_total_months();
    const int day = game_time_day();
    const int tick = game_time_tick();
    const uint64_t total_ticks =
        static_cast<uint64_t>(total_months > 0 ? total_months : 0) * static_cast<uint64_t>(month_ticks) +
        static_cast<uint64_t>(day > 0 ? day : 0) * static_cast<uint64_t>(day_ticks) +
        static_cast<uint64_t>(tick > 0 ? tick : 0);
    const uint64_t visit_stamp = total_ticks + 1;
    return visit_stamp > std::numeric_limits<uint32_t>::max() ?
        std::numeric_limits<uint32_t>::max() :
        static_cast<uint32_t>(visit_stamp);
}

} // namespace

extern "C" void map_road_service_history_clear(void)
{
    for (int effect = 0; effect < ROAD_SERVICE_EFFECT_MAX; effect++) {
        map_grid_clear_u32(g_history[effect].items);
    }
}

extern "C" uint32_t map_road_service_history_get(road_service_effect effect, int grid_offset)
{
    if (!is_valid_effect(effect) || !map_grid_is_valid_offset(grid_offset)) {
        return 0;
    }
    return g_history[effect].items[grid_offset];
}

extern "C" void map_road_service_history_record(road_service_effect effect, int grid_offset)
{
    if (!is_valid_effect(effect) || !map_grid_is_valid_offset(grid_offset)) {
        return;
    }
    g_history[effect].items[grid_offset] = current_visit_stamp();
}

extern "C" void map_road_service_history_save_state(buffer *buf)
{
    // The payload is ordinal by road_service_effect. Append new effects only;
    // keep removed meanings as reserved ids so old saves do not shift columns.
    const size_t payload_size =
        kSaveHeaderSize +
        (static_cast<size_t>(ROAD_SERVICE_EFFECT_MAX) - 1) * kEffectGridCells * sizeof(uint32_t);
    buffer_init_dynamic(buf, payload_size);
    buffer_write_u32(buf, kSaveFormatVersion);
    buffer_write_u32(buf, static_cast<uint32_t>(ROAD_SERVICE_EFFECT_MAX));
    for (int effect = 1; effect < ROAD_SERVICE_EFFECT_MAX; effect++) {
        map_grid_save_state_u32(g_history[effect].items, buf);
    }
}

extern "C" void map_road_service_history_load_state(buffer *buf, int has_saved_state, int has_religion_effects)
{
    map_road_service_history_clear();

    if (!has_saved_state || !buf || !buf->data || !buf->size) {
        const ErrorContextScope scope("Savegame road service history");
        error_context_report_info(
            "Savegame has no road service history; smart walker pathing history will start at zero.",
            "This is expected when loading saves created before road service history was added.");
        return;
    }

    const size_t payload_size = buffer_load_dynamic(buf);
    if (payload_size < kSaveHeaderSize) {
        const ErrorContextScope scope("Savegame road service history");
        error_context_report_warning("Invalid road service history; resetting it to zero.", 0);
        return;
    }

    const uint32_t format_version = buffer_read_u32(buf);
    const uint32_t effect_count = buffer_read_u32(buf);
    if (format_version != kSaveFormatVersion ||
        effect_count <= static_cast<uint32_t>(ROAD_SERVICE_EFFECT_NONE)) {
        const ErrorContextScope scope("Savegame road service history");
        error_context_report_warning("Unsupported road service history; resetting it to zero.", 0);
        return;
    }

    const uint32_t max_effect_count = has_religion_effects ?
        static_cast<uint32_t>(ROAD_SERVICE_EFFECT_MAX) :
        kLastPreReligionEffectCount;
    const int effects_to_read = static_cast<int>(std::min(effect_count, max_effect_count));
    for (int effect = 1; effect < effects_to_read; effect++) {
        map_grid_load_state_u32(g_history[effect].items, buf);
    }

    // Saves before religion-specific road history only contain effects [1, 9).
    // The appended religion grids remain zeroed by the initial clear. This also
    // consumes any future effect grids so the fixed ordinal payload stays aligned.
    for (uint32_t effect = effects_to_read; effect < effect_count; effect++) {
        for (size_t i = 0; i < kEffectGridCells; i++) {
            buffer_read_u32(buf);
        }
    }
}

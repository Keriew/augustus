#include "figuretype/maintenance.h"

#include "core/crash_context.h"

extern "C" {
#include "building/building.h"
}

namespace {

constexpr short kWorkerMaxRoamLength = 384;

void retire_native_fallback(figure *f, const char *figure_name)
{
    const int token = crash_context_push_scope("Retired native figure fallback", figure_name, nullptr, nullptr);
    error_context_report_warning(
        "A native FigureType walker reached its retired legacy action callback.",
        "Engineer and Prefect behavior is owned by figure_runtime; this fallback now removes the invalid figure.");
    crash_context_pop_scope(token);

    if (f) {
        f->state = FIGURE_STATE_DEAD;
    }
}

} // namespace

extern "C" void figure_engineer_action(figure *f)
{
    retire_native_fallback(f, "engineer");
}

extern "C" void figure_prefect_action(figure *f)
{
    retire_native_fallback(f, "prefect");
}

extern "C" void figure_worker_action(figure *f)
{
    if (!f) {
        return;
    }

    f->terrain_usage = TERRAIN_USAGE_ROADS;
    f->use_cross_country = 0;
    f->max_roam_length = kWorkerMaxRoamLength;

    building *owner = building_get(f->building_id);
    if (!owner || owner->state != BUILDING_STATE_IN_USE || owner->figure_id != f->id) {
        f->state = FIGURE_STATE_DEAD;
    }
}

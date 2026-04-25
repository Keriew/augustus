# Walker Pathing Runtime

This note maps the native walker pathing work so future sessions can find the
runtime, XML, save data, and compatibility rules without rediscovering them.

## Entry Points

- `Mods/Vespasian/FigureType/_README.md` documents the XML contract.
- `src/figure/figure_type_registry.cpp` loads and validates native FigureType XML.
- `src/figure/figure_runtime.cpp` owns native controllers and smart direction choice.
- `src/figure/movement.cpp` keeps the legacy roaming movement loop and calls the native pathing hook.
- `src/figuretype/maintenance.cpp` keeps only Worker behavior plus retired Engineer/Prefect action-table guards.
- `src/map/road_service_history.h/.cpp` owns per-road, per-effect visit stamps.
- `src/game/file_io.cpp` saves and loads road service history.
- `src/game/save_version.h` records the save-version boundary for road service history.

## XML Contract

Native service walkers use:

- `<movement terrain_usage="roads" roam_ticks="N" max_roam_length="N" return_mode="..." />`
- `<pathing mode="vanilla_roaming" />`
- `<pathing mode="smart_service" effect="damage_risk|fire_risk|barber|bathhouse|school|academy|library|labor" />`

`smart_service` is only valid for road-only walkers with a non-none effect. Loader
validation should fail early through crash-context reporting if either condition
is missing.

## Runtime Contract

The legacy roaming loop remains the source of tile movement. Native runtime code
only gets a chance to override the chosen direction after vanilla logic has found
a valid forward direction.

`smart_service` applies only when there is more than one valid outgoing road. It
chooses the candidate road tile with the lowest visit stamp for that effect.
Equal stamps fall back to the vanilla-preferred direction so old behavior stays
stable where history gives no preference.

Prefect emergency behavior is separate from smart roaming. Fire and enemy
response states keep using targeted movement and should not be redirected by
road recency.

Engineer and Prefect legacy switch bodies were retired after the native
controllers covered their normal, emergency, attack, and corpse states. The
action table still has guard callbacks, but those callbacks should not be part
of normal gameplay.

## Road Service History

Road service history is pathing telemetry only. It does not provide service,
reset building risk, affect coverage overlays, or change building state.

Each effect has a full road grid of `uint32_t` visit stamps. Zero means "never
visited" and is also the default for old saves and newly placed roads. The stamp
uses game time plus one, preserving zero as the stale sentinel.

Current effect ids are stored in `road_service_effect`. Treat those numeric ids
as a save compatibility contract:

- Add new effects only at the end.
- Do not reorder existing ids.
- Do not reuse removed ids for a different meaning.
- Keep removed meanings as reserved/deprecated slots.
- If a service meaning changes enough that old recency would mislead pathing,
  either migrate that id explicitly or clear just that effect on load.

The current save format writes grids in ordinal enum order. If frequent effect
schema changes become likely, move the payload to explicit `(effect_id, grid)`
records before doing removals or reordering.

Loading a save from before road service history existed is expected compatibility
behavior. It should clear the history and emit a concise `Info` report with no
context scope. Invalid or unsupported road service history is recoverable but
unintended, so it remains a `Warning` while still resetting the history to zero
and appending neutral context in the same log entry.

## Related Context

Start new sessions with the four core Codex files, then read this file for walker
runtime work and `Mods/Vespasian/FigureType/_README.md` for XML details.
Renderer or overlay work that visualizes recency should also read
`../codex_augustus_repo_map_memory.md` for renderer/widget chokepoints before
touching `src/widget/city_with_overlay.cpp`.

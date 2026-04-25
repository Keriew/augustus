# Codex Augustus handoff memory

Snapshot: 2026-04-06
Workspace: C:\Users\imper\Documents\GitHub\augustus_cpp_huge

## Recommended next-chat posture
- Yes: start a new chat for the next feature.
- Reason: the current arc now spans VS/MSBuild repair, mixed C/C++ migration, asset-pack precedence, load-failure doctrine, renderer backend refactor, and the first shared UI runtime rollout.
- Session bootstrap:
  - read the four core Codex memory files first
  - then read task-specific docs such as `docs/walker_pathing_runtime.md`, `Mods/Vespasian/FigureType/_README.md`, or the renderer/widget sections in `codex_augustus_repo_map_memory.md`
  - when adding a new system doc, link it from the most relevant core memory file and from nearby subsystem READMEs so it is findable without crowding this file
- The next chat should begin from the current renderer baseline:
  - request-based 2D backend active
  - render domains active
- shared UI object chain active for common widget primitives
  - graphics asset fallback chain active

## Current stable architecture
- Root build entrypoint remains:
- `Vespasian.sln`
- `Vespasian.vcxproj`
- Do not build/run unless the user explicitly asks in that chat.
- Keep CRLF on touched files.
- Preserve existing comments when editing files.
- When code behavior, XML contracts, save/load layout, or runtime classes change, update the relevant markdown and add concise function comments for non-obvious touched logic unless the user says not to.

## Recently added gameplay runtime context
- Native walker definitions now use FigureType XML for the currently ported service walkers.
- Main implementation notes live in:
  - `docs/walker_pathing_runtime.md`
  - `Mods/Vespasian/FigureType/_README.md`
- Code chokepoints:
  - `src/figure/figure_type_registry.cpp`
  - `src/figure/figure_runtime.cpp`
  - `src/figure/movement.cpp`
  - `src/map/road_service_history.cpp`
- `map_road_service_history` is pathing telemetry only. It is saved separately and should not be treated as building coverage or risk state.

## Current graphics checkpoint
- Canonical graphics extraction is now load-bearing for the building runtime:
  - `src/core/legacy_image_extractor.cpp`
  - `src/assets/augustus_asset_extractor.cpp`
- The recent native building graphics glitch was fixed at extraction time, not by changing final draw math:
  - Julius footprint exports now trim bottom transparent padding and preserve logical placement through XML metadata
  - Augustus active building variants now inherit local `group="this"` footprint/top parts instead of collapsing to footprint-only
- Native building footprint rendering still routes through `src/widget/city_draw.cpp`, and native-owned buildings only submit their whole-building footprint on the owning draw tile.
- The temporary runtime footprint crop/offset compensation was removed from `src/assets/image_group_payload_materialize.cpp`; extractor output is now the sole source of truth for the corrected placement.

## Renderer / display status
- The renderer now separates:
  - real window pixels
  - logical UI size
  - user UI scale percentage
  - world/city pixel scale
- `src/platform/render_2d_pipeline.cpp`
  - owns request-based 2D scaling/filter policy
  - maps UI/pixel/tooltip/snapshot render domains
- `src/platform/renderer.cpp`
  - consumes explicit `render_2d_request`
  - preserves/restores render target, viewport, clip, blend, and scale across tooltip/snapshot paths
- `src/graphics/window.cpp`
  - is the pass orchestrator for full-window drawing
  - is not the widget-composition chokepoint
  - keeps reasserting UI render scale around window/tooltip/warning flow

## Shared UI runtime status
- Shared UI now has a layered object model:
  - `src/graphics/ui_primitives.h/.cpp`
  - `src/graphics/ui_widget.h`
  - `src/graphics/button_widget.h`
  - `src/graphics/bordered_button_widget.h/.cpp`
  - `src/graphics/image_button_widget.h/.cpp`
  - `src/graphics/panel_widget.h/.cpp`
  - `src/graphics/label_widget.h/.cpp`
  - `src/graphics/top_menu_panel_widget.h/.cpp`
  - `src/graphics/scrollbar_widget.h/.cpp`
  - `src/graphics/ui_runtime.h/.cpp`
  - `src/graphics/ui_runtime_api.h`
- Current flow is:
  - primitives own low-level request-based drawing
  - widget classes own widget behavior/data
  - `SharedUiRuntime` orchestrates widget objects and keeps the C facade narrow
- New widget/runtime diagnostics should go through `src/core/crash_context.h`, using:
  - `ErrorContextScope` as the preferred C++ scope name
  - `error_context_report_warning/error_context_report_error/error_context_report_fatal_error_dialog` for reports
- `CrashContextScope` and direct `log.h` calls remain compatibility paths for older code, not the preferred pattern for new UI/runtime work
- Legacy shared widget files now call the facade instead of drawing directly:
  - `src/graphics/button.c`
  - `src/graphics/panel.c`
  - `src/graphics/image_button.c`
  - `src/graphics/scrollbar.c`
  - `src/graphics/graphics.cpp`
- Important boundary decision:
  - `window.cpp` remains responsible for draw-pass flow
  - reusable widget composition lives in the widget/primitives chain, not in per-window rewrites

## Graphics pack / asset loading status
- Active graphics precedence is now:
  - `Mods/<selected mod>/Graphics`
  - root Augustus assets folder (`assets/Graphics`)
  - Caesar 3/original atlas-backed content
- Canonical path-keyed loaders are now the preferred direction for authored graphics, including UI groups that have extracted keys such as `UI\Top_Menu`; flat `assets_get_image_id("UI", ...)` lookups are compatibility fallback only
- The startup path no longer hard-fails before that fallback chain is attempted.
- Missing optional overrides should warn/fallback.
- Critical failures should stop startup with a retained reason only after the fallback chain is actually attempted.

## Critical startup failure doctrine now wired
- `src/assets/assets.cpp`
  - retains startup failure reasons for critical asset load failures
- `src/core/image.c`
  - propagates asset-init failure upward
- `src/game/game.c`
  - retains a user-facing init failure message
- `src/platform/augustus.cpp`
  - shows the retained startup error if startup fails
- Good examples of intended critical failures:
  - selected-language font load failure
  - malformed or invalid XML/defines input
  - structurally incomplete runtime definitions that would predictably explode later

## Config / filter status
- Config persistence now lives in `Vespasian.ini`, with legacy fallback reads from `augustus.ini`.
- `CONFIG_UI_SCALE_FILTER` exists and currently supports:
  - auto
  - nearest
  - linear
- The setting is already respected by the request-based 2D pipeline and related SDL texture/filter handling.

## Recent migration landmarks that matter
- Renderer/platform:
  - `src/platform/augustus.cpp`
  - `src/platform/screen.cpp`
  - `src/platform/renderer.cpp`
  - `src/platform/render_2d_pipeline.cpp`
- Shared UI runtime:
  - `src/graphics/ui_runtime.cpp`
- Other recent `.cpp` migrations relevant to current architecture:
  - `src/graphics/image.cpp`
  - `src/graphics/text.cpp`
  - `src/graphics/font.cpp`
  - `src/graphics/tooltip.cpp`
  - `src/graphics/window.cpp`
  - `src/core/config.cpp`
  - `src/game/speed.cpp`

## UTF / text direction
- Do not broad-brush migrate the engine to UTF-native storage yet.
- Chosen direction is boundary-first:
  - new C++ runtime/parsing/text-adjacent code may prefer `std::string` / `std::string_view`
  - legacy C-facing storage remains on existing `uint8_t *` paths until a dedicated text migration phase
- New widget/runtime code should not assume `1 byte == 1 glyph`.

## Best next-phase target after this checkpoint
- Continue the shared UI runtime rollout without widening into unrelated windows.
- Good next candidates:
  - shared menu/list/grid framing helpers that still compose through older leaf calls
  - richer button/panel composition policies
  - better explicit logical metrics for top-menu and other shared UI strips
- Leave world/zoom/quad-map work for a later renderer phase.

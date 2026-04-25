# Codex Augustus VS2022 / MSBuild migration working memory

Snapshot: 2026-04-06
Workspace: C:\Users\imper\Documents\GitHub\augustus_cpp_huge

## What is true in this checkout now
- The repo already has the native Visual Studio solution/project at the repository root:
  - `Vespasian.sln`
  - `Vespasian.vcxproj`
  - `Vespasian.vcxproj.filters`
- MSBuild/VS2022 at repo root is the real Windows workflow.
- `vendor/` contains local SDL2 and SDL2_mixer drops and stays ignored.
- Generated/local build state is already excluded from Git.

## Migration/bootstrap status
- The earlier linker/bootstrap wall is resolved.
- The project has survived several mixed C/C++ migration waves without changing the save format.
- The repo is in incremental-migration mode now, not bootstrap-rescue mode.

## Proven migration pattern
- Import legacy C headers inside `extern "C"` blocks from `.cpp` files.
- Keep public headers C-callable where old C code still depends on them.
- Prefer introducing one C++ runtime object plus one narrow C facade rather than converting an entire subsystem in one jump.
- Add short contract comments around new C/C++ facade functions and converted control points, especially where save compatibility or legacy callback ownership is involved.
- Update the relevant markdown in the same run when the migration changes runtime ownership, XML contracts, save/load pieces, or newly introduced classes.
- Be explicit about all touched conversions:
  - enum/int
  - float/int
  - 64-bit/time/int
  - legacy struct fields that still store enum-like values as `int`

## Current reference examples
### Platform / renderer
- `src/platform/augustus.cpp`
- `src/platform/screen.cpp`
- `src/platform/render_2d_pipeline.cpp`
- `src/platform/renderer.cpp`

### Gameplay/runtime
- `src/building/tool_mode.cpp`
- `src/building/building_runtime.cpp`
- `src/figure/figure_type_registry.cpp`
- `src/figure/figure_runtime.cpp`
- `src/figure/movement.cpp`
- `src/figuretype/maintenance.cpp`
- `src/map/road_service_history.cpp`

### Graphics/runtime
- `src/graphics/image.cpp`
- `src/graphics/font.cpp`
- `src/graphics/text.cpp`
- `src/graphics/tooltip.cpp`
- `src/graphics/window.cpp`
- `src/graphics/ui_runtime.cpp`
- `src/core/legacy_image_extractor.cpp`
- `src/assets/augustus_asset_extractor.cpp`
- `src/assets/image_group_payload_materialize.cpp`
- `src/widget/city_draw.cpp`

### Other recent conversions that matter
- `src/core/config.cpp`
- `src/game/speed.cpp`

## Recent migration lesson that now matters most
- The renderer work and the shared UI widget work both reinforced the same rule:
  - do not spread behavior across many legacy files if one class-based chokepoint can own it
- Current examples:
  - `Render2DPipeline`
    - owns backend 2D scaling/filter/domain policy
  - `UiPrimitives` + `UiWidget` hierarchy + `SharedUiRuntime`
    - primitives own low-level request drawing
    - widgets own widget behavior
    - runtime only orchestrates and exposes the facade
  - `building_runtime`
    - owns the building-side runtime migration direction

## Headers/linkage lessons worth preserving
- When a C file is converted or begins calling C++ code, check header linkage immediately.
- Unresolved externals after a `.cpp` conversion are usually a missing `extern "C"` guard, not a "real" missing implementation.
- This repo is particularly sensitive to:
  - graphics/image/asset boundaries
  - input headers
  - file/log/dir utility headers
  - locale/time helpers

## Enum/cast discipline rule
- Treat implicit enum/int mixing as a bug waiting to become a C++ compile error.
- Cast explicitly at the call site whenever:
  - a legacy struct stores an enum-like value in an `int`
  - a parser returns an integer enum id
  - a loop/index is crossing typed-enum boundaries

## Current architecture-specific checkpoint
- Request-based 2D backend is active.
- Render domains are active.
- Shared UI primitives/widget/runtime chain is active for common widget primitives.
- Graphics-pack precedence and retained critical startup failures are now part of the architecture, not temporary patches.
- Native FigureType XML and `figure_runtime` are active for the currently ported service walkers.
- Road service history is a save-backed, pathing-only telemetry grid used by smart service walkers.
- `window.cpp` is the full-window/pass orchestrator.
- `ui_runtime.cpp` is the shared widget facade/orchestration chokepoint.

## Practical migration guidance from this repo
- Convert only where the touched area benefits.
- Do not force large designated-initializer-heavy data files into MSVC C++ if keeping them as C is cleaner.
- If a graphics bug is really an extracted-data bug, fix the extractor/output contract first and treat runtime offsets as temporary compatibility shims.
- If a file grows toward giant-runtime-object territory, split:
  - runtime behavior
  - API facade
  - parser/load logic
  - data definitions
- Keep CRLF on touched files.
- Preserve existing comments when editing files.
- Do not build unless the user explicitly asks in that chat.

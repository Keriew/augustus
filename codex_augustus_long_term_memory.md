# Codex Augustus long-term working memory

Snapshot: 2026-04-06

## Project identity
This branch is still best understood as a simulation-rhythm fork of Augustus.

Primary design goals remain:
- make months feel longer
- make the city look busier with more visible walkers
- rebalance production and building costs to fit the slower cadence
- adjust demographics coherently, not cosmetically
- improve the rendering pipeline so higher-resolution UI/fonts/assets can exist without layout collapsing into atlas-native sizing assumptions

## Long-term engineering doctrine
- Keep Windows/MSBuild at repo root as the practical dev workflow.
- Prefer incremental C-to-C++ migration through narrow, class-based chokepoints.
- Keep save compatibility by growing runtime wrappers around legacy serialized structs instead of replacing serialized truth all at once.
- Do not broaden a rewrite just because a subsystem is old; center the work on the best control point.
- Comment functions consistently when touching code. Prefer concise contract comments that explain ownership, invariants, save/load behavior, validation, or surprising legacy interactions; avoid comments that merely restate assignments.
- Update the relevant markdown whenever behavior, XML contracts, save formats, new runtime classes, or major chokepoints change, unless the user explicitly says not to. Add cross-references so future sessions can find the information from the four core Codex files without crowding those files with every detail.

## Renderer doctrine
- Native window pixels, logical UI size, user UI scale, and world zoom are separate concepts and should stay separate.
- All screen-space drawing should trend toward explicit logical destination geometry rather than "native asset size == layout size".
- Backend renderer policy should stay concentrated in `Render2DPipeline`.
- Shared widget primitives should stay concentrated in `UiPrimitives`.
- Shared widget behavior should live on widget classes, with `SharedUiRuntime` acting as the facade/orchestrator rather than as a logic monolith.
- `window.cpp` should remain a draw-pass orchestrator unless there is a deliberate decision to move more composition into it.

## Asset / startup failure doctrine
- Graphics/content precedence is:
  - selected mod graphics
  - root Augustus assets
  - Caesar 3/original atlas-backed content
- When a graphics bug is caused by biased canonical extracted data, prefer fixing the extractor and exported XML/PNG semantics before adding more runtime offset compensation.
- Preserve authored XML animation `x/y` offsets exactly unless there is direct proof they are wrong; they are part of the content contract for Augustus overlay animations.
- Missing optional overrides should usually warn/fallback.
- Broken critical assets or invalid definitions should fail at load, not halfway through gameplay.
- Startup errors should retain a precise user-facing reason whenever practical.

## Runtime reporting doctrine
- Use `Info` for expected compatibility or diagnostic notes, `Warning` for probably unintended but safe recovery, `Error` for definitely unintended survivable failures, and `Fatal error` only when the process must stop.
- Reserve the word "Error" in user-facing logs for actual error/fatal reports. Context scopes are neutral location breadcrumbs and should be labeled "Context", not "Error context", when printed.
- `Info` reports should omit context scopes. Warning/error/fatal reports should log the message, detail, and active context scopes as one log entry. Avoid emitting a report line followed by separate scope-count/scope-name lines.

## Save file doctrine
- Vespasian-owned save files use `.svv`. This replaced the temporary `.savf` extension so the fork has a standard three-letter extension while remaining distinct from legacy Caesar/Julius/Augustus `.sav` and expanded `.svx` saves.
- Existing `.savf` saves can be renamed to `.svv`; the on-disk payload format did not change with the extension rename.

## Text / UTF doctrine
- Do not attempt a blanket UTF-native storage migration during unrelated renderer or widget work.
- Use boundary-first modernization instead:
  - `std::string` / `std::string_view` in new C++ runtime/parsing boundaries where practical
  - keep legacy internal `uint8_t *` storage until a dedicated text migration phase
- Future UTF/styled-text work should be modeled around:
  - glyph availability
  - fallback behavior
  - codepoint/grapheme-aware measurement
  - text-run/style metadata such as bold/italic, not widget-side hacks

## Load-bearing chokepoints now established
- `Render2DPipeline`
  - request-based 2D backend policy
- `UiPrimitives`
  - low-level shared UI drawing primitives
- widget hierarchy (`UiWidget`, button/panel/scrollbar widgets)
  - shared UI object behavior/composition
- `SharedUiRuntime`
  - shared UI facade/orchestration
- `building_runtime`
  - building runtime migration direction
- `figure_type_registry` / `figure_runtime`
  - native FigureType XML loading and walker runtime migration direction
- `map_road_service_history`
  - pathing-only road recency storage for smart service walkers

## Practical priorities
1. Continue the shared UI runtime rollout where it improves control of common widgets without forcing a whole-window rewrite.
2. Keep load-time validation/failure reporting growing alongside new XML/runtime data systems.
3. Resume gameplay/system tuning in attributable phases once the targeted runtime/control-point work is stable.
4. Treat future world-renderer/zoom/quad-map work as a later phase built on the current backend, not something to mix into every UI pass.

## Guardrails
- Do not build unless the user explicitly asks in the current chat.
- Keep CRLF consistent on touched files.
- Preserve existing comments when editing files.
- Add or refresh comments for touched functions when the behavior is non-obvious.
- Refresh relevant markdown in the same run when changing XML, save/load, runtime classes, or system contracts.
- Do not spread implicit enum/int conversions through migrated code.
- Do not deepen dependence on byte-oriented text assumptions in new runtime/widget code.
- Do not collapse renderer policy back into many ad hoc draw helpers once the chokepoints exist.

## Short mnemonic
Build is stable; renderer backend exists; shared UI runtime now exists; asset fallback and retained startup failures are part of the architecture; keep moving through explicit chokepoints, not broad rewrites.

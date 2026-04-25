# Augustus Manual Sync Ledger (`2026-04-21`)

This ledger tracks the current hand-port batch from upstream `Augustus` into the Vespasian fork. It is organized by upstream commit and disposition so later sessions can continue the sync without losing the fork-aware rationale.

## Audit Refresh (`2026-04-23`)

- Refreshed `upstream/master` on 2026-04-23. Augustus is now 50 commits ahead of Vespasian `master`, not 49.
- New upstream commit after the original ledger window: `9d550e322` (`Hopefully fix #1741`), touching `src/editor/tool.c`.
- Local branch `bringing-new-commits` now includes a major reservoir/water overlay rework after the original ledger entry was written:
  - `src/building/water_access_runtime.cpp`
  - `src/building/water_access_runtime.h`
  - `src/map/water_supply.cpp`
  - `src/widget/city_water_ghost.cpp`
  - `src/graphics/runtime_overlay_images.cpp`
  - `Mods/*/BuildingType/{reservoir,fountain,well}.xml`
- That rework supersedes the older "partial water overlay parity" notes below. Treat the water-related commit dispositions in this ledger as a revision target, not as final parity proof, until the new runtime is tested in-engine.
- Follow-up audit on 2026-04-23 found no observed regressions after porting `9d550e322` and tightening water overlay/access runtime parity. The branch now uses the generated native runtime overlay image for live water coverage and construction preview tinting.
- Follow-up audit on 2026-04-24 found GitHub's 64-behind count is graph-correct: upstream `2de6361d8` pulls 50 side-parent commits into `Augustus`. For manual Vespasian sync purposes, the remaining queue after the prior 50-commit hand merge is the 14 upstream first-parent commits listed below.

## Applied In This Working Tree

| Upstream commit | Disposition | Status | Notes |
| --- | --- | --- | --- |
| `1d295aa` | `hand-port directly` | applied | Cyrillic city name import/export fixes. |
| `7b648e034` | `hand-port directly` | applied | City ambient sound additions. |
| `d121dcc2` | `hand-port directly` | applied | Button focus alignment fixes in current window seams. |
| `a0070a801` | `hand-port directly` | applied | Hotkey config now resolves pref-dir load/save correctly. |
| `db4258306` | `hand-port directly` | applied | Empire object safety fix. |
| `b17c979e7` | `hand-port directly` | applied | Empire object size fix. |
| `7e3fb0438` | `hand-port directly` | applied | Native attack loop fix adapted without shared-building assumptions. |
| `194495651` | `hand-port directly` | applied | First dropdown fix. |
| `298add75c` | `hand-port directly` | applied | Second dropdown fix. |
| `51420f2f2` | `adapt to fork seam` | applied | Risks overlay/main-building seam fixed through current `building_main` usage. |
| `bc9ae3ec3` | `adapt to fork seam` | applied | Tax-income overlay tooltip now ignores non-house tiles. |
| `04e63e972` | `adapt to fork seam` | applied | Efficiency, desirability, and sentiment tooltips now include numeric values. |
| `93db4e14d` | `adapt to fork seam` | applied | Desirability overlay color thresholds updated. |
| `9af45f9ff` | `adapt to fork seam` | applied | Depot shows on problems overlay when no instructions are set. |
| `ae6c183fe` | `adapt to fork seam` | applied | Depot carts reroute and return cargo instead of dumping on instruction changes; review follow-up fixes now cover cleared-order and recall edge cases. |
| `7df628402` | `adapt to fork seam` | applied | Granary/warehouse walker-default config added to the current config UI and building defaults. |
| `1bedb8599` | `hand-port directly` | applied | Emptying granary/warehouse request dispatch handling fixed. |
| `65ed3f7d1` | `hand-port directly` | applied | Wrong rubble names fixed through the current localization path. |
| `f6141c8b9` | `hand-port directly` | applied | Worst-desirability naming fix applied. |
| `4fe4938e9` | `localization/data-only` | applied | French payload now includes the sync-added depot, warning, tooltip, and storage-walker config keys in `Mods/Augustus/Localization/*.json`. |
| `105c02e70` | `adapt to fork seam` | applied | Reservoir-over-aqueduct handling updated in the current construction and pathing chokepoints. |
| `5a9a8c6f6` | `adapt to fork seam` | applied | Roads/highways under aqueducts enabled in current routing and ghost seams. |

## Applied As Supporting Parity Work

| Upstream commit line | Status | Notes |
| --- | --- | --- |
| `c504c8e92` / `32a2ffbfe` | audited/applied through runtime | Water-range and ghost behavior route through the BuildingType-driven water access runtime and generated native runtime overlay image. No observed regressions in the 2026-04-23 audit. |
| `29c84bf53` | audited/applied through runtime | Fountain and well range previews route through `city_water_ghost_draw_preview()` and `water_access_runtime_begin_preview()` with provider-specific tinting. No observed regressions in the 2026-04-23 audit. |
| `6666a6139` / `773b5295a` / `ef6b172c3` / `23a7f370c` | audited/applied through runtime | Config text, preview coverage, range tint/transparency, and road-reservoir overlay behavior are covered by `water_access_runtime` and the generated overlay image. No observed regressions in the 2026-04-23 audit. |

## Still Deferred Or Not Imported

| Upstream commit | Disposition | Notes |
| --- | --- | --- |
| `b51b7e33c` | `no further action after runtime audit` | Water overlay tile-shape parity is covered by the generated native runtime overlay image path; no observed regressions in the 2026-04-23 audit. |
| `dd5241229` | `no further action after runtime audit` | The broad overlay refactor remains intentionally unreplayed; no missing water/access behavior was observed after the current `water_access_runtime` audit. |
| `165b7c2a3` | `revision needed` | Native overlay visual parity still needs in-engine audit after the new water/access runtime. |
| `8f5a5f736` | `no further action after runtime audit` | Upstream water construction custom overlay intent maps to `city_water_ghost_draw_preview()` using provider-specific preview tinting; no observed regressions in the 2026-04-23 audit. |
| `848b9d52a` | `no further action after runtime audit` | Reservoir side-connection rules, road/highway under aqueducts, dry aqueduct images, and BuildingType water access nodes were re-checked against the current runtime; no observed regressions. |
| `8251ca91f` | `adapt to fork seam` | Hippodrome visual-only follow-up still needs manual overlay review. |
| `c122ab18f` | `adapt to fork seam` | Broader inconsistent building-name audit remains open. |
| `ddfcde631` | `asset-only` | Wild boar asset payload not imported yet. |
| `5b2a592d3` | `defer to later stage` | Shared buildings remains out of scope for this sync batch. |
| `6c4c82c30` | `defer to later stage` | City drawing refactor deferred. |
| `e69bfb98f` | `defer to later stage` | Overlay refactor completion deferred. |
| `83b3c57e7` | `defer to later stage` | Overlay refactor follow-up deferred. |

## Not Ported Or No Local Action

| Upstream commit line | Disposition | Notes |
| --- | --- | --- |
| `13bd31bd5` / `67a31e13b` / `de30858d9` / `05b216998` / `e5bac7466` / `31bf244d1` | `not ported` | Emscripten/CMake/GitHub Actions maintenance. Vespasian's current local workflow is the root Visual Studio/MSBuild project, so these should not be replayed unless the web/CMake workflow is deliberately revived. |
| `dd2bc36ea` | `not ported` | Upstream warning cleanup is not directly applicable to the fork's current rubble-window code path; current code still uses the draw cursor for burning-ruin text. |
| `d37698495` | `no-op/local equivalent` | Whitespace-only empire object cleanup. The branch already rewrote this area while applying the empire object size/safety fixes. |

## New Upstream Commits After Original 49

| Upstream commit | Disposition | Status | Notes |
| --- | --- | --- | --- |
| `9d550e322` | `adapt to fork seam` | applied | Editor terrain placement fix for issue `#1741` ported through the current `src/editor/tool.c` seam: terrain painting now preserves multi-tile XY metadata on elevation/access-ramp terrain. |

## Remaining Upstream First-Parent Commits After 2026-04-23 Sync

GitHub currently reports Vespasian as 64 commits behind `Augustus` because upstream commit `2de6361d8` merges a side branch containing 50 reachable commits. The operational manual-sync queue is the 14 first-parent commits after `9d550e322`.

| Upstream commit | Disposition | Status | Notes |
| --- | --- | --- | --- |
| `2de6361d8` Merge remote-tracking branch `upstream/master` into `augustus-master` | selective audit | partially ported/adapted | Ported fork-relevant runtime fixes: hotkey migration, animation offset fixes, multiline text null guard, mouse window-focus scrolling, and stale storage enum removal. Android SDK/theme/insets, Flatpak metadata, and legacy Czech C-table edits were not ported; this fork uses JSON localization and root MSBuild as the active workflow. |
| `7ed31fd6d` Fix flatpak build | build/CI | intentionally not ported | Flatpak dependency maintenance remains out of scope unless the fork revives upstream Flatpak packaging. |
| `6131809cd` Fix flatpak build again | build/CI | intentionally not ported | Same Flatpak packaging-only rationale as `7ed31fd6d`. |
| `07c9631ef` Yet another attempt at fixing flatpak | build/CI | intentionally not ported | Flatpak install script and workflow changes remain deferred with the inactive packaging workflow. |
| `fe9637540` Fix road/highway under aqueduct images not updating when cancelling build/using undo | runtime fix | ported | `src/game/undo.cpp` now restores map images on empty tiles and aqueduct terrain so roads/highways under aqueducts redraw after cancel/undo. |
| `49301fc45` Update github actions dependencies | CI | intentionally not ported | GitHub Actions dependency refresh is not part of the active root Visual Studio/MSBuild workflow. |
| `93978ac12` Update codeql action dependencies | CI | intentionally not ported | CodeQL dependency refresh remains deferred with CI maintenance. |
| `cbd181ae1` feat(editor): preview terrain (#1692) | editor feature | ported | `src/widget/map_editor_tool.c` now previews trees, rocks, shrubs, meadow, and custom earthquake terrain with representative ghosted terrain images. |
| `bb56ac880` Add active counters for price and demand changes (#1745) | editor/scenario feature | ported/adapted | Added active price/demand counters, editor attribute display, model-data label key, key-table entries, and JSON localization entries for `en`, `fr`, and `ru`. Other locale catalog translations remain a known divergence unless they receive localized strings later. |
| `d58d3c403` Update easyav1 | dependency | intentionally not ported | Only updates the `ext/easyav1` submodule pointer; no local encoder dependency need was identified. |
| `7931b9e22` Allow reservoirs over any aqueducts | runtime rule fix | ported | Routing now allows draggable reservoirs to start on aqueduct terrain while preserving the existing guard against covering nearby aqueducts from non-aqueduct source tiles. |
| `b8923f053` Fix minimap | runtime/UI fix | ported/adapted | Save/scenario minimap readers now choose 8/16-bit bitfield and 16/32-bit building-id readers by version, matching the fork's `.cpp` file-IO seam. |
| `4e81f5af3` Fix aqueduct and wall colors on minimap (#1746) | runtime/UI fix | superseded by final minimap port | The intermediate aqueduct/wall building-color branch was not copied because `caa61f5ce` corrects it. |
| `caa61f5ce` Properly fix minimap | runtime/UI fix | ported | Minimap drawing now lets aqueduct and wall terrain color paths run before generic building rendering while palisades still render like walls. |

## Validation Completed

- 2026-04-23 water overlay/access runtime audit: no observed regressions for reservoir/fountain/well preview behavior, aqueduct preview, reservoir side-connection rules, road/highway under aqueducts, dry aqueduct images, concrete maker/well coverage, and Neptune module coverage.
- 2026-04-23 editor terrain audit: `9d550e322` is ported; elevation/access-ramp multi-tile XY metadata is preserved during terrain painting.
- 2026-04-24 manual port pass: applicable runtime/editor/minimap fixes from the 14 remaining upstream first-parent commits were ported or explicitly marked as intentionally unported above.
- 2026-04-24 static checks: `git diff --check` passed; touched JSON catalogs parsed successfully; no stale calls remain to the replaced minimap buffer helper names.

## Validation To Run Later

- Full build once you want compilation validation.
- Depot regression sweep: clear source/destination, recall all carts, change instructions mid-route, change resource while carts are out.
- Storage dispatch sweep: granary/warehouse emptying and Caesar request behavior.
- Pathing sweep: roads and highways under aqueducts and reservoir placement over aqueduct footprints.
- Undo/cancel image invalidation: verify roads/highways under aqueducts redraw correctly after cancelled build previews and undo operations.
- Editor preview: verify terrain preview behavior if `cbd181ae1` is ported.
- Scenario editor counters: verify price/demand active counters if `bb56ac880` is ported.
- Minimap rendering: verify aqueducts, walls, reservoirs, and overlay-adjacent water features after any minimap fix-set port.

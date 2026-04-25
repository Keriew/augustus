# Tile Graphics Runtime Working Memory

Snapshot: 2026-04-06
Workspace: `C:\Users\imper\Documents\GitHub\augustus_cpp_huge`

## 2026-04-06 extractor correction
- Older notes below that describe Augustus bootstrap relying mainly on local heuristic part assignment are now stale.
- Current extractor truth that also matters for tile-adjacent assets:
  - alias-only logical entries still must not consume materialized `Image_####` numbering
  - canonical Augustus output-path selection still must ignore internal legacy-group references inside materialized composites
  - same-group `group="this"` isometric references now inherit parts recursively from already-inferred Augustus images instead of collapsing through layer-count heuristics
  - footprint padding fixes now live in the extractors, not in tile or building draw math, and the temporary runtime footprint compensation has been removed

## 2026-04-02 extractor/runtime follow-up
- Plaza still stays code-driven in `src/map/tiles.cpp`, but the Augustus extractor now needs to preserve alias-only logical entries without letting them consume materialized `Image_####` numbering.
- Tile runtime now records generic crash/runtime context scopes in `src/map/tile_runtime.cpp`, and red runtime error dialogs should close the game after the user presses OK instead of trying to continue.
- Preferred naming for new code is:
  - `ErrorContextScope` for scope breadcrumbs
  - `error_context_report_*` for runtime diagnostics
- `CrashContextScope` and direct `log.h` calls are compatibility names for older code, not the preferred path for new runtime/widget work.
- `ImageGroupPayload` whole-image alias handling is now part of the tile story because plaza wrappers can reference reused logical entries that must carry full legacy image semantics.
- Augustus bootstrap must tolerate brand-new Augustus-only assets with no Julius template; extractor heuristics now cover local multi-slice footprint composites instead of treating them as fatal.
- Canonical Augustus output-path selection must ignore internal legacy-group references inside materialized composite images, or unrelated art like obelisks can be emitted to the wrong legacy folder.
- Plaza uses same-group forward aliases in the extracted canonical XML, so `ImageGroupPayload` now needs deferred same-group resolution instead of assuming every local alias points backward to an already-committed entry.
- Tile graphics resolution is back on soft-fallback semantics like buildings: if the plaza payload path cannot resolve, it logs once and returns `nullptr` so the legacy tile image path can render instead.

## Goal
- Add a tile-side runtime pipeline that mirrors the building runtime pattern for graphics, indexed by `grid_offset`.
- Canonicalize Augustus wrapper extraction output to the semantic `group=` path when that reference is valid and unambiguous.
- Start with plaza in the tile runtime and keep large statue on the building runtime side.

## Locked decisions
- Do not build unless the user explicitly asks.
- Keep CRLF on touched files.
- Keep both happy path and legacy path alive at the same time.
- `Tiles/*.xml` is minimal for v1 and only carries `graphics.path`.
- Plaza variant selection stays code-first for now.
- Large statue remains building-side.

## Current design
- `src/map/tile_type_registry.*`
  - loads `Mods/<mod>/Tiles/*.xml`
  - currently supports `type="plaza"`
- `src/map/tile_runtime.*`
  - stores per-tile graphics state by `grid_offset`
  - v1 stores plaza image-order index selected by legacy placement logic
- `src/map/tile_runtime_api.h`
  - legacy-facing bridge for reset, clear, plaza registration, and render lookup

## Plaza happy path
- Legacy `set_plaza_image()` still decides the plaza variant.
- It now also registers the selected image-order index with tile runtime.
- City renderers ask `tile_runtime_get_graphic_image(grid_offset)` before falling back to `map_image_at(grid_offset)`.
- `map_image` stays populated with legacy ids as compatibility state.

## Canonical extractor rule
- For each exported Augustus output group:
  - inspect non-local `group=` references after translation
  - if exactly one valid semantic group key is referenced, export the XML and PNG folder there
  - otherwise keep the current assetlist-derived output key
- Invalid or conflicting references log and fall back without aborting extraction.

## Payload note
- `ImageGroupPayload` now exposes stable access by image order index so plaza can reuse legacy variant selection without hardcoding image ids.

## Large statue follow-up
- `large_statue.xml` still points to `Aesthetics\Statue`, but now also pins `l statue anim` through the new `graphics.image` field on the building-side runtime.
- If large statue still misbehaves after this slice, the next place to inspect is the extracted `Aesthetics\Statue` asset membership/order rather than plaza/tile routing.

## Re-read after compaction
- `src/assets/augustus_asset_extractor.cpp`
- `src/core/crash_context.h`
- `src/core/crash_context.cpp`
- `src/assets/image_group_payload.h`
- `src/assets/image_group_payload.cpp`
- `src/map/tile_type_registry_internal.h`
- `src/map/tile_type_registry_xml.cpp`
- `src/map/tile_runtime.cpp`
- `src/map/tiles.cpp`
- `src/widget/city_without_overlay.cpp`
- `src/widget/city_with_overlay.cpp`
- `src/building/building_runtime.cpp`

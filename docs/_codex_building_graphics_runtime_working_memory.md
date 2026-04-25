# Building Graphics Runtime Working Memory

Snapshot: 2026-04-06
Workspace: `C:\Users\imper\Documents\GitHub\augustus_cpp_huge`

## 2026-04-06 building graphics resolution checkpoint
- Native building footprint rendering now routes through `src/widget/city_draw.cpp`, not the older direct `const image *` helper path documented below.
- The footprint-placement bug was ultimately an extractor/data-contract bug first and a runtime ownership bug second:
  - Julius footprint exports were preserving bottom transparent padding and needed that padding trimmed at extraction time, with XML height / layer `y` preserving logical placement
  - Augustus direct-crop footprint exports needed the same bottom-padding trim rules
  - Augustus active ON variants were losing their top slice because local `group="this"` isometric inheritance collapsed to footprint-only during extraction
  - the native footprint stage also needed to submit the whole-building footprint only on the owning draw tile
- Current true state:
  - Julius and Augustus extractors now carry the main placement fix
  - Augustus extractor now resolves same-group part inheritance recursively and expands inherited full-image references into `footprint` + `top` where appropriate
  - native footprint draw ownership is corrected through `grid_offset`
  - the temporary runtime footprint compensation in `src/assets/image_group_payload_materialize.cpp` has been removed; extractor output is the placement contract now

## 2026-04-02 extractor/runtime follow-up
- `BuildingType.graphics` now also needs optional `<image value="..."/>` for grouped assets like `large_statue`.
- `large_statue.xml` in both mods now pins `Aesthetics\Statue` to `l statue anim` instead of relying on the group default.
- New generic crash/runtime breadcrumbs now live in `src/core/crash_context.h/.cpp` and are logged by `src/platform/crash_handler.c`.
- Preferred naming for new code is now:
  - `ErrorContextScope` for the C++ scope object
  - `error_context_report_*` for warning/error/fatal reporting
- `CrashContextScope` and direct `log.h` usage remain compatibility names for older code, but should not be the default choice in new runtime/widget work.
- `building_runtime.cpp`, `tile_runtime.cpp`, `image_group_payload.cpp`, and `augustus_asset_extractor.cpp` now stamp stage/context before high-risk graphics operations.
- Red runtime error dialogs should now close the game after the user presses OK instead of trying to continue.
- Augustus extractor startup failures must remain fatal to startup; do not warn-and-continue there.
- Building graphics resolution is temporarily back on soft-fallback semantics: if the new building path cannot resolve an image or animation frame, it logs once and returns `nullptr` so the legacy renderer path takes over.
- `ImageGroupPayload` needed a same-group alias fast path plus a re-entrant load guard; local references like `group="this"` or aliases to earlier images in the same group must resolve against the payload currently being built instead of recursively reloading the group.
- Large statue definitions now explicitly declare `water_access mode="reservoir_range"` so the existing legacy animation gate for fountains can still key off `has_water_access` under the runtime-managed path.
- The old "best-effort fallback" assumption is no longer the active rule for explicit new-path invariants in this slice; targeted failures now prefer fatal+log.

## Goal
- Route live city building rendering through `building_runtime` plus `ImageGroupPayload` when a `BuildingType` defines graphics.
- Keep compatibility by falling back to legacy `building_image_get` and legacy `map_image` ids when the new path is absent or incomplete.
- Keep this rollout limited to live city rendering for now.

## User decisions locked in
- Do not build unless the user explicitly asks.
- Keep CRLF on touched files.
- Add this file to `.gitignore`.
- Replace the old `<graphic .../>` node instead of supporting both formats.
- New BuildingType graphics node uses child nodes only.
- New logical path format is relative to `Graphics`, backslash-delimited, without `Graphics\` and without `.xml`.
- First-pass runtime rule:
  - building has runtime BuildingType
  - BuildingType graphics path is non-null
  - use happy path with payload manager
  - else use legacy path
- Live city only for now; placement ghosts and editor previews stay legacy.

## Planned BuildingType XML shape
```xml
<building type="theater">
    <graphics>
        <path value="Health_Culture\Theatre" />
        <upgrade threshold="45" operator="gt" />
        <water_access mode="reservoir_range" />
    </graphics>
</building>
```

## Important code seams
- `src/building/building_type_registry_internal.h`
  - `BuildingType` now owns nested `GraphicsDefinition`
- `src/building/building_type_registry_xml.cpp`
  - parser now expects `<graphics>` with child `<path>`, optional `<image>`, `<upgrade>`, and `<water_access>`
- `src/building/building_runtime.h/.cpp`
  - runtime now resolves new-path building images and still maintains legacy compatibility state
- `src/assets/image_group_payload.h/.cpp`
  - path-keyed group manager now exposes default-image lookup, caches failed loads, stores implicit animation metadata/frame keys plus footprint/top composition data, and clones whole-image aliases including top/animation
- `src/core/image_payload.h/.cpp`
  - payload-backed `image` compatibility view already exists
- `src/graphics/image.cpp`
  - now has pointer-based isometric helpers for direct `const image *` draws and a generic pointer draw helper for payload-backed animation frames
- `src/widget/city_with_overlay.cpp`
  - live overlay city building footprint/top now tries runtime image first, then legacy tile id; runtime animations can also draw from payload-backed frame keys
- `src/widget/city_without_overlay.cpp`
  - base live city building footprint/top now tries runtime image first, then legacy tile id; runtime animations can also draw from payload-backed frame keys

## Current compatibility doctrine
- Keep `map_image` populated with legacy ids for untouched systems.
- New runtime render lookup should prefer runtime-owned footprint/top/animation slices and fall back cleanly to legacy ids where that rollout is still incomplete.
- Missing or incomplete extracted graphics content should warn/fallback in this phase, not hard-fail startup.

## Implemented runtime resolver scope
- Supported first-pass building families:
  - theater
  - amphitheater
  - arena
  - school
  - academy
  - library
  - forum
  - actor colony
  - gladiator school
  - doctor
  - hospital
  - barber
  - well
  - work camp
- The resolver uses guessed candidate image ids per family and falls back to the default image in the group when appropriate.
- Unsupported families currently return `nullptr` and stay on legacy rendering.
- For supported families, the payload path now synthesizes a legacy-style `image` view with `top` and implicit animation metadata from the image-group XML.

## Content caveats
- Live BuildingType XML files in `Mods/Vespasian/BuildingType` and `Mods/Augustus/BuildingType` were migrated to `<graphics>`.
- The current graphics path values are scaffolding guesses based on extracted content naming conventions.
- Augustus extractor now distinguishes materialized images from alias-only wrapper nodes so anonymous aliases no longer consume `Image_####` slots.
- Augustus isometric replacement export now prefers canonical template inheritance for numeric Julius references and recursive same-group part inheritance for local Augustus references.
- Missing Julius template files for brand-new Augustus-only assets are still treated as an expected bootstrap miss, but local isometric wrappers now inherit parts from already-inferred Augustus images instead of collapsing through layer-count heuristics.
- Active Augustus ON variants such as arena, amphitheater, colosseum, and large statue depend on that same-group part inheritance to preserve their top slice and keep animation overlays aligned with the base image.
- Legacy template parsing must tolerate `animation` / `frame` children because extracted Julius XML includes them, even when we only care about layer parts for inheritance.
- Augustus canonical output-path selection should only look at alias-only wrapper images, not materialized composite images that happen to reference a legacy group internally.

## Likely touched files
- `.gitignore`
- `docs/_codex_building_graphics_runtime_working_memory.md`
- `src/building/building_runtime.h`
- `src/building/building_runtime.cpp`
- `src/building/building_runtime_api.h`
- `src/building/building_type_registry_internal.h`
- `src/building/building_type_registry_xml.cpp`
- `src/graphics/image.cpp`
- `src/graphics/image.h`
- `src/widget/city_with_overlay.cpp`
- `src/widget/city_with_overlay.h`
- `src/widget/city_without_overlay.cpp`
- `src/widget/city_without_overlay.h`
- `Mods/Vespasian/BuildingType/_README.md`
- `Mods/Vespasian/BuildingType/_template.xml.example`
- live BuildingType XML files in `Mods/Vespasian/BuildingType` and `Mods/Augustus/BuildingType`

## Re-read after compaction
- `src/building/building_runtime.h`
- `src/building/building_runtime.cpp`
- `src/building/building_runtime_api.h`
- `src/building/building_type_registry_internal.h`
- `src/building/building_type_registry_xml.cpp`
- `src/core/crash_context.h`
- `src/core/crash_context.cpp`
- `src/assets/image_group_payload.h`
- `src/assets/image_group_payload.cpp`
- `src/assets/augustus_asset_extractor.cpp`
- `src/core/image_payload.h`
- `src/core/image_payload.cpp`
- `src/graphics/image.h`
- `src/graphics/image.cpp`
- `src/widget/city_with_overlay.cpp`
- `src/widget/city_with_overlay.h`
- `src/widget/city_without_overlay.cpp`
- `src/widget/city_without_overlay.h`

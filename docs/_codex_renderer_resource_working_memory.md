# Renderer Resource Refactor Working Memory

Snapshot: 2026-04-06
Workspace: `C:\Users\imper\Documents\GitHub\augustus_cpp_huge`

## 2026-04-06 canonical extractor correction
- Historical notes below that describe Julius extraction as not yet real, or Augustus extraction as mainly a scaffolding/heuristic pass, are now stale.
- Current true state:
  - Julius extraction is active and is responsible for canonical footprint/top exports used by the runtime fallback chain
  - Julius isometric footprint exports now trim bottom transparent padding and preserve logical placement through exported XML metadata
  - Augustus extraction is also load-bearing and now handles:
    - canonical direct-crop footprint padding trim
    - recursive same-group `group="this"` part inheritance
    - expansion of inherited isometric full-image references into `footprint` + `top`
- The recent active-building animation glitch was fixed by correcting extractor semantics, not by changing authored XML animation `x/y` offsets.

## User-directed architecture
- Do not do narrow "boundary-first" rewrites that leave the old system as the real core.
- Preferred migration pattern is coexistence/cohabitation like the building/figure runtime work:
  - the new runtime owns the real logic/data model
  - legacy struct access still exists alongside it
- Desired long-term image core:
  - path-based key
  - payload object value
  - simple `Key -> Payload`
- Rendering should stop depending on atlas sub-rect sampling for final draw submission.

## Seam diagnosis
- The visible seams at UI scale `> 100` with linear filtering are strongly consistent with atlas bleed.
- The screenshot path uses legacy Caesar 3 UI groups like `GROUP_DIALOG_BACKGROUND`, not just XML assets.
- Fixing only `res/assets/Graphics` would not solve the screenshot case.

## Important repo facts
- `src/platform/renderer.cpp`
  - current 2D draw path still samples atlas sub-rects from `img->atlas.*`
  - linear filtering is enabled depending on scale/filter config
- `src/core/image.c`
  - main climate, enemy, and font images are still decoded then packed into renderer atlases
  - atlas buffers are also used as source pixel data for XML/layer composition
- `src/assets/image.c`
  - XML/composed assets are packed into extra-asset atlases or treated as unpacked extras
  - reference translation currently copies atlas metadata from referenced images
- `src/assets/assets.cpp`
  - current XML load order is selected mod then root assets
- `src/assets/xml.cpp`
  - current path resolution is mod/root oriented only
- `src/platform/augustus.cpp`
  - startup is the right place for canonical Augustus/Julius bootstrap work

## Current implementation direction
- Introduce per-image runtime resource handles alongside legacy `image` metadata.
- Keep legacy `image` structs available while new runtime image resources take over draw submission.
- Upload per-image textures from loader-generated pixel data so the renderer can stop drawing from atlas sub-rects.
- Preserve atlas buffers temporarily where they are still needed for load-time composition.
- Keep the migration in a true cohabitation shape rather than a temporary boundary shim.
- New renderer/widget diagnostics should prefer `src/core/crash_context.h`:
  - `ErrorContextScope` is the preferred C++ scope name for new code
  - `error_context_report_*` is preferred over direct `log.h` calls for runtime warnings/errors/fatal dialogs

## Changes already started
- `src/core/image.h`
  - converted to `#pragma once`
  - added `image_handle`
  - added `resource_handle` to `image`
- `src/graphics/renderer.h`
  - converted to `#pragma once`
  - `render_2d_request` now has `handle`
  - renderer interface now exposes:
    - `upload_image_resource`
    - `release_image_resource`
- `src/platform/renderer.cpp`
  - started managed per-image texture storage
  - draw requests now prefer managed resource handles before atlas textures
  - silhouette/custom helper paths started using managed textures when available
- `src/graphics/image.cpp`
  - legacy requests now populate `request.handle`
- `src/graphics/ui_primitives.cpp`
  - UI requests now populate `request.handle`
- `src/assets/image.c`
  - reference translation now copies `resource_handle`
  - unload path now releases image resources
  - helper functions added for uploading cropped/atlas-backed image resources
  - packed and unpacked extra assets now begin uploading runtime image resources
- `src/core/image.c`
  - reload paths now release image resources for main/enemy/font images
  - atlas conversion paths now upload runtime image resources after decode/packing
- `src/game/mod_manager.h/.cpp`
  - added Augustus/Julius graphics path helpers
- `src/assets/xml.h/.cpp`
  - added Augustus/Julius asset-source enums
  - path resolution now understands current mod, Augustus fallback, Julius fallback, and root fallback
- `src/assets/assets.cpp`
  - XML assetlist loading now includes Augustus and Julius fallback graphics directories
- `src/platform/augustus.cpp`
  - startup bootstrap helpers added for canonical fallback graphics directories
  - current compile issue was caused by missing `assets/assets.h` include for `ASSETS_IMAGE_PATH`
- `.gitignore`
  - private working-memory file is ignored

## Current status
- The renderer can now prefer managed per-image textures over atlas sub-rects.
- Legacy `image` structs still cohabit and carry the runtime `resource_handle`.
- Main, enemy, font, and extra-asset loaders have started populating runtime image resources.
- Augustus fallback bootstrap currently mirrors root graphics into `Mods/Augustus/Graphics` behind a stamp.
- Julius bootstrap now performs canonical legacy extraction into `Mods/Julius/Graphics`, and that extracted output is load-bearing for building/runtime graphics fallback.

## Refresh notes after context compaction
- Re-read:
  - `docs/renderer_ui_vertical_slice_design.md`
  - `src/building/building_runtime.h`
  - `src/building/building_runtime.cpp`
  - `src/platform/renderer.cpp`
  - `src/platform/augustus.cpp`
  - `src/assets/xml.cpp`
  - `src/assets/assets.cpp`
  - `src/core/image_payload.h`
  - `src/core/image_payload.cpp`
- Important restated architectural rule from the user:
  - the new image system must become the real core
  - legacy `image` access is allowed as a compatibility view, not as the authoritative model
  - path-based `Key -> Payload` remains the target
- `building_runtime` is the clearest coexistence reference:
  - C++ runtime object owns behavior
  - legacy `::building &data` remains attached and publicly reachable during migration
  - this is the pattern to mirror for images
- Current `ImagePayload` is still too thin:
  - it started as mostly a keyed handle/refcount table
  - user requested the actual image object be split into its own class and then imported by payload
  - current direction is now:
    - standalone `ImageData` class owns image-facing metadata/resolution and embedded legacy view
    - `ImagePayload` owns key/handle/refcount plus an `ImageData`
    - legacy `image` structs point at payload through opaque `resource_payload`
- Important implementation correction:
  - `image_payload_register` must adopt freshly uploaded renderer handles
  - do not release the handle before transferring ownership into the payload
- Current renderer still violates the final plan in specific ways:
  - `draw_image_request` still falls back to `get_texture(request->img->atlas.id)` if no managed texture exists
  - `draw_texture_request` still uses `img->atlas.x_offset` / `img->atlas.y_offset` for source rects when atlas fallback is active
  - `get_texture(...)`, unpacked-image caching, custom texture IDs, and several helpers still decode atlas IDs directly
  - silhouette and blend-texture helpers still fall back to atlas textures when no managed texture exists
- This means the current state is a real coexistence step, but not yet the plan-complete path-keyed resource core.
- `docs/renderer_ui_vertical_slice_design.md` is still the contract for placement:
  - widgets/adapters convert legacy inputs centrally
  - renderer owns final sampling/domain scale
  - callers must not pre-correct for renderer internals
  - destination rect semantics stay coherent across UI and world
- Source precedence currently in code:
  - current mod
  - Augustus fallback
  - Julius fallback
  - root safety fallback
- For authored path-keyed graphics, including extracted UI groups such as `UI\Top_Menu`, prefer the same logical-key loader flow used by building/tile runtime over flat `assets_get_image_id("UI", ...)` registry assumptions
- Bootstrap status:
  - `augustus.cpp` already materializes `Mods/Augustus/Graphics`
  - Julius extraction is now active and produces canonical fallback graphics content under `Mods/Julius/Graphics`
- New direct-source happy path now added:
  - `image_payload_load_png(image*, path_key, file_path)` loads a PNG directly into a payload-managed renderer resource
  - `asset_image_load_all()` now detects simple non-isometric single-layer XML/mod images and routes them through that direct payload path before atlas packing
  - those direct assets use the resolved PNG path as the payload key
  - complex/composed/isometric assets still fall back to the older composition + atlas-assisted path for now
- Julius extraction is now being implemented as a separate C++ module:
  - `src/core/legacy_image_extractor.h`
  - `src/core/legacy_image_extractor.cpp`
  - hooked from `src/core/image.c` immediately after `convert_images()` and `make_plain_fonts_white()` for the main climate atlas
- Important user correction during extraction work:
  - do not dump legacy output into a generic `Graphics/Extracted/...` tree
  - extracted Julius art should be split into normal top-level graphics families and look comparable to `res/assets/Graphics`
  - using the Augustus buckets exactly where they fit is preferred
  - explicit new normal folders are acceptable when the Augustus buckets are a poor fit
- Current Julius extraction layout direction:
  - PNGs go directly under `Mods/Julius/Graphics/<Family>/Legacy_<source>/Group_<id>/...`
  - XML manifests now target the root of `Mods/Julius/Graphics/`, one XML per extracted legacy group
  - each XML assetlist name is the actual relative graphics folder path, for example `UI/Legacy_c3/Group_132`
  - this keeps the root XML discoverable by the existing loader while still letting the grouped PNGs live in normal family folders
- Current family split being encoded in the extractor:
  - existing Augustus-style buckets:
    - `Admin_Logistics`
    - `Aesthetics`
    - `Health_Culture`
    - `Industry`
    - `Military`
    - `Monuments`
    - `Ships`
    - `Terrain_Maps`
    - `UI`
    - `Walkers`
    - `Warriors`
  - new normal folders for legacy material that does not fit cleanly:
    - `Fonts`
    - `FX`
    - `PaperMap`
    - `Map`
    - `LoadingScreens`
    - `Portraits`
    - `Environment`
    - `Buildings`
- Current extractor output semantics:
  - one colocated XML per scaffolded image group
  - assetlist names equal the actual family/source/group folder path
  - image `src` values are local to that folder, for example `Image_0000`
  - isometric images are exported as layered XML with separate footprint/top PNGs
  - animation metadata currently exports as consecutive-frame `<animation frames=...>` metadata on the base image
- Current storage-model correction from the user:
  - do not treat extracted XMLs as per-legacy-group root manifests
  - instead treat them as scaffolded image-group definitions that live with their grouped images
  - later gameplay/building definitions should point at these XML paths directly
  - image-group manager will become the intermediary resource layer above individual image payloads
- Current naming direction:
  - keep using legacy boundaries as a temporary scaffold
  - where obvious, use human-readable scaffold names like `Theater`, `Dialog_Background`, `Road`
  - where not obvious yet, fall back to `Group_<id>` until the building-type and gameplay hookups reveal the better semantic grouping
- Static issue already caught and corrected during implementation:
  - do not classify groups by loose numeric ranges, because the legacy enum space interleaves unrelated categories
  - classification must be explicit per known building/figure group
- Near-term implementation direction after this refresh:
  1. Evolve `ImagePayload` from keyed handle table into the authoritative runtime image object with metadata ownership.
  2. Attach legacy `image` structs to that runtime object in the same spirit as `building_runtime`.
  3. Move renderer draw submission and metadata lookup to the payload/handle path first, leaving atlas fallback only as temporary coexistence debt.
  4. Then push the same ownership model into XML assets, legacy climate/font/enemy loads, and external images.
  5. After that, remove more atlas-based draw assumptions and revisit UI seam behavior on top of the cleaner resource core.

## Next concrete steps
1. Finish static cleanup of the current resource-handle wave
   - remove obvious compile issues from the bootstrap/resource changes
   - audit null-safety around renderer interface calls during shutdown/reload
2. Push the managed-resource path further into the legacy climate/UI path
   - ensure the UI reproducer uses managed textures end-to-end instead of silently falling back to atlas sampling
   - inspect silhouette/custom-image helpers for remaining atlas assumptions
3. Flesh out canonical source bootstrap
   - keep Augustus stamp/checksum logic
   - implement real Julius extraction/materialization of legacy graphics into canonical files
   - preserve precedence `Current Mod > Augustus > Julius`
4. Revisit UI primitives
   - keep explicit container/border/fill composition
   - stabilize shared snapped edges where needed after renderer path is changed
5. Move toward the intended path-keyed payload core
   - introduce an actual C++ image payload/container keyed by canonical path
   - keep legacy `image` access as an attached compatibility view rather than the authoritative core

## Latest known compile issue
- Fixed locally in source:
  - `src/platform/augustus.cpp` needed `assets/assets.h` for `ASSETS_IMAGE_PATH`
- User-reported MSVC diagnostics were around line 144 in `src/platform/augustus.cpp` and looked like a cascading parse failure from that missing include.

## Constraints
- Do not build unless the user explicitly asks.
- Keep CRLF on touched files.
- `git` is not available in this shell, so rely on direct file inspection and static searches.
- BuildingType templates/examples are now single-source:
  - keep `_README.md` and `_template.xml.example` only in `Mods\Vespasian\BuildingType`
  - `Mods\Augustus\BuildingType` should contain live XML data only

## 2026-04-02 Importer Refresh
- The user clarified the canonical logical key format again:
  - keys are plain graphics stems, without `Graphics/` and without file extensions
  - the example should be read as `Military\Barracks`
  - forward slashes can be tolerated in parsing, but the intended canonical stored form is backslash-delimited
- Override identity must therefore be path-only:
  - current mod beats Augustus
  - Augustus beats Julius
  - no extra source namespace like `Legacy_c3` may appear in the logical key
- Extractor progress after that correction:
  - `legacy_image_extractor.cpp` now targets canonical group paths instead of `Legacy_<source>` subfolders
  - group XML names are being emitted as backslash logical keys such as `Military\Barracks`
  - PNGs still live under `Mods/Julius/Graphics/<Family>/<GroupName>/...`
  - XMLs live at `Mods/Julius/Graphics/<Family>/<GroupName>.xml`
  - extractor stamps are now global generated-output stamps, and a generated manifest file is being used to clear prior generated output before rewriting
  - extractor stamp contents now combine:
    - a metadata format version in the prefix
    - the legacy source stem
    - a fingerprint of the decoded source data plus extraction-relevant metadata
  - extractor now logs:
    - why extraction is happening
    - when extraction starts
    - a completion summary with exported group/image/png counts
- Shared resolver progress:
  - `xml.h/.cpp` now expose public helpers for:
    - resolving a logical assetlist key to `<key>.xml`
    - resolving a logical group/image pair to `<key>/<image>.png`
    - resolving direct group images
  - these reuse the same precedence chain already established in xml loading
- New importer path:
  - `src/assets/image_group_payload.h/.cpp`
  - `src/assets/image_group_entry.h/.cpp`
  - `ImageGroupPayload` is the path-keyed XML-group manager above image payloads
  - `ImageGroupEntry` is a separate class holding one group image relationship, mirroring the earlier `ImageData` / `ImagePayload` split
  - importer behavior currently:
    - resolve logical key to the winning XML path and winning source
    - parse the assetlist with a dedicated parser instead of reusing the old flat asset registry
    - load `src` PNGs into `ImagePayload` using logical image keys like `Military\Barracks\Image_0000`
    - support group/image references by recursively loading referenced image groups and retaining the referenced image payload keys
- Important caveat:
  - the new importer exists as a parallel happy path, but it is not yet hooked into building definitions or other gameplay callers
  - next real milestone is to route a first live caller onto `image_group_payload_load(...)` / `image_group_payload_get_image(...)`
- Augustus bootstrap refresh:
  - `augustus.cpp` now fingerprints the source `res/assets/Graphics` tree using recursive file/directory names plus modified times
  - the Augustus bootstrap stamp includes both a version prefix and that source fingerprint
  - bootstrap now logs when it is rebuilding because the stamp is missing, the fingerprint changed, or the fallback directory is empty


- Augustus fingerprint path join bug fixed: the resolved asset root may not end in a slash, so joining it with `Graphics` must insert a separator or it becomes `assetsGraphics` on Windows.
- Augustus bootstrap no longer relies on `platform_file_manager_copy_directory` for the canonical Graphics mirror; it now does its own recursive copy with per-file/per-directory error logging and a success summary, because the generic copier was failing too opaquely during startup.

## 2026-04-02 Augustus Extractor Pivot
- The user confirmed the raw Augustus mirror was the wrong direction:
  - Augustus should be treated like Julius
  - packed `assets\Graphics\*.xml` + `*.png` atlases must be exploded into canonical per-group XMLs and per-image PNGs under `Mods\Augustus\Graphics`
  - folder families should match the Julius/canonical layout shape, not a generic copied root atlas set
- Current implementation change:
  - new module `src/assets/augustus_asset_extractor.h/.cpp`
  - `src/platform/augustus.cpp` now calls `augustus_asset_extractor_bootstrap()` instead of copying `assets\Graphics`
  - the new extractor:
    - fingerprints top-level Augustus source XML/PNG files using names + modified times with metadata version in the stamp prefix
    - clears/rebuilds `Mods/Augustus/Graphics`
    - parses each packed Augustus atlas XML
    - groups images by local-reference connected components, with unnamed runs attached to the previous named image as a scaffold heuristic
    - writes one XML per extracted image group under `Mods/Augustus/Graphics/<Family>/<Group>.xml`
    - writes direct atlas crops as individual PNGs under `Mods/Augustus/Graphics/<Family>/<Group>/...`
    - rewrites `group="this"` references to the explicit canonical group key
    - rewrites numeric legacy group references through `legacy_image_extractor_get_group_key(...)`
    - keeps its extraction stamp under `Mods/Augustus/Graphics/.graphics_extract.stamp`, matching Julius keeping its stamp under `Mods/Julius/Graphics/...`
- Shared helper update:
  - `src/core/legacy_image_extractor.cpp` now implements `legacy_image_extractor_get_group_key(...)`
  - this is the bridge from Augustus packed XML legacy numeric refs back into canonical Julius-style path keys
- Important caveat for the next pass:
  - this Augustus extractor is a scaffolding pass, not the final semantic grouping pass
  - representative group names are inferred from the first named image in each connected component
  - duplicate names within a family get numeric suffixes
  - this should be good enough to reach the stop point of "extract canonical Augustus data to disk" before the building-type/image-group runtime hooks are wired

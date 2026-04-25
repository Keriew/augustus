# Graphics Upscale Pipeline Notes

Date: 2026-03-30

## Goal

Create a repeatable pipeline that can produce higher-resolution graphics for
`res/assets/Graphics/` so the renderer can display cleaner art at both low and
high zoom levels while preserving the original in-game footprint.

## Repo Findings

- `res/assets/Graphics/` contains 11 XML files at the root and 11 matching asset
  folders:
  `Admin_Logistics`, `Aesthetics`, `Health_Culture`, `Industry`, `Military`,
  `Monuments`, `Ships`, `Terrain_Maps`, `UI`, `Walkers`, `Warriors`.
- Total local PNG inventory under `res/assets/Graphics/`:
  3,605 PNG files, about 10.42 MB.
- The biggest asset categories by file count are:
  `Walkers` (1,534 PNGs), `Warriors` (916 PNGs), `Monuments` (263 PNGs),
  `UI` (256 PNGs).
- The most common raw image sizes are very small:
  `Walkers` is dominated by `39x39`, `Warriors` by `39x39` and `80x80`,
  `UI` has many `24x24`, while buildings/monuments go up to `298x150`,
  `391x191`, and `418x231`.

## What The XML Actually Describes

The XML is not just a file list. It is an asset graph.

Common patterns found across the XML files:

- Direct image bindings:
  `<image id="..." src="Some_Png_Basename" />`
- Layered composites:
  `<layer src="..." x="..." y="..."/>`
- References to previously declared local images:
  `<layer group="this" image="Some_Image_Id"/>`
- Animation sequences:
  `<animation speed="..." x="..." y="..."> ... </animation>`
- Numeric group references to external or pre-existing asset groups:
  `<layer group="53" image="7"/>`
- Transform-like behavior:
  `invert`, `rotate`, `mask`, `part`

Relevant attribute inventory seen in this repo:

- `image`: `group`, `height`, `id`, `image`, `isometric`, `src`, `width`
- `layer`: `group`, `height`, `image`, `invert`, `mask`, `part`, `rotate`,
  `src`, `width`, `x`, `y`
- `animation`: `reversible`, `speed`, `x`, `y`
- `frame`: `group`, `image`, `src`

## Important Consequence

Upscaling only the PNG files is not enough for the XML-driven assets.

If a source PNG is upscaled by factor `N`, then pixel-space XML values usually
must also be scaled by `N`, especially:

- `image.width`
- `image.height`
- `layer.x`
- `layer.y`
- `layer.width`
- `layer.height`
- `animation.x`
- `animation.y`

Otherwise composite assets drift apart. Example: a layer placed at `x="18"`
must become `x="72"` after a 4x upscale if the overlaid PNG also became 4x
larger.

`speed`, `reversible`, `group`, `image`, `src`, `invert`, `rotate`, `mask`,
and `part` should not be scaled.

## Animation Findings

- Animation-heavy folders:
  `Monuments` (33 animations), `UI` (21), `Military` (20),
  `Industry` (16), `Aesthetics` (15), `Health_Culture` (8),
  `Admin_Logistics` (5).
- Many animation frames are not standalone PNGs. They are composite images
  defined in XML and then used as frame references.
- Some animations reference local image ids, some reference local PNG basenames,
  and some reference numeric external groups.

Conclusion:

- Frame interpolation should be treated as a separate optional stage, not part
  of v1.
- For v1, keep the existing frame count and order exactly as authored in XML.
- First solve clean spatial upscale and XML coordinate scaling.

## Integrity Issues Worth Reporting In The Pipeline

The pipeline should validate before processing and emit a report.

Examples found during this pass:

- Case / naming mismatch in `monuments.xml`:
  XML references `Caravanserai_S_ON_01`, `Caravanserai_S_On_02`,
  `Caravanserai_S_On_03`, while folder filenames differ in case.
- Missing `Mothball_1` through `Mothball_4` in `ui.xml`.
- Missing death-frame PNGs in `walkers.xml`, including
  `overseer_death_05`,
  `caravanserai_overseer_death_05`,
  `caravanserai_walker_death_06`,
  `caravanserai_walker_death_07`,
  `caravanserai_walker_death_08`.
- Some folders also contain unused local PNGs that are not referenced by the
  matching XML.

The tool should not silently fail on these. It should output:

- missing local PNG refs
- case-mismatch refs
- unused PNGs
- external numeric group refs

## Recommendation

Use a local-first pipeline, not an OpenAI-first pipeline.

Reasoning:

- This job is small in storage size, but it needs determinism and very low
  artistic drift.
- Many assets are tiny sprites where generative edits can easily change
  silhouettes, outlines, or readability.
- A bulk API workflow would also need careful prompt control and per-asset QA,
  which reduces the benefit of simple automation.

OpenAI note, as checked on 2026-03-30:

- OpenAI's current image API is positioned as image generation / editing, not a
  dedicated deterministic sprite super-resolution tool:
  https://platform.openai.com/docs/guides/tools-image-generation/
- OpenAI pricing also charges for image input and output, so a large batch is
  possible but not obviously the best fit for precise asset preservation:
  https://openai.com/api/pricing/

For local processing, the best first candidate is:

- `Real-ESRGAN-ncnn-vulkan`
  https://github.com/xinntao/Real-ESRGAN-ncnn-vulkan

Why:

- Works locally on Windows.
- Supports directory input and PNG output.
- Supports GPU inference.
- Supports 2x / 3x / 4x scaling.
- Includes an anime/video-oriented model (`realesr-animevideov3`) that is at
  least directionally closer to stylized game art than a photo-only model.

Important caveat:

- Tiny walkers / warriors may look worse with aggressive AI upscaling.
- I would not force one model across every asset class.

## Suggested Strategy

Use a staged, bucketed pipeline.

### Stage 1: XML Analysis And Manifest

Write a Python analysis script that:

- walks each XML file at `res/assets/Graphics/*.xml`
- resolves the matching folder from `<assetlist name="...">`
- builds a manifest of:
  - local PNG basenames
  - local composite image ids
  - animation blocks and frame order
  - local dependencies
  - external numeric group dependencies
  - validation warnings

Suggested outputs:

- `out/graphics_upscale/manifest.json`
- `out/graphics_upscale/report.md`

### Stage 2: Asset Bucketing

Split PNGs into processing buckets before upscaling:

- Bucket A: buildings, monuments, terrain, ships
- Bucket B: UI panels and icons
- Bucket C: walkers and warriors
- Bucket D: effect / animation overlays

Recommended first-pass policy:

- Bucket A: try AI upscale first
- Bucket B: compare AI upscale against a classic scaler and keep whichever is
  clearer
- Bucket C: be conservative; test AI against non-generative upscale before
  committing
- Bucket D: preserve transparency and animation consistency very carefully

### Stage 3: PNG Upscale

Do not overwrite originals.

Write results to a parallel tree, for example:

- `out/graphics_upscale/png/Admin_Logistics/...`
- `out/graphics_upscale/png/Aesthetics/...`
- etc.

Keep the same relative filenames and folder names.

### Stage 4: XML Rewrite

Emit scaled XML copies into a parallel tree, for example:

- `out/graphics_upscale/xml/admin_logistics.xml`
- `out/graphics_upscale/xml/aesthetics.xml`
- etc.

For scale factor `N`, rewrite:

- `width`, `height`, `x`, `y` on the relevant tags

Keep all ids, refs, ordering, animation speed, and reversibility untouched.

### Stage 5: Validation

Add checks that:

- every local `src` ref exists in the output PNG tree
- every case-sensitive filename matches on disk
- every scaled XML file preserves the same reference graph
- every PNG remains RGBA-compatible
- animation frame counts are unchanged

### Stage 6: Visual QA

Generate contact sheets for selected buckets:

- 20 buildings
- 20 UI assets
- 20 walkers
- 10 animated assets with all frames tiled side by side

This will catch bad hallucinations faster than in-game manual review.

## About Frame Interpolation

My view: do not use frame interpolation in the first version.

Why:

- Existing XML animations are often short, authored loops with deliberate frame
  timing.
- Interpolated in-between frames can create wobble, ghosting, or timing drift.
- Some frame sequences are composite overlays on top of static bases, not
  natural-motion video.

When it may still be worth testing:

- water / fire / smoke-like overlays
- large monument spectacle animations
- only as an optional experiment after basic upscale is stable

If explored later, a separate local tool such as RIFE can be tested:

- https://github.com/hzwer/ECCV2022-RIFE

But I would keep this entirely out of the initial production pipeline.

## My Recommended v1

If I were building this now, I would do:

1. Python manifest + validator for the XML graph.
2. Parallel output tree, never destructive overwrite.
3. 4x upscale as the primary target.
4. Local processing first, using Real-ESRGAN-based experiments for bigger art.
5. Conservative treatment for tiny sprites, with side-by-side comparison before
   adopting AI globally.
6. XML numeric scaling in the generated copy.
7. No frame interpolation in v1.

## Practical Next Step

Build the analysis/manifest generator first.

That gives us:

- a reliable picture of what is local vs external
- a complete list of files to process
- a validation report for broken refs
- the exact XML fields that must be rewritten alongside PNG upscale

Once that exists, the actual upscale runner becomes much easier to swap between:

- local AI
- classic scaler
- future API-based experiments

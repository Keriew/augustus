# Image Group Payload Pipeline

This note exists to keep the new XML-driven runtime graphics path legible and predictable.

## Stages

1. Parse
   Input: one XML file for one `(group key, source)`.
   Output: one `ImageGroupDoc` with ordered entry ids and symbolic references only.

2. Merge
   Input: all existing docs for one group in source priority order.
   Output: one `MergedImageGroup` where override grain is the entry id, not the whole file.

3. Resolve / Materialize
   Input: one merged entry id or one source-local selector.
   Output: one `ResolvedImageEntry` with:
   - split-source pixels for future references
   - footprint/top managed payload keys for rendering
   - animation frame payload keys

4. Payload Facade
   Input: resolved entries for one merged group.
   Output: one `ImageGroupPayload` exposing the legacy-shaped `image` contract used by runtime drawing.

## Source Priority

Current priority order is:

1. Vespasian mod override
2. Augustus
3. Julius
4. Root/base

The important rule is: entry override happens per image id, not per XML file.

## Manual Reference Cases

These are the hand-simulated cases we should keep using when reasoning about bugs.

### Case 1: `Aesthetics\Statue`

Inputs:

- Augustus `Aesthetics\Statue.xml` defines `V Small Statue`
- Julius `Aesthetics\Statue.xml` defines `Image_0000`, `Image_0001`, `Image_0002`

Expected merged order:

1. `V Small Statue`
2. `Image_0000`
3. `Image_0001`
4. `Image_0002`

Expected resolution:

- `V Small Statue` same-group references to `Image_0000` resolve against the merged namespace.
- No file-level fallback is needed because the merged group already contains both entries.

### Case 2: `Health_Culture\Library`

Inputs:

- Augustus defines `Downgraded_Library`
- Julius defines `Image_0000`

Expected merged order:

1. `Downgraded_Library`
2. `Image_0000`

Expected resolution:

- The downgraded state resolves locally.
- The upgraded state can still resolve `Image_0000` from Julius because entry lookup is merged by id.

### Case 3: `Aesthetics\l_statue_anim`

Inputs:

- Augustus animation entry references `Aesthetics\Statue` image `Image_0002`

Expected resolution:

- Cross-group lookup first merges `Aesthetics\Statue`
- Then resolves `Image_0002` from Julius in that merged namespace
- Then reconstructs the requested raster part from the resolved split-source buffer

## Isometric Split Contract

The resolved-entry split buffer must stay compatible with the legacy `layer.cpp` expectations.

For an isometric image with:

- width = `W`
- logical combined height = `H`
- top height = `T`
- footprint draw height = `F`

The resolved entry stores:

- `split_width = W`
- `split_height = T + F`
- `top_height = T`
- `split_pixels[0:T]` = top rows
- `split_pixels[T:T+F]` = footprint rows

Expected reconstruction behavior:

- `PART_TOP`
  Output height = `T`
  Output pixels = top rows only

- `PART_FOOTPRINT`
  Output height = `T + F / 2`
  Output contains only the reconstructed footprint on the logical canvas

- `PART_BOTH`
  Output height = `T + F / 2`
  Output contains top rows plus reconstructed footprint on the logical canvas

## Renderer Boundary

The runtime renderer should only consume the public payload contract:

- base footprint image
- optional top image
- animation frame images

It should not need to know how references were resolved or how the split-source buffer is stored.

That split-source buffer exists for composition/reference extraction only, not as a render-facing abstraction.

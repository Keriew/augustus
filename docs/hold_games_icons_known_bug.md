# Hold Games Icons Known Bug

Snapshot: 2026-04-08

## Current bug
- On the hold games window, the first card image for naval games renders correctly.
- The Roman Games and Animal Games cards still do not render the correct preview images.

## Reported symptoms
- The hold games screen shows the proper naval-games artwork.
- The other two selectable event cards do not show the expected images.
- This affects the card previews on the event-selection window, not just tooltip text or button state.

## What was tried
- The highlight overlay bug was fixed separately by drawing the highlight as a normal image instead of as a 4-slice border.
- The icon draw path was changed away from blindly using `base_image_id + i`.
- An explicit lookup attempt was added using:
  - `Naumachia Icon`
  - `Image_0018`
  - `Image_0019`
- That did not resolve the remaining two card previews in-game.

## Important characteristics
- The issue appears specific to the event preview artwork selection.
- The naval-games card proves the overall layout, border, highlight, and image draw path are functioning.
- The remaining failure is likely in asset identification or atlas/image-id assumptions rather than in generic UI scaling.

## Most relevant code areas
- Hold-games window rendering:
  - `src/window/hold_games.cpp`
- Game definitions and ids:
  - `src/city/games.c`
  - `src/city/games.h`
- Asset lookup / image id resolution:
  - `src/assets/assets.*`
  - `src/graphics/image.*`
- Extracted sample asset clue:
  - `extracted_graphics_sample/Augustus/Graphics/UI/Naumachia_Icon.xml`

## Best current theory
- The Roman Games and Animal Games previews are not guaranteed to live at predictable contiguous image ids relative to `Naumachia Icon`.
- The names `Image_0018` and `Image_0019` found in the extracted XML may not correspond to the real runtime ids the game is using, or they may be intermediate/generated names that are not reliable for direct lookup.
- The correct assets may exist only in the packed runtime atlas data, may not have been extracted clearly, or may be referenced under different ids than the extracted sample suggests.

## Next debugging direction
1. Inspect the runtime asset registry for the actual image ids associated with the hold-games card artwork.
2. Trace how the original UI expected these three previews to be resolved, instead of inferring adjacency from one known icon.
3. If needed, add temporary logging around `assets_get_image_id("UI", ...)` for the relevant hold-games art names.
4. Only after the true asset ids are confirmed, replace the current placeholder mapping in `src/window/hold_games.cpp`.

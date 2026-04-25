# Tree Clear Known Bug

Snapshot: 2026-03-30

## Current bug
- The trees-only variant of the clear land tool does not yet clear every kind of natural tree-looking obstacle on the map.
- Confirmed example: desert maps can still leave natural palm trees behind.

## What is already working
- The clear land tool was migrated into the new mode-switch framework.
- Modes are:
  - default: delete
  - `Shift`: trees only
  - `Ctrl`: repair
- Trees-only mode now:
  - selects directly from the clear icon flow
  - previews the full dragged area
  - uses the same translucent highlight style as delete/repair
  - has working tooltip text on the sidebar icon

## Important distinction discovered
- There are at least two separate "tree" concepts in the codebase.

### Natural terrain trees
- Terrain flags live in `src/map/terrain.h`.
- Natural terrain tree rendering and updates are mainly handled in `src/map/tiles.c`.
- The main obvious flag is `TERRAIN_TREE`.

### Decorative tree buildings
- Decorative / ornamental trees are actual building types, not terrain.
- These are defined in `src/building/type.h` and `src/building/properties.c`.
- Examples include:
  - `BUILDING_PINE_TREE`
  - `BUILDING_FIR_TREE`
  - `BUILDING_OAK_TREE`
  - `BUILDING_ELM_TREE`
  - `BUILDING_FIG_TREE`
  - `BUILDING_PLUM_TREE`
  - `BUILDING_PALM_TREE`
  - `BUILDING_DATE_TREE`
- These should not be removed by the trees-only clear-land tool, because they are player-placed desirability ornaments.

## Tested hypotheses that were not the fix
- Treating `TERRAIN_SHRUB` as equivalent to trees was tried.
  - This was removed again because shrubs are not confirmed to be the intended target, and the user reported the test map issue was not shrubs.
- Treating `TERRAIN_ORIGINALLY_TREE` as equivalent to trees was tried.
  - This was also removed again after user testing confirmed it was not the culprit.

## Current code state after rollback
- The trees-only clear tool is back to targeting only `TERRAIN_TREE` in `src/building/construction_clear.cpp`.
- No decorative tree building types are included in the clear-trees logic.
- `TERRAIN_SHRUB` is not included.
- `TERRAIN_ORIGINALLY_TREE` is not included.

## Most relevant files for future investigation
- Natural terrain flags:
  - `src/map/terrain.h`
- Natural terrain tree/shrub image/update logic:
  - `src/map/tiles.c`
- Original-tree bookkeeping:
  - `src/map/terrain.c`
- Clear-trees gameplay logic:
  - `src/building/construction_clear.cpp`
- Decorative tree building definitions:
  - `src/building/type.h`
  - `src/building/properties.c`

## Best current theory
- Some natural tree variants on certain climates, especially desert palms/cypress-like vegetation, are represented by a different terrain bit, image-state rule, or tile-state combination than `TERRAIN_TREE`, `TERRAIN_SHRUB`, or `TERRAIN_ORIGINALLY_TREE` alone.
- The missing target is likely in the natural terrain system, not in the decorative tree building system.

## Next debugging direction
1. Inspect the exact terrain bits present on one of the stubborn desert palm tiles at runtime.
2. Trace which update path in `src/map/tiles.c` renders that tile as a tree-like obstacle.
3. Extend the clear-trees mask only after identifying the exact natural-tree representation with certainty.

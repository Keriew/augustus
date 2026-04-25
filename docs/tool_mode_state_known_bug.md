# Tool Mode State Known Bug

Snapshot: 2026-03-30

## Current bug
- After playing for a while, the clear tool entered an incorrect state where it painted the map using the same pathing / routed preview logic as roads.
- The issue appeared after both the clear tool and the road tool had already been working normally earlier in the session.

## Reported symptoms
- Selecting the clear tool showed road-like pathing behavior instead of normal clear-area behavior.
- Toggling away from the tool and back again did not fix it.
- The bad state survived normal tool swapping inside that running session.

## What was tried
- Switching on and off the clear tool did not help.
- Pressing `Shift` and `Ctrl` did not reliably reproduce or clear the issue.
- Swapping between the road tool and the clear tool did not reliably reproduce it either.

## What cleared the problem
- Exiting to the main menu and loading another save removed the bug.
- After that reset, the user could not reproduce the issue again.

## Important characteristics
- This currently appears to be intermittent.
- It is not yet tied to a reliable key combination.
- It may depend on long-lived session state rather than only the currently selected tool.
- It may involve stale mode-switch or drag-state data leaking between tool families.

## Most relevant code areas
- Tool family resolution:
  - `src/building/tool_mode.cpp`
  - `src/building/tool_mode.h`
- Construction state and drag-point handling:
  - `src/building/construction.cpp`
  - `src/building/construction.h`
- City input loop:
  - `src/widget/city.c`
- Sidebar tool selection:
  - `src/widget/sidebar/city.cpp`

## Best current theory
- Some construction state that should be tool-family-specific is occasionally surviving across a tool switch and causing clear-land updates to run with routed-drag assumptions.
- The most suspicious candidates are:
  - stale selected tool family state
  - stale compatibility alias state
  - stale raw/effective drag points
  - an in-progress flag or preview state not being reset on one edge path

## Next debugging direction
1. Add temporary logging around tool selection and `building_construction_type()` resolution to capture:
   - `selected_type`
   - `compatibility_alias_type`
   - resolved `type`
   - `in_progress`
2. Log transitions into and out of:
   - `building_construction_set_type()`
   - `building_construction_clear_type()`
   - `building_construction_cancel()`
   - `building_construction_start()`
   - `building_construction_update()`
3. Specifically verify that switching to clear always resets any road-family raw drag state and that clear preview never inherits routed placement assumptions.
4. If the bug is reproduced again, record the exact prior sequence of tool changes before the corruption appears.

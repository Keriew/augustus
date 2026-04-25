# UI Widgets And Primitives Working Memory

Snapshot: 2026-04-25

## Purpose

This note captures the current UI widget/primitives vocabulary and the findings from the ad-hoc UI deduplication pass. Read it before planning UI cleanup, C-to-C++ UI migrations, or new shared control work.

## Vocabulary

- Primitives are draw-level building blocks. They submit low-level sprite, border, panel, text, strip, slider, or saved-region drawing through the request-based renderer path.
- Widgets are control-level composition objects. They own reusable UI behavior and layout for a recognizable control such as a bordered button, image button, advisor text button, panel, label, scrollbar dot, or top-menu panel.
- `SharedUiRuntime` is the C++ facade/orchestrator. It should expose narrow entry points to widgets for legacy callers, while keeping widget behavior in widget classes.
- Legacy C APIs such as `button_border_draw`, `outer_panel_draw`, `image_buttons_draw`, and `scrollbar_draw` are compatibility paths. They are acceptable as adapters, but new reusable composition should move toward widget/runtime methods.

## Current Widget And Primitive Stock

Core widget classes currently include:

- `UiWidget`
- `ButtonWidget`
- `BorderedButtonWidget`
- `ImageButtonWidget`
- `PanelWidget`
- `LabelWidget`
- `TopMenuPanelWidget`
- `ScrollbarWidget`
- `AdvisorTextButtonWidget`
- `AdvisorCardButtonWidget`
- `EmpireTradeRouteButtonWidget`

Current draw-level primitives include:

- `UiPrimitives`
- `UiBorderPrimitive`
- `UiOneRowBorderPrimitive`
- `UiPanelPrimitive`
- `UiSpritePrimitive`
- `UiTextPrimitive`
- `UiTiledStripPrimitive`
- `UiSliderPrimitive`
- `UiSavedRegionPrimitive`

Important distinction:

- Do not introduce a new widget before checking whether one of the existing widget classes can be extended cleanly.
- Do not move control behavior into primitives. Primitives should remain draw methods/building blocks.

## Ad-Hoc UI Tally Outside `src/graphics`

The deduplication survey counted common non-graphics call sites roughly as:

- `244` `button_border_draw` calls
- `176` `outer_panel_draw` calls
- `115` `inner_panel_draw` calls
- `75` label calls
- `6` one-row border calls
- isolated slider/unbordered panel calls

Panels, labels, image buttons, scrollbars, and sliders already route through stock widget/runtime adapters. The most useful low-risk cleanup target is repeated "bordered button plus simple content" composition in `src/window` and `src/widget`.

## 2026-04-25 Deduplication Pass

The first safe migration pass deliberately avoided adding a new icon-button class. Instead:

- `BorderedButtonWidget` gained an optional `BorderedButtonIconSpec` for single-icon content with explicit offset and optional logical size/color.
- `SharedUiRuntime` gained C++ methods for bordered icon buttons and for the existing advisor/empire button widgets.
- `src/window/advisor/military.c` and `src/window/advisor/imperial.c` were converted to `.cpp` using the established `figure.cpp` style: C++ includes first, legacy headers inside `extern "C"`.
- Military legion action icon buttons now use the extended bordered button widget through `SharedUiRuntime`.
- Trade advisor policy icon buttons now use the extended bordered button widget through `SharedUiRuntime`.
- Trade advisor footer buttons now use `AdvisorTextButtonWidget` through `SharedUiRuntime`.
- Imperial advisor donate/gift footer buttons now use `AdvisorTextButtonWidget` through `SharedUiRuntime`.

The pass intentionally left custom rows with complex conditional text/data layout alone unless they matched an existing widget with no special cases.

## Migration Guidance For Future UI Tasks

- Start by inspecting existing widget classes and primitives before proposing new classes.
- Prefer small widget extensions when the visual/control contract is already represented by an existing widget.
- Add a new widget only when the target control has a distinct reusable behavior/layout contract, not merely because one caller has unusual coordinates.
- Keep `ui_runtime.cpp` as orchestration/facade glue; avoid turning it into a logic monolith.
- Use `ui_runtime_api.h` only when C-callable compatibility is needed. C++ window files should usually call `SharedUiRuntime` directly.
- When converting C UI files to C++, follow the `figure.cpp` include/linkage style and add explicit enum/integer casts where needed.
- Keep CRLF line endings on touched files.
- Do not build unless the user explicitly asks in the current chat.

## Safe Next Candidates

Good future cleanup candidates:

- More advisor footer/text buttons that already hand-compose `button_border_draw` plus centered text.
- Small icon buttons where border plus one image can use `BorderedButtonIconSpec`.
- Existing direct construction of `AdvisorTextButtonWidget` / `AdvisorCardButtonWidget` in advisor windows can be routed through `SharedUiRuntime`.

Avoid as first-pass targets:

- Religion advisor/building/figure work when another session is active there.
- Custom request/resource rows with multiple conditional text segments and resource calculations.
- Rewriting panels/labels/scrollbars that already travel through stock adapters unless there is a concrete rendering bug.

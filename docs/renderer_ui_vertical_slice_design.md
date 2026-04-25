# Renderer / UI Vertical Slice Design

Date: 2026-04-02

## Purpose

This file is the working design note for the current renderer and shared-UI
vertical slice. It is intentionally separate from the four always-read memory
files. The goal is to keep one explicit contract in front of every edit while
the renderer, widget layer, and legacy adapters are being rewritten.

## Current Objective

Stabilize the first end-to-end render contract so both UI and world rendering
can describe draw operations coherently as:

- draw image `I`
- at logical destination position `x`, `y`
- with logical destination size `w`, `h`

The renderer then decides how source resolution maps to that destination rect.

This pass is not about command queues, z-buffers, or multi-pass depth logic.
Those are future renderer concerns.

## Scope Of This Slice

In scope:

- explicit 2D draw request contract
- UI logical scaling through display scale
- world logical scaling through city zoom
- shared UI primitive / widget layering
- legacy adapter cleanup
- stabilization of common controls and config-menu rendering

Out of scope for now:

- renderer-side draw queues
- z-buffer / depth scheduling
- 2.5D or SimCity-4-style final depth strategy
- world lighting/effects rewrite
- full text/UTF rewrite

## The Core Contract

`render_2d_request` must mean the same thing everywhere:

- `img`:
  source image metadata and source pixel resolution
- `x`, `y`:
  logical destination-space top-left position
- `logical_width`, `logical_height`:
  logical destination-space size on screen
- `domain`:
  which logical scale regime applies
- `scaling_policy`:
  how filtering/upscale/downscale is chosen

This means:

- source resolution is a property of the image
- destination geometry is a property of the request
- renderer output scale belongs to the render domain
- request coordinates must never secretly be stored in source-scaled space

## Domain Responsibilities

The renderer has render domains. The domain owns the external scale regime:

- `RENDER_DOMAIN_UI`
  logical UI units become screen pixels through display scale
- `RENDER_DOMAIN_PIXEL`
  logical world units become screen pixels through city zoom conventions
- tooltip / snapshot variants:
  same contract, different targets

The important rule is:

- domain scale changes how logical destination geometry lands on screen
- domain scale does not redefine what `request.x/y/w/h` mean

## World And UI Are The Same Kind Of Request

World rendering is not a different conceptual API.

UI and world should both end up as:

- image
- destination rect
- policy

The difference is only which domain-owned scale transforms logical units into
final screen output:

- UI uses display/UI scale
- world uses zoom/city scale

Important world zoom note for this codebase:

- the player-facing zoom percentage is inverse-browser style
- `50%` means zoomed in and therefore larger logical on-screen size
- `200%` means zoomed out and therefore smaller logical on-screen size

This lets us later queue both kinds of commands through the renderer without
teaching widgets or city code different placement contracts.

## Legacy Adapter Rules

Legacy APIs may still accept historical parameters like:

- source-pixel-oriented `x`, `y`
- implicit `scale`
- `draw_image_advanced(...)`

Those legacy functions are adapters. Their only job is:

- convert legacy inputs into the coherent request contract
- do so centrally
- never leak conversion burden into widget classes or callers

Therefore:

- UI classes must not "pre-correct" renderer-space coordinates
- city/world code must not know renderer internals
- hacks like pre-dividing coordinates in widget code are forbidden

## Renderer Rules

Inside the renderer:

- destination placement must be computed from destination-space request geometry
- source scaling must affect sampling and output size, not reinterpret the
  coordinate space of `x` and `y`
- image offsets stored in source pixels may be converted to logical-space
  offsets using the source-to-destination ratio

Allowed:

- converting source pixel offsets into logical destination offsets
- choosing nearest vs linear filter from policy and domain
- domain-specific final output scale

Not allowed:

- treating `request.x/y` as source-scaled coordinates
- forcing callers to know whether coordinates should be pre-divided
- mixing source-scale math into placement semantics

## UI Layer Responsibilities

UI code is layered as:

- primitives
- widgets
- runtime facade
- legacy C wrappers

Primitives:

- own low-level composition semantics
- examples: sprite, tiled strip, panel, border, slider, saved region

Widgets:

- own control-specific composition
- examples: bordered button, image button, label, scrollbar dot, panel widget

Runtime facade:

- narrow object construction / orchestration seam
- C-callable wrapper boundary

Legacy wrappers:

- preserve old call sites while routing into runtime/widgets/primitives

## Compact Controls Versus Large Frames

One of the active bug sources is assuming that every bordered control is just a
smaller version of a large tiled frame.

This vertical slice distinguishes at least two visual contracts:

- large tiled frames
  dialogs, big bordered panels
- compact framed controls
  tabs, checkbox boxes, select rows, footer buttons, one-row controls

These should not silently share one tiling assumption if the art is not
actually authored the same way.

If compact controls still glitch, split them further rather than layering more
conditionals into one monolithic border routine.

Current implementation direction:

- `UiBorderPrimitive`
  remains the large or multi-row frame primitive
- one-row controls may use a dedicated strip-style frame primitive instead of
  reusing the large-frame tiler
- compact one-row controls should stretch their middle strip segments
  horizontally rather than visibly repeating seamful edge tiles across the full
  control width
- config tabs, selects, checkbox frames, category highlights, and footer
  buttons are the first deliberate users of that split

## Saved Region And Offscreen Rules

Snapshot-style redraw and first-time offscreen texture creation are part of the
same contract problem. The following rules now apply:

- any renderer operation that temporarily changes render target, viewport,
  clipping, blend mode, or output scale must save and restore the full renderer
  state
- offscreen texture creation must render at neutral target scale (`1x`) and
  only then restore the previous render domain and SDL scale
- first-use helpers like silhouette generation or footprint-blend texture
  creation are not allowed to hand-roll partial restore logic
- underlying-window redraw must be wrapped in a full renderer-state push/pop,
  not rebuilt by scattered cleanup calls after the fact
- any saved-region capture must happen in the domain that matches the content
  being captured
  - UI strips captured for dialogs/tabs use UI domain
  - city/empire/world snapshots use pixel domain if the source content is world
    scaled

## Figure Preview Rule

Figure previews embedded in UI windows are a mixed-domain flow:

- render the city preview in pixel domain
- capture the preview from pixel-domain screen content
- restore UI domain before drawing the preview into the window chrome

That conversion belongs at the preview-preparation seam, not in the generic
saved-region primitive.

## Text Rules For This Slice

Text remains on the legacy subsystem for now, but new code must respect:

- text measurement is owned by the text/font subsystem
- widget code should not assume one byte equals one glyph
- repeated `n/m` or `n x m` counters should use shared measured helpers such
  as `text_draw_number_pair` instead of hand-tuned negative offsets
- future UTF work should happen at C++ boundaries first, likely with
  `std::string` / `std::string_view`

This slice may touch text placement if needed to restore correct rendering, but
it must not deepen coupling between widgets and legacy byte-level assumptions.

## Debugging Checklist

When a rendering bug appears, check responsibilities in this order:

1. Is the request geometry already coherent?
   `x`, `y`, `logical_width`, `logical_height`
2. Is the legacy adapter converting old parameters correctly?
3. Is the renderer reinterpreting destination coordinates as source-scale data?
4. Is the primitive using the right visual contract?
   sprite vs strip vs border vs panel vs slider
5. Is the render domain correct?
   UI vs pixel vs tooltip vs snapshot
6. Is the problem actually text layout rather than frame geometry?

## Current Known Risks

- compact bordered controls may still need to be split into more than one
  primitive family
- top-bar text buttons may expose text baseline/label-size assumptions that are
  separate from frame drawing
- zoom regressions are the clearest sign that old world adapters and explicit
  request semantics are still partially mixed

## Immediate Standard For New Changes

Before changing code in this slice, check the change against these rules:

- one coherent destination-rect contract
- adapters convert legacy inputs centrally
- widgets do not compensate for renderer internals
- renderer owns final sampling and domain scale
- do not add queue/depth/z behavior in the UI layer

If a proposed fix violates one of those rules, stop and redesign the seam
instead of adding another local exception.

## Pinned Debt For The Next Cleanup Pass

The current checkpoint work still contains intentional containment fixes around
renderer state restoration. They are acceptable only as temporary debt.

Important pinned debt:

- add a renderer-side scoped domain/target helper
  - callers should enter a UI, world, tooltip, snapshot, or offscreen target
    scope explicitly
  - the helper should restore the full renderer state automatically
  - callers should not manually bounce scale/domain state around
- make render-domain state private enough that accidental direct mutation is
  hard or impossible
  - prefer getters/setters or scoped helpers over open mutation
  - if that breaks many call sites, that is useful signal about where hidden
    coupling still exists
- UI scale should be globally owned by config / reload flow only
- city/world zoom should be globally owned by zoom/game-state flow only
- neither system should rely on an ambient shared "current scale" that can
  drift between unrelated passes

## Config Menu Composition Note

The config menu tab row is not the same primitive contract as a generic small
framed control.

For this slice, treat the config tabs as:

- top edge
- left edge
- right edge
- open bottom

The open bottom is not "missing art". It is resolved by restoring the parchment
background strip that exists behind the selected tab before the main dialog
frame border is drawn.

# Augustus Manual Sync Ledger (`2026-04-21`)

This ledger tracks the current hand-port batch from upstream `Augustus` into the Vespasian fork. It is organized by upstream commit and disposition so later sessions can continue the sync without losing the fork-aware rationale.

## Applied In This Working Tree

| Upstream commit | Disposition | Status | Notes |
| --- | --- | --- | --- |
| `1d295aa` | `hand-port directly` | applied | Cyrillic city name import/export fixes. |
| `7b648e034` | `hand-port directly` | applied | City ambient sound additions. |
| `d121dcc2` | `hand-port directly` | applied | Button focus alignment fixes in current window seams. |
| `a0070a801` | `hand-port directly` | applied | Hotkey config now resolves pref-dir load/save correctly. |
| `db4258306` | `hand-port directly` | applied | Empire object safety fix. |
| `b17c979e7` | `hand-port directly` | applied | Empire object size fix. |
| `7e3fb0438` | `hand-port directly` | applied | Native attack loop fix adapted without shared-building assumptions. |
| `194495651` | `hand-port directly` | applied | First dropdown fix. |
| `298add75c` | `hand-port directly` | applied | Second dropdown fix. |
| `51420f2f2` | `adapt to fork seam` | applied | Risks overlay/main-building seam fixed through current `building_main` usage. |
| `bc9ae3ec3` | `adapt to fork seam` | applied | Tax-income overlay tooltip now ignores non-house tiles. |
| `04e63e972` | `adapt to fork seam` | applied | Efficiency, desirability, and sentiment tooltips now include numeric values. |
| `93db4e14d` | `adapt to fork seam` | applied | Desirability overlay color thresholds updated. |
| `9af45f9ff` | `adapt to fork seam` | applied | Depot shows on problems overlay when no instructions are set. |
| `ae6c183fe` | `adapt to fork seam` | applied | Depot carts reroute and return cargo instead of dumping on instruction changes; review follow-up fixes now cover cleared-order and recall edge cases. |
| `7df628402` | `adapt to fork seam` | applied | Granary/warehouse walker-default config added to the current config UI and building defaults. |
| `1bedb8599` | `hand-port directly` | applied | Emptying granary/warehouse request dispatch handling fixed. |
| `65ed3f7d1` | `hand-port directly` | applied | Wrong rubble names fixed through the current localization path. |
| `f6141c8b9` | `hand-port directly` | applied | Worst-desirability naming fix applied. |
| `4fe4938e9` | `localization/data-only` | applied | French payload now includes the sync-added depot, warning, tooltip, and storage-walker config keys in `Mods/Augustus/Localization/*.json`. |
| `105c02e70` | `adapt to fork seam` | applied | Reservoir-over-aqueduct handling updated in the current construction and pathing chokepoints. |
| `5a9a8c6f6` | `adapt to fork seam` | applied | Roads/highways under aqueducts enabled in current routing and ghost seams. |

## Applied As Supporting Parity Work

| Upstream commit line | Status | Notes |
| --- | --- | --- |
| `c504c8e92` / `32a2ffbfe` | partial | Water-range asset split and ghost consistency landed in the current asset and ghost seams. Full overlay-layer parity still needs visual playtesting. |
| `29c84bf53` | already present/refined | Fountain and well range rendering was already present in the fork and kept aligned with the current ghost path. |
| `6666a6139` / `773b5295a` / `ef6b172c3` / `23a7f370c` | partial | Only the safe fork-local behavior and wording pieces were applied; the broader upstream overlay draw refactor was intentionally not replayed. |

## Still Deferred Or Not Imported

| Upstream commit | Disposition | Notes |
| --- | --- | --- |
| `b51b7e33c` | `adapt to fork seam` | Water overlay tile-shape parity still needs in-engine visual verification before widening the overlay draw changes. |
| `dd5241229` | `adapt to fork seam` | Large overlay refactor not replayed into Vespasian’s diverged overlay seams. |
| `165b7c2a3` | `adapt to fork seam` | Native overlay visual parity not specifically audited in-engine yet. |
| `8f5a5f736` | `adapt to fork seam` | Upstream overlay-routing refactor not replayed; the fork still uses its existing ghost and overlay chokepoints. |
| `848b9d52a` | `adapt to fork seam` | Only the clearly compatible water and building-placement pieces were taken. |
| `8251ca91f` | `adapt to fork seam` | Hippodrome visual-only follow-up still needs manual overlay review. |
| `c122ab18f` | `adapt to fork seam` | Broader inconsistent building-name audit remains open. |
| `ddfcde631` | `asset-only` | Wild boar asset payload not imported yet. |
| `5b2a592d3` | `defer to later stage` | Shared buildings remains out of scope for this sync batch. |
| `6c4c82c30` | `defer to later stage` | City drawing refactor deferred. |
| `e69bfb98f` | `defer to later stage` | Overlay refactor completion deferred. |
| `83b3c57e7` | `defer to later stage` | Overlay refactor follow-up deferred. |

## Validation To Run Later

- Full build once you want compilation validation.
- Water overlay and building-ghost visual sweep: reservoir, fountain, well, bathhouse, concrete maker, Neptune module.
- Depot regression sweep: clear source/destination, recall all carts, change instructions mid-route, change resource while carts are out.
- Storage dispatch sweep: granary/warehouse emptying and Caesar request behavior.
- Pathing sweep: roads and highways under aqueducts and reservoir placement over aqueduct footprints.

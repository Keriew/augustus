# ProductionMethod XML

The loader reads every `*.xml` file in this folder at startup. Keep templates and notes in non-`.xml` files so they do not get loaded as live data.

Registry keys are the file path relative to this folder, without the `.xml` suffix.

Example:

- `Mods\Augustus\ProductionMethod\pottery_workshop_basic.xml` is referenced as `pottery_workshop_basic`

Current supported nodes:

- `<kind value="farm|workshop" />`
- `<output resource="..." />`
- `<batch_size value="N" />`
- `<climate_bonuses>` containing `<bonus climate="central|northern|desert" percent="N" />`
- `<input resource="..." amount="N" />`

Rules:

- `<kind>` is required
- `<output>` is required
- `<batch_size>` is optional and defaults to `1`
- `<climate_bonuses>` is optional
- `<input>` is optional and may appear more than once
- `amount` is authored per one legacy one-load batch
- input costs scale by `batch_size`
- `batch_size` changes delivery grouping, not monthly throughput

Native timing uses the legacy throughput formula:

- `max_progress = 100 * GAME_TIME_DAYS_PER_MONTH * 2 * laborers / effective_resource_production_per_month`
- native production then multiplies that max progress by `batch_size`
- a completed native output cart carries `batch_size` loads

Climate bonuses adjust the base output resource throughput for native production only.

Notes:

- Augustus and Vespasian both currently run at `50` ticks per in-game day
- the production formula depends on `GAME_TIME_DAYS_PER_MONTH`, not on a hardcoded mod-specific tick rate

Current native vertical slice:

- `farm` drives native farm progress, blessing/curse handling, and crop image refresh
- `workshop` drives native raw-material checks, consumption, and progress tracking

# ProductionMethod XML

The loader reads every `*.xml` file in this folder at startup. Keep templates and notes in non-`.xml` files so they do not get loaded as live data.

Registry keys are the file path relative to this folder, without the `.xml` suffix.

Example:

- `Mods\Vespasian\ProductionMethod\pottery_workshop_basic.xml` is referenced as `pottery_workshop_basic`

Current supported nodes:

- `<kind value="farm|workshop" />`
- `<output resource="..." />`
- `<input resource="..." amount="N" />`

Rules:

- `<kind>` is required
- `<output>` is required
- `<input>` is optional and may appear more than once
- `amount` is in raw resource units

Current native vertical slice:

- `farm` drives native farm progress, blessing/curse handling, and crop image refresh
- `workshop` drives native raw-material checks, consumption, and progress tracking

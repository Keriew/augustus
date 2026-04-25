# FigureType XML

The loader reads every `*.xml` file in the winning `FigureType` folders using this precedence:

1. active mod
2. `Mods\Augustus\FigureType`
3. `Mods\Julius\FigureType`

Later fallback definitions are allowed, but the first resolved figure type wins.
Templates/examples belong in non-`.xml` files so they do not load as live data.

Current supported nodes:

- `<figure type="...">`
- `<native class="..." />`
- `<owner ... />`
- `<movement ... />`
- `<pathing ... />`
- `<graphics ... />`

Current supported figure ids:

- `labor_seeker`
- `engineer`
- `prefect`
- `teacher`
- `librarian`
- `barber`
- `bathhouse_worker`
- `school_child`

Current supported native classes:

- `roaming_service`
- `engineer_service`
- `prefect_service`

Current supported `<owner>` attributes:

- `slot="none|primary|secondary|quaternary"`
- `building="any|school|..."` using the building attr names
- `state="any|in_use|in_use_or_mothballed"`

Current supported `<movement>` attributes:

- `terrain_usage="any|roads|roads_highway|prefer_roads|prefer_roads_highway"`
- `roam_ticks="N"`
- `max_roam_length="N"`
- `return_mode="return_to_owner_road|die_at_limit"`

Current supported `<pathing>` attributes:

- `mode="vanilla_roaming|smart_service"`
- `effect="damage_risk|fire_risk|barber|bathhouse|school|academy|library|labor"`

Current supported `<graphics>` attributes:

- `image_group="labor_seeker|engineer|prefect|teacher_librarian|barber|bathhouse_worker|school_child"`
- `max_image_offset="N"`

Current engine behavior:

- FigureType v1 is native-subset only; all other figure types stay on legacy behavior.
- The runtime writes state back into the legacy `figure` struct.
- If a native figure hits an unsupported action state, the runtime can decline and the legacy action path will still run.
- `vanilla_roaming` preserves the existing random roaming policy.
- `smart_service` chooses the least recently serviced road branch at intersections and requires road movement plus an effect.
- Smart service visit history is pathing telemetry only; building coverage and risk resets still use the normal service callbacks.
- Road service history is saved separately from figures. Old saves start with zeroed history and a crash-context warning.

Related implementation notes:

- `docs/walker_pathing_runtime.md` explains runtime flow, save compatibility, and effect-id rules.
- `codex_augustus_repo_map_memory.md` indexes the native walker chokepoints for future sessions.

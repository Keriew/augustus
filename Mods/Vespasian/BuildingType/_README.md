# BuildingType XML

The loader reads every `*.xml` file in this folder at startup. Keep templates/examples in non-`.xml` files so they do not get loaded as live data.

Templates and examples are maintained only in `Mods\Vespasian\BuildingType`.
`Mods\Augustus\BuildingType` keeps live XML data only.

Current supported nodes:

- `<graphics> ... </graphics>`
- `<labor labor_seeker_mode="..." labor_min_houses="N" />`
- `<spawn_group ...>`
- `<spawn ... />`

Current supported `<graphics>` child nodes:

- `<water_access mode="reservoir_range" />`
- Legacy flat mode:
  - `<path value="Health_Culture\Theatre" />`
  - `<image value="Image_0000" />`
  - `<upgrade threshold="N" operator="gt|gte" />`
- Structured mode:
  - `<default> ... </default>`
  - `<variant> ... </variant>`
  - `<condition type="working" />`
  - `<condition type="desirability" operator="gt|gte" threshold="N" />`

`<path value="...">` rules:

- path is relative to the winning `Graphics` folder
- use backslash-delimited logical keys
- do not include the `Graphics\` prefix
- do not include the `.xml` suffix
- example: `Mods\Augustus\Graphics\Health_Culture\Theatre.xml` becomes `Health_Culture\Theatre`

Structured `<graphics>` rules:

- `<default>` is required when using structured mode
- `<variant>` entries are checked in XML order
- all `<condition>` nodes inside one `<variant>` must match
- the first matching variant wins
- the `<default>` target is used when no variant matches
- do not mix legacy root `<path>/<image>/<upgrade>` nodes with structured `<default>/<variant>` nodes
- `<image value="..."/>` is optional inside `<default>` or `<variant>` when a graphics group needs a specific named image

Current supported graphics conditions:

- `type="working"` means `num_workers > 0 && has_water_access`
- `type="desirability" operator="gt|gte" threshold="N"` compares the building desirability against `N`

Current supported `<labor>` attributes:

- `labor_seeker_mode="none|spawn_if_below|generate_if_below"`
- `labor_min_houses="N"`

Current supported `<spawn_group>` attributes:

- `road_access="normal"`
- `delay_bands="100:3,75:7,50:15,25:29,1:44"` as a comma-separated list of `worker_percentage:delay` pairs
- `existing_figure="actor|barber|bathhouse_worker|doctor|gladiator|librarian|lion_tamer|school_child|surgeon|tax_collector|teacher|work_camp_architect|work_camp_worker"`
- `guard_timing="before_road_access|after_labor_seeker"`

`delay_bands` sanity rules:

- worker percentage must be an integer from `1` to `100`
- delay must be a positive integer
- pairs must be written in strictly descending worker-percentage order

Current supported `<spawn>` mode:

- `mode="service_roamer"`

Current supported `<spawn>` attributes:

- `spawn_figure="..."` using the same identifiers
- `action_state="roaming|tax_collector_created|entertainer_roaming|entertainer_school_created|work_camp_worker_created|work_camp_architect_created"`
- `direction="top|bottom"`
- `figure_slot="primary|secondary|quaternary|none"`
- `spawn_count="N"` for one policy spawning the same figure several times on one trigger
- `init_roaming="true|false"`
- `graphic_timing="none|on_spawn_entry|before_delay_check|before_successful_spawn"`
- `require_water_access="true|false"`
- `mark_problem_if_no_water="true|false"`
- `condition="always|days1_positive|days1_not_positive|days2_positive|days1_or_days2_positive"`
- `block_on_success="true|false"`

Current engine behavior:

- Only the migrated building families use `<spawn>` so far.
- The old `<graphic .../>` node is no longer part of the supported format; migrate to `<graphics>...</graphics>`.
- A `spawn_group` owns the shared delay/guard phase, then runs its child `<spawn>` policies in order.
- Delay evaluation now uses the explicit `delay_bands` data from XML rather than a hardcoded named profile.
- Ordered policies can coordinate: a policy that succeeds with `block_on_success="true"` stops later sibling policies in the same group.
- Use `block_on_success="true"` when a building should spawn either A or B on the same trigger.
- Use `spawn_count="N"` when one successful policy should create several copies of the same figure at once.
- Today a multi-spawn policy only writes one legacy tracked figure slot; extra spawned figures still exist, but they are not separately tracked by XML-defined slots yet.
- Use `<image value="..."/>` only when a graphics group contains several named members and the building must lock to one of them, such as grouped statue assets.
- Use structured `<graphics>` when one building can swap between several whole graphics groups based on runtime state, like the bathhouse.
- Buildings with a runtime `BuildingType` and a non-empty graphics path now try the new image-group payload path during live city rendering and fall back to legacy rendering when content is missing or unsupported in this phase.
- The current implementation is building-side only. Future work is expected to introduce startup-loaded `FigureType` definitions and runtime `FigureInstance` wrappers while keeping save-compatible C structs underneath.

See `_template.xml.example` here in `Mods\Vespasian\BuildingType` for a copy/paste starter.

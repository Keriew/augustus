# BuildingType XML

The loader reads every `*.xml` file in this folder at startup. Keep templates/examples in non-`.xml` files so they do not get loaded as live data.

Templates and examples are maintained only in `Mods\Vespasian\BuildingType`.
`Mods\Augustus\BuildingType` keeps live XML data only.

Current supported nodes:

- `<state> ... </state>`
- `<graphics> ... </graphics>`
- `<labor> ... </labor>`
- `<spawn_group ...>`
- `<spawn ... />`

Current supported `<state>` child nodes:

- `<water_access mode="reservoir_range" />`

Current supported `<graphics>` child nodes:

- `<default> ... </default>`
- `<variant> ... </variant>`
- `<condition type="has_workers" />`
- `<condition type="water_access" />`
- `<condition type="figure_slot_occupied" slot="primary|secondary|quaternary" />`
- `<condition type="resource_positive" resource="wine" />`
- `<condition type="desirability" operator="lt|lte|eq|gt|gte" threshold="N" />`

`<path value="...">` rules:

- path is relative to the winning `Graphics` folder
- use backslash-delimited logical keys
- do not include the `Graphics\` prefix
- do not include the `.xml` suffix
- example: `Mods\Augustus\Graphics\Health_Culture\Theatre.xml` becomes `Health_Culture\Theatre`

Structured `<graphics>` rules:

- `<default>` is required
- `<variant>` entries are checked in XML order
- all `<condition>` nodes inside one `<variant>` must match
- the first matching variant wins
- the `<default>` target is used when no variant matches
- `<path>` and optional `<image>` must live inside `<default>` or `<variant>`
- put water refresh rules under `<state>`, not under `<graphics>`

Current supported graphics conditions:

- `type="has_workers"` means `num_workers > 0`
- `type="water_access"` means `has_water_access`
- `type="figure_slot_occupied" slot="primary|secondary|quaternary"` means the named tracked legacy figure slot is occupied
- `type="resource_positive" resource="wine"` means the building has at least one unit of that resource
- `type="desirability" operator="lt|lte|eq|gt|gte" threshold="N"` compares the building desirability against `N`

Current supported `<labor>` child nodes:

- `<employees count="N" />`
- `<seeker mode="none|spawn_if_below|generate_if_below" min_houses="N" />`

Current supported `<spawn_group>` attributes:

- `road_access="normal"`
- `delay_bands="100:3,75:7,50:15,25:29,1:44"` as a comma-separated list of `worker_percentage:delay` pairs
- `existing_figure="actor|barber|bathhouse_worker|doctor|engineer|gladiator|librarian|lion_tamer|prefect|school_child|surgeon|tax_collector|teacher|work_camp_architect|work_camp_worker"`
- `guard_timing="before_road_access|after_labor_seeker"`

`delay_bands` sanity rules:

- worker percentage must be an integer from `1` to `100`
- delay must be a non-negative integer
- pairs must be written in strictly descending worker-percentage order

Current supported `<spawn>` mode:

- `mode="service_roamer"`

Current supported `<spawn>` attributes:

- `spawn_figure="..."` using the same identifiers
- `action_state="roaming|engineer_created|prefect_created|tax_collector_created|entertainer_roaming|entertainer_school_created|work_camp_worker_created|work_camp_architect_created"`
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

- Repo-owned BuildingType graphics use only the structured `<graphics>` schema.
- A `spawn_group` owns the shared delay/guard phase, then runs its child `<spawn>` policies in order.
- Delay evaluation now uses the explicit `delay_bands` data from XML rather than a hardcoded named profile.
- Ordered policies can coordinate: a policy that succeeds with `block_on_success="true"` stops later sibling policies in the same group.
- Use `block_on_success="true"` when a building should spawn either A or B on the same trigger.
- Use `spawn_count="N"` when one successful policy should create several copies of the same figure at once.
- Today a multi-spawn policy only writes one legacy tracked figure slot; extra spawned figures still exist, but they are not separately tracked by XML-defined slots yet.
- Use `<image value="..."/>` only when a graphics group contains several named members and the building must lock to one of them.
- Graphics-only vertical-slice definitions are valid; current `wheat_farm` and `pottery_workshop` XMLs intentionally author graphics metadata before those live industry render paths move to runtime ownership.
- Put shared derived state such as water access under `<state>` so graphics and spawn behavior read the same runtime facts.
- Put BuildingType-authored employee defaults under `<labor><employees ... /></labor>` so the XML and live model table stay in sync.
- Buildings with a validated runtime `BuildingType` graphics block usually use the native runtime renderer path as the authoritative live path; current data-only vertical slices remain on legacy live rendering until their runtime rollout lands.

See `_template.xml.example` here in `Mods\Vespasian\BuildingType` for a copy/paste starter.

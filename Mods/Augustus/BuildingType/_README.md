# BuildingType XML

The loader reads every `*.xml` file in this folder at startup. Keep templates/examples in non-`.xml` files so they do not get loaded as live data.

Current supported nodes:

- `<graphic threshold="N" operator="gt|gte" [water_access="reservoir_range"] />`
- `<spawn ... />`

Current supported `<spawn>` mode:

- `mode="service_roamer"`

Current supported `<spawn>` attributes:

- `road_access="normal"`
- `labor_seeker_mode="none|spawn_if_below|generate_if_below"`
- `labor_min_houses="N"`
- `delay_profile="default"`
- `existing_figure="actor|barber|bathhouse_worker|doctor|gladiator|librarian|school_child|surgeon|teacher|work_camp_architect|work_camp_worker"`
- `spawn_figure="..."` using the same identifiers
- `action_state="roaming|entertainer_roaming|entertainer_school_created|work_camp_worker_created|work_camp_architect_created"`
- `direction="top|bottom"`
- `figure_slot="primary|secondary|quaternary|none"`
- `guard_timing="before_road_access|after_labor_seeker"`
- `init_roaming="true|false"`
- `graphic_timing="none|before_delay_check|before_successful_spawn"`
- `require_water_access="true|false"`
- `mark_problem_if_no_water="true|false"`

Current engine behavior:

- Only the migrated building families use `<spawn>` so far.
- The current implementation is building-side only. Future work is expected to introduce startup-loaded `FigureType` definitions and runtime `FigureInstance` wrappers while keeping save-compatible C structs underneath.

See `_template.xml.example` for a copy/paste starter.

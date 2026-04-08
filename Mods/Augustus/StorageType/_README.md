# StorageType XML

The loader reads every `*.xml` file in this folder at startup. Keep templates and notes in non-`.xml` files so they do not get treated as live data.

Registry keys are the file path relative to this folder, without the `.xml` suffix.

Example:

- `Mods\Augustus\StorageType\clay_input_pottery_workshop.xml` is referenced as `clay_input_pottery_workshop`

Prefer descriptive names that include both the resource and the owning building context.

Current supported nodes:

- `<accepts resource="..." />`
- `<capacity amount="N" />`

Rules:

- `<accepts>` may appear one or more times
- `resource` must use the existing resource xml names
- `<capacity>` is optional
- `amount` is stored as raw resource units

The current native vertical slice uses StorageType as shared authored metadata for native building-owned storage slots.

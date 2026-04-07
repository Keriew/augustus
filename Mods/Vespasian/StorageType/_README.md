# StorageType XML

The loader reads every `*.xml` file in this folder at startup. Keep templates and notes in non-`.xml` files so they do not get treated as live data.

Registry keys are the file path relative to this folder, without the `.xml` suffix.

Example:

- `Mods\Vespasian\StorageType\pottery_input.xml` is referenced as `pottery_input`

Current supported nodes:

- `<accepts resource="..." />`
- `<capacity amount="N" />`

Rules:

- `<accepts>` may appear one or more times
- `resource` must use the existing resource xml names
- `<capacity>` is optional
- `amount` is stored as raw resource units

The current vertical slice uses StorageType as shared authored metadata for native building-owned storage slots.

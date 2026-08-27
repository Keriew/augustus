# Custom campaign message localization

Custom campaigns may provide optional language overlays for their custom messages. The original scenario remains the canonical source and is not modified when an overlay is loaded.

## Package structure

For a scenario named `RC01 Ostia.mapx` and a selected language directory named `pt-br`, Augustus looks for:

```text
localization/pt-br/messages/RC01 Ostia.xml
```

For language directories containing a two-letter region, Augustus also tries the BCP 47-style capitalization. For example, `pt-br` also tries:

```text
localization/pt-BR/messages/RC01 Ostia.xml
```

This works for both unpacked campaign directories and `.campaign` packages.

## File format

```xml
<?xml version="1.0" encoding="UTF-8"?>
<localization version="1" language="pt-BR">
    <message uid="Drought">
        <title>Rei solicita trigo</title>
        <subtitle>Seca no Vale do Tibre</subtitle>
        <text>Governador, as colheitas fracassaram...</text>
    </message>
</localization>
```

The `language` attribute is descriptive. The selected language directory and the file path determine which overlay is loaded.

Only `title`, `subtitle`, and `text` are accepted. Each message is matched through the existing custom message `uid`. Media, background music, events, triggers, and scenario data remain in the canonical scenario.

All fields are optional. A missing or empty field falls back to the source message. An unknown UID is ignored and logged. A malformed file, unsupported version, missing UID, or duplicate UID rejects the complete overlay and falls back to the source messages.

The overlay is held separately in memory. It is not serialized into save games and is not used when exporting the canonical custom messages from the editor.

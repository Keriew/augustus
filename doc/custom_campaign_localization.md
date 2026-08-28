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

The selected directory is tried first, followed by its normalized locale form. A campaign may also provide `localization/locales.xml` to map distribution-specific language directory names and to select a regional locale when the base game language is installed as the default and no language directory is selected:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<locales version="1">
    <locale id="pt-BR" aliases="pt-br|pt_BR|portuguese" default-for="pt"/>
</locales>
```

`default-for` uses the detected base-game language tag. This explicit mapping is necessary because the original game identifies Portuguese but does not distinguish Brazilian from European Portuguese. If the manifest is absent, Augustus tries a language-neutral directory such as `localization/pt/` for a default Portuguese installation. An explicit selection such as `pt-PT` is never redirected through `default-for`.

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

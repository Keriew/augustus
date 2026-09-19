# Custom campaign text localization

Custom campaigns may provide optional language overlays for their custom messages and campaign/mission display metadata. Original campaign and scenario files remain canonical and are not modified when an overlay is loaded.

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
Campaign scenarios stored as either editable `.mapx` files or packaged `.svx` files are supported.

The selected directory is tried first, followed by its normalized locale form. A campaign may also provide `localization/locales.xml` to map distribution-specific language directory names and to select a regional locale when the base game language is installed as the default and no language directory is selected:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<locales version="1">
    <locale id="pt-BR" aliases="pt-br|pt_BR|portuguese" default-for="pt"/>
</locales>
```

`default-for` uses the detected base-game language tag. This explicit mapping is necessary because the original game identifies Portuguese but does not distinguish Brazilian from European Portuguese. If the manifest is absent, Augustus tries a language-neutral directory such as `localization/pt/` for a default Portuguese installation. An explicit selection such as `pt-PT` is never redirected through `default-for`.

## Message file format

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

## Campaign and mission display metadata

An optional `localization/<locale>/campaign.xml` translates campaign names and
descriptions, mission titles, and scenario names/descriptions. It uses the same
locale resolution as messages, but does not require a message overlay. Both
unpacked directories and `.campaign` packages are supported.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<campaign_localization version="1" language="fr">
    <name>Campagne de démonstration</name>
    <description>Description de la campagne.</description>
    <mission first-scenario="Mission01">
        <title>Première mission</title>
    </mission>
    <scenario file="Mission01">
        <name>Premier scénario</name>
        <description>Description du scénario.</description>
    </scenario>
</campaign_localization>
```

A copy is available as [a complete metadata overlay example](examples/custom_campaign_metadata_localization.xml).
The `language` attribute is descriptive; the selected directory determines the
locale. Text is UTF-8 and converted using the game's current encoding. This
feature does not add font glyphs.

`file` and `first-scenario` exactly match the canonical scenario filename without
its directory or final extension. Matching is case-sensitive: `Mission01.mapx`
and `Mission01.svx` share the key `Mission01`. A mission is identified by its first
scenario, not its position or translated title. Reordering missions keeps the
match; renaming a scenario or changing the first scenario requires updating the
overlay. Duplicate scenario stems are ambiguous and never select the first match.

Missing or empty fields fall back independently to canonical text. Unknown keys
are logged and ignored. Duplicate keys/fields, ambiguous matches, invalid
structure and unsupported versions reject the complete metadata overlay. This
does not reject the campaign or its separate message overlay.

Only presentation getters use translated text. Campaign filenames, scenario
paths, authors, ranks, progression, save names and serialized data remain
canonical. Campaign and mission screens use these getters; long titles are
ellipsized to their available width. Description layout is otherwise unchanged.

The campaign picker's left list also uses localized campaign names. Names are
read into a screen-owned cache when opening the picker, without selecting each
campaign or modifying its progress. Drawing and tooltips do not read files.
Missing/invalid translations retain the original directory/package label, and
selecting an entry still uses its unchanged directory/package name.

Metadata is reloaded after successful language changes and campaign restoration,
hidden while the campaign is suspended, and freed on campaign clear. UI code must
not retain translated pointers across reloads. The editor keeps canonical data.

Developer regression tests: [campaign localization tests](../tests/campaign_localization/README.md).

## Scope

This feature translates text only. Localized audio, images, video, custom empire
city names and font rendering are separate features. Campaigns without
localization files retain their existing behavior.

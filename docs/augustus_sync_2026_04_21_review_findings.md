# Augustus Sync Review Findings (`2026-04-21`)

This note records the three review findings identified against the manual sync batch documented in [augustus_sync_2026_04_21_ledger.md](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/docs/augustus_sync_2026_04_21_ledger.md:1).

Relevant ledger context:
- The depot behavior import is documented as applied in [augustus_sync_2026_04_21_ledger.md](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/docs/augustus_sync_2026_04_21_ledger.md:23).
- The granary/warehouse walker-default config import is documented as applied in [augustus_sync_2026_04_21_ledger.md](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/docs/augustus_sync_2026_04_21_ledger.md:24).
- The French localization work is documented as only partial in [augustus_sync_2026_04_21_ledger.md](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/docs/augustus_sync_2026_04_21_ledger.md:28).
- The deferred validation list explicitly calls for a depot regression sweep covering clear source/destination, recall all carts, and mid-route instruction changes in [augustus_sync_2026_04_21_ledger.md](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/docs/augustus_sync_2026_04_21_ledger.md:61).

## Finding 1

**[P1] Depot carts can get stranded when instructions are cleared mid-route**

The new clear/recall flow does not uphold the ledger's documented "reroute and return cargo" behavior in all cases.

Primary code evidence:
- Loaded carts in cancel-order state are sent back to `src_storage_id` with no fallback if that id has already been cleared in [depot.c](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/src/figuretype/depot.c:424).
- `set_destination()` is a no-op when `building_id` is 0 in [depot.c](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/src/figuretype/depot.c:142).
- The clear-source and clear-destination UI handlers directly zero out the order ids in [window/building/depot.c](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/src/window/building/depot.c:984) and [window/building/depot.c](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/src/window/building/depot.c:994).
- There are some guarded reroute paths earlier in the depot state machine, but they do not cover the final `FIGURE_ACTION_244_DEPOT_CART_PUSHER_CANCEL_ORDER` fallback when source has already been cleared in [depot.c](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/src/figuretype/depot.c:202) and [depot.c](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/src/figuretype/depot.c:228).

Impact:
- A cart carrying goods can enter `FIGURE_ACTION_244_DEPOT_CART_PUSHER_CANCEL_ORDER` with `src_storage_id == 0`.
- In that state, `set_destination()` never updates the figure, so the cart remains stuck instead of returning cargo or going home.

Related documentation:
- The sync ledger claims this import landed as "Depot carts reroute and return cargo instead of dumping on instruction changes" in [augustus_sync_2026_04_21_ledger.md](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/docs/augustus_sync_2026_04_21_ledger.md:23).
- The validation checklist specifically calls for testing this exact family of cases in [augustus_sync_2026_04_21_ledger.md](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/docs/augustus_sync_2026_04_21_ledger.md:61).

## Finding 2

**[P2] French locale is missing newly added depot/config strings**

This sync added new depot status, warning, tooltip, and storage-walker config keys in English, but the French localization file stops before those keys are added.

Primary code and data evidence:
- The new English depot status strings are present in [en.json](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/Mods/Augustus/Localization/en.json:1411).
- The new English depot warnings are present in [en.json](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/Mods/Augustus/Localization/en.json:1760).
- The new English depot/config keys are present in [en.json](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/Mods/Augustus/Localization/en.json:2059).
- The French file ends before the corresponding config keys are added in [fr.json](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/Mods/Augustus/Localization/fr.json:2046).

Why this is user-facing in this fork:
- Missing project keys fall back to the translation key name, not to English text, in [localization_runtime.cpp](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/src/translation/localization_runtime.cpp:176).
- The fallback implementation explicitly stores the raw key name in [localization_support.cpp](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/src/translation/localization_support.cpp:141).

Impact:
- French users will see literal key ids for the newly added depot/config strings instead of localized text.

Related documentation:
- The ledger already records the French payload import as only partial in [augustus_sync_2026_04_21_ledger.md](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/docs/augustus_sync_2026_04_21_ledger.md:28).

## Finding 3

**[P3] Hidden Recall button is still clickable in basic depot mode**

The extra depot controls are only drawn when `advanced_mode` is enabled, but the mouse handler still registers the hidden Recall button in basic mode.

Primary code evidence:
- The extra depot controls, including the Recall button, are only drawn inside the `if (data.advanced_mode)` block in [window/building/depot.c](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/src/window/building/depot.c:561).
- In basic mode the mouse handler still enables the first 10 buttons, which includes `depot_order_buttons[9]` in [window/building/depot.c](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/src/window/building/depot.c:592).
- The hidden button's callback is `depot_recall_all_cart_pushers()` in [window/building/depot.c](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/src/window/building/depot.c:962).

Impact:
- There is an invisible clickable region in the basic depot UI that can recall all carts unexpectedly.

Related documentation:
- The ledger ties the depot behavior/UI import to the same upstream depot batch in [augustus_sync_2026_04_21_ledger.md](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/docs/augustus_sync_2026_04_21_ledger.md:23).


#ifndef CAMPAIGN_LOCALIZATION_H
#define CAMPAIGN_LOCALIZATION_H

#include <stdint.h>

void campaign_localization_clear(void);

/* Load only after the canonical campaign XML parser has been freed. */
int campaign_localization_load(void);

/* Preview a named campaign without changing the active campaign. Caller frees
 * the returned name; NULL means absent/invalid metadata. No XML parser may be active. */
uint8_t *campaign_localization_preview_name(const char *campaign_name);

/* Return an overlay field, or NULL. These getters never change canonical data. */
const uint8_t *campaign_localization_name(void);
const uint8_t *campaign_localization_description(void);
const uint8_t *campaign_localization_mission_title(int scenario_id);
const uint8_t *campaign_localization_scenario_name(int scenario_id);
const uint8_t *campaign_localization_scenario_description(int scenario_id);

#endif // CAMPAIGN_LOCALIZATION_H

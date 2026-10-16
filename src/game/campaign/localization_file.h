#ifndef CAMPAIGN_LOCALIZATION_FILE_H
#define CAMPAIGN_LOCALIZATION_FILE_H

#include <stddef.h>
#include "game/campaign/file.h"

/**
 * Loads a file relative to the selected campaign locale, such as campaign.xml
 * or messages/Mission01.xml. The caller frees the returned allocation.
 * resolved_language must hold FILE_NAME_MAX bytes and is empty on failure.
 * This may parse locales.xml, so no other XML parser may be active.
 */
void *campaign_localization_load_file(const char *relative_path, char *resolved_language, size_t *size);

/* Same resolution using an independent reader; NULL selects the active campaign. */
void *campaign_localization_load_file_from(campaign_file_reader *reader, const char *relative_path,
    char *resolved_language, size_t *size);

#endif // CAMPAIGN_LOCALIZATION_FILE_H

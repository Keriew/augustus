#include "translation/localization.h"
#include "translation/translation.h"

extern "C" {
#include "core/lang.h"
#include "core/log.h"
}

extern "C" void translation_load(language_type language)
{
    if (!localization::rebuild_legacy_cache(language)) {
        log_error("Invalid translation selected", 0, 0);
        return;
    }
    lang_refresh_message_cache();
}

extern "C" const uint8_t *translation_for(translation_key key)
{
    return localization::legacy_project_string(key);
}

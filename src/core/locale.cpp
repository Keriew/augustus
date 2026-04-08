#include "locale.h"

#include "translation/localization.h"

extern "C" {
#include "core/log.h"
}

static struct {
    language_type last_determined_language;
} data;

static void log_language(void)
{
    const char *desc;
    switch (data.last_determined_language) {
        case LANGUAGE_ENGLISH: desc = "English"; break;
        case LANGUAGE_FRENCH: desc = "French"; break;
        case LANGUAGE_GERMAN: desc = "German"; break;
        case LANGUAGE_GREEK: desc = "Greek"; break;
        case LANGUAGE_ITALIAN: desc = "Italian"; break;
        case LANGUAGE_SPANISH: desc = "Spanish"; break;
        case LANGUAGE_POLISH: desc = "Polish"; break;
        case LANGUAGE_PORTUGUESE: desc = "Portuguese"; break;
        case LANGUAGE_RUSSIAN: desc = "Russian"; break;
        case LANGUAGE_SWEDISH: desc = "Swedish"; break;
        case LANGUAGE_TRADITIONAL_CHINESE: desc = "Traditional Chinese"; break;
        case LANGUAGE_SIMPLIFIED_CHINESE: desc = "Simplified Chinese"; break;
        case LANGUAGE_KOREAN: desc = "Korean"; break;
        case LANGUAGE_JAPANESE: desc = "Japanese"; break;
        case LANGUAGE_CZECH: desc = "Czech"; break;
        case LANGUAGE_UKRAINIAN: desc = "Ukrainian"; break;
        default: desc = "Unknown"; break;
    }
    log_info("Detected language:", desc, 0);
}

language_type locale_determine_language(void)
{
    data.last_determined_language = localization::active_language();
    log_language();
    return data.last_determined_language;
}

language_type locale_last_determined_language(void)
{
    if (!data.last_determined_language) {
        return LANGUAGE_UNKNOWN;
    } else {
        return data.last_determined_language;
    }

}


int locale_year_before_ad(void)
{
    // In all languages it's "200 AD" except for English
    return data.last_determined_language != LANGUAGE_ENGLISH;
}

int locale_translate_money_dn(void)
{
    // In Korean, 'Dn' translate to 'Funds', which makes no sense for
    // constructions costs and other places where Dn is used for money.
    return data.last_determined_language != LANGUAGE_KOREAN;
}

int locale_paragraph_indent(void)
{
    return data.last_determined_language == LANGUAGE_JAPANESE ? 17 : 50;
}

int locale_translate_rank_autosaves(void)
{
    switch (data.last_determined_language) {
        case LANGUAGE_ENGLISH:
        case LANGUAGE_FRENCH:
        case LANGUAGE_GERMAN:
        case LANGUAGE_ITALIAN:
        case LANGUAGE_POLISH:
        case LANGUAGE_PORTUGUESE:
        case LANGUAGE_SPANISH:
        case LANGUAGE_SWEDISH:
        case LANGUAGE_RUSSIAN:
        case LANGUAGE_CZECH:
        case LANGUAGE_UKRAINIAN:
            return 1;

        case LANGUAGE_JAPANESE:
        case LANGUAGE_KOREAN:
        case LANGUAGE_TRADITIONAL_CHINESE: // original adds 01_ prefixes
        case LANGUAGE_SIMPLIFIED_CHINESE:
        default:
            return 0;
    }
}


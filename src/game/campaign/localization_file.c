#include "localization_file.h"

#include "core/config.h"
#include "core/file.h"
#include "core/locale.h"
#include "core/log.h"
#include "core/xml_parser.h"
#include "game/campaign/file.h"

#include <stdlib.h>
#include <string.h>

#define LOCALIZATION_LOCALES_VERSION 1
#define XML_LOCALES_TOTAL_ELEMENTS 2
#define LOCALIZATION_LOCALES_FILE "localization/locales.xml"

static struct {
    const char *requested_language;
    const char *canonical_language;
    const char *detected_language;
    char resolved_language[FILE_NAME_MAX];
    int allow_detected_default;
    int success;
} locale_data;

static int xml_start_locales(void);
static int xml_start_locale(void);

static const xml_parser_element locale_xml_elements[XML_LOCALES_TOTAL_ELEMENTS] = {
    { "locales", xml_start_locales },
    { "locale", xml_start_locale, 0, "locales" },
};

static int strings_equal_case_insensitive(const char *first, const char *second)
{
    if (!first || !second) {
        return 0;
    }
    while (*first && *second) {
        char first_char = *first;
        char second_char = *second;
        if (first_char >= 'A' && first_char <= 'Z') {
            first_char += 'a' - 'A';
        }
        if (second_char >= 'A' && second_char <= 'Z') {
            second_char += 'a' - 'A';
        }
        if (first_char != second_char) {
            return 0;
        }
        first++;
        second++;
    }
    return *first == *second;
}

static int locale_aliases_contain(const char *aliases, const char *language)
{
    if (!aliases || !language) {
        return 0;
    }
    const char *alias = aliases;
    while (*alias) {
        const char *separator = strchr(alias, '|');
        size_t length = separator ? (size_t) (separator - alias) : strlen(alias);
        if (strlen(language) == length) {
            char candidate[FILE_NAME_MAX];
            if (length >= FILE_NAME_MAX) {
                return 0;
            }
            memcpy(candidate, alias, length);
            candidate[length] = 0;
            if (strings_equal_case_insensitive(candidate, language)) {
                return 1;
            }
        }
        alias = separator ? separator + 1 : alias + length;
    }
    return 0;
}

static int is_valid_locale_directory(const char *locale)
{
    if (!locale || !*locale || strlen(locale) >= FILE_NAME_MAX) {
        return 0;
    }
    for (const char *cursor = locale; *cursor; cursor++) {
        if (!((*cursor >= 'a' && *cursor <= 'z') || (*cursor >= 'A' && *cursor <= 'Z') ||
            (*cursor >= '0' && *cursor <= '9') || *cursor == '-' || *cursor == '_')) {
            return 0;
        }
    }
    return 1;
}

static int xml_start_locales(void)
{
    int version = xml_parser_get_attribute_int("version");
    if (version != LOCALIZATION_LOCALES_VERSION) {
        log_error("Unsupported campaign locale manifest version", 0, version);
        locale_data.success = 0;
        return 0;
    }
    return 1;
}

static int is_valid_language_directory(const char *language)
{
    if (!language || !*language || strlen(language) >= FILE_NAME_MAX ||
        strcmp(language, ".") == 0 || strcmp(language, "..") == 0 ||
        language[strlen(language) - 1] == '.' || language[strlen(language) - 1] == ' ' ||
        strpbrk(language, "<>:\"/\\|?*")) {
        return 0;
    }
    for (const unsigned char *cursor = (const unsigned char *) language; *cursor; cursor++) {
        if (*cursor < 32) {
            return 0;
        }
    }
    return 1;
}

static int xml_start_locale(void)
{
    const char *id = xml_parser_get_attribute_string("id");
    const char *aliases = xml_parser_get_attribute_string("aliases");
    const char *default_for = xml_parser_get_attribute_string("default-for");
    int alias_matches = locale_aliases_contain(aliases, locale_data.requested_language) ||
        locale_aliases_contain(aliases, locale_data.canonical_language);
    int default_matches = locale_data.allow_detected_default &&
        strings_equal_case_insensitive(default_for, locale_data.detected_language);
    if (!alias_matches && !default_matches) {
        return 1;
    }
    if (!is_valid_locale_directory(id)) {
        log_error("Invalid locale id in campaign locale manifest", id, 0);
        locale_data.success = 0;
        return 0;
    }
    if (*locale_data.resolved_language &&
        !strings_equal_case_insensitive(locale_data.resolved_language, id)) {
        log_error("Ambiguous locale mapping in campaign locale manifest", id, 0);
        locale_data.success = 0;
        return 0;
    }
    snprintf(locale_data.resolved_language, FILE_NAME_MAX, "%s", id);
    return 1;
}

static void canonicalize_language_directory(const char *language, char *canonical, size_t length)
{
    snprintf(canonical, length, "%s", language);
    char *separator = strchr(canonical, '-');
    if (!separator) {
        separator = strchr(canonical, '_');
    }
    char *language_end = separator ? separator : canonical + strlen(canonical);
    for (char *cursor = canonical; cursor < language_end; cursor++) {
        if (*cursor >= 'A' && *cursor <= 'Z') {
            *cursor += 'a' - 'A';
        }
    }
    if (separator && strlen(separator + 1) == 2) {
        *separator = '-';
        if (separator[1] >= 'a' && separator[1] <= 'z') {
            separator[1] -= 'a' - 'A';
        }
        if (separator[2] >= 'a' && separator[2] <= 'z') {
            separator[2] -= 'a' - 'A';
        }
    }
}

static const char *detected_language_tag(void)
{
    switch (locale_last_determined_language()) {
        case LANGUAGE_ENGLISH: return "en";
        case LANGUAGE_FRENCH: return "fr";
        case LANGUAGE_GERMAN: return "de";
        case LANGUAGE_GREEK: return "el";
        case LANGUAGE_ITALIAN: return "it";
        case LANGUAGE_SPANISH: return "es";
        case LANGUAGE_JAPANESE: return "ja";
        case LANGUAGE_KOREAN: return "ko";
        case LANGUAGE_POLISH: return "pl";
        case LANGUAGE_PORTUGUESE: return "pt";
        case LANGUAGE_RUSSIAN: return "ru";
        case LANGUAGE_SWEDISH: return "sv";
        case LANGUAGE_SIMPLIFIED_CHINESE: return "zh-Hans";
        case LANGUAGE_TRADITIONAL_CHINESE: return "zh-Hant";
        case LANGUAGE_CZECH: return "cs";
        case LANGUAGE_UKRAINIAN: return "uk";
        default: return 0;
    }
}

static void *load_campaign_file(campaign_file_reader *reader, const char *path, size_t *size)
{
    return reader ? campaign_file_reader_load(reader, path, size) : campaign_file_load(path, size);
}

static int resolve_language_from_manifest(campaign_file_reader *reader, const char *requested_language,
    const char *canonical_language, int allow_detected_default, char *resolved_language)
{
    size_t xml_size = 0;
    char *xml_text = load_campaign_file(reader, LOCALIZATION_LOCALES_FILE, &xml_size);
    if (!xml_text) {
        return 0;
    }

    memset(&locale_data, 0, sizeof(locale_data));
    locale_data.requested_language = requested_language;
    locale_data.canonical_language = canonical_language;
    locale_data.detected_language = detected_language_tag();
    locale_data.allow_detected_default = allow_detected_default;
    locale_data.success = 1;
    int parsed = xml_parser_init(locale_xml_elements, XML_LOCALES_TOTAL_ELEMENTS, 1) &&
        xml_parser_parse(xml_text, (unsigned int) xml_size, 1) && locale_data.success;
    xml_parser_free();
    free(xml_text);
    if (!parsed || !*locale_data.resolved_language) {
        return 0;
    }
    snprintf(resolved_language, FILE_NAME_MAX, "%s", locale_data.resolved_language);
    return 1;
}

static void *load_file_for_language(campaign_file_reader *reader, const char *language, const char *relative_path,
    char *resolved_language, size_t *size)
{
    char path[FILE_NAME_MAX];
    int path_size = snprintf(path, FILE_NAME_MAX, "localization/%s/%s", language, relative_path);
    if (path_size < 0 || path_size >= FILE_NAME_MAX) {
        log_error("Campaign localization path is too long", relative_path, 0);
        return 0;
    }
    void *file_data = load_campaign_file(reader, path, size);
    if (file_data) {
        snprintf(resolved_language, FILE_NAME_MAX, "%s", language);
    }
    return file_data;
}

void *campaign_localization_load_file_from(campaign_file_reader *reader, const char *relative_path,
    char *resolved_language, size_t *size)
{
    *resolved_language = 0;
    *size = 0;
    const char *language = config_get_string(CONFIG_STRING_UI_LANGUAGE_DIR);
    if (language && *language && !is_valid_language_directory(language)) {
        log_error("Invalid campaign localization directory", language, 0);
        return 0;
    }

    char canonical_language[FILE_NAME_MAX] = { 0 };
    char mapped_language[FILE_NAME_MAX] = { 0 };
    void *file_data = 0;
    if (language && *language) {
        file_data = load_file_for_language(reader, language, relative_path, resolved_language, size);
        canonicalize_language_directory(language, canonical_language, FILE_NAME_MAX);
        if (!file_data && strcmp(language, canonical_language) != 0) {
            file_data = load_file_for_language(reader, canonical_language, relative_path, resolved_language, size);
        }
        if (!file_data && resolve_language_from_manifest(reader, language, canonical_language, 0, mapped_language)) {
            file_data = load_file_for_language(reader, mapped_language, relative_path, resolved_language, size);
        }
    } else if (resolve_language_from_manifest(reader, 0, 0, 1, mapped_language)) {
        file_data = load_file_for_language(reader, mapped_language, relative_path, resolved_language, size);
    } else {
        const char *detected_language = detected_language_tag();
        if (detected_language) {
            file_data = load_file_for_language(reader, detected_language, relative_path, resolved_language, size);
        }
    }
    return file_data;
}

void *campaign_localization_load_file(const char *relative_path, char *resolved_language, size_t *size)
{
    return campaign_localization_load_file_from(0, relative_path, resolved_language, size);
}

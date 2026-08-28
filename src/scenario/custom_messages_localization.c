#include "custom_messages_localization.h"

#include "core/config.h"
#include "core/encoding.h"
#include "core/file.h"
#include "core/locale.h"
#include "core/log.h"
#include "core/xml_parser.h"
#include "game/campaign.h"
#include "game/campaign/file.h"
#include "scenario/custom_messages.h"
#include "scenario/property.h"

#include <stdlib.h>
#include <string.h>

#define CUSTOM_MESSAGE_LOCALIZATION_VERSION 1
#define XML_TOTAL_ELEMENTS 5
#define XML_LOCALES_TOTAL_ELEMENTS 2
#define LOCALIZATION_LOCALES_FILE "localization/locales.xml"

typedef struct {
    uint8_t *title;
    uint8_t *subtitle;
    uint8_t *text;
} localized_custom_message;

typedef enum {
    LOCALIZED_FIELD_TITLE,
    LOCALIZED_FIELD_SUBTITLE,
    LOCALIZED_FIELD_TEXT,
} localized_custom_message_field;

static struct {
    localized_custom_message *messages;
    uint8_t *seen_messages;
    int message_count;
    int current_message_id;
    int success;
} data;

static struct {
    const char *requested_language;
    const char *canonical_language;
    const char *detected_language;
    char resolved_language[FILE_NAME_MAX];
    int allow_detected_default;
    int success;
} locale_data;

static int xml_start_localization(void);
static int xml_start_message(void);
static void xml_end_message(void);
static void xml_on_title(const char *text);
static void xml_on_subtitle(const char *text);
static void xml_on_text(const char *text);
static int xml_start_locales(void);
static int xml_start_locale(void);

static const xml_parser_element xml_elements[XML_TOTAL_ELEMENTS] = {
    { "localization", xml_start_localization },
    { "message", xml_start_message, xml_end_message, "localization" },
    { "title", 0, 0, "message", xml_on_title },
    { "subtitle", 0, 0, "message", xml_on_subtitle },
    { "text", 0, 0, "message", xml_on_text },
};

static const xml_parser_element locale_xml_elements[XML_LOCALES_TOTAL_ELEMENTS] = {
    { "locales", xml_start_locales },
    { "locale", xml_start_locale, 0, "locales" },
};

void custom_messages_localization_clear(void)
{
    if (data.messages) {
        for (int i = 0; i <= data.message_count; i++) {
            free(data.messages[i].title);
            free(data.messages[i].subtitle);
            free(data.messages[i].text);
        }
        free(data.messages);
    }
    free(data.seen_messages);
    memset(&data, 0, sizeof(data));
}

static uint8_t *copy_encoded_text(const char *text)
{
    if (!text || !*text) {
        return 0;
    }
    size_t length = strlen(text) + 1;
    uint8_t *result = malloc(length);
    if (!result) {
        data.success = 0;
        return 0;
    }
    encoding_from_utf8(text, result, (int) length);
    return result;
}

static int xml_start_localization(void)
{
    int version = xml_parser_get_attribute_int("version");
    if (version != CUSTOM_MESSAGE_LOCALIZATION_VERSION) {
        log_error("Unsupported custom message localization version", 0, version);
        data.success = 0;
        return 0;
    }
    return 1;
}

static int xml_start_message(void)
{
    data.current_message_id = 0;
    if (!xml_parser_has_attribute("uid")) {
        log_error("Custom message localization entry has no uid", 0, 0);
        data.success = 0;
        return 0;
    }

    const char *uid = xml_parser_get_attribute_string("uid");
    if (!uid || strlen(uid) >= FILE_NAME_MAX) {
        log_error("Custom message localization uid is too long", 0, 0);
        data.success = 0;
        return 0;
    }
    uint8_t encoded_uid[FILE_NAME_MAX];
    encoding_from_utf8(uid, encoded_uid, FILE_NAME_MAX);
    int message_id = custom_messages_get_id_by_uid(encoded_uid);
    if (!message_id) {
        log_error("Unknown custom message uid in localization overlay", uid, 0);
        return 1;
    }
    if (message_id > data.message_count || data.seen_messages[message_id]) {
        log_error("Duplicate custom message uid in localization overlay", uid, 0);
        data.success = 0;
        return 0;
    }

    data.seen_messages[message_id] = 1;
    data.current_message_id = message_id;
    return 1;
}

static void xml_end_message(void)
{
    data.current_message_id = 0;
}

static void replace_text(uint8_t **destination, const char *text)
{
    if (!data.current_message_id) {
        return;
    }
    uint8_t *replacement = copy_encoded_text(text);
    if (!data.success) {
        return;
    }
    free(*destination);
    *destination = replacement;
}

static void xml_on_title(const char *text)
{
    if (data.current_message_id) {
        replace_text(&data.messages[data.current_message_id].title, text);
    }
}

static void xml_on_subtitle(const char *text)
{
    if (data.current_message_id) {
        replace_text(&data.messages[data.current_message_id].subtitle, text);
    }
}

static void xml_on_text(const char *text)
{
    if (data.current_message_id) {
        replace_text(&data.messages[data.current_message_id].text, text);
    }
}

static int parse_localization(const char *xml_text, size_t xml_size)
{
    data.message_count = custom_messages_count();
    data.messages = calloc((size_t) data.message_count + 1, sizeof(localized_custom_message));
    data.seen_messages = calloc((size_t) data.message_count + 1, sizeof(uint8_t));
    if (!data.messages || !data.seen_messages) {
        log_error("Unable to allocate custom message localization data", 0, 0);
        custom_messages_localization_clear();
        return 0;
    }

    data.success = 1;
    if (!xml_parser_init(xml_elements, XML_TOTAL_ELEMENTS, 1) ||
        !xml_parser_parse(xml_text, (unsigned int) xml_size, 1) || !data.success) {
        xml_parser_free();
        custom_messages_localization_clear();
        return 0;
    }
    xml_parser_free();
    free(data.seen_messages);
    data.seen_messages = 0;
    return 1;
}

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
    if (version != CUSTOM_MESSAGE_LOCALIZATION_VERSION) {
        log_error("Unsupported custom message locale manifest version", 0, version);
        locale_data.success = 0;
        return 0;
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
        log_error("Invalid locale id in custom message locale manifest", id, 0);
        locale_data.success = 0;
        return 0;
    }
    if (*locale_data.resolved_language &&
        !strings_equal_case_insensitive(locale_data.resolved_language, id)) {
        log_error("Ambiguous locale mapping in custom message locale manifest", id, 0);
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

static int resolve_language_from_manifest(const char *requested_language,
    const char *canonical_language, int allow_detected_default, char *resolved_language)
{
    size_t xml_size = 0;
    char *xml_text = campaign_file_load(LOCALIZATION_LOCALES_FILE, &xml_size);
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

static char *load_localization_file(const char *language, const char *scenario_name,
    char *path, size_t path_length, size_t *xml_size)
{
    int path_size = snprintf(path, path_length, "localization/%s/messages/%s.xml", language, scenario_name);
    if (path_size < 0 || (size_t) path_size >= path_length) {
        log_error("Custom message localization path is too long", scenario_name, 0);
        return 0;
    }
    return campaign_file_load(path, xml_size);
}

int custom_messages_localization_load(void)
{
    custom_messages_localization_clear();
    if (!game_campaign_is_active() || !game_campaign_is_custom()) {
        return 0;
    }
    if (custom_messages_count() <= 0) {
        return 0;
    }

    const char *language = config_get_string(CONFIG_STRING_UI_LANGUAGE_DIR);

    const campaign_scenario *scenario = game_campaign_get_scenario(scenario_campaign_mission());
    if (!scenario || !scenario->path) {
        return 0;
    }

    char scenario_name[FILE_NAME_MAX];
    int scenario_name_size = snprintf(scenario_name, FILE_NAME_MAX, "%s", file_remove_path(scenario->path));
    if (scenario_name_size < 0 || scenario_name_size >= FILE_NAME_MAX) {
        log_error("Custom campaign scenario name is too long for localization", scenario->path, 0);
        return 0;
    }
    file_remove_extension(scenario_name);

    char path[FILE_NAME_MAX];
    size_t xml_size = 0;
    char canonical_language[FILE_NAME_MAX] = { 0 };
    char resolved_language[FILE_NAME_MAX] = { 0 };
    char *xml_text = 0;
    if (language && *language) {
        xml_text = load_localization_file(language, scenario_name, path, FILE_NAME_MAX, &xml_size);
        canonicalize_language_directory(language, canonical_language, FILE_NAME_MAX);
        if (!xml_text && strcmp(language, canonical_language) != 0) {
            xml_text = load_localization_file(canonical_language, scenario_name, path, FILE_NAME_MAX, &xml_size);
        }
        if (!xml_text && resolve_language_from_manifest(language, canonical_language, 0, resolved_language)) {
            xml_text = load_localization_file(resolved_language, scenario_name, path, FILE_NAME_MAX, &xml_size);
        }
    } else if (resolve_language_from_manifest(0, 0, 1, resolved_language)) {
        xml_text = load_localization_file(resolved_language, scenario_name, path, FILE_NAME_MAX, &xml_size);
    } else {
        const char *detected_language = detected_language_tag();
        if (detected_language) {
            xml_text = load_localization_file(detected_language, scenario_name, path, FILE_NAME_MAX, &xml_size);
        }
    }
    if (!xml_text) {
        return 0;
    }

    int result = parse_localization(xml_text, xml_size);
    free(xml_text);
    if (result) {
        log_info("Loaded custom message localization", path, 0);
    } else {
        log_error("Unable to load custom message localization", path, 0);
    }
    return result;
}

static uint8_t *get_localized_field(int message_id, localized_custom_message_field field)
{
    if (!data.messages || message_id <= 0 || message_id > data.message_count) {
        return 0;
    }
    switch (field) {
        case LOCALIZED_FIELD_TITLE:
            return data.messages[message_id].title;
        case LOCALIZED_FIELD_SUBTITLE:
            return data.messages[message_id].subtitle;
        case LOCALIZED_FIELD_TEXT:
            return data.messages[message_id].text;
        default:
            return 0;
    }
}

uint8_t *custom_messages_localization_get_title(int message_id)
{
    return get_localized_field(message_id, LOCALIZED_FIELD_TITLE);
}

uint8_t *custom_messages_localization_get_subtitle(int message_id)
{
    return get_localized_field(message_id, LOCALIZED_FIELD_SUBTITLE);
}

uint8_t *custom_messages_localization_get_text(int message_id)
{
    return get_localized_field(message_id, LOCALIZED_FIELD_TEXT);
}

#include "localization.h"

#include "core/encoding.h"
#include "core/file.h"
#include "core/log.h"
#include "core/xml_parser.h"
#include "game/campaign.h"
#include "game/campaign/file.h"
#include "game/campaign/localization_file.h"
#include "game/campaign/mission.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *path;
    int first_in_mission;
    uint8_t *name;
    uint8_t *description;
    uint8_t *mission_title;
} localized_scenario;

typedef struct seen_key {
    char *value;
    int is_mission;
    struct seen_key *next;
} seen_key;

enum {
    FIELD_NAME = 1,
    FIELD_DESCRIPTION = 2,
    FIELD_TITLE = 4
};

typedef struct {
    uint8_t *name;
    uint8_t *description;
    localized_scenario *scenarios;
    int scenario_count;
    int loaded;
    int success;
    int root_seen;
    int root_closed;
    int campaign_fields;
    int current_fields;
    int current_scenario;
    seen_key *keys;
    const char *preview_campaign;
    campaign_file_reader *preview_reader;
} localization_data;

static localization_data data;
/* The XML parser is synchronous; callbacks only access this temporary target. */
static localization_data *parsing;

static void clear_keys(localization_data *target)
{
    while (target->keys) {
        seen_key *next = target->keys->next;
        free(target->keys->value);
        free(target->keys);
        target->keys = next;
    }
}

static void clear_localization(localization_data *target)
{
    free(target->name);
    free(target->description);
    for (int i = 0; i < target->scenario_count; i++) {
        free(target->scenarios[i].name);
        free(target->scenarios[i].description);
        free(target->scenarios[i].mission_title);
        if (target->preview_campaign) {
            free((char *) target->scenarios[i].path);
        }
    }
    free(target->scenarios);
    clear_keys(target);
    memset(target, 0, sizeof(*target));
}

void campaign_localization_clear(void)
{
    clear_localization(&data);
}

static int fail(const char *reason, const char *key)
{
    log_error(reason, key, 0);
    parsing->success = 0;
    return 0;
}

static int is_empty(const char *text)
{
    return !text || strspn(text, " \t\r\n") == strlen(text);
}

static void unexpected_text(const char *text)
{
    if (!is_empty(text)) {
        fail("Unexpected text in campaign localization", 0);
    }
}

static int start_root(void)
{
    const char *version = xml_parser_get_attribute_string("version");
    if (parsing->root_seen || !version || strcmp(version, "1") != 0) {
        return fail("Invalid campaign localization root or version", version);
    }
    parsing->root_seen = 1;
    return 1;
}

static void end_root(void)
{
    parsing->root_closed = 1;
}

static int remember_key(const char *key, int is_mission)
{
    for (seen_key *seen = parsing->keys; seen; seen = seen->next) {
        if (seen->is_mission == is_mission && strcmp(seen->value, key) == 0) {
            return fail("Duplicate campaign localization key", key);
        }
    }
    seen_key *seen = malloc(sizeof(seen_key));
    if (!seen) {
        return fail("Unable to allocate campaign localization key", 0);
    }
    size_t length = strlen(key) + 1;
    seen->value = malloc(length);
    if (!seen->value) {
        free(seen);
        return fail("Unable to allocate campaign localization key", 0);
    }
    memcpy(seen->value, key, length);
    seen->is_mission = is_mission;
    seen->next = parsing->keys;
    parsing->keys = seen;
    return 1;
}

static int matches_stem(const char *path, const char *key)
{
    if (!path) {
        return 0;
    }
    const char *filename = file_remove_path(path);
    const char *extension = strrchr(filename, '.');
    size_t length = extension ? (size_t) (extension - filename) : strlen(filename);
    return strlen(key) == length && strncmp(filename, key, length) == 0;
}

static int start_entry(int is_mission)
{
    parsing->current_scenario = -1;
    parsing->current_fields = 0;
    if (!parsing->success) {
        return 0;
    }
    const char *key = xml_parser_get_attribute_string(is_mission ? "first-scenario" : "file");
    if (is_empty(key) || strlen(key) >= FILE_NAME_MAX || strchr(key, '/') || strchr(key, '\\')) {
        return fail("Invalid campaign localization key", key);
    }
    if (!remember_key(key, is_mission)) {
        return 0;
    }
    for (int i = 0; i < parsing->scenario_count; i++) {
        if (!matches_stem(parsing->scenarios[i].path, key)) {
            continue;
        }
        if (parsing->current_scenario >= 0) {
            return fail("Ambiguous campaign localization key", key);
        }
        parsing->current_scenario = i;
    }
    if (is_mission && parsing->current_scenario >= 0) {
        if (!parsing->scenarios[parsing->current_scenario].first_in_mission) {
            parsing->current_scenario = -1;
        }
    }
    if (parsing->current_scenario < 0) {
        log_error("Unknown campaign localization key", key, 0);
    }
    return 1;
}

static int start_mission(void)
{
    return start_entry(1);
}

static int start_scenario(void)
{
    return start_entry(0);
}

static void end_entry(void)
{
    parsing->current_scenario = -1;
}

static int start_field(void)
{
    const char *name = xml_parser_get_current_element_name();
    const char *parent = xml_parser_get_parent_element_name();
    int field = strcmp(name, "name") == 0 ? FIELD_NAME :
        strcmp(name, "description") == 0 ? FIELD_DESCRIPTION : FIELD_TITLE;
    int *fields = strcmp(parent, "campaign_localization") == 0 ?
        &parsing->campaign_fields : &parsing->current_fields;
    if (!parsing->success) {
        return 0;
    }
    if (*fields & field) {
        return fail("Duplicate campaign localization field", name);
    }
    *fields |= field;
    return 1;
}

static void field_text(const char *text)
{
    if (!parsing->success || is_empty(text)) {
        return;
    }
    const char *parent = xml_parser_get_parent_element_name();
    const char *field = xml_parser_get_current_element_name();
    uint8_t **destination;
    if (strcmp(parent, "campaign_localization") == 0) {
        destination = strcmp(field, "name") == 0 ? &parsing->name : &parsing->description;
    } else {
        if (parsing->current_scenario < 0) {
            return;
        }
        localized_scenario *localized = &parsing->scenarios[parsing->current_scenario];
        destination = strcmp(field, "title") == 0 ? &localized->mission_title :
            strcmp(field, "name") == 0 ? &localized->name : &localized->description;
    }
    size_t length = strlen(text);
    if (length >= INT_MAX) {
        fail("Campaign localization text is too long", field);
        return;
    }
    uint8_t *encoded = malloc(length + 1);
    if (!encoded) {
        fail("Unable to allocate campaign localization text", field);
        return;
    }
    encoding_from_utf8(text, encoded, (int) length + 1);
    free(*destination);
    *destination = encoded;
}

static const xml_parser_element xml_elements[] = {
    { "campaign_localization", start_root, end_root, 0, unexpected_text },
    { "mission", start_mission, end_entry, "campaign_localization", unexpected_text },
    { "scenario", start_scenario, end_entry, "campaign_localization", unexpected_text },
    { "name", start_field, 0, "campaign_localization|scenario", field_text },
    { "description", start_field, 0, "campaign_localization|scenario", field_text },
    { "title", start_field, 0, "mission", field_text }
};

static int parse_overlay(localization_data *target, const char *xml, size_t size)
{
    // The XML parser and its growable text buffers use signed integer sizes.
    if (!size || size > INT_MAX / 2) {
        return 0;
    }
    target->current_scenario = -1;
    target->success = 1;
    target->root_seen = target->root_closed = 0;
    target->campaign_fields = target->current_fields = 0;
    parsing = target;
    int parsed = xml_parser_init(xml_elements, sizeof(xml_elements) / sizeof(xml_elements[0]), 1) &&
        xml_parser_parse(xml, (unsigned int) size, 1);
    xml_parser_free();
    parsing = 0;
    clear_keys(target);
    target->loaded = parsed && target->success && target->root_seen && target->root_closed;
    return target->loaded;
}

int campaign_localization_load(void)
{
    campaign_localization_clear();
    if (!game_campaign_is_active() || !game_campaign_is_custom()) {
        return 0;
    }
    char language[FILE_NAME_MAX] = { 0 };
    size_t xml_size = 0;
    char *xml = campaign_localization_load_file("campaign.xml", language, &xml_size);
    if (!xml) {
        return 0;
    }
    int count = 0;
    while (count < INT_MAX && campaign_mission_get_scenario(count)) {
        count++;
    }
    if (count == INT_MAX || (size_t) count > SIZE_MAX / sizeof(localized_scenario)) {
        free(xml);
        return 0;
    }
    if (count) {
        data.scenarios = calloc((size_t) count, sizeof(localized_scenario));
        if (!data.scenarios) {
            free(xml);
            return 0;
        }
    }
    data.scenario_count = count;
    for (int i = 0; i < count; i++) {
        data.scenarios[i].path = campaign_mission_get_scenario(i)->path;
        const campaign_mission *mission = campaign_mission_current(i);
        data.scenarios[i].first_in_mission = mission && mission->first_scenario == i;
    }
    int parsed = parse_overlay(&data, xml, xml_size);
    free(xml);
    if (!parsed) {
        log_error("Unable to load campaign localization", language, 0);
        campaign_localization_clear();
    }
    return parsed;
}

/* Read only canonical scenario identities for preview validation. The full
 * campaign parser initializes global missions and must not be used here. */
static int preview_start_campaign(void)
{
    if (parsing->root_seen || xml_parser_get_attribute_int("version") > 1) {
        return fail("Invalid campaign metadata preview", 0);
    }
    parsing->root_seen = 1;
    return 1;
}

static int preview_start_description(void)
{
    const char *title = xml_parser_get_attribute_string("title");
    return title && *title ? 1 : fail("Missing canonical campaign title", 0);
}

static int preview_start_missions(void)
{
    if (parsing->campaign_fields) {
        return fail("Duplicate canonical campaign mission list", 0);
    }
    parsing->campaign_fields = 1;
    return 1;
}

static int preview_start_scenario(void)
{
    const char *file = xml_parser_get_attribute_string("file");
    if (!file || !*file || parsing->scenario_count == INT_MAX ||
        (size_t) parsing->scenario_count + 1 > SIZE_MAX / sizeof(localized_scenario)) {
        return fail("Invalid canonical scenario for campaign preview", file);
    }
    char path[FILE_NAME_MAX];
    int length = snprintf(path, sizeof(path), "scenario/%s", file);
    if (length < 0 || length >= FILE_NAME_MAX) {
        return fail("Canonical scenario path is too long", file);
    }
    int exists = campaign_file_reader_exists(parsing->preview_reader, path);
    // The canonical loader allows missing debug scenarios only in folders.
    if (!exists && file_has_extension(parsing->preview_campaign, "campaign")) {
        return fail("Missing canonical scenario in campaign preview", file);
    }
    char *copy = 0;
    if (exists) {
        copy = malloc(strlen(file) + 1);
        if (!copy) {
            return fail("Unable to allocate canonical scenario preview", 0);
        }
        strcpy(copy, file);
    }
    localized_scenario *scenarios = realloc(parsing->scenarios,
        ((size_t) parsing->scenario_count + 1) * sizeof(localized_scenario));
    if (!scenarios) {
        free(copy);
        return fail("Unable to allocate canonical scenario preview", 0);
    }
    parsing->scenarios = scenarios;
    localized_scenario *scenario = &scenarios[parsing->scenario_count];
    memset(scenario, 0, sizeof(*scenario));
    scenario->path = copy;
    scenario->first_in_mission = parsing->scenario_count == parsing->current_scenario;
    parsing->scenario_count++;
    return 1;
}

static int preview_start_mission(void)
{
    parsing->current_scenario = parsing->scenario_count;
    return !xml_parser_has_attribute("file") || preview_start_scenario();
}

static void preview_end_mission(void)
{
    if (parsing->scenario_count == parsing->current_scenario) {
        fail("Canonical campaign mission has no scenarios", 0);
    }
}

static const xml_parser_element preview_elements[] = {
    { "campaign", preview_start_campaign, end_root },
    { "description", preview_start_description, 0, "campaign" },
    { "missions", preview_start_missions, 0, "campaign" },
    { "mission", preview_start_mission, preview_end_mission, "missions" },
    { "scenario", preview_start_scenario, 0, "mission" }
};

uint8_t *campaign_localization_preview_name(const char *campaign_name)
{
    if (!campaign_name || !*campaign_name) {
        return 0;
    }
    campaign_file_reader *reader = campaign_file_reader_open(campaign_name);
    if (!reader) {
        return 0;
    }
    char language[FILE_NAME_MAX];
    size_t size = 0;
    char *xml = campaign_localization_load_file_from(reader, "campaign.xml", language, &size);
    if (!xml) {
        campaign_file_reader_close(reader);
        return 0;
    }
    size_t canonical_size = 0;
    char *canonical = campaign_file_reader_load(reader, "settings.xml", &canonical_size);
    localization_data preview = { 0 };
    preview.preview_campaign = campaign_name;
    preview.preview_reader = reader;
    preview.success = 1;
    parsing = &preview;
    int valid = canonical && canonical_size && canonical_size <= INT_MAX / 2 &&
        xml_parser_init(preview_elements, sizeof(preview_elements) / sizeof(preview_elements[0]), 0) &&
        xml_parser_parse(canonical, (unsigned int) canonical_size, 1);
    xml_parser_free();
    parsing = 0;
    free(canonical);
    valid = valid && preview.success && preview.root_seen && preview.root_closed;
    uint8_t *name = 0;
    if (valid && parse_overlay(&preview, xml, size)) {
        name = preview.name;
        preview.name = 0;
    }
    free(xml);
    clear_localization(&preview);
    campaign_file_reader_close(reader);
    return name;
}

static int is_available(void)
{
    return data.loaded && game_campaign_is_active() && game_campaign_is_custom();
}

const uint8_t *campaign_localization_name(void)
{
    return is_available() ? data.name : 0;
}

const uint8_t *campaign_localization_description(void)
{
    return is_available() ? data.description : 0;
}

const uint8_t *campaign_localization_mission_title(int scenario_id)
{
    if (!is_available() || scenario_id < 0 || scenario_id >= data.scenario_count) {
        return 0;
    }
    const campaign_mission *mission = campaign_mission_current(scenario_id);
    return mission && mission->first_scenario >= 0 && mission->first_scenario < data.scenario_count ?
        data.scenarios[mission->first_scenario].mission_title : 0;
}

const uint8_t *campaign_localization_scenario_name(int scenario_id)
{
    return is_available() && scenario_id >= 0 && scenario_id < data.scenario_count ?
        data.scenarios[scenario_id].name : 0;
}

const uint8_t *campaign_localization_scenario_description(int scenario_id)
{
    return is_available() && scenario_id >= 0 && scenario_id < data.scenario_count ?
        data.scenarios[scenario_id].description : 0;
}

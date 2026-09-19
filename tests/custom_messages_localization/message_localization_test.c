/* Production message/locale parsers, folder/ZIP loading and text encoding.
 * Adapters isolate game state, language selection and filesystem roots. */
#include "core/config.h"
#include "core/encoding.h"
#include "core/file.h"
#include "core/log.h"
#include "game/campaign.h"
#include "game/campaign/file.h"
#include "scenario/custom_messages.h"
#include "scenario/custom_messages_localization.h"
#include "scenario/property.h"
#include "zip/zip.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <wchar.h>
#endif

static const char *ui_language;
static language_type detected_language;
static int campaign_active, campaign_custom, message_count;
static campaign_scenario fixture_scenario = { .id = 1 };
static int checks, failures, cases, archive_mode;
static const char *case_name;
static char run_root[64], fixture_root[128];
static struct zip_t *fixture_zip;

const char *config_get_string(config_string_key key) { (void) key; return ui_language; }
language_type locale_last_determined_language(void) { return detected_language; }
int game_campaign_is_active(void) { return campaign_active; }
int game_campaign_is_custom(void) { return campaign_custom; }
int custom_messages_count(void) { return message_count; }
int custom_messages_get_id_by_uid(const uint8_t *uid)
{
    return !strcmp((const char *) uid, "briefing") ? 1 : !strcmp((const char *) uid, "victory") ? 2 : 0;
}
int scenario_campaign_mission(void) { return 1; }
const campaign_scenario *game_campaign_get_scenario(int id) { return id == 1 ? &fixture_scenario : NULL; }
void log_info(const char *m, const char *s, int i) { (void) m; (void) s; (void) i; }
void log_error(const char *m, const char *s, int i) { (void) m; (void) s; (void) i; }

/* Use UTF-8 paths on both platforms, including direct accented locale folders. */
FILE *file_open(const char *name, const char *mode)
{
#ifdef _WIN32
    uint16_t wide_name[1024], wide_mode[16];
    encoding_utf8_to_utf16(name, wide_name);
    encoding_utf8_to_utf16(mode, wide_mode);
    return _wfopen((const wchar_t *) wide_name, (const wchar_t *) wide_mode);
#else
    return fopen(name, mode);
#endif
}
int file_close(FILE *file) { return fclose(file); }
const char *dir_get_file_at_location(const char *name, int location)
{
    (void) location;
#ifdef _WIN32
    uint16_t wide_name[1024];
    struct _stat info;
    encoding_utf8_to_utf16(name, wide_name);
    return _wstat((const wchar_t *) wide_name, &info) == 0 ? name : NULL;
#else
    struct stat info;
    return stat(name, &info) == 0 ? name : NULL;
#endif
}
static int make_directory(const char *name)
{
#ifdef _WIN32
    uint16_t wide_name[1024];
    encoding_utf8_to_utf16(name, wide_name);
    return _wmkdir((const wchar_t *) wide_name);
#else
    return mkdir(name, 0700);
#endif
}
int file_has_extension(const char *name, const char *extension)
{
    const char *dot = strrchr(name, '.');
    return dot && strcmp(dot + 1, extension) == 0;
}
const char *file_remove_path(const char *name)
{
    const char *separator = strrchr(name, '/');
    if (!separator) separator = strrchr(name, '\\');
    return separator ? separator + 1 : name;
}
void file_remove_extension(char *name)
{
    char *dot = strrchr(name, '.');
    if (dot) *dot = 0;
}

static void check(int success, const char *expression, int line)
{
    checks++;
    if (!success) {
        failures++;
        printf("FAIL [%s/%s] %d: %s\n", archive_mode ? "ZIP" : "folder", case_name, line, expression);
    }
}
#define CHECK(expression) check(!!(expression), #expression, __LINE__)
static int equals(const uint8_t *actual, const char *expected)
{
    if (!actual) return expected == NULL;
    if (!expected) return 0;
    char utf8[1024];
    encoding_to_utf8(actual, utf8, sizeof(utf8), 0);
    return strcmp(utf8, expected) == 0;
}
static void begin_case(const char *name, const char *language)
{
    custom_messages_localization_clear();
    campaign_file_close_zip();
    cases++; case_name = name; ui_language = language;
    detected_language = LANGUAGE_PORTUGUESE;
    encoding_determine(detected_language);
    campaign_active = campaign_custom = 1; message_count = 2;
    fixture_scenario.path = "scenarios/RC01.mapx";
    snprintf(fixture_root, sizeof(fixture_root), "%s/case_%03d%s", run_root, cases,
        archive_mode ? ".campaign" : "");
    if (archive_mode) {
        fixture_zip = zip_open(fixture_root, 6, 'w');
        if (!fixture_zip) exit(2);
    } else if (make_directory(fixture_root) != 0) exit(2);
}
static void add_file(const char *relative, const char *content)
{
    if (archive_mode) {
        if (zip_entry_open(fixture_zip, relative) || zip_entry_write(fixture_zip, content, strlen(content))) exit(2);
        zip_entry_close(fixture_zip);
        return;
    }
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", fixture_root, relative);
    for (char *p = path; *p; p++) {
        if (*p != '/') continue;
        *p = 0;
        if (make_directory(path) != 0 && errno != EEXIST) exit(2);
        *p = '/';
    }
    FILE *file = file_open(path, "wb");
    if (!file || fwrite(content, 1, strlen(content), file) != strlen(content)) exit(2);
    file_close(file);
}
static void add_text(const char *language, const char *xml)
{
    char path[512];
    snprintf(path, sizeof(path), "localization/%s/messages/RC01.xml", language);
    add_file(path, xml);
}
static int load(void)
{
    if (fixture_zip) { zip_close(fixture_zip); fixture_zip = NULL; }
    campaign_file_set_path(fixture_root);
    CHECK(campaign_file_is_zip() == archive_mode);
    return custom_messages_localization_load();
}
static void expect_empty(void)
{
    CHECK(!custom_messages_localization_get_title(1));
    CHECK(!custom_messages_localization_get_subtitle(1));
    CHECK(!custom_messages_localization_get_text(1));
}
static const char *text_xml =
    "<localization version=\"1\"><message uid=\"briefing\"><title>Missão inicial</title>"
    "<subtitle>Subtítulo</subtitle><text>Descrição e ação</text></message></localization>";
static void expect_fields(void)
{
    CHECK(equals(custom_messages_localization_get_title(1), "Missão inicial"));
    CHECK(equals(custom_messages_localization_get_subtitle(1), "Subtítulo"));
    CHECK(equals(custom_messages_localization_get_text(1), "Descrição e ação"));
    CHECK(!custom_messages_localization_get_title(2));
    CHECK(!custom_messages_localization_get_title(0));
    CHECK(!custom_messages_localization_get_subtitle(-1));
    CHECK(!custom_messages_localization_get_text(3));
}
static void test_matrix(void)
{
    begin_case("complete text overlay", "pt-BR");
    add_text("pt-BR", text_xml); CHECK(load()); expect_fields();

    begin_case("packaged save uses the same scenario stem", "pt-BR");
    fixture_scenario.path = "scenarios/RC01.svx";
    add_text("pt-BR", text_xml); CHECK(load()); expect_fields();

    begin_case("missing overlay returns canonical fallback markers", "pt-BR");
    CHECK(!load()); expect_empty();

    begin_case("missing and empty fields fall back independently", "pt-BR");
    add_text("pt-BR", "<localization version=\"1\"><message uid=\"briefing\">"
        "<title>Only title</title><subtitle/></message><message uid=\"victory\">"
        "<text></text></message></localization>");
    CHECK(load()); CHECK(equals(custom_messages_localization_get_title(1), "Only title"));
    CHECK(!custom_messages_localization_get_subtitle(1));
    CHECK(!custom_messages_localization_get_text(1));
    CHECK(!custom_messages_localization_get_text(2));

    begin_case("unknown UID leaves the known entry usable", "pt-BR");
    add_text("pt-BR", "<localization version=\"1\"><message uid=\"unknown\"><title>Ignored</title>"
        "</message><message uid=\"briefing\"><title>Known</title></message></localization>");
    CHECK(load()); CHECK(equals(custom_messages_localization_get_title(1), "Known"));
    CHECK(!custom_messages_localization_get_title(2));

    const struct { const char *label, *xml; } invalid[] = {
        {"unsupported overlay version", "<localization version=\"2\"/>"},
        {"truncated overlay", "<localization version=\"1\"><message uid=\"briefing\"><title>broken"},
        {"missing message UID", "<localization version=\"1\"><message><title>No UID</title></message></localization>"},
        {"duplicate message UID rejects earlier text", "<localization version=\"1\">"
            "<message uid=\"briefing\"><title>First</title></message>"
            "<message uid=\"briefing\"><title>Second</title></message></localization>"}
    };
    for (int i = 0; i < sizeof(invalid) / sizeof(*invalid); i++) {
        begin_case(invalid[i].label, "pt-BR");
        add_text("pt-BR", invalid[i].xml); CHECK(!load()); expect_empty();
    }

    const struct {
        const char *label, *requested, *resolved, *manifest;
        int expected;
        language_type detected;
    } locales[] = {
        {"canonical case and separator", "PT_br", "pt-BR", NULL, 1, LANGUAGE_ENGLISH},
        {"French canonicalization", "FR_fr", "fr-FR", NULL, 1, LANGUAGE_FRENCH},
        {"direct directory with spaces", "Portuguese (Brazil)", "Portuguese (Brazil)", NULL, 1, LANGUAGE_ENGLISH},
        {"direct accented directory", "Português", "Português", NULL, 1, LANGUAGE_ENGLISH},
        {"manifest alias", "portuguese", "pt-BR",
            "<locales version=\"1\"><locale id=\"pt-BR\" aliases=\"Portuguese|pt_BR\"/></locales>", 1, LANGUAGE_ENGLISH},
        {"manifest alias with spaces", "Portuguese (Brazil)", "pt-BR",
            "<locales version=\"1\"><locale id=\"pt-BR\" aliases=\"Portuguese (Brazil)\"/></locales>", 1, LANGUAGE_ENGLISH},
        {"manifest alias with accents", "Português", "pt-BR",
            "<locales version=\"1\"><locale id=\"pt-BR\" aliases=\"Português\"/></locales>", 1, LANGUAGE_ENGLISH},
        {"detected manifest default", "", "pt-BR",
            "<locales version=\"1\"><locale id=\"pt-BR\" default-for=\"pt\"/></locales>", 1, LANGUAGE_PORTUGUESE},
        {"detected language without manifest", "", "fr", NULL, 1, LANGUAGE_FRENCH},
        {"explicit selection ignores detected default", "pt-PT", "pt-BR",
            "<locales version=\"1\"><locale id=\"pt-BR\" default-for=\"pt\"/></locales>", 0, LANGUAGE_PORTUGUESE},
        {"invalid manifest directory", "portuguese", "pt-BR",
            "<locales version=\"1\"><locale id=\"../pt-BR\" aliases=\"portuguese\"/></locales>", 0, LANGUAGE_PORTUGUESE},
        {"ambiguous alias", "portuguese", "pt-BR",
            "<locales version=\"1\"><locale id=\"pt-BR\" aliases=\"portuguese\"/>"
            "<locale id=\"pt-PT\" aliases=\"portuguese\"/></locales>", 0, LANGUAGE_PORTUGUESE},
        {"unsupported manifest version", "portuguese", "pt-BR",
            "<locales version=\"2\"><locale id=\"pt-BR\" aliases=\"portuguese\"/></locales>", 0, LANGUAGE_PORTUGUESE},
        {"missing explicitly requested language", "de", "pt-BR", NULL, 0, LANGUAGE_PORTUGUESE}
    };
    for (int i = 0; i < sizeof(locales) / sizeof(*locales); i++) {
        begin_case(locales[i].label, locales[i].requested);
        detected_language = locales[i].detected;
        add_text(locales[i].resolved, text_xml);
        if (locales[i].manifest) add_file("localization/locales.xml", locales[i].manifest);
        CHECK(load() == locales[i].expected);
        if (locales[i].expected) expect_fields(); else expect_empty();
    }

    begin_case("exact directory precedes normalization and aliases", "pt_br");
    add_text("pt_br", text_xml);
    add_text("pt-BR", "<localization version=\"1\"><message uid=\"briefing\"><title>Wrong choice</title>"
        "</message></localization>");
    add_file("localization/locales.xml", "<locales version=\"1\"><locale id=\"pt-BR\" aliases=\"pt_br\"/></locales>");
    CHECK(load()); expect_fields();

    begin_case("language switch and explicit clear release old values", "pt-BR");
    add_text("pt-BR", text_xml);
    add_text("fr", "<localization version=\"1\"><message uid=\"briefing\"><title>Français</title>"
        "</message></localization>");
    CHECK(load()); expect_fields();
    ui_language = "fr";
    CHECK(custom_messages_localization_load());
    CHECK(equals(custom_messages_localization_get_title(1), "Français"));
    CHECK(!custom_messages_localization_get_text(1));
    ui_language = "de";
    CHECK(!custom_messages_localization_load()); expect_empty();
    ui_language = "pt-BR";
    CHECK(custom_messages_localization_load()); expect_fields();
    custom_messages_localization_clear(); expect_empty();
    custom_messages_localization_clear(); expect_empty();

    const char *inactive[] = {"inactive campaign clears old text", "original campaign clears old text",
        "no custom messages clears old text", "missing scenario path clears old text"};
    for (int mode = 0; mode < sizeof(inactive) / sizeof(*inactive); mode++) {
        begin_case(inactive[mode], "pt-BR");
        add_text("pt-BR", text_xml); CHECK(load());
        if (mode == 0) campaign_active = 0;
        if (mode == 1) campaign_custom = 0;
        if (mode == 2) message_count = 0;
        if (mode == 3) fixture_scenario.path = NULL;
        CHECK(!custom_messages_localization_load()); expect_empty();
    }
    custom_messages_localization_clear();
    campaign_file_close_zip();
}
int main(void)
{
    if (make_directory("fixtures") != 0 && errno != EEXIST) return 2;
    for (int run = 0; ; run++) {
        snprintf(run_root, sizeof(run_root), "fixtures/messages_%03d", run);
        if (make_directory(run_root) == 0) break;
        if (errno != EEXIST) return 2;
    }
    for (archive_mode = 0; archive_mode < 2; archive_mode++) test_matrix();
    printf("RESULT: %d cases, %d checks, %d failures (folder + real ZIP .campaign).\n", cases, checks, failures);
    return failures ? 1 : 0;
}

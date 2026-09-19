/* Real campaign lifecycle, XML parsers, file/ZIP loading and encoding.
 * Adapters below isolate OS roots, settings, progression and scenario execution.
 * No game assets, saves or profiles are required or modified. */
#include "core/config.h"
#include "core/encoding.h"
#include "core/file.h"
#include "core/lang.h"
#include "core/log.h"
#include "game/campaign.h"
#include "game/campaign/file.h"
#include "game/campaign/mission.h"
#include "game/campaign/localization.h"
#include "game/file.h"
#include "zip/zip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define make_directory(path) _mkdir(path)
#else
#define make_directory(path) mkdir(path, 0700)
#endif

static const char *ui_language = "pt-BR";
static language_type detected_language = LANGUAGE_PORTUGUESE;
static int checks, failures, cases, archive_mode, rank, progression_reads, progression_writes, rank_updates, fixture_id;
static const char *case_name;
static char fixture_root[512];
static char run_root[64];
static struct zip_t *fixture_zip;
static char watched_path[512];
static FILE *watched_stream;
static int watched_opens, watched_closes;

const char *config_get_string(config_string_key key) { (void) key; return ui_language; }
language_type locale_last_determined_language(void) { return detected_language; }
const uint8_t *lang_get_string(int group, int index) {
    (void) group; (void) index; return (const uint8_t *) "Original campaign text";
}
void log_info(const char *m, const char *s, int i) { (void) m; (void) s; (void) i; }
void log_error(const char *m, const char *s, int i) { (void) m; (void) s; (void) i; }
int city_emperor_rank(void) { return rank; }
void city_emperor_set_rank(int value) { rank = value; rank_updates++; }
int campaign_player_data_get_current_mission(const char *name) { (void) name; progression_reads++; return 1; }
void campaign_player_data_update_current_mission(const char *name, int value) {
    (void) name; (void) value; progression_writes++;
}
int campaign_original_setup(void) { return campaign_mission_init(); }
uint8_t *campaign_original_load_scenario(int id, size_t *size) { (void) id; *size = 0; return NULL; }
int game_file_start_scenario_from_buffer(uint8_t *data, int size, int save) {
    (void) data; (void) size; (void) save; return 0;
}
int game_file_io_read_scenario_info_from_buffer(buffer *buf, saved_game_info *info) {
    (void) buf; (void) info; return 0;
}
int game_file_io_read_saved_game_info_from_buffer(buffer *buf, saved_game_info *info) {
    (void) buf; (void) info; return 0;
}
FILE *file_open(const char *name, const char *mode) {
    FILE *file = fopen(name, mode);
    if (file && watched_path[0] && strcmp(name, watched_path) == 0) {
        watched_stream = file;
        watched_opens++;
    }
    return file;
}
int file_close(FILE *file) {
    if (file == watched_stream) {
        watched_stream = NULL;
        watched_closes++;
    }
    return fclose(file);
}
const char *dir_get_file_at_location(const char *name, int location) {
    (void) location;
    struct stat info;
    return stat(name, &info) == 0 ? name : NULL;
}
int file_exists(const char *name, int localizable) {
    (void) localizable; return dir_get_file_at_location(name, 0) != NULL;
}
int file_has_extension(const char *name, const char *ext) {
    const char *dot = strrchr(name, '.');
    return dot && strcmp(dot + 1, ext) == 0;
}
const char *file_remove_path(const char *name) {
    const char *result = name;
    for (; *name; name++) if (*name == '/' || *name == '\\') result = name + 1;
    return result;
}
void file_remove_extension(char *name) { char *dot = strrchr(name, '.'); if (dot) *dot = 0; }

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
    char converted[4096];
    encoding_to_utf8(actual, converted, sizeof(converted), 0);
    return strcmp(converted, expected) == 0;
}
static void check_preview(const char *campaign, const char *expected)
{
    uint8_t *name = campaign_localization_preview_name(campaign);
    CHECK(equals(name, expected));
    free(name);
}
static void add_file(const char *relative, const char *content)
{
    if (fixture_zip) {
        if (zip_entry_open(fixture_zip, relative) || zip_entry_write(fixture_zip, content, strlen(content))) exit(2);
        zip_entry_close(fixture_zip);
        return;
    }
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", fixture_root, relative);
    for (char *p = path; *p; p++) {
        if (*p != '/') continue;
        *p = 0; make_directory(path); *p = '/';
    }
    FILE *file = fopen(path, "wb");
    if (!file) exit(2);
    if (fwrite(content, 1, strlen(content), file) != strlen(content)) exit(2);
    fclose(file);
}
static void add_translation(const char *locale, const char *text)
{
    char path[256];
    snprintf(path, sizeof(path), "localization/%s/campaign.xml", locale);
    add_file(path, text);
}
static const char *settings =
    "<campaign version=\"1\"><description title=\"Canonical campaign\" author=\"Author\">Canonical description</description>"
    "<missions starting_rank=\"clerk\"><mission title=\"First mission\" next_rank=\"engineer\">"
    "<scenario file=\"One.mapx\" name=\"One\" description=\"One description\"/>"
    "<scenario file=\"Two.svx\" name=\"Two\" description=\"Two description\"/></mission>"
    "<mission title=\"Second mission\" file=\"Three.mapx\" name=\"Three\"/></missions></campaign>";
static const char *localized =
    "<campaign_localization version=\"1\" language=\"pt-BR\"><name>Campanha traduzida</name>"
    "<description>Descrição e ação</description><mission first-scenario=\"One\"><title>Missão inicial</title></mission>"
    "<scenario file=\"One\"><name>Primeiro</name><description>Descrição do primeiro</description></scenario>"
    "<scenario file=\"Two\"><name>Segundo</name></scenario></campaign_localization>";

static void create_fixture(const char *canonical)
{
    snprintf(fixture_root, sizeof(fixture_root), "%s/case_%03d%s", run_root, fixture_id++, archive_mode ? ".campaign" : "");
    if (archive_mode) {
        fixture_zip = zip_open(fixture_root, 6, 'w');
        if (!fixture_zip) exit(2);
    } else make_directory(fixture_root);
    add_file("settings.xml", canonical ? canonical : settings);
    add_file("scenario/One.mapx", "synthetic");
    add_file("scenario/Two.svx", "synthetic");
    add_file("scenario/Three.mapx", "synthetic");
}
static void begin_case(const char *name, const char *canonical)
{
    game_campaign_clear();
    cases++; case_name = name;
    ui_language = "pt-BR"; detected_language = LANGUAGE_PORTUGUESE;
    encoding_determine(detected_language);
    rank = 7; progression_reads = 0; progression_writes = 0; rank_updates = 0;
    create_fixture(canonical);
}
static void finish_fixture(void)
{
    if (fixture_zip) { zip_close(fixture_zip); fixture_zip = NULL; }
}
static void load(void)
{
    finish_fixture();
    CHECK(game_campaign_load(fixture_root));
}
static void check_canonical(void)
{
    CHECK(equals(game_campaign_get_info()->name, "Canonical campaign"));
    CHECK(equals(game_campaign_get_info()->description, "Canonical description"));
    CHECK(equals(game_campaign_get_info()->author, "Author"));
    CHECK(equals(game_campaign_get_scenario(0)->name, "One"));
    CHECK(equals(campaign_mission_current(0)->title, "First mission"));
    CHECK(strcmp(game_campaign_get_scenario(0)->path, "***campaigns***/scenario/One.mapx") == 0);
    CHECK(strcmp(game_campaign_get_name(), fixture_root) == 0);
    CHECK(game_campaign_get_info()->current_mission == 1);
    CHECK(progression_writes == 0);
    CHECK(rank == 7); /* Display getters must not call resolve_rank. */
}
static void test_matrix(void)
{
    begin_case("complete metadata without message overlays", NULL);
    add_translation("pt-BR", localized); finish_fixture();
    check_preview(fixture_root, "Campanha traduzida");
    CHECK(!game_campaign_is_active());
    CHECK(!campaign_mission_get_scenario(0));
    load();
    CHECK(equals(game_campaign_display_name(), "Campanha traduzida"));
    CHECK(equals(game_campaign_display_description(), "Descrição e ação"));
    CHECK(equals(game_campaign_display_mission_title(0), "Missão inicial"));
    CHECK(equals(game_campaign_display_mission_title(1), "Missão inicial"));
    CHECK(equals(game_campaign_display_mission_title(2), "Second mission"));
    CHECK(equals(game_campaign_display_scenario_name(0), "Primeiro"));
    CHECK(equals(game_campaign_display_scenario_name(1), "Segundo"));
    CHECK(equals(game_campaign_display_scenario_description(0), "Descrição do primeiro"));
    CHECK(equals(game_campaign_display_scenario_description(1), "Two description"));
    CHECK(!game_campaign_display_scenario_name(-1));
    CHECK(!game_campaign_display_scenario_description(99));
    CHECK(!game_campaign_display_mission_title(-1));
    check_canonical();

    const uint8_t *canonical_name = game_campaign_get_scenario(0)->name;
    game_campaign_suspend();
    CHECK(!game_campaign_display_name());
    CHECK(!game_campaign_display_mission_title(0));
    CHECK(!campaign_localization_name());
    game_campaign_restore();
    CHECK(equals(game_campaign_display_name(), "Campanha traduzida"));
    CHECK(game_campaign_get_scenario(0)->name == canonical_name);
    game_campaign_suspend(); ui_language = "de"; game_campaign_restore();
    CHECK(equals(game_campaign_display_name(), "Canonical campaign"));
    CHECK(game_campaign_get_scenario(0)->name == canonical_name);
    ui_language = "pt-BR"; game_campaign_reload_localization();
    CHECK(equals(game_campaign_display_name(), "Campanha traduzida"));
    game_campaign_clear();
    CHECK(!game_campaign_display_scenario_name(0)); CHECK(!campaign_localization_name());

    begin_case("language reload and same-file load", NULL);
    add_translation("pt-BR", localized);
    add_translation("fr", "<campaign_localization version=\"1\"><name>Campagne française</name></campaign_localization>");
    add_translation("broken", "<campaign_localization version=\"1\"><name>Never apply</name>");
    load(); ui_language = "fr"; game_campaign_reload_localization();
    CHECK(equals(game_campaign_display_name(), "Campagne française"));
    CHECK(equals(game_campaign_display_scenario_name(0), "One"));
    ui_language = "de"; CHECK(game_campaign_load(fixture_root));
    CHECK(equals(game_campaign_display_name(), "Canonical campaign")); check_canonical();
    ui_language = "pt-BR"; game_campaign_reload_localization();
    ui_language = "broken"; game_campaign_reload_localization();
    CHECK(equals(game_campaign_display_name(), "Canonical campaign"));
    CHECK(equals(game_campaign_display_scenario_name(0), "One"));

    begin_case("restore after clearing or switching campaign", NULL);
    add_translation("pt-BR", localized); load();
    game_campaign_suspend();
    CHECK(game_campaign_load(CAMPAIGN_ORIGINAL_NAME));
    game_campaign_restore();
    CHECK(equals(game_campaign_display_name(), "Campanha traduzida"));
    game_campaign_suspend(); game_campaign_clear(); game_campaign_restore();
    CHECK(equals(game_campaign_display_name(), "Campanha traduzida"));

    const char *invalid[] = {
        "<campaign_localization version=\"2\"><name>Bad</name></campaign_localization>",
        "<campaign_localization><name>Bad</name></campaign_localization>",
        "<campaign_localization version=\"1\"><name>Bad</name>",
        "<campaign_localization version=\"1\"><name>Bad</description></campaign_localization>",
        "<wrong/>", "", "<!-- no root -->",
        "<campaign_localization version=\"1\"/><campaign_localization version=\"1\"/>",
        "<campaign_localization version=\"1\"><name>A</name><name>B</name></campaign_localization>",
        "<campaign_localization version=\"1\"><name/><name>B</name></campaign_localization>",
        "<campaign_localization version=\"1\"><name>A</name><scenario file=\"One\"/><scenario file=\"One\"/></campaign_localization>",
        "<campaign_localization version=\"1\"><name>A</name><scenario file=\"Absent\"/><scenario file=\"Absent\"/></campaign_localization>",
        "<campaign_localization version=\"1\"><name>A</name><mission first-scenario=\"One\"/><mission first-scenario=\"One\"/></campaign_localization>",
        "<campaign_localization version=\"1\"><name>A</name><scenario><name>B</name></scenario></campaign_localization>",
        "<campaign_localization version=\"1\"><name>A</name><mission><title>B</title></mission></campaign_localization>",
        "<campaign_localization version=\"1\"><scenario file=\"One\"><description>A</description><description/></scenario></campaign_localization>",
        "<campaign_localization version=\"1\"><mission first-scenario=\"One\"><title>A</title><title/></mission></campaign_localization>",
        "<campaign_localization version=\"1\"><unknown/></campaign_localization>",
        "<campaign_localization version=\"1\">Unexpected root text</campaign_localization>",
        "<campaign_localization version=\"1\"><scenario file=\"../One\"><name>Bad key</name></scenario></campaign_localization>"
    };
    for (unsigned int i = 0; i < sizeof(invalid) / sizeof(*invalid); i++) {
        char label[80]; snprintf(label, sizeof(label), "invalid overlay atomic fallback %u", i);
        begin_case(label, NULL); add_translation("pt-BR", invalid[i]); finish_fixture();
        check_preview(fixture_root, NULL); load();
        CHECK(equals(game_campaign_display_name(), "Canonical campaign"));
        CHECK(equals(game_campaign_display_scenario_name(0), "One"));
        CHECK(!campaign_localization_name());
    }

    begin_case("unknown keys ignored and empty fields fallback", NULL);
    add_translation("pt-BR", "<campaign_localization version=\"1\"><name/>"
        "<description> </description><scenario file=\"Absent\"><name>Ignored</name></scenario>"
        "<mission first-scenario=\"Two\"><title>Not the first</title></mission>"
        "<scenario file=\"Two\"><name>Translated two</name></scenario></campaign_localization>");
    finish_fixture(); check_preview(fixture_root, NULL);
    load(); CHECK(equals(game_campaign_display_name(), "Canonical campaign"));
    CHECK(equals(game_campaign_display_description(), "Canonical description"));
    CHECK(equals(game_campaign_display_mission_title(0), "First mission"));
    CHECK(equals(game_campaign_display_scenario_name(1), "Translated two"));

    const struct { const char *requested, *resolved, *manifest; int expected; } locales[] = {
        { "PT_br", "pt-BR", NULL, 1 },
        { "French", "fr", "<locales version=\"1\"><locale id=\"fr\" aliases=\"French\"/></locales>", 1 },
        { "Portuguese (Brazil)", "pt-BR", "<locales version=\"1\"><locale id=\"pt-BR\" aliases=\"Portuguese (Brazil)\"/></locales>", 1 },
        { "Português", "pt-BR", "<locales version=\"1\"><locale id=\"pt-BR\" aliases=\"Português\"/></locales>", 1 },
        { "Portuguese (Brazil)", "Portuguese (Brazil)", NULL, 1 },
        { "Português", "Português", NULL, 1 },
        { "", "pt-BR", "<locales version=\"1\"><locale id=\"pt-BR\" default-for=\"pt\"/></locales>", 1 },
        { "", "pt", NULL, 1 },
        { "pt", "pt-BR", "<locales version=\"1\"><locale id=\"pt-BR\" default-for=\"pt\"/></locales>", 0 },
        { "../pt-BR", "pt-BR", NULL, 0 },
        { ".", "pt-BR", NULL, 0 },
        { "..", "pt-BR", NULL, 0 },
        { ".. ", "pt-BR", "<locales version=\"1\"><locale id=\"pt-BR\" aliases=\".. \"/></locales>", 0 },
        { "Portuguese.", "pt-BR", "<locales version=\"1\"><locale id=\"pt-BR\" aliases=\"Portuguese.\"/></locales>", 0 },
        { "French", "fr", "<locales version=\"1\"><locale id=\"fr\" aliases=\"French\"/><locale id=\"fr-FR\" aliases=\"French\"/></locales>", 0 }
    };
    for (unsigned int i = 0; i < sizeof(locales) / sizeof(*locales); i++) {
        begin_case("locale selection", NULL); ui_language = locales[i].requested;
        add_translation(locales[i].resolved, localized);
        if (locales[i].manifest) add_file("localization/locales.xml", locales[i].manifest);
        finish_fixture();
        check_preview(fixture_root, locales[i].expected ? "Campanha traduzida" : NULL);
        load(); CHECK(equals(game_campaign_display_name(), locales[i].expected ? "Campanha traduzida" : "Canonical campaign"));
    }

    begin_case("reordered missions retain stem association", "<campaign version=\"1\"><description title=\"Reordered\"/>"
        "<missions><mission title=\"Second\" file=\"Three.mapx\"/><mission title=\"First\" file=\"One.mapx\"/></missions></campaign>");
    add_translation("pt-BR", localized); load();
    CHECK(equals(game_campaign_display_mission_title(0), "Second"));
    CHECK(equals(game_campaign_display_mission_title(1), "Missão inicial"));
    CHECK(equals(game_campaign_display_scenario_name(1), "Primeiro"));

    begin_case("changed first scenario does not misapply title", "<campaign version=\"1\"><description title=\"Changed\"/>"
        "<missions><mission title=\"New first\"><scenario file=\"Two.svx\"/><scenario file=\"One.mapx\"/></mission></missions></campaign>");
    add_translation("pt-BR", localized); load();
    CHECK(equals(game_campaign_display_mission_title(0), "New first"));
    CHECK(equals(game_campaign_display_scenario_name(1), "Primeiro"));

    begin_case("ambiguous stems reject entire overlay", "<campaign version=\"1\"><description title=\"Collision\"/>"
        "<missions><mission file=\"One.mapx\"/><mission file=\"One.svx\"/></missions></campaign>");
    add_file("scenario/One.svx", "synthetic"); add_translation("pt-BR", localized); finish_fixture();
    check_preview(fixture_root, NULL); load();
    CHECK(equals(game_campaign_display_name(), "Collision")); CHECK(!campaign_localization_name());

    begin_case("multiple dots in stem", "<campaign version=\"1\"><description title=\"Dots\"/>"
        "<missions><mission file=\"One.part.mapx\"/></missions></campaign>");
    add_file("scenario/One.part.mapx", "synthetic");
    add_translation("pt-BR", "<campaign_localization version=\"1\"><scenario file=\"One.part\"><name>Part</name></scenario></campaign_localization>");
    load(); CHECK(equals(game_campaign_display_scenario_name(0), "Part"));

    begin_case("same stem with saved scenario extension", "<campaign version=\"1\"><description title=\"Save scenario\"/>"
        "<missions><mission file=\"One.svx\"/></missions></campaign>");
    add_file("scenario/One.svx", "synthetic"); add_translation("pt-BR", localized); load();
    CHECK(equals(game_campaign_display_scenario_name(0), "Primeiro"));
    CHECK(equals(game_campaign_display_mission_title(0), "Missão inicial"));

    begin_case("exact directory wins over alias", NULL);
    add_translation("pt-BR", localized);
    add_translation("other", "<campaign_localization version=\"1\"><name>Wrong alias</name></campaign_localization>");
    add_file("localization/locales.xml", "<locales version=\"1\"><locale id=\"other\" aliases=\"pt-BR\"/></locales>");
    load(); CHECK(equals(game_campaign_display_name(), "Campanha traduzida"));

    const struct { const char *locale, *text; language_type language; } encodings[] = {
        { "fr", "Campagne française", LANGUAGE_FRENCH },
        { "ru", "Кампания", LANGUAGE_RUSSIAN },
        { "el", "Ελλάδα", LANGUAGE_GREEK },
        { "ja", "日本", LANGUAGE_JAPANESE }
    };
    for (unsigned int i = 0; i < sizeof(encodings) / sizeof(*encodings); i++) {
        begin_case("real non-Portuguese encoding", NULL);
        ui_language = encodings[i].locale;
        encoding_determine(encodings[i].language);
        char xml[512];
        snprintf(xml, sizeof(xml), "<campaign_localization version=\"1\"><name>%s</name></campaign_localization>",
            encodings[i].text);
        add_translation(ui_language, xml); load();
        CHECK(equals(game_campaign_display_name(), encodings[i].text));
    }

    begin_case("long display text preserves full getter and identity", NULL);
    char long_text[2048], long_xml[2300];
    memset(long_text, 'A', sizeof(long_text) - 1); long_text[sizeof(long_text) - 1] = 0;
    snprintf(long_xml, sizeof(long_xml), "<campaign_localization version=\"1\"><name>%s</name></campaign_localization>", long_text);
    add_translation("pt-BR", long_xml); finish_fixture();
    check_preview(fixture_root, long_text); load();
    CHECK(equals(game_campaign_display_name(), long_text)); check_canonical();

    begin_case("valid empty overlay keeps all canonical fields", NULL);
    add_translation("pt-BR", "<campaign_localization version=\"1\"/>"); finish_fixture();
    check_preview(fixture_root, NULL); load();
    CHECK(equals(game_campaign_display_name(), "Canonical campaign"));
    CHECK(equals(game_campaign_display_scenario_description(0), "One description"));

    begin_case("campaign switch clears previous translation", NULL); finish_fixture();
    check_preview(fixture_root, NULL); load();
    CHECK(equals(game_campaign_display_name(), "Canonical campaign"));
    CHECK(game_campaign_load(CAMPAIGN_ORIGINAL_NAME));
    CHECK(equals(game_campaign_display_name(), "Original campaign text"));
    CHECK(!campaign_localization_name());
    game_campaign_clear();
}

static void check_preview_preserves_active(const char *campaign, const char *expected, const char *active_campaign)
{
    const campaign_info *info = game_campaign_get_info();
    const campaign_scenario *scenario = game_campaign_get_scenario(0);
    const campaign_mission *mission = campaign_mission_current(0);
    const uint8_t *name = game_campaign_display_name();
    int reads = progression_reads;
    int writes = progression_writes;
    int updates = rank_updates;
    int opens = watched_opens;
    int closes = watched_closes;
    check_preview(campaign, expected);
    CHECK(game_campaign_is_active() && game_campaign_is_custom());
    CHECK(strcmp(game_campaign_get_name(), active_campaign) == 0);
    CHECK(game_campaign_get_info() == info);
    CHECK(game_campaign_get_scenario(0) == scenario);
    CHECK(campaign_mission_current(0) == mission);
    CHECK(game_campaign_display_name() == name);
    CHECK(equals(game_campaign_display_name(), "Campanha traduzida"));
    CHECK(equals(game_campaign_get_info()->name, "Canonical campaign"));
    CHECK(equals(game_campaign_get_scenario(0)->name, "One"));
    CHECK(equals(campaign_mission_current(0)->title, "First mission"));
    CHECK(game_campaign_get_info()->current_mission == 1);
    CHECK(game_campaign_get_info()->number_of_missions == 2);
    CHECK(campaign_mission_get_scenario(2) && !campaign_mission_get_scenario(3));
    CHECK(rank == 7 && rank_updates == updates);
    CHECK(progression_reads == reads && progression_writes == writes);
    CHECK(campaign_file_is_zip() == archive_mode);
    size_t size = 0;
    char *contents = game_campaign_load_file(CAMPAIGNS_DIRECTORY "/sentinel.txt", &size);
    CHECK(contents && size == strlen("active campaign") && memcmp(contents, "active campaign", size) == 0);
    free(contents);
    CHECK(watched_opens == opens && watched_closes == closes);
}

static void test_preview_isolation(void)
{
    begin_case("preview preserves active campaign and open archive", NULL);
    add_translation("pt-BR", localized);
    add_file("sentinel.txt", "active campaign");
    load();
    char active_campaign[512];
    snprintf(active_campaign, sizeof(active_campaign), "%s", fixture_root);
    snprintf(watched_path, sizeof(watched_path), "%s", fixture_root);
    watched_opens = watched_closes = 0;
    CHECK(campaign_file_open_zip());
    CHECK(watched_opens == archive_mode && watched_closes == 0);

    create_fixture(NULL);
    add_file("sentinel.txt", "preview campaign");
    add_translation("pt-BR", "<campaign_localization version=\"1\"><name>Outra campanha</name></campaign_localization>");
    add_translation("fr", "<campaign_localization version=\"1\"><name>Autre campagne</name></campaign_localization>");
    add_translation("broken", "<campaign_localization version=\"1\"><name>Never apply</name><scenario");
    finish_fixture();
    char preview_campaign[512];
    snprintf(preview_campaign, sizeof(preview_campaign), "%s", fixture_root);
    check_preview_preserves_active(preview_campaign, "Outra campanha", active_campaign);
    ui_language = "fr";
    check_preview_preserves_active(preview_campaign, "Autre campagne", active_campaign);
    ui_language = "de";
    check_preview_preserves_active(preview_campaign, NULL, active_campaign);
    ui_language = "broken";
    check_preview_preserves_active(preview_campaign, NULL, active_campaign);
    ui_language = "pt-BR";

    // Also read the other storage format while the active campaign remains open.
    archive_mode = !archive_mode;
    create_fixture(NULL);
    add_file("sentinel.txt", "other storage format");
    add_translation("pt-BR", "<campaign_localization version=\"1\"><name>Terceira campanha</name></campaign_localization>");
    finish_fixture();
    archive_mode = !archive_mode;
    check_preview_preserves_active(fixture_root, "Terceira campanha", active_campaign);
    check_preview_preserves_active(preview_campaign, "Outra campanha", active_campaign);
    check_preview_preserves_active("fixtures/not-a-campaign", NULL, active_campaign);
    check_preview_preserves_active(CAMPAIGN_ORIGINAL_NAME, NULL, active_campaign);
    campaign_file_close_zip();
    watched_path[0] = 0;
    game_campaign_clear();
}

int main(void)
{
    if (make_directory("fixtures") != 0 && errno != EEXIST) return 2;
    for (int run = 0; ; run++) {
        snprintf(run_root, sizeof(run_root), "fixtures/metadata_%03d", run);
        if (make_directory(run_root) == 0) break;
        if (errno != EEXIST) return 2;
    }
    for (archive_mode = 0; archive_mode < 2; archive_mode++) {
        test_matrix();
        test_preview_isolation();
    }
    printf("RESULT: %d cases, %d checks, %d failures (folder + ZIP, real encoding).\n", cases, checks, failures);
    return failures ? 1 : 0;
}

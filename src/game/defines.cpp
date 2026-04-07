#include "game/defines.h"
#include "game/time.h"
#include "core/crash_context.h"

extern "C" {
#include "core/file.h"
#include "core/log.h"
#include "core/xml_parser.h"
#include "game/mod_manager.h"
}

#include <array>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr const char *kActiveCalendarId = "default";
constexpr const char *kActiveMortalityTableId = "default";
constexpr int kHealthBuckets = 11;
constexpr int kAgeDecennia = 10;

struct CalendarDefinition {
    int ticks_per_day = 50;
    std::array<int, GAME_TIME_MONTHS_PER_YEAR> month_days = { 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16 };
    int has_month_days = 0;
};

struct MortalityDefinition {
    std::array<std::array<int, kAgeDecennia>, kHealthBuckets> values = {{
        {{20, 10, 5, 10, 20, 30, 50, 85, 100, 100}},
        {{15, 8, 4, 8, 16, 25, 45, 70, 90, 100}},
        {{10, 6, 2, 6, 12, 20, 30, 55, 80, 90}},
        {{5, 4, 0, 4, 8, 15, 25, 40, 65, 80}},
        {{3, 2, 0, 2, 6, 12, 20, 30, 50, 70}},
        {{2, 0, 0, 0, 4, 8, 15, 25, 40, 60}},
        {{1, 0, 0, 0, 2, 6, 12, 20, 30, 50}},
        {{0, 0, 0, 0, 0, 4, 8, 15, 20, 40}},
        {{0, 0, 0, 0, 0, 2, 6, 10, 15, 30}},
        {{0, 0, 0, 0, 0, 0, 4, 5, 10, 20}},
        {{0, 0, 0, 0, 0, 0, 0, 2, 5, 10}}
    }};
    std::array<int, kHealthBuckets> seen_buckets = { 0 };
};

struct DefinesDocument {
    std::unordered_map<std::string, CalendarDefinition> calendars;
    std::unordered_map<std::string, MortalityDefinition> mortality_tables;
};

struct DefinesParseState {
    DefinesDocument document;
    std::string filename;
    std::string current_calendar_id;
    std::string current_mortality_id;
    CalendarDefinition current_calendar;
    MortalityDefinition current_mortality;
    int parsing_calendar = 0;
    int parsing_mortality = 0;
    int saw_root = 0;
    int error = 0;
};

DefinesParseState g_parse_state;
CalendarDefinition g_active_calendar;
MortalityDefinition g_active_mortality;
std::string g_failure_reason;

static void set_failure_reason(const char *message, const char *detail = nullptr)
{
    if (detail && *detail) {
        g_failure_reason = std::string(message ? message : "") + "\n\n" + detail;
    } else {
        g_failure_reason = message ? message : "";
    }
}

static std::string trim_copy(const std::string &value)
{
    size_t start = 0;
    while (start < value.size() &&
        (value[start] == ' ' || value[start] == '\t' || value[start] == '\r' || value[start] == '\n')) {
        start++;
    }

    size_t end = value.size();
    while (end > start &&
        (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r' || value[end - 1] == '\n')) {
        end--;
    }

    return value.substr(start, end - start);
}

static int parse_int_strict(const std::string &text, int *out_value)
{
    if (!out_value) {
        return 0;
    }

    char *end = nullptr;
    const long parsed = strtol(text.c_str(), &end, 10);
    if (!end || *end != '\0' || parsed < INT_MIN || parsed > INT_MAX) {
        return 0;
    }

    *out_value = static_cast<int>(parsed);
    return 1;
}

template <size_t N>
static int parse_csv_ints(const char *value, std::array<int, N> &out_values, int require_positive)
{
    if (!value || !*value) {
        return 0;
    }

    std::string list = value;
    size_t start = 0;
    size_t index = 0;
    while (start <= list.size()) {
        size_t end = list.find(',', start);
        std::string token = trim_copy(list.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (token.empty() || index >= N) {
            return 0;
        }

        int parsed = 0;
        if (!parse_int_strict(token, &parsed)) {
            return 0;
        }
        if ((require_positive && parsed <= 0) || (!require_positive && parsed < 0)) {
            return 0;
        }

        out_values[index++] = parsed;
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }

    return index == N;
}

static void report_parse_error(const char *message, const char *detail = nullptr)
{
    char context[FILE_NAME_MAX + 128];
    if (detail && *detail) {
        snprintf(
            context,
            sizeof(context),
            "file=%s line=%d detail=%s",
            g_parse_state.filename.c_str(),
            xml_parser_get_current_line_number(),
            detail);
    } else {
        snprintf(
            context,
            sizeof(context),
            "file=%s line=%d",
            g_parse_state.filename.c_str(),
            xml_parser_get_current_line_number());
    }

    log_error(message, detail ? detail : g_parse_state.filename.c_str(), 0);
    error_context_report_error(message, context);
    g_parse_state.error = 1;
}

static int parse_defines_root()
{
    if (g_parse_state.saw_root) {
        report_parse_error("Defines XML contains duplicate root nodes");
        return 0;
    }

    g_parse_state.saw_root = 1;
    return 1;
}

static int parse_calendar()
{
    if (g_parse_state.parsing_calendar) {
        report_parse_error("Nested calendar nodes are not supported");
        return 0;
    }
    if (!xml_parser_has_attribute("id")) {
        report_parse_error("Calendar node is missing required attribute 'id'");
        return 0;
    }
    if (!xml_parser_has_attribute("ticks_per_day")) {
        report_parse_error("Calendar node is missing required attribute 'ticks_per_day'");
        return 0;
    }

    const std::string calendar_id = trim_copy(xml_parser_get_attribute_string("id"));
    if (calendar_id.empty()) {
        report_parse_error("Calendar node has an invalid id");
        return 0;
    }

    int ticks_per_day = 0;
    const char *ticks_text = xml_parser_get_attribute_string("ticks_per_day");
    if (!ticks_text || !parse_int_strict(ticks_text, &ticks_per_day) || ticks_per_day <= 0) {
        report_parse_error("Calendar node has an invalid ticks_per_day", ticks_text);
        return 0;
    }

    g_parse_state.current_calendar = {};
    g_parse_state.current_calendar.ticks_per_day = ticks_per_day;
    g_parse_state.current_calendar_id = calendar_id;
    g_parse_state.parsing_calendar = 1;
    return 1;
}

static void finish_calendar()
{
    if (!g_parse_state.parsing_calendar) {
        return;
    }

    if (!g_parse_state.current_calendar.has_month_days) {
        report_parse_error("Calendar node is missing required child node 'month_days'", g_parse_state.current_calendar_id.c_str());
        g_parse_state.parsing_calendar = 0;
        return;
    }

    if (g_parse_state.document.calendars.find(g_parse_state.current_calendar_id) != g_parse_state.document.calendars.end()) {
        report_parse_error("Defines XML contains duplicate calendar ids", g_parse_state.current_calendar_id.c_str());
        g_parse_state.parsing_calendar = 0;
        return;
    }

    g_parse_state.document.calendars.emplace(g_parse_state.current_calendar_id, g_parse_state.current_calendar);
    g_parse_state.parsing_calendar = 0;
}

static int parse_month_days()
{
    if (!g_parse_state.parsing_calendar) {
        report_parse_error("month_days must appear inside calendar");
        return 0;
    }
    if (!xml_parser_has_attribute("values")) {
        report_parse_error("month_days node is missing required attribute 'values'");
        return 0;
    }

    std::array<int, GAME_TIME_MONTHS_PER_YEAR> values = { 0 };
    const char *values_text = xml_parser_get_attribute_string("values");
    if (!parse_csv_ints(values_text, values, 1)) {
        report_parse_error("month_days values must contain exactly 12 positive integers", values_text);
        return 0;
    }

    g_parse_state.current_calendar.month_days = values;
    g_parse_state.current_calendar.has_month_days = 1;
    return 1;
}

static int parse_mortality_table()
{
    if (g_parse_state.parsing_mortality) {
        report_parse_error("Nested mortality_table nodes are not supported");
        return 0;
    }
    if (!xml_parser_has_attribute("id")) {
        report_parse_error("mortality_table node is missing required attribute 'id'");
        return 0;
    }

    const std::string mortality_id = trim_copy(xml_parser_get_attribute_string("id"));
    if (mortality_id.empty()) {
        report_parse_error("mortality_table node has an invalid id");
        return 0;
    }

    g_parse_state.current_mortality = {};
    g_parse_state.current_mortality_id = mortality_id;
    g_parse_state.parsing_mortality = 1;
    return 1;
}

static void finish_mortality_table()
{
    if (!g_parse_state.parsing_mortality) {
        return;
    }

    for (int bucket = 0; bucket < kHealthBuckets; bucket++) {
        if (!g_parse_state.current_mortality.seen_buckets[static_cast<size_t>(bucket)]) {
            report_parse_error("mortality_table is missing a required health bucket", g_parse_state.current_mortality_id.c_str());
            g_parse_state.parsing_mortality = 0;
            return;
        }
    }

    if (g_parse_state.document.mortality_tables.find(g_parse_state.current_mortality_id) != g_parse_state.document.mortality_tables.end()) {
        report_parse_error("Defines XML contains duplicate mortality_table ids", g_parse_state.current_mortality_id.c_str());
        g_parse_state.parsing_mortality = 0;
        return;
    }

    g_parse_state.document.mortality_tables.emplace(g_parse_state.current_mortality_id, g_parse_state.current_mortality);
    g_parse_state.parsing_mortality = 0;
}

static int parse_health_row()
{
    if (!g_parse_state.parsing_mortality) {
        report_parse_error("health must appear inside mortality_table");
        return 0;
    }
    if (!xml_parser_has_attribute("bucket")) {
        report_parse_error("health node is missing required attribute 'bucket'");
        return 0;
    }
    if (!xml_parser_has_attribute("values")) {
        report_parse_error("health node is missing required attribute 'values'");
        return 0;
    }

    const char *bucket_text = xml_parser_get_attribute_string("bucket");
    int bucket = 0;
    if (!bucket_text || !parse_int_strict(bucket_text, &bucket) || bucket < 0 || bucket >= kHealthBuckets) {
        report_parse_error("health node has an invalid bucket", bucket_text);
        return 0;
    }
    if (g_parse_state.current_mortality.seen_buckets[static_cast<size_t>(bucket)]) {
        report_parse_error("mortality_table contains duplicate health buckets", bucket_text);
        return 0;
    }

    std::array<int, kAgeDecennia> values = { 0 };
    const char *values_text = xml_parser_get_attribute_string("values");
    if (!parse_csv_ints(values_text, values, 0)) {
        report_parse_error("health values must contain exactly 10 non-negative integers", values_text);
        return 0;
    }

    g_parse_state.current_mortality.values[static_cast<size_t>(bucket)] = values;
    g_parse_state.current_mortality.seen_buckets[static_cast<size_t>(bucket)] = 1;
    return 1;
}

static const xml_parser_element XML_ELEMENTS[] = {
    { "defines", parse_defines_root, nullptr, nullptr, nullptr },
    { "calendar", parse_calendar, finish_calendar, "defines", nullptr },
    { "month_days", parse_month_days, nullptr, "calendar", nullptr },
    { "mortality_table", parse_mortality_table, finish_mortality_table, "defines", nullptr },
    { "health", parse_health_row, nullptr, "mortality_table", nullptr }
};

static int load_file_to_buffer(const char *filename, std::vector<char> &buffer)
{
    FILE *fp = file_open(filename, "rb");
    if (!fp) {
        log_error("Unable to open defines xml", filename, 0);
        set_failure_reason("Failed to load gameplay defines.", filename);
        return 0;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        file_close(fp);
        log_error("Unable to seek defines xml", filename, 0);
        set_failure_reason("Failed to load gameplay defines.", filename);
        return 0;
    }

    long size = ftell(fp);
    if (size < 0) {
        file_close(fp);
        log_error("Unable to size defines xml", filename, 0);
        set_failure_reason("Failed to load gameplay defines.", filename);
        return 0;
    }

    rewind(fp);
    buffer.resize(static_cast<size_t>(size));
    const size_t read = buffer.empty() ? 0 : fread(buffer.data(), 1, buffer.size(), fp);
    file_close(fp);

    if (read != buffer.size()) {
        log_error("Unable to read defines xml", filename, 0);
        set_failure_reason("Failed to load gameplay defines.", filename);
        return 0;
    }

    return 1;
}

static int parse_defines_file(const char *filename, DefinesDocument &document_out)
{
    std::vector<char> buffer;
    if (!load_file_to_buffer(filename, buffer)) {
        return 0;
    }

    g_parse_state = {};
    g_parse_state.filename = filename;

    if (!xml_parser_init(XML_ELEMENTS, static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])), 1)) {
        log_error("Unable to initialize defines parser", filename, 0);
        set_failure_reason("Failed to load gameplay defines.", filename);
        return 0;
    }

    const ErrorContextScope scope("Defines XML", filename);
    const int parsed = xml_parser_parse(buffer.data(), static_cast<unsigned int>(buffer.size()), 1);
    xml_parser_free();

    if (!parsed || g_parse_state.error || !g_parse_state.saw_root) {
        set_failure_reason("Failed to load gameplay defines.", filename);
        return 0;
    }

    document_out = std::move(g_parse_state.document);
    return 1;
}

static void merge_document(
    const DefinesDocument &source,
    std::unordered_map<std::string, CalendarDefinition> &calendars,
    std::unordered_map<std::string, MortalityDefinition> &mortality_tables)
{
    for (const auto &entry : source.calendars) {
        calendars[entry.first] = entry.second;
    }
    for (const auto &entry : source.mortality_tables) {
        mortality_tables[entry.first] = entry.second;
    }
}

static int load_and_merge_defines()
{
    std::unordered_map<std::string, CalendarDefinition> calendars;
    std::unordered_map<std::string, MortalityDefinition> mortality_tables;

    const int mod_count = mod_manager_get_mod_count();
    for (int i = 0; i < mod_count; i++) {
        const char *mod_path = mod_manager_get_mod_path_at(i);
        if (!mod_path || !*mod_path) {
            continue;
        }

        char full_path[FILE_NAME_MAX] = { 0 };
        if (snprintf(full_path, FILE_NAME_MAX, "%sdefines.xml", mod_path) >= FILE_NAME_MAX) {
            set_failure_reason("Defines path is too long.", mod_path);
            return 0;
        }

        if (!file_exists(full_path, NOT_LOCALIZED)) {
            continue;
        }

        DefinesDocument document;
        if (!parse_defines_file(full_path, document)) {
            return 0;
        }

        merge_document(document, calendars, mortality_tables);
    }

    const auto calendar_it = calendars.find(kActiveCalendarId);
    if (calendar_it == calendars.end()) {
        set_failure_reason("Failed to load gameplay defines.", "No merged calendar/default entry was found.");
        return 0;
    }

    const auto mortality_it = mortality_tables.find(kActiveMortalityTableId);
    if (mortality_it == mortality_tables.end()) {
        set_failure_reason("Failed to load gameplay defines.", "No merged mortality_table/default entry was found.");
        return 0;
    }

    g_active_calendar = calendar_it->second;
    g_active_mortality = mortality_it->second;
    return 1;
}

static int days_before_month(int month)
{
    int total_days = 0;
    for (int i = 0; i < month; i++) {
        total_days += g_active_calendar.month_days[static_cast<size_t>(i)];
    }
    return total_days;
}

} // namespace

extern "C" int game_defines_load(void)
{
    g_failure_reason.clear();
    return load_and_merge_defines();
}

extern "C" const char *game_defines_get_failure_reason(void)
{
    return g_failure_reason.c_str();
}

extern "C" int game_defines_ticks_per_day(void)
{
    return g_active_calendar.ticks_per_day;
}

extern "C" int game_defines_days_in_month(int month)
{
    if (month < 0 || month >= GAME_TIME_MONTHS_PER_YEAR) {
        return 0;
    }
    return g_active_calendar.month_days[static_cast<size_t>(month)];
}

extern "C" int game_defines_days_per_year(void)
{
    return days_before_month(GAME_TIME_MONTHS_PER_YEAR);
}

extern "C" int game_defines_ticks_per_month(int month)
{
    return game_defines_ticks_per_day() * game_defines_days_in_month(month);
}

extern "C" int game_defines_ticks_per_year(void)
{
    return game_defines_ticks_per_day() * game_defines_days_per_year();
}

extern "C" int game_defines_is_last_day_of_month(int month, int day)
{
    const int days_in_month = game_defines_days_in_month(month);
    return days_in_month > 0 && day == days_in_month - 1;
}

extern "C" int game_defines_is_last_day_of_year(int month, int day)
{
    return month == GAME_TIME_MONTHS_PER_YEAR - 1 && game_defines_is_last_day_of_month(month, day);
}

extern "C" int game_defines_mortality_percentage(int health_bucket, int age_decennium)
{
    if (health_bucket < 0) {
        health_bucket = 0;
    } else if (health_bucket >= kHealthBuckets) {
        health_bucket = kHealthBuckets - 1;
    }

    if (age_decennium < 0 || age_decennium >= kAgeDecennia) {
        return 0;
    }

    return g_active_mortality.values[static_cast<size_t>(health_bucket)][static_cast<size_t>(age_decennium)];
}

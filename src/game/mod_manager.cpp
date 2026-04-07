#include "game/mod_manager.h"
#include "core/crash_context.h"

extern "C" {
#include "assets/assets.h"
#include "core/dir.h"
#include "core/file.h"
#include "core/log.h"
#include "core/xml_parser.h"
#include "platform/file_manager.h"
}

#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

constexpr const char *kDefaultModListFileName = "mod-list";
constexpr const char *kDefaultModListXml =
    "<mod_list>\r\n"
    "  <mod name=\"Julius\" />\r\n"
    "  <mod name=\"Augustus\" />\r\n"
    "  <mod name=\"Vespasian\" />\r\n"
    "</mod_list>\r\n";

struct ModListParseState {
    std::vector<std::string> mods;
    int error = 0;
    int saw_root = 0;
};

std::string g_mod_name = "Vespasian";
std::string g_mod_path = "Mods/Vespasian/";
std::string g_graphics_path = "Mods/Vespasian/Graphics/";
std::string g_augustus_graphics_path = "Mods/Augustus/Graphics/";
std::string g_julius_graphics_path = "Mods/Julius/Graphics/";
std::vector<std::string> g_mod_names = { "Julius", "Augustus", "Vespasian" };
std::vector<std::string> g_mod_paths = { "Mods/Julius/", "Mods/Augustus/", "Mods/Vespasian/" };
std::vector<std::string> g_graphics_paths = {
    "Mods/Julius/Graphics/",
    "Mods/Augustus/Graphics/",
    "Mods/Vespasian/Graphics/"
};
std::string g_failure_reason;
ModListParseState g_parse_state;

static int stop_on_first_entry(const char *name, long unused)
{
    (void) name;
    (void) unused;
    return LIST_MATCH;
}

static int validate_directory_path(const char *path)
{
    return static_cast<int>(static_cast<bool>(
        platform_file_manager_list_directory_contents(path, TYPE_DIR | TYPE_FILE, 0, stop_on_first_entry) != LIST_ERROR));
}

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

static std::string build_mod_path(const std::string &mod_name)
{
    return "Mods/" + mod_name + "/";
}

static std::string build_graphics_path(const std::string &mod_name)
{
    return build_mod_path(mod_name) + "Graphics/";
}

static void rebuild_legacy_selected_mod_paths()
{
    if (!g_mod_names.empty()) {
        g_mod_name = g_mod_names.back();
    } else if (g_mod_name.empty()) {
        g_mod_name = "Vespasian";
    }

    g_mod_path = build_mod_path(g_mod_name);
    g_graphics_path = build_graphics_path(g_mod_name);
}

static void rebuild_mod_lists()
{
    g_mod_paths.clear();
    g_graphics_paths.clear();
    for (const std::string &mod_name : g_mod_names) {
        g_mod_paths.push_back(build_mod_path(mod_name));
        g_graphics_paths.push_back(build_graphics_path(mod_name));
    }
    rebuild_legacy_selected_mod_paths();
}

static int load_file_to_buffer(const char *filename, std::vector<char> &buffer)
{
    FILE *fp = file_open(filename, "rb");
    if (!fp) {
        log_error("Unable to open mod list file", filename, 0);
        set_failure_reason("Failed to load mod list.", filename);
        return 0;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        file_close(fp);
        log_error("Unable to seek mod list file", filename, 0);
        set_failure_reason("Failed to load mod list.", filename);
        return 0;
    }

    long size = ftell(fp);
    if (size < 0) {
        file_close(fp);
        log_error("Unable to size mod list file", filename, 0);
        set_failure_reason("Failed to load mod list.", filename);
        return 0;
    }

    rewind(fp);
    buffer.resize(static_cast<size_t>(size));
    const size_t read = buffer.empty() ? 0 : fread(buffer.data(), 1, buffer.size(), fp);
    file_close(fp);

    if (read != buffer.size()) {
        log_error("Unable to read mod list file", filename, 0);
        set_failure_reason("Failed to load mod list.", filename);
        return 0;
    }

    return 1;
}

static int parse_mod_list_root()
{
    if (g_parse_state.saw_root) {
        g_parse_state.error = 1;
        log_error("Duplicate mod list root node", 0, 0);
        return 0;
    }

    g_parse_state.saw_root = 1;
    return 1;
}

static int parse_mod_entry()
{
    if (!xml_parser_has_attribute("name")) {
        g_parse_state.error = 1;
        log_error("Mod list entry is missing required attribute 'name'", 0, 0);
        return 0;
    }

    std::string mod_name = trim_copy(xml_parser_get_attribute_string("name"));
    if (mod_name.empty()) {
        g_parse_state.error = 1;
        log_error("Mod list entry has an invalid name", 0, 0);
        return 0;
    }

    g_parse_state.mods.push_back(std::move(mod_name));
    return 1;
}

static const xml_parser_element XML_ELEMENTS[] = {
    { "mod_list", parse_mod_list_root, nullptr, nullptr, nullptr },
    { "mod", parse_mod_entry, nullptr, "mod_list", nullptr }
};

static int parse_mod_list_file(const char *filename, std::vector<std::string> &mods_out)
{
    std::vector<char> buffer;
    if (!load_file_to_buffer(filename, buffer)) {
        return 0;
    }

    g_parse_state = {};
    if (!xml_parser_init(XML_ELEMENTS, static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])), 1)) {
        log_error("Unable to initialize mod list parser", filename, 0);
        set_failure_reason("Failed to load mod list.", filename);
        return 0;
    }

    const ErrorContextScope scope("Mod list XML", filename);
    const int parsed = xml_parser_parse(buffer.data(), static_cast<unsigned int>(buffer.size()), 1);
    xml_parser_free();

    if (!parsed || g_parse_state.error || !g_parse_state.saw_root || g_parse_state.mods.empty()) {
        error_context_report_error("Invalid mod list XML", filename);
        set_failure_reason("Failed to load mod list.", filename);
        return 0;
    }

    mods_out = std::move(g_parse_state.mods);
    return 1;
}

static int write_default_mod_list()
{
    const char *filename = dir_append_location(kDefaultModListFileName, PATH_LOCATION_CONFIG);
    if (!filename || !*filename) {
        set_failure_reason("Failed to create default mod list.", "Config directory is unavailable.");
        return 0;
    }

    FILE *fp = file_open(filename, "wb");
    if (!fp) {
        log_error("Unable to create default mod list file", filename, 0);
        set_failure_reason("Failed to create default mod list.", filename);
        return 0;
    }

    const size_t xml_length = strlen(kDefaultModListXml);
    const size_t written = fwrite(kDefaultModListXml, 1, xml_length, fp);
    file_close(fp);

    if (written != xml_length) {
        log_error("Unable to write default mod list file", filename, 0);
        set_failure_reason("Failed to create default mod list.", filename);
        return 0;
    }

    return 1;
}

static int ensure_mod_list_file_exists()
{
    if (dir_get_file_at_location(kDefaultModListFileName, PATH_LOCATION_CONFIG)) {
        return 1;
    }

    return write_default_mod_list();
}

static int validate_loaded_mod_names(const std::vector<std::string> &mods)
{
    if (mods.empty()) {
        set_failure_reason("Mod list must contain at least one mod.");
        return 0;
    }

    std::unordered_set<std::string> normalized_names;
    for (const std::string &mod_name : mods) {
        std::string normalized = mod_name;
        for (char &ch : normalized) {
            if (ch >= 'A' && ch <= 'Z') {
                ch = static_cast<char>(ch - 'A' + 'a');
            }
        }

        if (!normalized_names.insert(normalized).second) {
            set_failure_reason("Mod list contains duplicate mods.", mod_name.c_str());
            return 0;
        }

        const std::string mod_path = build_mod_path(mod_name);
        if (!validate_directory_path(mod_path.c_str())) {
            set_failure_reason("Listed mod folder was not found.", mod_path.c_str());
            return 0;
        }
    }

    return 1;
}

} // namespace

extern "C" void mod_manager_set_mod_name(const char *mod_name)
{
    if (mod_name && *mod_name) {
        g_mod_name = mod_name;
    } else {
        g_mod_name = "Vespasian";
    }
    rebuild_legacy_selected_mod_paths();
}

extern "C" int mod_manager_load_mod_list(void)
{
    g_failure_reason.clear();

    if (!ensure_mod_list_file_exists()) {
        return 0;
    }

    const char *filename = dir_get_file_at_location(kDefaultModListFileName, PATH_LOCATION_CONFIG);
    if (!filename || !*filename) {
        set_failure_reason("Failed to load mod list.", "Config mod-list file was not found.");
        return 0;
    }

    std::vector<std::string> loaded_mods;
    if (!parse_mod_list_file(filename, loaded_mods)) {
        return 0;
    }

    if (!validate_loaded_mod_names(loaded_mods)) {
        return 0;
    }

    g_mod_names = std::move(loaded_mods);
    rebuild_mod_lists();
    return 1;
}

extern "C" const char *mod_manager_get_failure_reason(void)
{
    return g_failure_reason.c_str();
}

extern "C" const char *mod_manager_get_mod_name(void)
{
    return g_mod_name.c_str();
}

extern "C" const char *mod_manager_get_mod_path(void)
{
    return g_mod_path.c_str();
}

extern "C" const char *mod_manager_get_graphics_path(void)
{
    return g_graphics_path.c_str();
}

extern "C" const char *mod_manager_get_augustus_graphics_path(void)
{
    return g_augustus_graphics_path.c_str();
}

extern "C" const char *mod_manager_get_julius_graphics_path(void)
{
    return g_julius_graphics_path.c_str();
}

extern "C" int mod_manager_get_mod_count(void)
{
    return static_cast<int>(g_mod_names.size());
}

extern "C" const char *mod_manager_get_mod_name_at(int index)
{
    if (index < 0 || index >= static_cast<int>(g_mod_names.size())) {
        return nullptr;
    }
    return g_mod_names[static_cast<size_t>(index)].c_str();
}

extern "C" const char *mod_manager_get_mod_path_at(int index)
{
    if (index < 0 || index >= static_cast<int>(g_mod_paths.size())) {
        return nullptr;
    }
    return g_mod_paths[static_cast<size_t>(index)].c_str();
}

extern "C" const char *mod_manager_get_graphics_path_at(int index)
{
    if (index < 0 || index >= static_cast<int>(g_graphics_paths.size())) {
        return nullptr;
    }
    return g_graphics_paths[static_cast<size_t>(index)].c_str();
}

extern "C" int mod_manager_validate_mod_path(void)
{
    return validate_directory_path(g_mod_path.c_str());
}

extern "C" int mod_manager_validate_graphics_path(void)
{
    const int has_mod_graphics = validate_directory_path(g_graphics_path.c_str()) != 0;
    const int has_augustus_graphics = validate_directory_path(g_augustus_graphics_path.c_str()) != 0;
    const int has_julius_graphics = validate_directory_path(g_julius_graphics_path.c_str()) != 0;
    const int has_root_graphics = validate_directory_path(ASSETS_DIRECTORY "/" ASSETS_IMAGE_PATH) != 0;
    return has_mod_graphics || has_augustus_graphics || has_julius_graphics || has_root_graphics;
}

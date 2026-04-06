#include "map/tile_type_registry_internal.h"

extern "C" {
#include "core/dir.h"
#include "core/file.h"
#include "core/log.h"
#include "core/xml_parser.h"
#include "map/tile_runtime_api.h"
}

#include <cstdio>
#include <cstring>
#include <vector>

namespace tile_type_registry_impl {

static std::string trim_copy(const std::string &value)
{
    size_t start = 0;
    while (start < value.size() && (value[start] == ' ' || value[start] == '\t' || value[start] == '\r' || value[start] == '\n')) {
        start++;
    }

    size_t end = value.size();
    while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r' || value[end - 1] == '\n')) {
        end--;
    }
    return value.substr(start, end - start);
}

static int equals_ignore_case_ascii(char left, char right)
{
    if (left >= 'A' && left <= 'Z') {
        left = static_cast<char>(left - 'A' + 'a');
    }
    if (right >= 'A' && right <= 'Z') {
        right = static_cast<char>(right - 'A' + 'a');
    }
    return left == right;
}

static int starts_with_ignore_case_ascii(const std::string &value, const char *prefix)
{
    if (!prefix) {
        return 0;
    }

    const size_t prefix_length = strlen(prefix);
    if (value.size() < prefix_length) {
        return 0;
    }

    for (size_t i = 0; i < prefix_length; i++) {
        if (!equals_ignore_case_ascii(value[i], prefix[i])) {
            return 0;
        }
    }
    return 1;
}

static int ends_with_ignore_case_ascii(const std::string &value, const char *suffix)
{
    if (!suffix) {
        return 0;
    }

    const size_t suffix_length = strlen(suffix);
    if (value.size() < suffix_length) {
        return 0;
    }

    const size_t start = value.size() - suffix_length;
    for (size_t i = 0; i < suffix_length; i++) {
        if (!equals_ignore_case_ascii(value[start + i], suffix[i])) {
            return 0;
        }
    }
    return 1;
}

static std::string normalize_graphics_path(const char *value)
{
    std::string normalized = trim_copy(value ? value : "");
    if (normalized.empty()) {
        return {};
    }

    for (char &ch : normalized) {
        if (ch == '/') {
            ch = '\\';
        }
    }

    std::string collapsed;
    collapsed.reserve(normalized.size());
    char previous = '\0';
    for (char ch : normalized) {
        if (ch == '\\' && previous == '\\') {
            continue;
        }
        collapsed.push_back(ch);
        previous = ch;
    }
    normalized = collapsed;

    if (starts_with_ignore_case_ascii(normalized, "graphics\\") ||
        starts_with_ignore_case_ascii(normalized, "\\") ||
        ends_with_ignore_case_ascii(normalized, ".xml") ||
        normalized.back() == '\\') {
        return {};
    }

    return normalized;
}

static int parse_tile_root()
{
    if (!xml_parser_has_attribute("type")) {
        log_error("Tile xml is missing required attribute 'type'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *type_attr = xml_parser_get_attribute_string("type");
    TileKind kind = tile_kind_from_attr(type_attr);
    if (kind == TileKind::None) {
        log_error("Unknown Tile xml type", type_attr, 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition = std::make_unique<TileType>(kind, type_attr);
    return 1;
}

static int parse_graphics()
{
    if (!g_parse_state.definition) {
        log_error("Encountered tile graphics definition before tile root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.definition->has_graphics()) {
        log_error("Tile xml contains duplicate graphics nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.saw_graphics = 1;
    g_parse_state.parsing_graphics = 1;
    g_parse_state.current_graphics_has_path = 0;
    return 1;
}

static void finish_graphics()
{
    if (!g_parse_state.parsing_graphics) {
        return;
    }
    if (!g_parse_state.current_graphics_has_path) {
        log_error("Tile graphics is missing required child node 'path'", 0, 0);
        g_parse_state.error = 1;
    }
    g_parse_state.parsing_graphics = 0;
    g_parse_state.current_graphics_has_path = 0;
}

static int parse_graphics_path()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_graphics) {
        log_error("Encountered tile graphics path outside graphics node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("value")) {
        log_error("Tile graphics path is missing required attribute 'value'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    std::string normalized_path = normalize_graphics_path(xml_parser_get_attribute_string("value"));
    if (normalized_path.empty()) {
        log_error("Unsupported Tile graphics path", xml_parser_get_attribute_string("value"), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->set_graphics_path(std::move(normalized_path));
    g_parse_state.current_graphics_has_path = 1;
    return 1;
}

static const xml_parser_element XML_ELEMENTS[] = {
    { "tile", parse_tile_root, nullptr, nullptr, nullptr },
    { "graphics", parse_graphics, finish_graphics, "tile", nullptr },
    { "path", parse_graphics_path, nullptr, "graphics", nullptr }
};

static int load_file_to_buffer(const char *filename, std::vector<char> &buffer)
{
    FILE *fp = file_open(filename, "rb");
    if (!fp) {
        log_error("Unable to open Tile xml", filename, 0);
        return 0;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        file_close(fp);
        log_error("Unable to seek Tile xml", filename, 0);
        return 0;
    }

    long size = ftell(fp);
    if (size < 0) {
        file_close(fp);
        log_error("Unable to size Tile xml", filename, 0);
        return 0;
    }
    rewind(fp);

    buffer.resize(static_cast<size_t>(size));
    const size_t read = fread(buffer.data(), 1, buffer.size(), fp);
    file_close(fp);
    if (read != buffer.size()) {
        log_error("Unable to read Tile xml", filename, 0);
        return 0;
    }
    return 1;
}

static int parse_definition_file(const char *filename)
{
    std::vector<char> buffer;
    if (!load_file_to_buffer(filename, buffer)) {
        return 0;
    }

    g_parse_state = {};
    if (!xml_parser_init(XML_ELEMENTS, static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])), 1)) {
        log_error("Unable to initialize Tile xml parser", filename, 0);
        return 0;
    }

    const int parsed = xml_parser_parse(buffer.data(), static_cast<unsigned int>(buffer.size()), 1);
    xml_parser_free();
    if (!parsed || g_parse_state.error || !g_parse_state.definition || !g_parse_state.saw_graphics) {
        if (!g_parse_state.saw_graphics) {
            log_error("Tile xml is missing required graphics node", filename, 0);
        }
        return 0;
    }

    g_tile_types[static_cast<size_t>(g_parse_state.definition->kind())] = std::move(g_parse_state.definition);
    return 1;
}

}

extern "C" int tile_type_registry_load(void)
{
    using namespace tile_type_registry_impl;

    refresh_tile_type_path();

    for (std::unique_ptr<TileType> &definition : g_tile_types) {
        definition.reset();
    }

    const dir_listing *files = dir_find_files_with_extension(g_tile_type_path.c_str(), "xml");
    if (!files || files->num_files <= 0) {
        log_error("No Tile xml files found in", g_tile_type_path.c_str(), 0);
        return 0;
    }

    for (int i = 0; i < files->num_files; i++) {
        char full_path[FILE_NAME_MAX];
        snprintf(full_path, FILE_NAME_MAX, "%s%s", g_tile_type_path.c_str(), files->files[i].name);
        if (!parse_definition_file(full_path)) {
            log_error("Unable to parse Tile xml", full_path, 0);
            return 0;
        }
    }

    tile_runtime_reset();
    return 1;
}

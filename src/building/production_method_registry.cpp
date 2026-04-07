#include "building/production_method_registry.h"

#include "core/crash_context.h"

extern "C" {
#include "core/dir.h"
#include "core/file.h"
#include "core/log.h"
#include "core/xml_parser.h"
#include "game/mod_manager.h"
}

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace building_type_registry_impl {

std::string g_production_method_path;

namespace {

struct ParseState {
    std::unique_ptr<ProductionMethod> definition;
    int saw_kind = 0;
    int saw_output = 0;
    int error = 0;
};

std::unordered_map<std::string, std::unique_ptr<ProductionMethod>> g_production_methods;
ParseState g_parse_state;

std::string trim_copy(const std::string &value)
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

int equals_ignore_case_ascii(char left, char right)
{
    if (left >= 'A' && left <= 'Z') {
        left = static_cast<char>(left - 'A' + 'a');
    }
    if (right >= 'A' && right <= 'Z') {
        right = static_cast<char>(right - 'A' + 'a');
    }
    return left == right;
}

int ends_with_ignore_case_ascii(const std::string &value, const char *suffix)
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

std::string normalize_definition_path(const char *value)
{
    std::string normalized = trim_copy(value ? value : "");
    if (normalized.empty()) {
        return std::string();
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

    if (!normalized.empty() && normalized.front() == '\\') {
        return std::string();
    }
    if (!normalized.empty() && normalized.back() == '\\') {
        return std::string();
    }
    if (ends_with_ignore_case_ascii(normalized, ".xml")) {
        normalized.resize(normalized.size() - 4);
    }
    return normalized;
}

int load_file_to_buffer(const char *filename, std::vector<char> &buffer)
{
    FILE *fp = file_open(filename, "rb");
    if (!fp) {
        log_error("Unable to open ProductionMethod xml", filename, 0);
        return 0;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        file_close(fp);
        log_error("Unable to seek ProductionMethod xml", filename, 0);
        return 0;
    }

    long size = ftell(fp);
    if (size < 0) {
        file_close(fp);
        log_error("Unable to size ProductionMethod xml", filename, 0);
        return 0;
    }
    rewind(fp);

    buffer.resize(static_cast<size_t>(size));
    const size_t read = fread(buffer.data(), 1, buffer.size(), fp);
    file_close(fp);
    if (read != buffer.size()) {
        log_error("Unable to read ProductionMethod xml", filename, 0);
        return 0;
    }
    return 1;
}

resource_type parse_resource_type_name(const char *name)
{
    if (!name || !*name) {
        return RESOURCE_NONE;
    }

    const std::string normalized_name = trim_copy(name);
    if (normalized_name.empty()) {
        return RESOURCE_NONE;
    }

    for (resource_type type = RESOURCE_MIN; type < RESOURCE_MAX; type = static_cast<resource_type>(type + 1)) {
        resource_data *data = resource_get_data(type);
        if (!data || !data->xml_attr_name) {
            continue;
        }
        if (xml_parser_compare_multiple(data->xml_attr_name, normalized_name.c_str())) {
            return type;
        }
    }
    return RESOURCE_NONE;
}

int parse_root()
{
    if (!g_parse_state.definition) {
        g_parse_state.definition = std::make_unique<ProductionMethod>(std::string());
    }
    return 1;
}

int parse_kind()
{
    if (!g_parse_state.definition) {
        log_error("Encountered ProductionMethod kind before root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_kind) {
        log_error("ProductionMethod xml contains duplicate kind nodes", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("value")) {
        log_error("ProductionMethod kind is missing required attribute 'value'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *kind_text = xml_parser_get_attribute_string("value");
    if (kind_text && strcmp(kind_text, "farm") == 0) {
        g_parse_state.definition->set_kind(ProductionMethodKind::Farm);
    } else if (kind_text && strcmp(kind_text, "workshop") == 0) {
        g_parse_state.definition->set_kind(ProductionMethodKind::Workshop);
    } else {
        log_error("Unsupported ProductionMethod kind", kind_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.saw_kind = 1;
    return 1;
}

int parse_output()
{
    if (!g_parse_state.definition) {
        log_error("Encountered ProductionMethod output before root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_output) {
        log_error("ProductionMethod xml contains duplicate output nodes", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("resource")) {
        log_error("ProductionMethod output is missing required attribute 'resource'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *resource_text = xml_parser_get_attribute_string("resource");
    const resource_type resource = parse_resource_type_name(resource_text);
    if (resource == RESOURCE_NONE) {
        log_error("Unsupported ProductionMethod output resource", resource_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->set_output_resource(resource);
    g_parse_state.saw_output = 1;
    return 1;
}

int parse_input()
{
    if (!g_parse_state.definition) {
        log_error("Encountered ProductionMethod input before root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("resource")) {
        log_error("ProductionMethod input is missing required attribute 'resource'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("amount")) {
        log_error("ProductionMethod input is missing required attribute 'amount'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *resource_text = xml_parser_get_attribute_string("resource");
    const resource_type resource = parse_resource_type_name(resource_text);
    if (resource == RESOURCE_NONE) {
        log_error("Unsupported ProductionMethod input resource", resource_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const int amount = xml_parser_get_attribute_int("amount");
    if (amount <= 0) {
        log_error("Unsupported ProductionMethod input amount", xml_parser_get_attribute_string("amount"), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->add_input({ resource, amount });
    return 1;
}

const xml_parser_element XML_ELEMENTS[] = {
    { "production_method", parse_root, nullptr, nullptr, nullptr },
    { "kind", parse_kind, nullptr, "production_method", nullptr },
    { "output", parse_output, nullptr, "production_method", nullptr },
    { "input", parse_input, nullptr, "production_method", nullptr }
};

int parse_definition_file(const char *filename, const char *definition_path)
{
    ErrorContextScope error_scope("production_method_registry.parse_definition", filename);

    std::vector<char> buffer;
    if (!load_file_to_buffer(filename, buffer)) {
        error_context_report_error("Failed to load ProductionMethod definition.", filename);
        return 0;
    }

    g_parse_state = {};
    g_parse_state.definition = std::make_unique<ProductionMethod>(definition_path ? definition_path : "");
    if (!xml_parser_init(XML_ELEMENTS, static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])), 1)) {
        log_error("Unable to initialize ProductionMethod xml parser", filename, 0);
        error_context_report_error("Unable to initialize ProductionMethod xml parser.", filename);
        return 0;
    }

    const int parsed = xml_parser_parse(buffer.data(), static_cast<unsigned int>(buffer.size()), 1);
    xml_parser_free();
    if (!parsed || g_parse_state.error || !g_parse_state.definition || !g_parse_state.saw_kind || !g_parse_state.saw_output) {
        char detail[512];
        snprintf(detail, sizeof(detail), "file=%s path=%s", filename, definition_path ? definition_path : "");
        error_context_report_error("Unable to parse ProductionMethod xml.", detail);
        return 0;
    }

    const std::string key = g_parse_state.definition->path();
    if (g_production_methods.find(key) != g_production_methods.end()) {
        char detail[512];
        snprintf(detail, sizeof(detail), "file=%s path=%s", filename, key.c_str());
        log_error("Duplicate ProductionMethod definition path", key.c_str(), 0);
        error_context_report_error("Duplicate ProductionMethod definition path.", detail);
        return 0;
    }

    g_production_methods.emplace(key, std::move(g_parse_state.definition));
    return 1;
}

} // namespace

const ProductionMethod *find_production_method_definition(const char *path)
{
    const std::string normalized = normalize_definition_path(path);
    const auto found = g_production_methods.find(normalized);
    return found != g_production_methods.end() ? found->second.get() : nullptr;
}

} // namespace building_type_registry_impl

extern "C" const char *production_method_registry_get_production_method_path(void)
{
    building_type_registry_impl::g_production_method_path = std::string(mod_manager_get_mod_path()) + "ProductionMethod/";
    return building_type_registry_impl::g_production_method_path.c_str();
}

extern "C" int production_method_registry_load(void)
{
    using namespace building_type_registry_impl;

    production_method_registry_get_production_method_path();
    g_production_methods.clear();

    const dir_listing *files = dir_find_files_with_extension(g_production_method_path.c_str(), "xml");
    if (!files || files->num_files <= 0) {
        return 1;
    }

    for (int i = 0; i < files->num_files; i++) {
        char full_path[FILE_NAME_MAX];
        snprintf(full_path, FILE_NAME_MAX, "%s%s", g_production_method_path.c_str(), files->files[i].name);
        const std::string normalized_path = normalize_definition_path(files->files[i].name);
        if (normalized_path.empty()) {
            log_error("Unsupported ProductionMethod file name", files->files[i].name, 0);
            return 0;
        }
        if (!parse_definition_file(full_path, normalized_path.c_str())) {
            return 0;
        }
    }

    return 1;
}

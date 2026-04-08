#include "translation/localization_internal.h"

namespace localization::detail {

json_value json_value::make_object()
{
    json_value value;
    value.type = json_type::object;
    return value;
}

json_value json_value::make_array()
{
    json_value value;
    value.type = json_type::array;
    return value;
}

json_parser::json_parser(std::string_view text) : text_(text) {}

bool json_parser::parse(json_value &value, std::string &error)
{
    skip_whitespace();
    if (!parse_value(value, error)) {
        return false;
    }
    skip_whitespace();
    if (position_ != text_.size()) {
        error = "Unexpected trailing content in localization JSON.";
        return false;
    }
    return true;
}

bool json_parser::parse_value(json_value &value, std::string &error)
{
    if (position_ >= text_.size()) {
        error = "Unexpected end of localization JSON.";
        return false;
    }

    switch (text_[position_]) {
        case '{': return parse_object(value, error);
        case '[': return parse_array(value, error);
        case '"': return parse_string_value(value, error);
        case 't': return parse_literal("true", json_type::boolean, value, error, 1);
        case 'f': return parse_literal("false", json_type::boolean, value, error, 0);
        case 'n': return parse_literal("null", json_type::null_value, value, error, 0);
        default: return parse_number(value, error);
    }
}

bool json_parser::parse_object(json_value &value, std::string &error)
{
    ++position_;
    value = json_value::make_object();
    skip_whitespace();
    if (consume('}')) {
        return true;
    }

    while (position_ < text_.size()) {
        std::string key;
        if (!parse_string(key, error)) {
            return false;
        }
        skip_whitespace();
        if (!consume(':')) {
            error = "Expected ':' in localization JSON object.";
            return false;
        }
        skip_whitespace();
        json_value child;
        if (!parse_value(child, error)) {
            return false;
        }
        value.object_value.emplace(std::move(key), std::move(child));
        skip_whitespace();
        if (consume('}')) {
            return true;
        }
        if (!consume(',')) {
            error = "Expected ',' or '}' in localization JSON object.";
            return false;
        }
        skip_whitespace();
    }

    error = "Unterminated localization JSON object.";
    return false;
}

bool json_parser::parse_array(json_value &value, std::string &error)
{
    ++position_;
    value = json_value::make_array();
    skip_whitespace();
    if (consume(']')) {
        return true;
    }

    while (position_ < text_.size()) {
        json_value child;
        if (!parse_value(child, error)) {
            return false;
        }
        value.array_value.push_back(std::move(child));
        skip_whitespace();
        if (consume(']')) {
            return true;
        }
        if (!consume(',')) {
            error = "Expected ',' or ']' in localization JSON array.";
            return false;
        }
        skip_whitespace();
    }

    error = "Unterminated localization JSON array.";
    return false;
}

bool json_parser::parse_string_value(json_value &value, std::string &error)
{
    value.type = json_type::string;
    return parse_string(value.string_value, error);
}

bool json_parser::parse_string(std::string &value, std::string &error)
{
    if (!consume('"')) {
        error = "Expected string in localization JSON.";
        return false;
    }

    value.clear();
    while (position_ < text_.size()) {
        const char ch = text_[position_++];
        if (ch == '"') {
            return true;
        }
        if (ch == '\\') {
            if (position_ >= text_.size()) {
                error = "Unterminated escape sequence in localization JSON string.";
                return false;
            }
            const char escaped = text_[position_++];
            switch (escaped) {
                case '"': value.push_back('"'); break;
                case '\\': value.push_back('\\'); break;
                case '/': value.push_back('/'); break;
                case 'b': value.push_back('\b'); break;
                case 'f': value.push_back('\f'); break;
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                case 'u': {
                    if (position_ + 4 > text_.size()) {
                        error = "Invalid unicode escape in localization JSON string.";
                        return false;
                    }
                    uint32_t codepoint = 0;
                    for (int i = 0; i < 4; ++i) {
                        codepoint <<= 4;
                        const char hex = text_[position_++];
                        if (hex >= '0' && hex <= '9') {
                            codepoint |= static_cast<uint32_t>(hex - '0');
                        } else if (hex >= 'a' && hex <= 'f') {
                            codepoint |= static_cast<uint32_t>(hex - 'a' + 10);
                        } else if (hex >= 'A' && hex <= 'F') {
                            codepoint |= static_cast<uint32_t>(hex - 'A' + 10);
                        } else {
                            error = "Invalid unicode escape in localization JSON string.";
                            return false;
                        }
                    }
                    append_utf8(codepoint, value);
                    break;
                }
                default:
                    error = "Unsupported escape sequence in localization JSON string.";
                    return false;
            }
            continue;
        }
        value.push_back(ch);
    }

    error = "Unterminated localization JSON string.";
    return false;
}

bool json_parser::parse_literal(const char *literal, json_type type, json_value &value, std::string &error, int boolean_value)
{
    const size_t length = strlen(literal);
    if (text_.substr(position_, length) != literal) {
        error = "Invalid literal in localization JSON.";
        return false;
    }
    position_ += length;
    value.type = type;
    value.boolean_value = boolean_value != 0;
    return true;
}

bool json_parser::parse_number(json_value &value, std::string &error)
{
    const size_t start = position_;
    if (text_[position_] == '-') {
        ++position_;
    }
    if (position_ >= text_.size() || !std::isdigit(static_cast<unsigned char>(text_[position_]))) {
        error = "Invalid number in localization JSON.";
        return false;
    }
    while (position_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[position_]))) {
        ++position_;
    }
    if (position_ < text_.size() && (text_[position_] == '.' || text_[position_] == 'e' || text_[position_] == 'E')) {
        error = "Floating-point numbers are not supported in localization JSON.";
        return false;
    }

    value.type = json_type::number;
    value.number_value = strtoll(std::string(text_.substr(start, position_ - start)).c_str(), nullptr, 10);
    return true;
}

bool json_parser::consume(char expected)
{
    if (position_ < text_.size() && text_[position_] == expected) {
        ++position_;
        return true;
    }
    return false;
}

void json_parser::skip_whitespace()
{
    while (position_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[position_]))) {
        ++position_;
    }
}

void json_parser::append_utf8(uint32_t codepoint, std::string &value)
{
    if (codepoint <= 0x7f) {
        value.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ff) {
        value.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
        value.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0xffff) {
        value.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
        value.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        value.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else {
        value.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
        value.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
        value.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        value.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
}

static const json_value *json_find(const json_value &object, const char *key)
{
    if (object.type != json_type::object) {
        return nullptr;
    }
    const auto it = object.object_value.find(key);
    return it == object.object_value.end() ? nullptr : &it->second;
}

static bool json_object_has_entries(const json_value *value)
{
    return value && value->type == json_type::object && !value->object_value.empty();
}

static std::string json_string_or_empty(const json_value *value)
{
    return value && value->type == json_type::string ? value->string_value : std::string();
}

static int json_number_or_default(const json_value *value, int default_value)
{
    return value && value->type == json_type::number ? static_cast<int>(value->number_value) : default_value;
}

static bool parse_locale_json_file(const std::string &path, json_value &root, std::string &error)
{
    std::string contents;
    if (!read_text_file(path, contents)) {
        error = "Unable to read localization JSON.";
        return false;
    }

    json_parser parser(contents);
    if (!parser.parse(root, error)) {
        return false;
    }
    if (root.type != json_type::object) {
        error = "Localization JSON root must be an object.";
        return false;
    }
    return true;
}

static void merge_string_groups(const json_value *section, std::map<int, std::vector<localized_text>> &target)
{
    if (!section || section->type != json_type::object) {
        return;
    }
    for (const auto &group_pair : section->object_value) {
        if (group_pair.second.type != json_type::object) {
            continue;
        }
        const int group = atoi(group_pair.first.c_str());
        std::vector<localized_text> &strings = target[group];
        for (const auto &index_pair : group_pair.second.object_value) {
            if (index_pair.second.type != json_type::string) {
                continue;
            }
            const int index = atoi(index_pair.first.c_str());
            if (index < 0) {
                continue;
            }
            if (strings.size() <= static_cast<size_t>(index)) {
                strings.resize(index + 1);
            }
            strings[index].utf8 = index_pair.second.string_value;
        }
    }
}

static void merge_message_string_field(const json_value *field_value, message_string_definition &field)
{
    if (!field_value || field_value->type != json_type::object) {
        return;
    }
    if (const json_value *text = json_find(*field_value, "text")) {
        if (text->type == json_type::string) {
            field.text.utf8 = text->string_value;
            field.has_text = true;
        }
    }
    if (const json_value *x = json_find(*field_value, "x")) {
        field.x = json_number_or_default(x, field.x);
        field.has_x = true;
    }
    if (const json_value *y = json_find(*field_value, "y")) {
        field.y = json_number_or_default(y, field.y);
        field.has_y = true;
    }
    if (const json_value *legacy_offset = json_find(*field_value, "legacy_offset")) {
        field.legacy_offset = json_number_or_default(legacy_offset, field.legacy_offset);
        field.has_legacy_offset = true;
    }
}

static void merge_messages_object(const json_value *messages_value, std::vector<message_definition> &target)
{
    if (!messages_value || messages_value->type != json_type::object) {
        return;
    }
    for (const auto &message_pair : messages_value->object_value) {
        if (message_pair.second.type != json_type::object) {
            continue;
        }
        const int id = atoi(message_pair.first.c_str());
        if (id < 0) {
            continue;
        }
        if (target.size() <= static_cast<size_t>(id)) {
            target.resize(id + 1);
        }
        message_definition &message = target[id];
        message.present = true;

        if (const json_value *type = json_find(message_pair.second, "type")) {
            message.type = static_cast<lang_type>(json_number_or_default(type, message.type));
            message.has_type = true;
        }
        if (const json_value *message_type = json_find(message_pair.second, "message_type")) {
            message.message_type = static_cast<lang_message_type>(json_number_or_default(message_type, message.message_type));
            message.has_message_type = true;
        }
        if (const json_value *x = json_find(message_pair.second, "x")) {
            message.x = json_number_or_default(x, message.x);
            message.has_x = true;
        }
        if (const json_value *y = json_find(message_pair.second, "y")) {
            message.y = json_number_or_default(y, message.y);
            message.has_y = true;
        }
        if (const json_value *width = json_find(message_pair.second, "width_blocks")) {
            message.width_blocks = json_number_or_default(width, message.width_blocks);
            message.has_width_blocks = true;
        }
        if (const json_value *height = json_find(message_pair.second, "height_blocks")) {
            message.height_blocks = json_number_or_default(height, message.height_blocks);
            message.has_height_blocks = true;
        }
        if (const json_value *urgent = json_find(message_pair.second, "urgent")) {
            message.urgent = json_number_or_default(urgent, message.urgent);
            message.has_urgent = true;
        }
        if (const json_value *image = json_find(message_pair.second, "image")) {
            if (const json_value *id_value = json_find(*image, "id")) {
                message.image.id = json_number_or_default(id_value, message.image.id);
                message.image.has_id = true;
            }
            if (const json_value *x_value = json_find(*image, "x")) {
                message.image.x = json_number_or_default(x_value, message.image.x);
                message.image.has_x = true;
            }
            if (const json_value *y_value = json_find(*image, "y")) {
                message.image.y = json_number_or_default(y_value, message.image.y);
                message.image.has_y = true;
            }
        }

        merge_message_string_field(json_find(message_pair.second, "title"), message.title);
        merge_message_string_field(json_find(message_pair.second, "subtitle"), message.subtitle);
        merge_message_string_field(json_find(message_pair.second, "video"), message.video);
        merge_message_string_field(json_find(message_pair.second, "content"), message.content);
    }
}

bool merge_locale_json(const std::string &path, locale_catalog &catalog, std::string &error)
{
    json_value root;
    if (!parse_locale_json_file(path, root, error)) {
        return false;
    }

    const json_value *metadata = json_find(root, "metadata");
    if (!metadata || metadata->type != json_type::object) {
        error = "Localization JSON is missing metadata.";
        return false;
    }

    if (catalog.info.code.empty()) {
        catalog.info.code = json_string_or_empty(json_find(*metadata, "code"));
        catalog.info.display_name = json_string_or_empty(json_find(*metadata, "display_name"));
        catalog.info.english_name = json_string_or_empty(json_find(*metadata, "english_name"));
        catalog.language = language_from_code(catalog.info.code);
    } else {
        const std::string display_name = json_string_or_empty(json_find(*metadata, "display_name"));
        const std::string english_name = json_string_or_empty(json_find(*metadata, "english_name"));
        if (!display_name.empty()) catalog.info.display_name = display_name;
        if (!english_name.empty()) catalog.info.english_name = english_name;
    }

    merge_string_groups(json_find(root, "main_strings"), catalog.main_strings);
    merge_string_groups(json_find(root, "editor_strings"), catalog.editor_strings);
    catalog.has_main_strings = catalog.has_main_strings || json_object_has_entries(json_find(root, "main_strings"));
    catalog.has_editor_strings = catalog.has_editor_strings || json_object_has_entries(json_find(root, "editor_strings"));

    if (const json_value *messages = json_find(root, "messages")) {
        merge_messages_object(json_find(*messages, "main"), catalog.main_messages);
        merge_messages_object(json_find(*messages, "editor"), catalog.editor_messages);
        catalog.has_main_messages = catalog.has_main_messages || json_object_has_entries(json_find(*messages, "main"));
        catalog.has_editor_messages = catalog.has_editor_messages || json_object_has_entries(json_find(*messages, "editor"));
    }

    if (const json_value *project_keys = json_find(root, "project_keys")) {
        if (project_keys->type == json_type::object) {
            for (const auto &entry : project_keys->object_value) {
                if (entry.second.type != json_type::string) continue;
                translation_key key;
                if (translation_key_from_name(entry.first.c_str(), &key) && key >= 0 && key < TRANSLATION_MAX_KEY) {
                    catalog.project_keys[key].utf8 = entry.second.string_value;
                }
            }
        }
    }
    return true;
}

int enumerate_available_locales_internal(std::vector<locale_info> &out_locales)
{
    std::map<std::string, locale_info> merged;
    for (int i = 0; i < mod_manager_get_mod_count(); ++i) {
        const std::string root = append_path_component(mod_manager_get_mod_path_at(i), "Localization");
        std::vector<std::string> files;
        list_directory_contents(root, TYPE_FILE, "json", files);
        for (const std::string &file_name : files) {
            json_value root_value;
            std::string error;
            if (!parse_locale_json_file(append_path_component(root, file_name), root_value, error)) {
                continue;
            }
            const json_value *metadata = json_find(root_value, "metadata");
            if (!metadata || metadata->type != json_type::object) {
                continue;
            }
            locale_info info;
            info.code = json_string_or_empty(json_find(*metadata, "code"));
            info.display_name = json_string_or_empty(json_find(*metadata, "display_name"));
            info.english_name = json_string_or_empty(json_find(*metadata, "english_name"));
            info.has_main_strings = json_object_has_entries(json_find(root_value, "main_strings"));
            info.has_editor_strings = json_object_has_entries(json_find(root_value, "editor_strings"));
            if (const json_value *messages = json_find(root_value, "messages")) {
                info.has_main_messages = json_object_has_entries(json_find(*messages, "main"));
                info.has_editor_messages = json_object_has_entries(json_find(*messages, "editor"));
            }
            if (info.code.empty()) {
                continue;
            }
            locale_info &merged_info = merged[info.code];
            if (!info.display_name.empty()) merged_info.display_name = info.display_name;
            if (!info.english_name.empty()) merged_info.english_name = info.english_name;
            merged_info.code = info.code;
            merged_info.has_main_strings = merged_info.has_main_strings || info.has_main_strings;
            merged_info.has_editor_strings = merged_info.has_editor_strings || info.has_editor_strings;
            merged_info.has_main_messages = merged_info.has_main_messages || info.has_main_messages;
            merged_info.has_editor_messages = merged_info.has_editor_messages || info.has_editor_messages;
        }
    }

    out_locales.clear();
    for (const auto &pair : merged) {
        if (pair.second.has_main_strings && pair.second.has_main_messages) {
            out_locales.push_back(pair.second);
        }
    }
    return static_cast<int>(out_locales.size());
}

std::string select_default_locale_code()
{
    std::vector<locale_info> locales;
    enumerate_available_locales_internal(locales);
    for (const locale_info &locale : locales) {
        if (locale.code == "en") {
            return locale.code;
        }
    }
    return locales.empty() ? std::string("en") : locales.front().code;
}

} // namespace localization::detail

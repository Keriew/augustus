#include "translation/localization_internal.h"

#include <algorithm>

namespace localization::detail {
namespace {

constexpr char kFileTextEng[] = "c3.eng";
constexpr char kFileMessageEng[] = "c3_mm.eng";
constexpr char kFileTextRus[] = "c3.rus";
constexpr char kFileMessageRus[] = "c3_mm.rus";
constexpr char kFileEditorTextEng[] = "c3_map.eng";
constexpr char kFileEditorMessageEng[] = "c3_map_mm.eng";
constexpr char kFileEditorTextRus[] = "c3_map.rus";
constexpr char kFileEditorMessageRus[] = "c3_map_mm.rus";

constexpr uint8_t kNewGameEnglish[] = { 0x4e, 0x65, 0x77, 0x20, 0x47, 0x61, 0x6d, 0x65, 0 };
constexpr uint8_t kNewGameFrench[] = { 0x4e, 0x6f, 0x75, 0x76, 0x65, 0x6c, 0x6c, 0x65, 0x20, 0x70, 0x61, 0x72, 0x74, 0x69, 0x65, 0 };
constexpr uint8_t kNewGameGerman[] = { 0x4e, 0x65, 0x75, 0x65, 0x73, 0x20, 0x53, 0x70, 0x69, 0x65, 0x6c, 0 };
constexpr uint8_t kNewGameGreek[] = { 0xcd, 0xdd, 0xef, 0x20, 0xd0, 0xe1, 0xe9, 0xf7, 0xed, 0xdf, 0xe4, 0xe9, 0 };
constexpr uint8_t kNewGameItalian[] = { 0x4e, 0x75, 0x6f, 0x76, 0x61, 0x20, 0x70, 0x61, 0x72, 0x74, 0x69, 0x74, 0x61, 0 };
constexpr uint8_t kNewGameSpanish[] = { 0x4e, 0x75, 0x65, 0x76, 0x61, 0x20, 0x70, 0x61, 0x72, 0x74, 0x69, 0x64, 0x61, 0 };
constexpr uint8_t kNewGamePortuguese[] = { 0x4e, 0x6f, 0x76, 0x6f, 0x20, 0x6a, 0x6f, 0x67, 0x6f, 0 };
constexpr uint8_t kNewGamePolish[] = { 0x4e, 0x6f, 0x77, 0x61, 0x20, 0x67, 0x72, 0x61, 0 };
constexpr uint8_t kNewGameRussian[] = { 0xcd, 0xee, 0xe2, 0xe0, 0xff, 0x20, 0xe8, 0xe3, 0xf0, 0xe0, 0 };
constexpr uint8_t kNewGameSwedish[] = { 0x4e, 0x79, 0x74, 0x74, 0x20, 0x73, 0x70, 0x65, 0x6c, 0 };
constexpr uint8_t kNewGameTraditionalChinese[] = { 0x83, 0x80, 0x20, 0x84, 0x80, 0x20, 0x85, 0x80, 0 };
constexpr uint8_t kNewGameSimplifiedChinese[] = { 0x82, 0x80, 0x20, 0x83, 0x80, 0x20, 0x84, 0x80, 0 };
constexpr uint8_t kNewGameKorean[] = { 0xbb, 0xf5, 0x20, 0xb0, 0xd4, 0xc0, 0xd3, 0 };
constexpr uint8_t kNewGameJapanese[] = { 0x83, 0x6a, 0x83, 0x85, 0x81, 0x5b, 0x83, 0x51, 0x81, 0x5b, 0x83, 0x80, 0 };
constexpr uint8_t kNewGameCzech[] = { 0x4e, 0x6f, 0x76, 0xe1, 0x20, 0x68, 0x72, 0x61, 0 };
constexpr uint8_t kNewGameUkrainian[] = { 0xcd, 0xee, 0xe2, 0xe0, 0x20, 0xe3, 0xf0, 0xe0, 0 };

bool matches_string(const uint8_t *candidate, const uint8_t *expected)
{
    return candidate && expected &&
        strcmp(reinterpret_cast<const char *>(candidate), reinterpret_cast<const char *>(expected)) == 0;
}

language_type detect_language_from_new_game(const uint8_t *new_game_string)
{
    if (matches_string(new_game_string, kNewGameEnglish)) return LANGUAGE_ENGLISH;
    if (matches_string(new_game_string, kNewGameFrench)) return LANGUAGE_FRENCH;
    if (matches_string(new_game_string, kNewGameGerman)) return LANGUAGE_GERMAN;
    if (matches_string(new_game_string, kNewGameGreek)) return LANGUAGE_GREEK;
    if (matches_string(new_game_string, kNewGameItalian)) return LANGUAGE_ITALIAN;
    if (matches_string(new_game_string, kNewGameSpanish)) return LANGUAGE_SPANISH;
    if (matches_string(new_game_string, kNewGamePortuguese)) return LANGUAGE_PORTUGUESE;
    if (matches_string(new_game_string, kNewGamePolish)) return LANGUAGE_POLISH;
    if (matches_string(new_game_string, kNewGameRussian)) return LANGUAGE_RUSSIAN;
    if (matches_string(new_game_string, kNewGameSwedish)) return LANGUAGE_SWEDISH;
    if (matches_string(new_game_string, kNewGameTraditionalChinese)) return LANGUAGE_TRADITIONAL_CHINESE;
    if (matches_string(new_game_string, kNewGameSimplifiedChinese)) return LANGUAGE_SIMPLIFIED_CHINESE;
    if (matches_string(new_game_string, kNewGameKorean)) return LANGUAGE_KOREAN;
    if (matches_string(new_game_string, kNewGameJapanese)) return LANGUAGE_JAPANESE;
    if (matches_string(new_game_string, kNewGameCzech)) return LANGUAGE_CZECH;
    if (matches_string(new_game_string, kNewGameUkrainian)) return LANGUAGE_UKRAINIAN;
    return LANGUAGE_UNKNOWN;
}

void json_write_escaped_string(std::string &output, std::string_view value)
{
    output.push_back('"');
    for (const unsigned char ch : value) {
        switch (ch) {
            case '"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (ch < 0x20) {
                    char buffer[8];
                    snprintf(buffer, sizeof(buffer), "\\u%04x", ch);
                    output += buffer;
                } else {
                    output.push_back(static_cast<char>(ch));
                }
                break;
        }
    }
    output.push_back('"');
}

void json_write_indent(std::string &output, int depth)
{
    output.append(depth * 2, ' ');
}

std::string encode_utf8_string(const uint8_t *input)
{
    if (!input) {
        return {};
    }
    char buffer[4096];
    encoding_to_utf8(input, buffer, static_cast<int>(sizeof(buffer)), 0);
    return buffer;
}

const uint8_t *get_group_string(const raw_text_table &table, int group, int index)
{
    if (group < 0 || group >= kMaxTextEntries) {
        return nullptr;
    }
    const raw_text_table::entry &entry = table.entries[group];
    if (!entry.in_use || entry.offset < 0 || static_cast<size_t>(entry.offset) >= table.data.size()) {
        return nullptr;
    }

    const uint8_t *cursor = table.data.data() + entry.offset;
    const uint8_t *end = table.data.data() + table.data.size();
    uint8_t previous = 0;
    while (index > 0 && cursor < end) {
        if (!*cursor && (previous >= ' ' || previous == 0)) {
            --index;
        }
        previous = *cursor;
        ++cursor;
    }
    while (cursor < end && *cursor < ' ') {
        ++cursor;
    }
    return cursor < end ? cursor : nullptr;
}

bool load_raw_text_file(const std::string &path, raw_text_table &table)
{
    std::vector<uint8_t> file_buffer(kBufferSize);
    const int file_size = io_read_file_into_buffer(path.c_str(), NOT_LOCALIZED, file_buffer.data(), static_cast<int>(file_buffer.size()));
    if (file_size < kMinTextSize || file_size > kMaxTextSize) {
        return false;
    }

    table.data.assign(kMaxTextData, 0);
    buffer raw_buffer;
    buffer_init(&raw_buffer, file_buffer.data(), file_size);
    buffer_skip(&raw_buffer, 28);
    for (int i = 0; i < kMaxTextEntries; ++i) {
        table.entries[i].offset = buffer_read_i32(&raw_buffer);
        table.entries[i].in_use = buffer_read_i32(&raw_buffer);
    }
    buffer_read_raw(&raw_buffer, table.data.data(), kMaxTextData);
    return true;
}

bool load_raw_message_file(const std::string &path, raw_message_table &table)
{
    std::vector<uint8_t> file_buffer(kBufferSize);
    const int file_size = io_read_file_into_buffer(path.c_str(), NOT_LOCALIZED, file_buffer.data(), static_cast<int>(file_buffer.size()));
    if (file_size < kMinMessageSize || file_size > kMaxMessageSize) {
        return false;
    }

    table.data.assign(kMaxMessageData, 0);
    buffer raw_buffer;
    buffer_init(&raw_buffer, file_buffer.data(), file_size);
    buffer_skip(&raw_buffer, 24);
    for (int i = 0; i < kMaxMessageEntries; ++i) {
        raw_message_table::entry &message = table.entries[i];
        message.type = static_cast<lang_type>(buffer_read_i16(&raw_buffer));
        message.message_type = static_cast<lang_message_type>(buffer_read_i16(&raw_buffer));
        buffer_skip(&raw_buffer, 2);
        message.x = buffer_read_i16(&raw_buffer);
        message.y = buffer_read_i16(&raw_buffer);
        message.width_blocks = buffer_read_i16(&raw_buffer);
        message.height_blocks = buffer_read_i16(&raw_buffer);
        message.image_id = buffer_read_i16(&raw_buffer);
        message.image_x = buffer_read_i16(&raw_buffer);
        message.image_y = buffer_read_i16(&raw_buffer);
        buffer_skip(&raw_buffer, 6);
        message.title_x = buffer_read_i16(&raw_buffer);
        message.title_y = buffer_read_i16(&raw_buffer);
        message.subtitle_x = buffer_read_i16(&raw_buffer);
        message.subtitle_y = buffer_read_i16(&raw_buffer);
        buffer_skip(&raw_buffer, 4);
        message.video_x = buffer_read_i16(&raw_buffer);
        message.video_y = buffer_read_i16(&raw_buffer);
        buffer_skip(&raw_buffer, 14);
        message.urgent = buffer_read_i32(&raw_buffer);
        message.video_offset = buffer_read_i32(&raw_buffer);
        buffer_skip(&raw_buffer, 4);
        message.title_offset = buffer_read_i32(&raw_buffer);
        message.subtitle_offset = buffer_read_i32(&raw_buffer);
        message.content_offset = buffer_read_i32(&raw_buffer);
    }
    buffer_read_raw(&raw_buffer, table.data.data(), kMaxMessageData);
    return true;
}

std::string extract_message_text(const raw_message_table &table, int offset)
{
    if (offset <= 0 || static_cast<size_t>(offset) >= table.data.size()) {
        return {};
    }
    return encode_utf8_string(table.data.data() + offset);
}

void populate_group_strings(const raw_text_table &table, std::map<int, std::vector<localized_text>> &groups)
{
    for (int group = 0; group < kMaxTextEntries; ++group) {
        const raw_text_table::entry &entry = table.entries[group];
        if (!entry.in_use || entry.offset < 0 || static_cast<size_t>(entry.offset) >= table.data.size()) {
            continue;
        }

        int end_offset = static_cast<int>(table.data.size());
        for (int next_group = group + 1; next_group < kMaxTextEntries; ++next_group) {
            const raw_text_table::entry &next_entry = table.entries[next_group];
            if (next_entry.in_use && next_entry.offset >= 0 && next_entry.offset > entry.offset &&
                static_cast<size_t>(next_entry.offset) < table.data.size()) {
                end_offset = next_entry.offset;
                break;
            }
        }

        const uint8_t *end = table.data.data() + end_offset;
        std::vector<localized_text> strings;

        for (int index = 0; ; ++index) {
            const uint8_t *start = get_group_string(table, group, index);
            if (!start || start >= end) {
                break;
            }

            localized_text text;
            text.utf8 = encode_utf8_string(start);
            if (strings.size() <= static_cast<size_t>(index)) {
                strings.resize(index + 1);
            }
            strings[index] = std::move(text);
        }

        while (!strings.empty() && strings.back().utf8.empty()) {
            strings.pop_back();
        }

        if (!strings.empty()) {
            groups[group] = std::move(strings);
        }
    }
}

void populate_messages(const raw_message_table &raw, std::vector<message_definition> &messages)
{
    messages.assign(kMaxMessageEntries, {});
    for (int i = 0; i < kMaxMessageEntries; ++i) {
        const raw_message_table::entry &source = raw.entries[i];
        message_definition &message = messages[i];
        message.present = source.type != TYPE_MANUAL || source.message_type != MESSAGE_TYPE_GENERAL ||
            source.title_offset || source.content_offset || source.subtitle_offset || source.video_offset ||
            source.width_blocks || source.height_blocks;
        if (!message.present) {
            continue;
        }

        message.type = source.type;
        message.message_type = source.message_type;
        message.x = source.x;
        message.y = source.y;
        message.width_blocks = source.width_blocks;
        message.height_blocks = source.height_blocks;
        message.urgent = source.urgent;
        message.has_type = true;
        message.has_message_type = true;
        message.has_x = true;
        message.has_y = true;
        message.has_width_blocks = true;
        message.has_height_blocks = true;
        message.has_urgent = true;
        message.image.id = source.image_id;
        message.image.x = source.image_x;
        message.image.y = source.image_y;
        message.image.has_id = true;
        message.image.has_x = true;
        message.image.has_y = true;

        auto set_field = [&](message_string_definition &field, int x, int y, int offset) {
            field.x = x;
            field.y = y;
            field.legacy_offset = offset;
            field.has_x = true;
            field.has_y = true;
            field.has_legacy_offset = offset > 0;
            field.text.utf8 = extract_message_text(raw, offset);
            field.has_text = !field.text.utf8.empty();
        };

        set_field(message.title, source.title_x, source.title_y, source.title_offset);
        set_field(message.subtitle, source.subtitle_x, source.subtitle_y, source.subtitle_offset);
        set_field(message.video, source.video_x, source.video_y, source.video_offset);
        set_field(message.content, 0, 0, source.content_offset);
    }
}

bool file_exists_at_path(const std::string &directory, const char *filename)
{
    const std::string full_path = append_path_component(directory, filename);
    return !full_path.empty() && file_exists(full_path.c_str(), NOT_LOCALIZED);
}

bool read_binary_file(const std::string &path, std::vector<uint8_t> &data)
{
    FILE *file = file_open(path.c_str(), "rb");
    if (!file) {
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        file_close(file);
        return false;
    }
    const long size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        file_close(file);
        return false;
    }
    data.assign(static_cast<size_t>(size), 0);
    const size_t bytes_read = fread(data.data(), 1, data.size(), file);
    file_close(file);
    return bytes_read == data.size();
}

bool hash_file_contents(uint64_t &hash, const std::string &path)
{
    std::vector<uint8_t> data;
    if (!read_binary_file(path, data)) {
        return false;
    }
    hash_string(hash, path);
    hash_bytes(hash, data.data(), data.size());
    return true;
}

void delete_manifest_files(const std::string &manifest_path)
{
    std::string contents;
    if (!read_text_file(manifest_path, contents)) {
        return;
    }
    size_t start = 0;
    while (start < contents.size()) {
        size_t end = contents.find('\n', start);
        if (end == std::string::npos) {
            end = contents.size();
        }
        std::string line = contents.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            file_remove(line.c_str());
        }
        start = end + 1;
    }
}

void write_manifest_file(const std::string &manifest_path, const std::vector<std::string> &paths)
{
    std::string contents;
    for (const std::string &path : paths) {
        contents += path;
        contents += "\r\n";
    }
    write_text_file(manifest_path, contents);
}

bool build_julius_expected_stamp(std::string &stamp)
{
    uint64_t hash = 1469598103934665603ull;
    hash_string(hash, kJuliusStampPrefix);

    std::vector<std::string> subdirectories;
    list_directory_contents(".", TYPE_DIR, nullptr, subdirectories);

    std::vector<std::string> candidate_directories;
    candidate_directories.push_back(".");
    for (const std::string &entry : subdirectories) {
        candidate_directories.push_back(entry);
    }

    static const char *kKnownFiles[] = {
        kFileTextEng, kFileMessageEng, kFileTextRus, kFileMessageRus,
        kFileEditorTextEng, kFileEditorMessageEng, kFileEditorTextRus, kFileEditorMessageRus
    };

    for (const std::string &directory : candidate_directories) {
        bool saw_any = false;
        for (const char *filename : kKnownFiles) {
            const std::string full_path = append_path_component(directory, filename);
            if (!full_path.empty() && file_exists(full_path.c_str(), NOT_LOCALIZED)) {
                saw_any = true;
                if (!hash_file_contents(hash, full_path)) {
                    return false;
                }
            }
        }
        if (saw_any) {
            hash_string(hash, directory);
        }
    }

    stamp = format_hash_stamp(kJuliusStampPrefix, hash);
    return true;
}

bool write_julius_locale_json(const std::string &source_directory, std::vector<std::string> &manifest_paths)
{
    const bool has_eng = file_exists_at_path(source_directory, kFileTextEng) && file_exists_at_path(source_directory, kFileMessageEng);
    const bool has_rus = file_exists_at_path(source_directory, kFileTextRus) && file_exists_at_path(source_directory, kFileMessageRus);
    if (!has_eng && !has_rus) {
        return false;
    }

    raw_text_table raw_main_text;
    raw_message_table raw_main_messages;
    if (!load_raw_text_file(append_path_component(source_directory, has_eng ? kFileTextEng : kFileTextRus), raw_main_text) ||
        !load_raw_message_file(append_path_component(source_directory, has_eng ? kFileMessageEng : kFileMessageRus), raw_main_messages)) {
        return false;
    }

    const language_type language = detect_language_from_new_game(get_group_string(raw_main_text, 1, 1));
    const char *locale_code = language_code_for(language);
    if (!*locale_code) {
        return false;
    }

    encoding_determine(language);

    raw_text_table raw_editor_text;
    raw_message_table raw_editor_messages;
    std::map<int, std::vector<localized_text>> main_strings;
    std::map<int, std::vector<localized_text>> editor_strings;
    std::vector<message_definition> main_messages;
    std::vector<message_definition> editor_messages;
    populate_group_strings(raw_main_text, main_strings);
    populate_messages(raw_main_messages, main_messages);

    const bool has_editor = (file_exists_at_path(source_directory, kFileEditorTextEng) && file_exists_at_path(source_directory, kFileEditorMessageEng)) ||
        (file_exists_at_path(source_directory, kFileEditorTextRus) && file_exists_at_path(source_directory, kFileEditorMessageRus));
    if (has_editor) {
        load_raw_text_file(append_path_component(source_directory, file_exists_at_path(source_directory, kFileEditorTextEng) ? kFileEditorTextEng : kFileEditorTextRus), raw_editor_text);
        load_raw_message_file(append_path_component(source_directory, file_exists_at_path(source_directory, kFileEditorMessageEng) ? kFileEditorMessageEng : kFileEditorMessageRus), raw_editor_messages);
        populate_group_strings(raw_editor_text, editor_strings);
        populate_messages(raw_editor_messages, editor_messages);
    }

    auto write_groups = [](std::string &output, const std::map<int, std::vector<localized_text>> &groups) {
        output += "{\r\n";
        bool wrote_group = false;
        for (const auto &group_pair : groups) {
            bool wrote_any = false;
            for (const localized_text &entry : group_pair.second) {
                if (!entry.utf8.empty()) {
                    wrote_any = true;
                    break;
                }
            }
            if (!wrote_any) {
                continue;
            }
            if (wrote_group) output += ",\r\n";
            wrote_group = true;
            json_write_indent(output, 2);
            json_write_escaped_string(output, std::to_string(group_pair.first));
            output += ": {\r\n";
            bool wrote_index = false;
            for (size_t index = 0; index < group_pair.second.size(); ++index) {
                if (group_pair.second[index].utf8.empty()) {
                    continue;
                }
                if (wrote_index) output += ",\r\n";
                wrote_index = true;
                json_write_indent(output, 3);
                json_write_escaped_string(output, std::to_string(index));
                output += ": ";
                json_write_escaped_string(output, group_pair.second[index].utf8);
            }
            output += "\r\n";
            json_write_indent(output, 2);
            output += "}";
        }
        output += "\r\n";
        json_write_indent(output, 1);
        output += "}";
    };

    auto write_messages = [](std::string &output, const std::vector<message_definition> &messages) {
        output += "{\r\n";
        bool wrote_message = false;
        for (size_t i = 0; i < messages.size(); ++i) {
            const message_definition &message = messages[i];
            if (!message.present) {
                continue;
            }
            if (wrote_message) output += ",\r\n";
            wrote_message = true;
            json_write_indent(output, 3);
            json_write_escaped_string(output, std::to_string(i));
            output += ": {\r\n";
            json_write_indent(output, 4); json_write_escaped_string(output, "type"); output += ": " + std::to_string(message.type) + ",\r\n";
            json_write_indent(output, 4); json_write_escaped_string(output, "message_type"); output += ": " + std::to_string(message.message_type) + ",\r\n";
            json_write_indent(output, 4); json_write_escaped_string(output, "x"); output += ": " + std::to_string(message.x) + ",\r\n";
            json_write_indent(output, 4); json_write_escaped_string(output, "y"); output += ": " + std::to_string(message.y) + ",\r\n";
            json_write_indent(output, 4); json_write_escaped_string(output, "width_blocks"); output += ": " + std::to_string(message.width_blocks) + ",\r\n";
            json_write_indent(output, 4); json_write_escaped_string(output, "height_blocks"); output += ": " + std::to_string(message.height_blocks) + ",\r\n";
            json_write_indent(output, 4); json_write_escaped_string(output, "urgent"); output += ": " + std::to_string(message.urgent) + ",\r\n";
            json_write_indent(output, 4); json_write_escaped_string(output, "image"); output += ": {\"id\": " + std::to_string(message.image.id) + ", \"x\": " + std::to_string(message.image.x) + ", \"y\": " + std::to_string(message.image.y) + "},\r\n";

            auto write_string_field = [&](const char *name, const message_string_definition &field, bool trailing_comma) {
                json_write_indent(output, 4);
                json_write_escaped_string(output, name);
                output += ": {";
                bool wrote_member = false;
                if (field.has_text) {
                    json_write_escaped_string(output, "text");
                    output += ": ";
                    json_write_escaped_string(output, field.text.utf8);
                    wrote_member = true;
                }
                if (field.has_x) {
                    output += wrote_member ? ", " : "";
                    json_write_escaped_string(output, "x");
                    output += ": " + std::to_string(field.x);
                    wrote_member = true;
                }
                if (field.has_y) {
                    output += wrote_member ? ", " : "";
                    json_write_escaped_string(output, "y");
                    output += ": " + std::to_string(field.y);
                    wrote_member = true;
                }
                if (field.has_legacy_offset) {
                    output += wrote_member ? ", " : "";
                    json_write_escaped_string(output, "legacy_offset");
                    output += ": " + std::to_string(field.legacy_offset);
                }
                output += "}";
                output += trailing_comma ? ",\r\n" : "\r\n";
            };

            write_string_field("title", message.title, true);
            write_string_field("subtitle", message.subtitle, true);
            write_string_field("video", message.video, true);
            write_string_field("content", message.content, false);
            json_write_indent(output, 3);
            output += "}";
        }
        output += "\r\n";
        json_write_indent(output, 2);
        output += "}";
    };

    std::string output;
    output += "{\r\n";
    json_write_indent(output, 1); json_write_escaped_string(output, "metadata"); output += ": {";
    json_write_escaped_string(output, "code"); output += ": "; json_write_escaped_string(output, locale_code);
    output += ", "; json_write_escaped_string(output, "display_name"); output += ": "; json_write_escaped_string(output, language_display_name(language));
    output += ", "; json_write_escaped_string(output, "english_name"); output += ": "; json_write_escaped_string(output, language_english_name(language));
    output += ", "; json_write_escaped_string(output, "source"); output += ": "; json_write_escaped_string(output, kJuliusSourceName);
    output += "},\r\n";
    json_write_indent(output, 1); json_write_escaped_string(output, "main_strings"); output += ": "; write_groups(output, main_strings); output += ",\r\n";
    json_write_indent(output, 1); json_write_escaped_string(output, "editor_strings"); output += ": "; write_groups(output, editor_strings); output += ",\r\n";
    json_write_indent(output, 1); json_write_escaped_string(output, "messages"); output += ": {\r\n";
    json_write_indent(output, 2); json_write_escaped_string(output, "main"); output += ": "; write_messages(output, main_messages); output += ",\r\n";
    json_write_indent(output, 2); json_write_escaped_string(output, "editor"); output += ": "; write_messages(output, editor_messages); output += "\r\n";
    json_write_indent(output, 1); output += "},\r\n";
    json_write_indent(output, 1); json_write_escaped_string(output, "project_keys"); output += ": {}\r\n";
    output += "}\r\n";

    const std::string output_path = append_path_component(make_julius_localization_root(), std::string(locale_code) + ".json");
    if (!write_text_file(output_path, output)) {
        return false;
    }
    manifest_paths.push_back(output_path);
    return true;
}

bool bootstrap_julius_localization()
{
    ensure_directory(make_julius_localization_root());
    std::string expected_stamp;
    if (!build_julius_expected_stamp(expected_stamp)) {
        return false;
    }

    std::string current_stamp;
    read_text_file(make_julius_stamp_path(), current_stamp);
    if (current_stamp == expected_stamp) {
        return true;
    }

    delete_manifest_files(make_julius_manifest_path());
    std::vector<std::string> manifest_paths;
    write_julius_locale_json(".", manifest_paths);
    std::vector<std::string> subdirectories;
    list_directory_contents(".", TYPE_DIR, nullptr, subdirectories);
    for (const std::string &entry : subdirectories) {
        write_julius_locale_json(entry, manifest_paths);
    }
    write_manifest_file(make_julius_manifest_path(), manifest_paths);
    return write_text_file(make_julius_stamp_path(), expected_stamp);
}

} // namespace

bool ensure_generated_localization_cache()
{
    return bootstrap_julius_localization();
}

} // namespace localization::detail

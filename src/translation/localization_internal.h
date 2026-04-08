#pragma once

#include "translation/localization.h"

extern "C" {
#include "core/buffer.h"
#include "core/config.h"
#include "core/encoding.h"
#include "core/file.h"
#include "core/io.h"
#include "core/log.h"
#include "game/mod_manager.h"
#include "platform/file_manager.h"
#include "translation/translation_key_table.h"
}

#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace localization::detail {

inline constexpr int kMaxTextEntries = 1000;
inline constexpr int kMaxTextData = 200000;
inline constexpr int kMinTextSize = 28 + kMaxTextEntries * 8;
inline constexpr int kMaxTextSize = kMinTextSize + kMaxTextData;

inline constexpr int kMaxMessageEntries = 400;
inline constexpr int kMaxMessageData = 460000;
inline constexpr int kMinMessageSize = 32024;
inline constexpr int kMaxMessageSize = kMinMessageSize + kMaxMessageData;
inline constexpr int kBufferSize = 400000;

inline constexpr char kJuliusStampPrefix[] = "localization_extract_v2:";
inline constexpr char kJuliusSourceName[] = "Julius";
inline constexpr uint8_t kEmptyLegacy[] = { 0 };

struct localized_text {
    std::string utf8;
    std::vector<uint8_t> legacy;

    void rebuild_legacy()
    {
        if (utf8.empty()) {
            legacy.assign(1, 0);
            return;
        }

        legacy.assign(utf8.size() * 4 + 8, 0);
        encoding_from_utf8(utf8.c_str(), legacy.data(), static_cast<int>(legacy.size()));
        legacy.resize(strlen(reinterpret_cast<const char *>(legacy.data())) + 1);
    }

    const uint8_t *legacy_ptr() const
    {
        return legacy.empty() ? kEmptyLegacy : legacy.data();
    }
};

struct message_string_definition {
    localized_text text;
    int x = 0;
    int y = 0;
    int legacy_offset = 0;
    bool has_text = false;
    bool has_x = false;
    bool has_y = false;
    bool has_legacy_offset = false;
};

struct message_image_definition {
    int id = 0;
    int x = 0;
    int y = 0;
    bool has_id = false;
    bool has_x = false;
    bool has_y = false;
};

struct message_definition {
    lang_type type = TYPE_MESSAGE;
    lang_message_type message_type = MESSAGE_TYPE_GENERAL;
    int x = 0;
    int y = 0;
    int width_blocks = 0;
    int height_blocks = 0;
    int urgent = 0;
    bool present = false;
    bool has_type = false;
    bool has_message_type = false;
    bool has_x = false;
    bool has_y = false;
    bool has_width_blocks = false;
    bool has_height_blocks = false;
    bool has_urgent = false;
    message_image_definition image;
    message_string_definition title;
    message_string_definition subtitle;
    message_string_definition video;
    message_string_definition content;
};

struct locale_catalog {
    locale_info info;
    language_type language = LANGUAGE_ENGLISH;
    std::map<int, std::vector<localized_text>> main_strings;
    std::map<int, std::vector<localized_text>> editor_strings;
    std::vector<message_definition> main_messages;
    std::vector<message_definition> editor_messages;
    std::vector<localized_text> project_keys = std::vector<localized_text>(TRANSLATION_MAX_KEY);
    bool has_main_strings = false;
    bool has_editor_strings = false;
    bool has_main_messages = false;
    bool has_editor_messages = false;
};

struct runtime_state {
    locale_catalog catalog;
    std::vector<lang_message> legacy_main_messages;
    std::vector<lang_message> legacy_editor_messages;
    std::string active_locale_code;
    std::string active_display_name;
    language_type active_language = LANGUAGE_UNKNOWN;
    int editor_mode = 0;
    bool locale_loaded = false;
};

enum class json_type {
    null_value,
    boolean,
    number,
    string,
    object,
    array
};

struct json_value {
    json_type type = json_type::null_value;
    bool boolean_value = false;
    int64_t number_value = 0;
    std::string string_value;
    std::map<std::string, json_value> object_value;
    std::vector<json_value> array_value;

    static json_value make_object();
    static json_value make_array();
};

class json_parser {
public:
    explicit json_parser(std::string_view text);
    bool parse(json_value &value, std::string &error);

private:
    bool parse_value(json_value &value, std::string &error);
    bool parse_object(json_value &value, std::string &error);
    bool parse_array(json_value &value, std::string &error);
    bool parse_string_value(json_value &value, std::string &error);
    bool parse_string(std::string &value, std::string &error);
    bool parse_literal(const char *literal, json_type type, json_value &value, std::string &error, int boolean_value);
    bool parse_number(json_value &value, std::string &error);
    bool consume(char expected);
    void skip_whitespace();
    static void append_utf8(uint32_t codepoint, std::string &value);

    std::string_view text_;
    size_t position_ = 0;
};

struct raw_text_table {
    struct entry {
        int32_t offset = 0;
        int32_t in_use = 0;
    };

    std::array<entry, kMaxTextEntries> entries {};
    std::vector<uint8_t> data;
};

struct raw_message_table {
    struct entry {
        lang_type type = TYPE_MESSAGE;
        lang_message_type message_type = MESSAGE_TYPE_GENERAL;
        int x = 0;
        int y = 0;
        int width_blocks = 0;
        int height_blocks = 0;
        int urgent = 0;
        int image_id = 0;
        int image_x = 0;
        int image_y = 0;
        int title_x = 0;
        int title_y = 0;
        int subtitle_x = 0;
        int subtitle_y = 0;
        int video_x = 0;
        int video_y = 0;
        int video_offset = 0;
        int title_offset = 0;
        int subtitle_offset = 0;
        int content_offset = 0;
    };

    std::array<entry, kMaxMessageEntries> entries {};
    std::vector<uint8_t> data;
};

extern runtime_state g_runtime;
extern std::vector<localized_text> g_missing_project_key_fallbacks;
extern std::vector<uint8_t> g_missing_project_key_reported;
extern std::map<uint64_t, bool> g_missing_legacy_string_reported;
extern std::vector<std::string> *g_list_result;

bool append_path_component(char *buffer, size_t buffer_size, const char *base_path, const char *component);
std::string append_path_component(const std::string &base_path, const std::string &component);
int collect_directory_entries(const char *name, long modified_time);
void list_directory_contents(const std::string &path, int type, const char *extension, std::vector<std::string> &out_entries);
bool read_text_file(const std::string &path, std::string &contents);
bool write_text_file(const std::string &path, const std::string &contents);
void ensure_directory(const std::string &path);
void hash_bytes(uint64_t &hash, const void *data_ptr, size_t size);
void hash_string(uint64_t &hash, const std::string &value);
std::string format_hash_stamp(const char *prefix, uint64_t hash);
std::string make_julius_localization_root();
std::string make_julius_stamp_path();
std::string make_julius_manifest_path();
localized_text &fallback_project_key(translation_key key);
void report_missing_project_key(translation_key key);
void report_missing_legacy_string(int is_editor, int group, int index);
const char *language_code_for(language_type language);
const char *language_display_name(language_type language);
const char *language_english_name(language_type language);
language_type language_from_code(std::string_view code);
void rebuild_text_groups(std::map<int, std::vector<localized_text>> &groups);
void rebuild_project_keys(std::vector<localized_text> &project_keys);
void rebuild_messages(std::vector<message_definition> &messages, language_type language, std::vector<lang_message> &legacy_messages);
bool ensure_generated_localization_cache();
bool merge_locale_json(const std::string &path, locale_catalog &catalog, std::string &error);
int enumerate_available_locales_internal(std::vector<locale_info> &out_locales);
std::string select_default_locale_code();

} // namespace localization::detail

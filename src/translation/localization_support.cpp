#include "translation/localization_internal.h"

#include <algorithm>

namespace localization::detail {

runtime_state g_runtime;
std::vector<localized_text> g_missing_project_key_fallbacks(TRANSLATION_MAX_KEY);
std::vector<uint8_t> g_missing_project_key_reported(TRANSLATION_MAX_KEY, 0);
std::map<uint64_t, bool> g_missing_legacy_string_reported;
std::vector<std::string> *g_list_result = nullptr;

bool append_path_component(char *buffer, size_t buffer_size, const char *base_path, const char *component)
{
    if (!buffer || buffer_size == 0 || !base_path || !*base_path || !component || !*component) {
        return false;
    }

    const size_t base_length = strlen(base_path);
    const bool has_separator = base_length > 0 &&
        (base_path[base_length - 1] == '/' || base_path[base_length - 1] == '\\');
    return snprintf(buffer, buffer_size, has_separator ? "%s%s" : "%s/%s", base_path, component) <
        static_cast<int>(buffer_size);
}

std::string append_path_component(const std::string &base_path, const std::string &component)
{
    char buffer[FILE_NAME_MAX];
    if (!append_path_component(buffer, sizeof(buffer), base_path.c_str(), component.c_str())) {
        return {};
    }
    return buffer;
}

int collect_directory_entries(const char *name, long modified_time)
{
    (void) modified_time;
    if (g_list_result && name) {
        g_list_result->push_back(name);
    }
    return LIST_CONTINUE;
}

void list_directory_contents(const std::string &path, int type, const char *extension, std::vector<std::string> &out_entries)
{
    out_entries.clear();
    g_list_result = &out_entries;
    platform_file_manager_list_directory_contents(path.c_str(), type, extension, collect_directory_entries);
    g_list_result = nullptr;
    std::sort(out_entries.begin(), out_entries.end());
}

bool read_text_file(const std::string &path, std::string &contents)
{
    FILE *file = file_open(path.c_str(), "rb");
    if (!file) {
        contents.clear();
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

    contents.assign(static_cast<size_t>(size), '\0');
    const size_t bytes_read = fread(contents.data(), 1, contents.size(), file);
    file_close(file);
    if (bytes_read != contents.size()) {
        contents.clear();
        return false;
    }
    return true;
}

bool write_text_file(const std::string &path, const std::string &contents)
{
    FILE *file = file_open(path.c_str(), "wb");
    if (!file) {
        return false;
    }
    const size_t bytes_written = fwrite(contents.data(), 1, contents.size(), file);
    file_close(file);
    return bytes_written == contents.size();
}

void ensure_directory(const std::string &path)
{
    if (!path.empty()) {
        platform_file_manager_create_directory(path.c_str(), 0, 1);
    }
}

void hash_bytes(uint64_t &hash, const void *data_ptr, size_t size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data_ptr);
    constexpr uint64_t fnv_prime = 1099511628211ull;
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= fnv_prime;
    }
}

void hash_string(uint64_t &hash, const std::string &value)
{
    hash_bytes(hash, value.data(), value.size());
}

std::string format_hash_stamp(const char *prefix, uint64_t hash)
{
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%s%016llx", prefix, static_cast<unsigned long long>(hash));
    return buffer;
}

std::string make_julius_localization_root()
{
    for (int i = 0; i < mod_manager_get_mod_count(); ++i) {
        if (strcmp(mod_manager_get_mod_name_at(i), "Julius") == 0) {
            return append_path_component(mod_manager_get_mod_path_at(i), "Localization");
        }
    }
    return append_path_component("Mods/Julius", "Localization");
}

std::string make_julius_stamp_path()
{
    return append_path_component(make_julius_localization_root(), ".localization_extract.stamp");
}

std::string make_julius_manifest_path()
{
    return append_path_component(make_julius_localization_root(), ".localization_extract.manifest");
}

localized_text &fallback_project_key(translation_key key)
{
    localized_text &fallback = g_missing_project_key_fallbacks[key];
    if (fallback.utf8.empty()) {
        const char *key_name = translation_key_to_name(key);
        if (key_name && *key_name) {
            fallback.utf8 = key_name;
        } else {
            fallback.utf8 = "MISSING_LOCALIZATION_KEY";
        }
        fallback.rebuild_legacy();
    }
    return fallback;
}

void report_missing_project_key(translation_key key)
{
    if (key < 0 || key >= TRANSLATION_MAX_KEY || g_missing_project_key_reported[key]) {
        return;
    }
    g_missing_project_key_reported[key] = 1;
    const char *key_name = translation_key_to_name(key);
    log_error("Missing localized project string", key_name ? key_name : "MISSING_LOCALIZATION_KEY", 0);
}

void report_missing_legacy_string(int is_editor, int group, int index)
{
    const uint64_t key = (static_cast<uint64_t>(is_editor != 0) << 63) |
        (static_cast<uint64_t>(static_cast<uint32_t>(group)) << 31) |
        static_cast<uint32_t>(index);
    if (g_missing_legacy_string_reported[key]) {
        return;
    }
    g_missing_legacy_string_reported[key] = true;

    char details[64];
    snprintf(details, sizeof(details), "mode=%s group=%d index=%d", is_editor ? "editor" : "main", group, index);
    log_error("Missing localized legacy string", details, 0);
}

const char *language_code_for(language_type language)
{
    switch (language) {
        case LANGUAGE_ENGLISH: return "en";
        case LANGUAGE_FRENCH: return "fr";
        case LANGUAGE_GERMAN: return "de";
        case LANGUAGE_GREEK: return "el";
        case LANGUAGE_ITALIAN: return "it";
        case LANGUAGE_SPANISH: return "es";
        case LANGUAGE_PORTUGUESE: return "pt";
        case LANGUAGE_POLISH: return "pl";
        case LANGUAGE_RUSSIAN: return "ru";
        case LANGUAGE_SWEDISH: return "sv";
        case LANGUAGE_TRADITIONAL_CHINESE: return "zh-Hant";
        case LANGUAGE_SIMPLIFIED_CHINESE: return "zh-Hans";
        case LANGUAGE_KOREAN: return "ko";
        case LANGUAGE_JAPANESE: return "ja";
        case LANGUAGE_CZECH: return "cs";
        case LANGUAGE_UKRAINIAN: return "uk";
        default: return "";
    }
}

const char *language_display_name(language_type language)
{
    switch (language) {
        case LANGUAGE_ENGLISH: return "English";
        case LANGUAGE_FRENCH: return "FranÃ§ais";
        case LANGUAGE_GERMAN: return "Deutsch";
        case LANGUAGE_GREEK: return "Î•Î»Î»Î·Î½Î¹ÎºÎ¬";
        case LANGUAGE_ITALIAN: return "Italiano";
        case LANGUAGE_SPANISH: return "EspaÃ±ol";
        case LANGUAGE_PORTUGUESE: return "PortuguÃªs";
        case LANGUAGE_POLISH: return "Polski";
        case LANGUAGE_RUSSIAN: return "Ð ÑƒÑÑÐºÐ¸Ð¹";
        case LANGUAGE_SWEDISH: return "Svenska";
        case LANGUAGE_TRADITIONAL_CHINESE: return "ç¹é«”ä¸­æ–‡";
        case LANGUAGE_SIMPLIFIED_CHINESE: return "ç®€ä½“ä¸­æ–‡";
        case LANGUAGE_KOREAN: return "í•œêµ­ì–´";
        case LANGUAGE_JAPANESE: return "æ—¥æœ¬èªž";
        case LANGUAGE_CZECH: return "ÄŒeÅ¡tina";
        case LANGUAGE_UKRAINIAN: return "Ð£ÐºÑ€Ð°Ñ—Ð½ÑÑŒÐºÐ°";
        default: return "Unknown";
    }
}

const char *language_english_name(language_type language)
{
    switch (language) {
        case LANGUAGE_ENGLISH: return "English";
        case LANGUAGE_FRENCH: return "French";
        case LANGUAGE_GERMAN: return "German";
        case LANGUAGE_GREEK: return "Greek";
        case LANGUAGE_ITALIAN: return "Italian";
        case LANGUAGE_SPANISH: return "Spanish";
        case LANGUAGE_PORTUGUESE: return "Portuguese";
        case LANGUAGE_POLISH: return "Polish";
        case LANGUAGE_RUSSIAN: return "Russian";
        case LANGUAGE_SWEDISH: return "Swedish";
        case LANGUAGE_TRADITIONAL_CHINESE: return "Traditional Chinese";
        case LANGUAGE_SIMPLIFIED_CHINESE: return "Simplified Chinese";
        case LANGUAGE_KOREAN: return "Korean";
        case LANGUAGE_JAPANESE: return "Japanese";
        case LANGUAGE_CZECH: return "Czech";
        case LANGUAGE_UKRAINIAN: return "Ukrainian";
        default: return "Unknown";
    }
}

language_type language_from_code(std::string_view code)
{
    if (code == "en") return LANGUAGE_ENGLISH;
    if (code == "fr") return LANGUAGE_FRENCH;
    if (code == "de") return LANGUAGE_GERMAN;
    if (code == "el") return LANGUAGE_GREEK;
    if (code == "it") return LANGUAGE_ITALIAN;
    if (code == "es") return LANGUAGE_SPANISH;
    if (code == "pt") return LANGUAGE_PORTUGUESE;
    if (code == "pl") return LANGUAGE_POLISH;
    if (code == "ru") return LANGUAGE_RUSSIAN;
    if (code == "sv") return LANGUAGE_SWEDISH;
    if (code == "zh-Hans") return LANGUAGE_SIMPLIFIED_CHINESE;
    if (code == "zh-Hant") return LANGUAGE_TRADITIONAL_CHINESE;
    if (code == "ko") return LANGUAGE_KOREAN;
    if (code == "ja") return LANGUAGE_JAPANESE;
    if (code == "cs") return LANGUAGE_CZECH;
    if (code == "uk") return LANGUAGE_UKRAINIAN;
    return LANGUAGE_UNKNOWN;
}

void rebuild_text_groups(std::map<int, std::vector<localized_text>> &groups)
{
    for (auto &group_pair : groups) {
        for (localized_text &entry : group_pair.second) {
            entry.rebuild_legacy();
        }
    }
}

void rebuild_project_keys(std::vector<localized_text> &project_keys)
{
    for (localized_text &entry : project_keys) {
        entry.rebuild_legacy();
    }
}

void rebuild_messages(std::vector<message_definition> &messages, language_type language, std::vector<lang_message> &legacy_messages)
{
    legacy_messages.assign(messages.size(), {});
    for (size_t i = 0; i < messages.size(); ++i) {
        message_definition &message = messages[i];
        if (!message.present) {
            continue;
        }

        auto rebuild_field = [&](message_string_definition &field) {
            if (language == LANGUAGE_GERMAN && field.has_legacy_offset && field.legacy_offset == 289) {
                const localized_text &fix = g_runtime.catalog.project_keys[TR_FIX_GERMAN_CITY_RETAKEN];
                if (!fix.utf8.empty()) {
                    field.text.utf8 = fix.utf8;
                }
            }
            field.text.rebuild_legacy();
        };

        rebuild_field(message.video);
        rebuild_field(message.title);
        rebuild_field(message.subtitle);
        rebuild_field(message.content);

        lang_message &legacy = legacy_messages[i];
        legacy.type = message.type;
        legacy.message_type = message.message_type;
        legacy.x = message.x;
        legacy.y = message.y;
        legacy.width_blocks = message.width_blocks;
        legacy.height_blocks = message.height_blocks;
        legacy.urgent = message.urgent;
        legacy.image.id = message.image.id;
        legacy.image.x = message.image.x;
        legacy.image.y = message.image.y;
        legacy.video.x = message.video.x;
        legacy.video.y = message.video.y;
        legacy.video.text = message.video.text.legacy_ptr();
        legacy.title.x = message.title.x;
        legacy.title.y = message.title.y;
        legacy.title.text = message.title.text.legacy_ptr();
        legacy.subtitle.x = message.subtitle.x;
        legacy.subtitle.y = message.subtitle.y;
        legacy.subtitle.text = message.subtitle.text.legacy_ptr();
        legacy.content.text = message.content.text.legacy_ptr();
    }
}

} // namespace localization::detail

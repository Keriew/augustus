#include "translation/localization_internal.h"

#include "core/crash_context.h"

#include <algorithm>

namespace localization {

utf8_text_builder &utf8_text_builder::append(std::string_view text)
{
    value_.append(text);
    return *this;
}

utf8_text_builder &utf8_text_builder::append_line_break()
{
    value_.push_back('\n');
    return *this;
}

void utf8_text_builder::clear()
{
    value_.clear();
}

std::string_view utf8_text_builder::view() const
{
    return value_;
}

bool load_active_locale(int is_editor)
{
    if (!detail::ensure_generated_localization_cache()) {
        return false;
    }

    const std::string locale_code = resolve_configured_locale_code(
        config_get_string(CONFIG_STRING_UI_LOCALE),
        config_get_string(CONFIG_STRING_UI_LANGUAGE_DIR));

    detail::locale_catalog catalog;
    std::string error;
    catalog.project_keys.resize(TRANSLATION_MAX_KEY);
    for (int i = 0; i < mod_manager_get_mod_count(); ++i) {
        const std::string locale_path = detail::append_path_component(
            detail::append_path_component(mod_manager_get_mod_path_at(i), "Localization"),
            locale_code + ".json");
        if (!file_exists(locale_path.c_str(), NOT_LOCALIZED)) {
            continue;
        }
        CrashContextScope crash_scope("localization.merge_locale_json", locale_path.c_str());
        if (!detail::merge_locale_json(locale_path, catalog, error)) {
            error_context_report_error("Localization load failed.", error.c_str());
            return false;
        }
    }

    if (!catalog.has_main_strings || !catalog.has_main_messages || (is_editor && (!catalog.has_editor_strings || !catalog.has_editor_messages))) {
        error_context_report_error("Selected locale is incomplete for the requested mode.", locale_code.c_str());
        return false;
    }

    if (catalog.info.code.empty()) catalog.info.code = locale_code;
    if (catalog.language == LANGUAGE_UNKNOWN) catalog.language = detail::language_from_code(catalog.info.code);
    if (catalog.info.display_name.empty()) catalog.info.display_name = detail::language_display_name(catalog.language);
    if (catalog.info.english_name.empty()) catalog.info.english_name = detail::language_english_name(catalog.language);

    detail::g_runtime.catalog = std::move(catalog);
    detail::g_runtime.active_locale_code = detail::g_runtime.catalog.info.code;
    detail::g_runtime.active_display_name = detail::g_runtime.catalog.info.display_name;
    detail::g_runtime.active_language = detail::g_runtime.catalog.language == LANGUAGE_UNKNOWN ? LANGUAGE_ENGLISH : detail::g_runtime.catalog.language;
    detail::g_runtime.editor_mode = is_editor;
    detail::g_runtime.locale_loaded = true;
    return rebuild_legacy_cache(detail::g_runtime.active_language) != 0;
}

bool is_locale_available(const char *locale_code, int is_editor)
{
    if (!locale_code || !*locale_code) {
        return false;
    }
    detail::ensure_generated_localization_cache();
    std::vector<locale_info> locales;
    detail::enumerate_available_locales_internal(locales);
    for (const locale_info &locale : locales) {
        if (locale.code == locale_code) {
            return is_editor ? (locale.has_editor_strings && locale.has_editor_messages) :
                (locale.has_main_strings && locale.has_main_messages);
        }
    }
    return false;
}

int enumerate_available_locales(std::vector<locale_info> &out_locales)
{
    detail::ensure_generated_localization_cache();
    return detail::enumerate_available_locales_internal(out_locales);
}

int rebuild_legacy_cache(language_type language)
{
    if (!detail::g_runtime.locale_loaded) {
        return 0;
    }

    encoding_determine(language);
    detail::rebuild_project_keys(detail::g_runtime.catalog.project_keys);
    detail::rebuild_text_groups(detail::g_runtime.catalog.main_strings);
    detail::rebuild_text_groups(detail::g_runtime.catalog.editor_strings);
    detail::rebuild_messages(detail::g_runtime.catalog.main_messages, language, detail::g_runtime.legacy_main_messages);
    detail::rebuild_messages(detail::g_runtime.catalog.editor_messages, language, detail::g_runtime.legacy_editor_messages);
    detail::g_runtime.active_language = language;
    return 1;
}

int copy_legacy_messages(int is_editor, lang_message *dst, int max_messages)
{
    if (!dst || max_messages <= 0 || !detail::g_runtime.locale_loaded) {
        return 0;
    }
    const std::vector<lang_message> &source = is_editor ? detail::g_runtime.legacy_editor_messages : detail::g_runtime.legacy_main_messages;
    const int count = std::min<int>(max_messages, static_cast<int>(source.size()));
    memset(dst, 0, sizeof(lang_message) * max_messages);
    for (int i = 0; i < count; ++i) {
        dst[i] = source[i];
    }
    return count;
}

const char *active_locale_code()
{
    return detail::g_runtime.active_locale_code.c_str();
}

const char *active_locale_display_name()
{
    return detail::g_runtime.active_display_name.c_str();
}

language_type active_language()
{
    return detail::g_runtime.active_language;
}

std::string_view utf8_project_string(translation_key key)
{
    if (!detail::g_runtime.locale_loaded || key < 0 || key >= TRANSLATION_MAX_KEY) {
        return {};
    }
    const detail::localized_text &entry = detail::g_runtime.catalog.project_keys[key];
    if (!entry.utf8.empty()) {
        return entry.utf8;
    }
    detail::report_missing_project_key(key);
    return detail::fallback_project_key(key).utf8;
}

std::string_view utf8_legacy_string(int is_editor, int group, int index)
{
    if (!detail::g_runtime.locale_loaded || group < 0 || index < 0) {
        return {};
    }
    const auto &groups = is_editor ? detail::g_runtime.catalog.editor_strings : detail::g_runtime.catalog.main_strings;
    const auto group_it = groups.find(group);
    if (group_it == groups.end() || group_it->second.size() <= static_cast<size_t>(index)) {
        detail::report_missing_legacy_string(is_editor, group, index);
        return {};
    }
    if (group_it->second[index].utf8.empty()) {
        detail::report_missing_legacy_string(is_editor, group, index);
        return {};
    }
    return group_it->second[index].utf8;
}

const uint8_t *legacy_project_string(translation_key key)
{
    if (!detail::g_runtime.locale_loaded || key < 0 || key >= TRANSLATION_MAX_KEY) {
        return detail::kEmptyLegacy;
    }
    const detail::localized_text &entry = detail::g_runtime.catalog.project_keys[key];
    if (!entry.utf8.empty()) {
        return entry.legacy_ptr();
    }
    detail::report_missing_project_key(key);
    return detail::fallback_project_key(key).legacy_ptr();
}

const uint8_t *legacy_legacy_string(int is_editor, int group, int index)
{
    if (!detail::g_runtime.locale_loaded || group < 0 || index < 0) {
        return detail::kEmptyLegacy;
    }
    const auto &groups = is_editor ? detail::g_runtime.catalog.editor_strings : detail::g_runtime.catalog.main_strings;
    const auto group_it = groups.find(group);
    if (group_it == groups.end() || group_it->second.size() <= static_cast<size_t>(index)) {
        detail::report_missing_legacy_string(is_editor, group, index);
        return detail::kEmptyLegacy;
    }
    if (group_it->second[index].utf8.empty()) {
        detail::report_missing_legacy_string(is_editor, group, index);
        return detail::kEmptyLegacy;
    }
    return group_it->second[index].legacy_ptr();
}

std::string resolve_configured_locale_code(const char *configured_locale, const char *legacy_dir)
{
    if (configured_locale && *configured_locale && is_locale_available(configured_locale, 0)) {
        return configured_locale;
    }
    if (legacy_dir && *legacy_dir && is_locale_available(legacy_dir, 0)) {
        return legacy_dir;
    }
    return detail::select_default_locale_code();
}

const char *normalize_locale_code_for_config(const char *configured_locale, const char *legacy_dir)
{
    static std::string normalized;
    normalized = resolve_configured_locale_code(configured_locale, legacy_dir);
    return normalized.c_str();
}

} // namespace localization

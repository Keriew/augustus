#pragma once

#include "core/lang.h"
#include "core/locale.h"
#include "translation/translation.h"

#include <string>
#include <string_view>
#include <vector>

namespace localization {

struct locale_info {
    std::string code;
    std::string display_name;
    std::string english_name;
    bool has_main_strings = false;
    bool has_editor_strings = false;
    bool has_main_messages = false;
    bool has_editor_messages = false;
};

class utf8_text_builder {
public:
    utf8_text_builder &append(std::string_view text);
    utf8_text_builder &append_line_break();
    void clear();
    std::string_view view() const;

private:
    std::string value_;
};

bool load_active_locale(int is_editor);
bool is_locale_available(const char *locale_code, int is_editor);
int enumerate_available_locales(std::vector<locale_info> &out_locales);
int rebuild_legacy_cache(language_type language);
int copy_legacy_messages(int is_editor, lang_message *dst, int max_messages);

const char *active_locale_code();
const char *active_locale_display_name();
language_type active_language();

std::string_view utf8_project_string(translation_key key);
std::string_view utf8_legacy_string(int is_editor, int group, int index);
const uint8_t *legacy_project_string(translation_key key);
const uint8_t *legacy_legacy_string(int is_editor, int group, int index);

std::string resolve_configured_locale_code(const char *configured_locale, const char *legacy_dir);
const char *normalize_locale_code_for_config(const char *configured_locale, const char *legacy_dir);

} // namespace localization

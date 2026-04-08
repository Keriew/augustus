#include "graphics/font_vector_runtime.h"

extern "C" {
#include "core/encoding.h"
#include "core/file.h"
#include "core/log.h"
#include "core/xml_parser.h"
#include "game/mod_manager.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/renderer.h"
#include "platform/screen.h"
}

#include "graphics/runtime_texture.h"
#include "translation/localization.h"

#include <SDL.h>
#if __has_include(<SDL_ttf.h>)
#include <SDL_ttf.h>
#elif __has_include(<SDL2/SDL_ttf.h>)
#include <SDL2/SDL_ttf.h>
#else
#error "SDL_ttf.h was not found. Add SDL_ttf to the include path."
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr int XML_BUFFER_SIZE = 1024;
constexpr int XML_TOTAL_ELEMENTS = 8;
constexpr color_t kOpaqueWhite = 0xffffffff;

enum class FaceVariant {
    Regular = 0,
    Bold = 1,
    Italic = 2,
    BoldItalic = 3
};

struct FacePaths {
    std::string regular;
    std::string bold;
    std::string italic;
    std::string bold_italic;
};

struct FallbackRule {
    std::string family_id;
    std::string locale;
    std::string script;
};

struct FamilyConfig {
    std::string id;
    FacePaths faces;
    std::vector<FallbackRule> fallbacks;
};

struct SurfaceConfig {
    int defined = 0;
    font_t font = FONT_NORMAL_PLAIN;
    std::string family_id;
    int logical_size = 0;
    int line_height = 0;
    int letter_spacing = 0;
    int baseline_offset = 0;
    int space_width = 0;
    FontSurfaceSemantic semantic = FontSurfaceSemantic::Body;
};

struct SizedFont {
    TTF_Font *handle = nullptr;
    int ascent = 0;
    int descent = 0;
    int height = 0;
};

struct GlyphCacheEntry {
    image resource = {};
    RuntimeDrawSlice slice = {};
    int minx = 0;
    int maxx = 0;
    int miny = 0;
    int maxy = 0;
    int advance = 0;
};

struct RuntimeState {
    int active = 0;
    int ttf_initialized = 0;
    int cached_ui_scale = 0;
    std::string failure_reason;
    std::string loaded_pack_path;
    std::string base_dir;
    std::unordered_map<std::string, FamilyConfig> families;
    std::array<SurfaceConfig, FONT_TYPES_MAX> surfaces = {};
    std::unordered_map<std::string, SizedFont> sized_fonts;
    std::unordered_map<std::string, GlyphCacheEntry> glyphs;
};

struct ParseState {
    RuntimeState *target = nullptr;
    std::string current_family_id;
    std::string base_dir;
    int error = 0;
    int saw_root = 0;
};

struct DrawColors {
    color_t main = COLOR_BLACK;
    color_t shadow = COLOR_MASK_NONE;
    int has_shadow = 0;
};

RuntimeState g_runtime;
ParseState g_parse;

const char *kSemanticValues[] = {
    "body",
    "header",
    "title",
    "link"
};

const char *kFontNames[FONT_TYPES_MAX] = {
    "FONT_NORMAL_PLAIN",
    "FONT_NORMAL_BLACK",
    "FONT_NORMAL_WHITE",
    "FONT_NORMAL_RED",
    "FONT_LARGE_PLAIN",
    "FONT_LARGE_BLACK",
    "FONT_LARGE_BROWN",
    "FONT_SMALL_PLAIN",
    "FONT_NORMAL_GREEN",
    "FONT_NORMAL_BROWN"
};

int xml_start_fonts_element(void);
int xml_start_family_element(void);
int xml_start_regular_face_element(void);
int xml_start_bold_face_element(void);
int xml_start_italic_face_element(void);
int xml_start_bold_italic_face_element(void);
int xml_start_fallback_element(void);
int xml_start_surface_element(void);

const xml_parser_element kXmlElements[XML_TOTAL_ELEMENTS] = {
    { "fonts", xml_start_fonts_element, nullptr, nullptr, nullptr },
    { "family", xml_start_family_element, nullptr, "fonts", nullptr },
    { "regular", xml_start_regular_face_element, nullptr, "family", nullptr },
    { "bold", xml_start_bold_face_element, nullptr, "family", nullptr },
    { "italic", xml_start_italic_face_element, nullptr, "family", nullptr },
    { "bold_italic", xml_start_bold_italic_face_element, nullptr, "family", nullptr },
    { "fallback", xml_start_fallback_element, nullptr, "family", nullptr },
    { "surface", xml_start_surface_element, nullptr, "fonts", nullptr }
};

std::string trim_copy(std::string value)
{
    size_t start = 0;
    while (start < value.size() && (value[start] == ' ' || value[start] == '\t' || value[start] == '\r' || value[start] == '\n')) {
        ++start;
    }

    size_t end = value.size();
    while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r' || value[end - 1] == '\n')) {
        --end;
    }

    return value.substr(start, end - start);
}

std::string to_lower_copy(std::string value)
{
    for (char &ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return value;
}

int string_equals_case_insensitive(const std::string &lhs, const char *rhs)
{
    if (!rhs) {
        return 0;
    }
    return to_lower_copy(lhs) == to_lower_copy(rhs);
}

int is_absolute_path(const char *path)
{
    if (!path || !*path) {
        return 0;
    }
    if (path[0] == '/' || path[0] == '\\') {
        return 1;
    }
    return path[1] == ':';
}

std::string append_path_component(const std::string &base, const std::string &component)
{
    if (base.empty()) {
        return component;
    }
    if (component.empty()) {
        return base;
    }
    if (base.back() == '/' || base.back() == '\\') {
        return base + component;
    }
    return base + "/" + component;
}

std::string resolve_face_path(const std::string &base_dir, const char *src)
{
    if (!src || !*src) {
        return {};
    }
    if (is_absolute_path(src)) {
        return src;
    }
    return append_path_component(base_dir, src);
}

std::string make_font_cache_key(const std::string &face_path, int pixel_size)
{
    return face_path + "|" + std::to_string(pixel_size);
}

std::string make_glyph_cache_key(const std::string &face_path, int pixel_size, uint32_t codepoint)
{
    return face_path + "|" + std::to_string(pixel_size) + "|" + std::to_string(codepoint);
}

int font_from_name(const std::string &name, font_t *font)
{
    if (!font) {
        return 0;
    }
    for (int i = 0; i < FONT_TYPES_MAX; ++i) {
        if (string_equals_case_insensitive(name, kFontNames[i])) {
            *font = static_cast<font_t>(i);
            return 1;
        }
    }
    return 0;
}

FamilyConfig *current_family()
{
    if (!g_parse.target || g_parse.current_family_id.empty()) {
        return nullptr;
    }
    auto it = g_parse.target->families.find(g_parse.current_family_id);
    if (it == g_parse.target->families.end()) {
        return nullptr;
    }
    return &it->second;
}

void set_parse_error(const char *message, const char *detail = nullptr)
{
    g_parse.error = 1;
    std::string failure;
    if (detail && *detail) {
        failure = std::string(message ? message : "") + "\n\n" + detail;
    } else {
        failure = message ? message : "";
    }
    g_runtime.failure_reason = failure;
    if (g_parse.target) {
        g_parse.target->failure_reason = failure;
    }
}

int xml_start_fonts_element(void)
{
    if (g_parse.saw_root) {
        set_parse_error("Duplicate <fonts> root in fonts.xml.");
        return 0;
    }
    g_parse.saw_root = 1;
    return 1;
}

int xml_start_family_element(void)
{
    if (!g_parse.target) {
        set_parse_error("Font parser state was not initialized.");
        return 0;
    }

    char *id = xml_parser_copy_attribute_string("id");
    if (!id || !*id) {
        free(id);
        set_parse_error("A <family> entry in fonts.xml is missing its id attribute.");
        return 0;
    }

    FamilyConfig family;
    family.id = trim_copy(id);
    free(id);
    if (family.id.empty()) {
        set_parse_error("A <family> entry in fonts.xml has an empty id.");
        return 0;
    }

    g_parse.current_family_id = family.id;
    g_parse.target->families[family.id] = std::move(family);
    return 1;
}

int set_current_family_face(FaceVariant variant)
{
    FamilyConfig *family = current_family();
    if (!family) {
        set_parse_error("A face entry was encountered outside of a valid <family>.");
        return 0;
    }

    const char *src = xml_parser_get_attribute_string("src");
    if (!src || !*src) {
        set_parse_error("A font face entry is missing its src attribute.");
        return 0;
    }

    std::string resolved = resolve_face_path(g_parse.base_dir, src);
    switch (variant) {
        case FaceVariant::Regular:
            family->faces.regular = resolved;
            break;
        case FaceVariant::Bold:
            family->faces.bold = resolved;
            break;
        case FaceVariant::Italic:
            family->faces.italic = resolved;
            break;
        case FaceVariant::BoldItalic:
            family->faces.bold_italic = resolved;
            break;
    }
    return 1;
}

int xml_start_regular_face_element(void)
{
    return set_current_family_face(FaceVariant::Regular);
}

int xml_start_bold_face_element(void)
{
    return set_current_family_face(FaceVariant::Bold);
}

int xml_start_italic_face_element(void)
{
    return set_current_family_face(FaceVariant::Italic);
}

int xml_start_bold_italic_face_element(void)
{
    return set_current_family_face(FaceVariant::BoldItalic);
}

int xml_start_fallback_element(void)
{
    FamilyConfig *family = current_family();
    if (!family) {
        set_parse_error("A <fallback> entry was encountered outside of a valid <family>.");
        return 0;
    }

    char *fallback_family = xml_parser_copy_attribute_string("family");
    if (!fallback_family || !*fallback_family) {
        free(fallback_family);
        set_parse_error("A <fallback> entry is missing its family attribute.");
        return 0;
    }

    FallbackRule rule;
    rule.family_id = trim_copy(fallback_family);
    free(fallback_family);
    const char *locale = xml_parser_get_attribute_string("locale");
    const char *script = xml_parser_get_attribute_string("script");
    rule.locale = trim_copy(locale ? locale : "");
    rule.script = trim_copy(script ? script : "");
    family->fallbacks.push_back(std::move(rule));
    return 1;
}

int xml_start_surface_element(void)
{
    if (!g_parse.target) {
        set_parse_error("Font parser state was not initialized.");
        return 0;
    }

    char *id = xml_parser_copy_attribute_string("id");
    char *family_id = xml_parser_copy_attribute_string("family");
    if (!id || !*id || !family_id || !*family_id) {
        free(id);
        free(family_id);
        set_parse_error("A <surface> entry is missing its required id or family attribute.");
        return 0;
    }

    font_t font = FONT_NORMAL_PLAIN;
    if (!font_from_name(trim_copy(id), &font)) {
        set_parse_error("fonts.xml references an unknown font surface.", id);
        free(id);
        free(family_id);
        return 0;
    }

    SurfaceConfig &surface = g_parse.target->surfaces[font];
    surface.defined = 1;
    surface.font = font;
    surface.family_id = trim_copy(family_id);
    surface.logical_size = xml_parser_get_attribute_int("logical_size");
    surface.line_height = xml_parser_get_attribute_int("line_height");
    surface.letter_spacing = xml_parser_get_attribute_int("letter_spacing");
    surface.baseline_offset = xml_parser_get_attribute_int("baseline_offset");
    int semantic = xml_parser_get_attribute_enum("semantic", kSemanticValues, 4, 0);
    surface.semantic = semantic >= 0 ? static_cast<FontSurfaceSemantic>(semantic) : FontSurfaceSemantic::Body;

    free(id);
    free(family_id);
    return 1;
}

void release_glyph_cache()
{
    for (auto &entry : g_runtime.glyphs) {
        graphics_renderer()->release_image_resource(&entry.second.resource);
    }
    g_runtime.glyphs.clear();
}

void release_font_cache()
{
    for (auto &entry : g_runtime.sized_fonts) {
        if (entry.second.handle) {
            TTF_CloseFont(entry.second.handle);
        }
    }
    g_runtime.sized_fonts.clear();
}

void release_runtime_state()
{
    release_glyph_cache();
    release_font_cache();
    g_runtime.families.clear();
    g_runtime.loaded_pack_path.clear();
    g_runtime.base_dir.clear();
    g_runtime.cached_ui_scale = 0;
    g_runtime.active = 0;
}

int ensure_ttf_initialized()
{
    if (g_runtime.ttf_initialized) {
        return 1;
    }
    if (TTF_Init() != 0) {
        g_runtime.failure_reason = std::string("Failed to initialize SDL_ttf.\n\n") + TTF_GetError();
        return 0;
    }
    g_runtime.ttf_initialized = 1;
    return 1;
}

void maybe_reset_scale_cache()
{
    if (!g_runtime.active) {
        return;
    }
    const int ui_scale = platform_screen_get_scale();
    if (g_runtime.cached_ui_scale != ui_scale) {
        release_glyph_cache();
        release_font_cache();
        g_runtime.cached_ui_scale = ui_scale;
    }
}

FaceVariant style_variant_for(unsigned style_flags)
{
    const int bold = (style_flags & FONT_INLINE_STYLE_BOLD) != 0;
    const int italic = (style_flags & FONT_INLINE_STYLE_ITALIC) != 0;
    if (bold && italic) {
        return FaceVariant::BoldItalic;
    }
    if (bold) {
        return FaceVariant::Bold;
    }
    if (italic) {
        return FaceVariant::Italic;
    }
    return FaceVariant::Regular;
}

unsigned sanitize_style_flags(font_t font, unsigned style_flags)
{
    FontSurfaceSemantic semantic = g_runtime.surfaces[font].semantic;
    if (semantic == FontSurfaceSemantic::Header || semantic == FontSurfaceSemantic::Title) {
        return FONT_INLINE_STYLE_NONE;
    }
    return style_flags;
}

const char *script_name_for_codepoint(uint32_t codepoint)
{
    if ((codepoint >= 0x3040 && codepoint <= 0x309f)) {
        return "hiragana";
    }
    if ((codepoint >= 0x30a0 && codepoint <= 0x30ff) || (codepoint >= 0xff66 && codepoint <= 0xff9d)) {
        return "katakana";
    }
    if ((codepoint >= 0x4e00 && codepoint <= 0x9fff) || (codepoint >= 0x3400 && codepoint <= 0x4dbf)) {
        return "han";
    }
    if ((codepoint >= 0xac00 && codepoint <= 0xd7af) || (codepoint >= 0x1100 && codepoint <= 0x11ff)) {
        return "hangul";
    }
    if (codepoint >= 0x0400 && codepoint <= 0x04ff) {
        return "cyrillic";
    }
    if (codepoint >= 0x0370 && codepoint <= 0x03ff) {
        return "greek";
    }
    return "latin";
}

int locale_matches_rule(const std::string &rule_locale, std::string locale_code)
{
    if (rule_locale.empty()) {
        return 1;
    }
    locale_code = to_lower_copy(std::move(locale_code));
    std::string wanted = to_lower_copy(rule_locale);
    if (locale_code == wanted) {
        return 1;
    }
    size_t divider = locale_code.find_first_of("-_");
    if (divider != std::string::npos && locale_code.substr(0, divider) == wanted) {
        return 1;
    }
    divider = wanted.find_first_of("-_");
    if (divider != std::string::npos && wanted.substr(0, divider) == locale_code) {
        return 1;
    }
    return 0;
}

int script_matches_rule(const std::string &rule_script, uint32_t codepoint)
{
    if (rule_script.empty()) {
        return 1;
    }
    return to_lower_copy(rule_script) == script_name_for_codepoint(codepoint);
}

std::vector<const FamilyConfig *> family_chain_for(font_t font, uint32_t codepoint)
{
    std::vector<const FamilyConfig *> families;
    if (font < 0 || font >= FONT_TYPES_MAX) {
        return families;
    }

    const SurfaceConfig &surface = g_runtime.surfaces[font];
    auto primary = g_runtime.families.find(surface.family_id);
    if (primary == g_runtime.families.end()) {
        return families;
    }

    families.push_back(&primary->second);
    const std::string locale_code = localization::active_locale_code() ? localization::active_locale_code() : "";
    for (const FallbackRule &rule : primary->second.fallbacks) {
        if (!locale_matches_rule(rule.locale, locale_code) || !script_matches_rule(rule.script, codepoint)) {
            continue;
        }
        auto it = g_runtime.families.find(rule.family_id);
        if (it == g_runtime.families.end()) {
            continue;
        }
        if (std::find(families.begin(), families.end(), &it->second) == families.end()) {
            families.push_back(&it->second);
        }
    }
    return families;
}

const std::string &face_path_for_variant(const FamilyConfig &family, FaceVariant variant)
{
    switch (variant) {
        case FaceVariant::Bold:
            if (!family.faces.bold.empty()) return family.faces.bold;
            break;
        case FaceVariant::Italic:
            if (!family.faces.italic.empty()) return family.faces.italic;
            break;
        case FaceVariant::BoldItalic:
            if (!family.faces.bold_italic.empty()) return family.faces.bold_italic;
            if (!family.faces.bold.empty()) return family.faces.bold;
            if (!family.faces.italic.empty()) return family.faces.italic;
            break;
        case FaceVariant::Regular:
        default:
            break;
    }
    return family.faces.regular;
}

SizedFont *ensure_sized_font(const std::string &face_path, int pixel_size)
{
    if (face_path.empty() || pixel_size <= 0) {
        return nullptr;
    }
    if (!ensure_ttf_initialized()) {
        return nullptr;
    }

    maybe_reset_scale_cache();

    const std::string key = make_font_cache_key(face_path, pixel_size);
    auto found = g_runtime.sized_fonts.find(key);
    if (found != g_runtime.sized_fonts.end()) {
        return &found->second;
    }

    TTF_Font *font = TTF_OpenFont(face_path.c_str(), pixel_size);
    if (!font) {
        g_runtime.failure_reason = std::string("Failed to open a font face.\n\n") + face_path + "\n\n" + TTF_GetError();
        return nullptr;
    }

    SizedFont sized_font;
    sized_font.handle = font;
    sized_font.ascent = TTF_FontAscent(font);
    sized_font.descent = TTF_FontDescent(font);
    sized_font.height = TTF_FontHeight(font);
    auto inserted = g_runtime.sized_fonts.emplace(key, sized_font);
    return &inserted.first->second;
}

float raster_multiplier_for(float scale)
{
    float ui_multiplier = platform_screen_get_scale() > 0 ? platform_screen_get_scale() / 100.0f : 1.0f;
    if (scale <= 0.0f) {
        scale = 1.0f;
    }
    return ui_multiplier / scale;
}

int scaled_pixel_size(font_t font, float scale)
{
    float multiplier = raster_multiplier_for(scale);
    int logical_size = g_runtime.surfaces[font].logical_size;
    int pixel_size = static_cast<int>(std::lround(logical_size * multiplier));
    return std::max(1, pixel_size);
}

struct ResolvedFace {
    SizedFont *font = nullptr;
    std::string face_path;
};

ResolvedFace resolve_face_for(font_t font, uint32_t codepoint, unsigned style_flags, float scale)
{
    ResolvedFace resolved;
    if (!g_runtime.active || font < 0 || font >= FONT_TYPES_MAX) {
        return resolved;
    }

    unsigned sanitized_flags = sanitize_style_flags(font, style_flags);
    FaceVariant variant = style_variant_for(sanitized_flags);
    const int pixel_size = scaled_pixel_size(font, scale);
    std::vector<const FamilyConfig *> family_chain = family_chain_for(font, codepoint);
    for (const FamilyConfig *family : family_chain) {
        if (!family) {
            continue;
        }
        const std::string &path = face_path_for_variant(*family, variant);
        SizedFont *candidate = ensure_sized_font(path, pixel_size);
        if (!candidate) {
            continue;
        }
        if (codepoint == ' ' || TTF_GlyphIsProvided32(candidate->handle, codepoint)) {
            resolved.font = candidate;
            resolved.face_path = path;
            return resolved;
        }
    }

    if (!family_chain.empty()) {
        const std::string &path = face_path_for_variant(*family_chain.front(), variant);
        resolved.font = ensure_sized_font(path, pixel_size);
        resolved.face_path = path;
    }
    return resolved;
}

int next_utf8_codepoint(std::string_view text, size_t offset, uint32_t *codepoint, size_t *num_bytes)
{
    if (offset >= text.size()) {
        return 0;
    }

    const unsigned char first = static_cast<unsigned char>(text[offset]);
    if (first < 0x80) {
        *codepoint = first;
        *num_bytes = 1;
        return 1;
    }
    if ((first & 0xe0) == 0xc0 && offset + 1 < text.size()) {
        *codepoint = ((first & 0x1f) << 6) | (static_cast<unsigned char>(text[offset + 1]) & 0x3f);
        *num_bytes = 2;
        return 1;
    }
    if ((first & 0xf0) == 0xe0 && offset + 2 < text.size()) {
        *codepoint = ((first & 0x0f) << 12)
            | ((static_cast<unsigned char>(text[offset + 1]) & 0x3f) << 6)
            | (static_cast<unsigned char>(text[offset + 2]) & 0x3f);
        *num_bytes = 3;
        return 1;
    }
    if ((first & 0xf8) == 0xf0 && offset + 3 < text.size()) {
        *codepoint = ((first & 0x07) << 18)
            | ((static_cast<unsigned char>(text[offset + 1]) & 0x3f) << 12)
            | ((static_cast<unsigned char>(text[offset + 2]) & 0x3f) << 6)
            | (static_cast<unsigned char>(text[offset + 3]) & 0x3f);
        *num_bytes = 4;
        return 1;
    }

    *codepoint = first;
    *num_bytes = 1;
    return 1;
}

GlyphCacheEntry *ensure_glyph(font_t font, uint32_t codepoint, unsigned style_flags, float scale)
{
    ResolvedFace resolved = resolve_face_for(font, codepoint, style_flags, scale);
    if (!resolved.font || resolved.face_path.empty()) {
        return nullptr;
    }

    const int pixel_size = scaled_pixel_size(font, scale);
    const std::string key = make_glyph_cache_key(resolved.face_path, pixel_size, codepoint);
    auto found = g_runtime.glyphs.find(key);
    if (found != g_runtime.glyphs.end()) {
        return &found->second;
    }

    GlyphCacheEntry entry;
    if (TTF_GlyphMetrics32(
        resolved.font->handle,
        codepoint,
        &entry.minx,
        &entry.maxx,
        &entry.miny,
        &entry.maxy,
        &entry.advance) != 0) {
        return nullptr;
    }

    SDL_Color white = { 255, 255, 255, 255 };
    SDL_Surface *surface = TTF_RenderGlyph32_Blended(resolved.font->handle, codepoint, white);
    if (!surface) {
        return nullptr;
    }

    SDL_Surface *converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(surface);
    if (!converted) {
        g_runtime.failure_reason = std::string("Failed to convert a rendered glyph surface.\n\n") + SDL_GetError();
        return nullptr;
    }

    std::vector<color_t> pixels(static_cast<size_t>(converted->w) * static_cast<size_t>(converted->h), 0);
    for (int y = 0; y < converted->h; ++y) {
        const color_t *src = reinterpret_cast<const color_t *>(static_cast<const uint8_t *>(converted->pixels) + y * converted->pitch);
        color_t *dst = pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(converted->w);
        memcpy(dst, src, static_cast<size_t>(converted->w) * sizeof(color_t));
    }

    graphics_renderer()->upload_image_resource(&entry.resource, pixels.data(), converted->w, converted->h);
    entry.slice.handle = entry.resource.resource_handle;
    entry.slice.width = converted->w;
    entry.slice.height = converted->h;
    entry.slice.draw_offset_x = 0;
    entry.slice.draw_offset_y = 0;
    entry.slice.is_isometric = 0;

    SDL_FreeSurface(converted);

    auto inserted = g_runtime.glyphs.emplace(key, std::move(entry));
    return &inserted.first->second;
}

DrawColors resolve_draw_colors(font_t font, color_t requested)
{
    DrawColors colors;
    switch (font) {
        case FONT_NORMAL_WHITE:
            colors.main = COLOR_WHITE;
            colors.shadow = 0xff311c10;
            colors.has_shadow = 1;
            return colors;
        case FONT_NORMAL_RED:
            colors.main = 0xff731408;
            colors.shadow = 0xffe7cfad;
            colors.has_shadow = 1;
            return colors;
        case FONT_NORMAL_GREEN:
            colors.main = 0xff180800;
            colors.shadow = 0xffe7cfad;
            colors.has_shadow = 1;
            return colors;
        case FONT_NORMAL_BLACK:
        case FONT_LARGE_BLACK:
            colors.main = COLOR_BLACK;
            colors.shadow = 0xffcead9c;
            colors.has_shadow = 1;
            return colors;
        case FONT_LARGE_BROWN:
        case FONT_NORMAL_BROWN:
            colors.main = COLOR_FONT_PLAIN;
            return colors;
        case FONT_NORMAL_PLAIN:
        case FONT_LARGE_PLAIN:
        case FONT_SMALL_PLAIN:
        default:
            colors.main = requested ? requested : COLOR_FONT_PLAIN;
            return colors;
    }
}

float kerning_logical_width(const ResolvedFace &resolved, uint32_t previous_codepoint, uint32_t codepoint, float scale)
{
    if (!resolved.font || !previous_codepoint) {
        return 0.0f;
    }
    int kerning = TTF_GetFontKerningSizeGlyphs32(resolved.font->handle, previous_codepoint, codepoint);
    if (!kerning) {
        return 0.0f;
    }
    return kerning / raster_multiplier_for(scale);
}

float glyph_advance_logical_width(const GlyphCacheEntry &glyph, float scale)
{
    return glyph.advance / raster_multiplier_for(scale);
}

float glyph_left_logical_offset(const GlyphCacheEntry &glyph, float scale)
{
    return glyph.minx / raster_multiplier_for(scale);
}

float line_top_logical_offset(font_t surface_font)
{
    return static_cast<float>(font_vector_runtime_baseline_offset(surface_font));
}

float snap_logical_to_raster(float logical_value, float scale)
{
    const float multiplier = raster_multiplier_for(scale);
    if (multiplier <= 0.0f) {
        return logical_value;
    }
    return std::round(logical_value * multiplier) / multiplier;
}

float glyph_logical_width(const GlyphCacheEntry &glyph, float scale)
{
    return glyph.slice.width / raster_multiplier_for(scale);
}

float glyph_logical_height(const GlyphCacheEntry &glyph, float scale)
{
    return glyph.slice.height / raster_multiplier_for(scale);
}

int measure_space_for_surface(SurfaceConfig &surface)
{
    if (!ensure_ttf_initialized()) {
        return std::max(1, surface.logical_size / 3);
    }

    auto family_it = g_runtime.families.find(surface.family_id);
    if (family_it == g_runtime.families.end()) {
        return std::max(1, surface.logical_size / 3);
    }

    const std::string &face_path = family_it->second.faces.regular;
    SizedFont *font = ensure_sized_font(face_path, std::max(1, surface.logical_size));
    if (!font || !font->handle) {
        return std::max(1, surface.logical_size / 3);
    }

    int width = 0;
    int height = 0;
    if (TTF_SizeUTF8(font->handle, " ", &width, &height) != 0 || width <= 0) {
        width = std::max(1, surface.logical_size / 3);
    }
    return width;
}

int validate_runtime(RuntimeState &runtime)
{
    for (int i = 0; i < FONT_TYPES_MAX; ++i) {
        SurfaceConfig &surface = runtime.surfaces[i];
        if (!surface.defined) {
            runtime.failure_reason = std::string("fonts.xml is missing a surface assignment for ") + kFontNames[i] + ".";
            return 0;
        }
        if (surface.logical_size <= 0 || surface.line_height <= 0) {
            runtime.failure_reason = std::string("fonts.xml has an invalid size or line height for ") + kFontNames[i] + ".";
            return 0;
        }

        auto family_it = runtime.families.find(surface.family_id);
        if (family_it == runtime.families.end()) {
            runtime.failure_reason = std::string("fonts.xml references an unknown family for ") + kFontNames[i] + ".";
            return 0;
        }

        const FamilyConfig &family = family_it->second;
        if (family.faces.regular.empty() || !file_exists(family.faces.regular.c_str(), NOT_LOCALIZED)) {
            runtime.failure_reason = std::string("The regular face for family '") + family.id + "' could not be found.";
            return 0;
        }

        if (surface.semantic == FontSurfaceSemantic::Body || surface.semantic == FontSurfaceSemantic::Link) {
            if (family.faces.bold.empty() || family.faces.italic.empty() || family.faces.bold_italic.empty()) {
                runtime.failure_reason = std::string("Family '") + family.id + "' is missing bold, italic, or bold_italic faces required for body text.";
                return 0;
            }
            if (!file_exists(family.faces.bold.c_str(), NOT_LOCALIZED)
                || !file_exists(family.faces.italic.c_str(), NOT_LOCALIZED)
                || !file_exists(family.faces.bold_italic.c_str(), NOT_LOCALIZED)) {
                runtime.failure_reason = std::string("One or more styled faces for family '") + family.id + "' could not be found.";
                return 0;
            }
        }

        for (const FallbackRule &rule : family.fallbacks) {
            if (runtime.families.find(rule.family_id) == runtime.families.end()) {
                runtime.failure_reason = std::string("Family '") + family.id + "' references an unknown fallback family.";
                return 0;
            }
        }
    }

    return 1;
}

int parse_font_pack(const std::string &pack_path, const std::string &base_dir, RuntimeState &runtime)
{
    runtime = {};
    runtime.base_dir = base_dir;
    runtime.loaded_pack_path = pack_path;

    g_parse = {};
    g_parse.target = &runtime;
    g_parse.base_dir = base_dir;
    g_runtime.failure_reason.clear();

    if (!xml_parser_init(kXmlElements, XML_TOTAL_ELEMENTS, 1)) {
        runtime.failure_reason = "Failed to initialize the fonts.xml parser.";
        return 0;
    }

    FILE *file = file_open(pack_path.c_str(), "rb");
    if (!file) {
        xml_parser_free();
        runtime.failure_reason = std::string("Failed to open fonts.xml.\n\n") + pack_path;
        return 0;
    }

    char buffer[XML_BUFFER_SIZE];
    int done = 0;
    int parsed = 1;
    while (!done) {
        size_t bytes_read = fread(buffer, 1, sizeof(buffer), file);
        done = bytes_read < sizeof(buffer);
        if (!xml_parser_parse(buffer, static_cast<unsigned int>(bytes_read), done)) {
            parsed = 0;
            break;
        }
    }

    file_close(file);
    xml_parser_free();

    if (!parsed || g_parse.error || !g_parse.saw_root) {
        if (runtime.failure_reason.empty()) {
            runtime.failure_reason = std::string("Failed to parse fonts.xml.\n\n") + pack_path;
        }
        return 0;
    }

    return validate_runtime(runtime);
}

std::vector<std::pair<std::string, std::string>> candidate_pack_paths()
{
    std::vector<std::pair<std::string, std::string>> paths;
    auto append_candidate = [&paths](const char *mod_path) {
        if (!mod_path || !*mod_path) {
            return;
        }
        const std::string base_dir = append_path_component(mod_path, "Fonts");
        const std::string pack_path = append_path_component(base_dir, "fonts.xml");
        for (const auto &existing : paths) {
            if (existing.first == pack_path) {
                return;
            }
        }
        paths.emplace_back(pack_path, base_dir);
    };

    append_candidate(mod_manager_get_mod_path());
    for (int i = 0; i < mod_manager_get_mod_count(); ++i) {
        const char *name = mod_manager_get_mod_name_at(i);
        if (name && string_equals_case_insensitive(name, "Augustus")) {
            append_candidate(mod_manager_get_mod_path_at(i));
        }
    }
    for (int i = 0; i < mod_manager_get_mod_count(); ++i) {
        const char *name = mod_manager_get_mod_name_at(i);
        if (name && string_equals_case_insensitive(name, "Julius")) {
            append_candidate(mod_manager_get_mod_path_at(i));
        }
    }
    return paths;
}

} // namespace

bool font_vector_runtime_load_pack()
{
    font_vector_runtime_reset();
    g_runtime.failure_reason.clear();

    const std::vector<std::pair<std::string, std::string>> candidates = candidate_pack_paths();
    for (const auto &candidate : candidates) {
        if (!file_exists(candidate.first.c_str(), NOT_LOCALIZED)) {
            continue;
        }

        RuntimeState parsed;
        if (!parse_font_pack(candidate.first, candidate.second, parsed)) {
            g_runtime.failure_reason = parsed.failure_reason.empty() ? g_runtime.failure_reason : parsed.failure_reason;
            return false;
        }

        if (!ensure_ttf_initialized()) {
            return false;
        }

        g_runtime.families = std::move(parsed.families);
        g_runtime.surfaces = std::move(parsed.surfaces);
        g_runtime.base_dir = parsed.base_dir;
        g_runtime.loaded_pack_path = parsed.loaded_pack_path;
        g_runtime.active = 1;
        g_runtime.cached_ui_scale = platform_screen_get_scale();

        for (int i = 0; i < FONT_TYPES_MAX; ++i) {
            g_runtime.surfaces[i].space_width = measure_space_for_surface(g_runtime.surfaces[i]);
        }
        release_font_cache();
        return true;
    }

    return true;
}

void font_vector_runtime_reset()
{
    release_runtime_state();
    if (g_runtime.ttf_initialized) {
        TTF_Quit();
        g_runtime.ttf_initialized = 0;
    }
}

bool font_vector_runtime_is_active()
{
    return g_runtime.active != 0;
}

const char *font_vector_runtime_failure_reason()
{
    return g_runtime.failure_reason.c_str();
}

int font_vector_runtime_space_width(font_t font)
{
    if (!g_runtime.active || font < 0 || font >= FONT_TYPES_MAX) {
        return 0;
    }
    return g_runtime.surfaces[font].space_width;
}

int font_vector_runtime_line_height(font_t font)
{
    if (!g_runtime.active || font < 0 || font >= FONT_TYPES_MAX) {
        return 0;
    }
    return g_runtime.surfaces[font].line_height;
}

int font_vector_runtime_letter_spacing(font_t font)
{
    if (!g_runtime.active || font < 0 || font >= FONT_TYPES_MAX) {
        return 0;
    }
    return g_runtime.surfaces[font].letter_spacing;
}

int font_vector_runtime_baseline_offset(font_t font)
{
    if (!g_runtime.active || font < 0 || font >= FONT_TYPES_MAX) {
        return 0;
    }
    return g_runtime.surfaces[font].baseline_offset;
}

FontSurfaceSemantic font_vector_runtime_semantic(font_t font)
{
    if (!g_runtime.active || font < 0 || font >= FONT_TYPES_MAX) {
        return FontSurfaceSemantic::Body;
    }
    return g_runtime.surfaces[font].semantic;
}

int font_vector_runtime_can_display_utf8(std::string_view utf8_character)
{
    if (!g_runtime.active) {
        return 0;
    }
    uint32_t codepoint = 0;
    size_t bytes = 0;
    if (!next_utf8_codepoint(utf8_character, 0, &codepoint, &bytes)) {
        return 0;
    }
    for (int font = 0; font < FONT_TYPES_MAX; ++font) {
        ResolvedFace resolved = resolve_face_for(static_cast<font_t>(font), codepoint, FONT_INLINE_STYLE_NONE, SCALE_NONE);
        if (resolved.font && TTF_GlyphIsProvided32(resolved.font->handle, codepoint)) {
            return 1;
        }
    }
    return 0;
}

int font_vector_runtime_measure_utf8(std::string_view text, font_t font, unsigned style_flags, float scale)
{
    if (!g_runtime.active || text.empty()) {
        return 0;
    }

    unsigned sanitized_flags = sanitize_style_flags(font, style_flags);
    float width = 0.0f;
    uint32_t previous_codepoint = 0;
    ResolvedFace previous_face;

    for (size_t offset = 0; offset < text.size();) {
        uint32_t codepoint = 0;
        size_t num_bytes = 0;
        if (!next_utf8_codepoint(text, offset, &codepoint, &num_bytes)) {
            break;
        }

        if (codepoint == '\n' || codepoint == '\r') {
            offset += num_bytes;
            previous_codepoint = 0;
            previous_face = {};
            continue;
        }

        if (codepoint == ' ') {
            width += font_vector_runtime_space_width(font);
            offset += num_bytes;
            previous_codepoint = 0;
            previous_face = {};
            continue;
        }

        ResolvedFace resolved = resolve_face_for(font, codepoint, sanitized_flags, scale);
        GlyphCacheEntry *glyph = ensure_glyph(font, codepoint, sanitized_flags, scale);
        if (!resolved.font || !glyph) {
            offset += num_bytes;
            continue;
        }

        if (previous_face.font == resolved.font) {
            width += kerning_logical_width(resolved, previous_codepoint, codepoint, scale);
        }
        width += glyph_advance_logical_width(*glyph, scale);
        width += font_vector_runtime_letter_spacing(font);

        previous_codepoint = codepoint;
        previous_face = resolved;
        offset += num_bytes;
    }

    return static_cast<int>(std::lround(width));
}

unsigned int font_vector_runtime_fit_utf8_bytes(
    std::string_view text,
    font_t font,
    unsigned int requested_width,
    int invert,
    unsigned style_flags)
{
    if (!g_runtime.active || text.empty()) {
        return 0;
    }

    std::vector<unsigned int> byte_boundaries;
    std::vector<int> widths;
    byte_boundaries.push_back(0);
    widths.push_back(0);

    unsigned int consumed = 0;
    int total_width = 0;
    for (size_t offset = 0; offset < text.size();) {
        uint32_t codepoint = 0;
        size_t num_bytes = 0;
        if (!next_utf8_codepoint(text, offset, &codepoint, &num_bytes)) {
            break;
        }
        consumed += static_cast<unsigned int>(num_bytes);
        total_width = font_vector_runtime_measure_utf8(text.substr(0, consumed), font, style_flags, SCALE_NONE);
        byte_boundaries.push_back(consumed);
        widths.push_back(total_width);
        offset += num_bytes;
    }

    if (!invert) {
        unsigned int best = 0;
        for (size_t i = 0; i < widths.size(); ++i) {
            if (widths[i] > static_cast<int>(requested_width)) {
                break;
            }
            best = byte_boundaries[i];
        }
        return best;
    }

    unsigned int suffix = byte_boundaries.empty() ? 0 : byte_boundaries.back();
    for (size_t i = 0; i < widths.size(); ++i) {
        int remaining = total_width - widths[i];
        if (remaining <= static_cast<int>(requested_width)) {
            suffix = byte_boundaries.back() - byte_boundaries[i];
            break;
        }
    }
    return suffix;
}

int font_vector_runtime_draw_utf8(
    std::string_view text,
    int x,
    int y,
    font_t font,
    color_t color,
    float scale,
    unsigned style_flags)
{
    if (!g_runtime.active || text.empty()) {
        return 0;
    }

    unsigned sanitized_flags = sanitize_style_flags(font, style_flags);
    const DrawColors colors = resolve_draw_colors(font, color);
    float current_x = static_cast<float>(x);
    uint32_t previous_codepoint = 0;
    ResolvedFace previous_face;

    for (size_t offset = 0; offset < text.size();) {
        uint32_t codepoint = 0;
        size_t num_bytes = 0;
        if (!next_utf8_codepoint(text, offset, &codepoint, &num_bytes)) {
            break;
        }

        if (codepoint == '\n' || codepoint == '\r') {
            offset += num_bytes;
            previous_codepoint = 0;
            previous_face = {};
            continue;
        }

        if (codepoint == ' ') {
            current_x += font_vector_runtime_space_width(font);
            offset += num_bytes;
            previous_codepoint = 0;
            previous_face = {};
            continue;
        }

        ResolvedFace resolved = resolve_face_for(font, codepoint, sanitized_flags, scale);
        GlyphCacheEntry *glyph = ensure_glyph(font, codepoint, sanitized_flags, scale);
        if (!resolved.font || !glyph || !glyph->slice.is_valid()) {
            offset += num_bytes;
            continue;
        }

        if (previous_face.font == resolved.font) {
            current_x += kerning_logical_width(resolved, previous_codepoint, codepoint, scale);
        }

        const float draw_x = snap_logical_to_raster(
            current_x + glyph_left_logical_offset(*glyph, scale),
            scale);
        // Match the legacy sprite-font contract: the provided y is the shared line-top
        // anchor for normal Latin text, not a typographic baseline per glyph.
        const float draw_y = snap_logical_to_raster(
            static_cast<float>(y) + line_top_logical_offset(font),
            scale);
        const float logical_width = glyph_logical_width(*glyph, scale);
        const float logical_height = glyph_logical_height(*glyph, scale);

        auto draw_pass = [&](color_t pass_color, int dx, int dy) {
            runtime_texture_draw_request(
                glyph->slice,
                draw_x + dx,
                draw_y + dy,
                logical_width,
                logical_height,
                pass_color ? pass_color : kOpaqueWhite,
                graphics_renderer()->get_render_domain(),
                RENDER_SCALING_POLICY_PIXEL_ART);
        };

        if (colors.has_shadow) {
            draw_pass(colors.shadow, 1, 1);
        }
        draw_pass(colors.main, 0, 0);

        current_x += glyph_advance_logical_width(*glyph, scale);
        current_x += font_vector_runtime_letter_spacing(font);

        previous_codepoint = codepoint;
        previous_face = resolved;
        offset += num_bytes;
    }

    if ((sanitized_flags & FONT_INLINE_STYLE_UNDERLINE) != 0 && current_x > x) {
        int underline_y = y + std::max(1, font_vector_runtime_line_height(font) - 2);
        int underline_width = static_cast<int>(std::lround(current_x - x));
        graphics_fill_rect(x, underline_y, underline_width, 1, colors.main);
    }

    return static_cast<int>(std::lround(current_x - x));
}

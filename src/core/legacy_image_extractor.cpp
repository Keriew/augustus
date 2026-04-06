#include "core/legacy_image_extractor.h"

extern "C" {
#include "core/file.h"
#include "core/log.h"
#include "game/mod_manager.h"
#include "platform/file_manager.h"
}

#include "spng/spng.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr char kExtractionStampPrefix[] = "legacy_extract_v4:";
struct LegacyFamily {
    const char *folder_name;
};

struct LegacyGroupRange {
    int group_id;
    int first_image_id;
    int last_image_id;
};

struct ExtractionStats {
    int groups_exported = 0;
    int images_exported = 0;
    int pngs_written = 0;
};

constexpr LegacyFamily kAdminLogistics { "Admin_Logistics" };
constexpr LegacyFamily kAesthetics { "Aesthetics" };
constexpr LegacyFamily kHealthCulture { "Health_Culture" };
constexpr LegacyFamily kIndustry { "Industry" };
constexpr LegacyFamily kMilitary { "Military" };
constexpr LegacyFamily kMonuments { "Monuments" };
constexpr LegacyFamily kShips { "Ships" };
constexpr LegacyFamily kTerrainMaps { "Terrain_Maps" };
constexpr LegacyFamily kUi { "UI" };
constexpr LegacyFamily kWalkers { "Walkers" };
constexpr LegacyFamily kWarriors { "Warriors" };
constexpr LegacyFamily kFonts { "Fonts" };
constexpr LegacyFamily kFx { "FX" };
constexpr LegacyFamily kPaperMap { "PaperMap" };
constexpr LegacyFamily kMap { "Map" };
constexpr LegacyFamily kLoadingScreens { "LoadingScreens" };
constexpr LegacyFamily kPortraits { "Portraits" };
constexpr LegacyFamily kEnvironment { "Environment" };
constexpr LegacyFamily kBuildings { "Buildings" };

constexpr const LegacyFamily *kAllFamilies[] = {
    &kAdminLogistics,
    &kAesthetics,
    &kHealthCulture,
    &kIndustry,
    &kMilitary,
    &kMonuments,
    &kShips,
    &kTerrainMaps,
    &kUi,
    &kWalkers,
    &kWarriors,
    &kFonts,
    &kFx,
    &kPaperMap,
    &kMap,
    &kLoadingScreens,
    &kPortraits,
    &kEnvironment,
    &kBuildings
};

static bool is_placeholder_main_image(int image_id)
{
    return image_id >= 6145 && image_id <= 6192;
}

static bool is_exportable_main_image(const image *img)
{
    if (!img || img->width <= 0 || img->height <= 0) {
        return false;
    }
    return (img->atlas.id >> IMAGE_ATLAS_BIT_OFFSET) == ATLAS_MAIN;
}

static std::string sanitize_component(const char *text)
{
    if (!text || !*text) {
        return "unnamed";
    }

    std::string sanitized;
    sanitized.reserve(strlen(text));
    for (const char *cursor = text; *cursor; ++cursor) {
        const char value = *cursor;
        const bool is_alpha = (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
        const bool is_digit = value >= '0' && value <= '9';
        if (is_alpha || is_digit || value == '_' || value == '-') {
            sanitized.push_back(value);
        } else {
            sanitized.push_back('_');
        }
    }
    while (!sanitized.empty() && sanitized.back() == '_') {
        sanitized.pop_back();
    }
    return sanitized.empty() ? std::string("unnamed") : sanitized;
}

static std::string make_source_stem(const char *source_name)
{
    const char *basename = file_remove_path(source_name);
    std::string stem = basename ? basename : "legacy";
    const std::string::size_type dot = stem.find_last_of('.');
    if (dot != std::string::npos) {
        stem.erase(dot);
    }
    stem = sanitize_component(stem.c_str());
    return stem.empty() ? std::string("legacy") : stem;
}

static std::string make_unknown_group_folder_name(int group_id)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "Group_%03d", group_id);
    return buffer;
}

static std::string make_group_folder_name(int group_id)
{
    switch (group_id) {
        case GROUP_TERRAIN_BLACK: return "Black";
        case GROUP_TERRAIN_SHRUB: return "Shrub";
        case GROUP_TERRAIN_UGLY_GRASS: return "Ugly_Grass";
        case GROUP_TERRAIN_TREE: return "Tree";
        case GROUP_TERRAIN_WATER: return "Water";
        case GROUP_TERRAIN_EARTHQUAKE: return "Earthquake";
        case GROUP_TERRAIN_GRASS_2: return "Grass_2";
        case GROUP_TERRAIN_ELEVATION_ROCK: return "Elevation_Rock";
        case GROUP_TERRAIN_ELEVATION: return "Elevation";
        case GROUP_TERRAIN_GRASS_1: return "Grass_1";
        case GROUP_TERRAIN_OVERLAY: return "Overlay";
        case GROUP_TERRAIN_FLAT_TILE: return "Flat_Tile";
        case GROUP_TERRAIN_PLAZA: return "Plaza";
        case GROUP_TERRAIN_GARDEN: return "Garden";
        case GROUP_TERRAIN_ROAD: return "Road";
        case GROUP_TERRAIN_RUBBLE: return "Rubble";
        case GROUP_TERRAIN_RUBBLE_TENT: return "Rubble_Tent";
        case GROUP_TERRAIN_RUBBLE_GENERAL: return "Rubble_General";
        case GROUP_TERRAIN_DESIRABILITY: return "Desirability";
        case GROUP_TERRAIN_MEADOW: return "Meadow";
        case GROUP_TERRAIN_WATER_SHORE: return "Water_Shore";
        case GROUP_TERRAIN_ACCESS_RAMP: return "Access_Ramp";
        case GROUP_TERRAIN_ROCK: return "Rock";
        case GROUP_TERRAIN_ENTRY_EXIT_FLAGS: return "Entry_Exit_Flags";

        case GROUP_TOP_MENU: return "Top_Menu";
        case GROUP_SIDE_PANEL: return "Side_Panel";
        case GROUP_SIDEBAR_ADVISORS_EMPIRE: return "Sidebar_Advisors_Empire";
        case GROUP_MAIN_MENU_BACKGROUND: return "Main_Menu_Background";
        case GROUP_PANEL_BUTTON: return "Panel_Button";
        case GROUP_FONT: return "Font";
        case GROUP_EMPIRE_MAP: return "Empire_Map";
        case GROUP_SIDEBAR_BRIEFING_ROTATE_BUTTONS: return "Sidebar_Briefing_Rotate_Buttons";
        case GROUP_MESSAGE_ICON: return "Message_Icon";
        case GROUP_SIDEBAR_BUTTONS: return "Sidebar_Buttons";
        case GROUP_LABOR_PRIORITY_LOCK: return "Labor_Priority_Lock";
        case GROUP_PANEL_WINDOWS: return "Panel_Windows";
        case GROUP_OK_CANCEL_SCROLL_BUTTONS: return "Ok_Cancel_Scroll_Buttons";
        case GROUP_OVERLAY_COLUMN: return "Overlay_Column";
        case GROUP_ADVISOR_ICONS: return "Advisor_Icons";
        case GROUP_RESOURCE_ICONS: return "Resource_Icons";
        case GROUP_DIALOG_BACKGROUND: return "Dialog_Background";
        case GROUP_SUNKEN_TEXTBOX_BACKGROUND: return "Sunken_Textbox_Background";
        case GROUP_CONTEXT_ICONS: return "Context_Icons";
        case GROUP_ADVISOR_BACKGROUND: return "Advisor_Background";
        case GROUP_EDITOR_SIDEBAR_BUTTONS: return "Editor_Sidebar_Buttons";
        case GROUP_MINIMAP_EMPTY_LAND: return "Minimap_Empty_Land";
        case GROUP_MINIMAP_WATER: return "Minimap_Water";
        case GROUP_MINIMAP_TREE: return "Minimap_Tree";
        case GROUP_MINIMAP_ROCK: return "Minimap_Rock";
        case GROUP_MINIMAP_MEADOW: return "Minimap_Meadow";
        case GROUP_MINIMAP_ROAD: return "Minimap_Road";
        case GROUP_MINIMAP_HOUSE: return "Minimap_House";
        case GROUP_MINIMAP_BUILDING: return "Minimap_Building";
        case GROUP_MINIMAP_WALL: return "Minimap_Wall";
        case GROUP_MINIMAP_AQUEDUCT: return "Minimap_Aqueduct";
        case GROUP_MINIMAP_BLACK: return "Minimap_Black";
        case GROUP_POPULATION_GRAPH_BAR: return "Population_Graph_Bar";
        case GROUP_BULLET: return "Bullet";
        case GROUP_MESSAGE_IMAGES: return "Message_Images";
        case GROUP_WIN_GAME_BACKGROUND: return "Win_Game_Background";
        case GROUP_CONFIG: return "Config";
        case GROUP_LOGO: return "Logo";
        case GROUP_EMPIRE_PANELS: return "Empire_Panels";
        case GROUP_EMPIRE_RESOURCES: return "Empire_Resources";
        case GROUP_EMPIRE_CITY: return "Empire_City";
        case GROUP_EMPIRE_CITY_TRADE: return "Empire_City_Trade";
        case GROUP_EMPIRE_CITY_DISTANT_ROMAN: return "Empire_City_Distant_Roman";
        case GROUP_EMPIRE_BATTLE: return "Empire_Battle";
        case GROUP_EMPIRE_ROMAN_ARMY: return "Empire_Roman_Army";
        case GROUP_EMPIRE_TRADE_ROUTE_TYPE: return "Empire_Trade_Route_Type";
        case GROUP_EMPIRE_BORDER_EDGE: return "Empire_Border_Edge";
        case GROUP_EMPIRE_TRADE_LAND: return "Empire_Trade_Land";
        case GROUP_EMPIRE_TRADE_SEA: return "Empire_Trade_Sea";
        case GROUP_RATINGS_COLUMN: return "Ratings_Column";
        case GROUP_BIG_PEOPLE: return "Big_People";
        case GROUP_RATINGS_BACKGROUND: return "Ratings_Background";
        case GROUP_FORT_FORMATIONS: return "Fort_Formations";
        case GROUP_MESSAGE_ADVISOR_BUTTONS: return "Message_Advisor_Buttons";
        case GROUP_BORDERED_BUTTON: return "Bordered_Button";
        case GROUP_FORT_ICONS: return "Fort_Icons";
        case GROUP_EMPIRE_FOREIGN_CITY: return "Empire_Foreign_City";
        case GROUP_EMPIRE_ENEMY_ARMY: return "Empire_Enemy_Army";
        case GROUP_GOD_BOLT: return "God_Bolt";
        case GROUP_PLAGUE_SKULL: return "Plague_Skull";
        case GROUP_TRADE_AMOUNT: return "Trade_Amount";
        case GROUP_SELECT_MISSION_BACKGROUND: return "Select_Mission_Background";
        case GROUP_SELECT_MISSION: return "Select_Mission";
        case GROUP_CCK_BACKGROUND: return "CCK_Background";
        case GROUP_SCENARIO_IMAGE: return "Scenario_Image";
        case GROUP_LOADING_SCREEN: return "Loading_Screen";
        case GROUP_INTERMEZZO_BACKGROUND: return "Intermezzo_Background";
        case GROUP_PANEL_WINDOWS_DESERT: return "Panel_Windows_Desert";
        case GROUP_SELECT_MISSION_BUTTON: return "Select_Mission_Button";

        case GROUP_BUILDING_TOWER: return "Tower";
        case GROUP_BUILDING_AQUEDUCT: return "Aqueduct";
        case GROUP_BUILDING_MARKET: return "Market";
        case GROUP_BUILDING_WELL: return "Well";
        case GROUP_BUILDING_WALL: return "Wall";
        case GROUP_BUILDING_RESERVOIR: return "Reservoir";
        case GROUP_BUILDING_HOUSE_TENT: return "House_Tent";
        case GROUP_BUILDING_HOUSE_SHACK: return "House_Shack";
        case GROUP_BUILDING_HOUSE_HOVEL: return "House_Hovel";
        case GROUP_BUILDING_HOUSE_CASA: return "House_Casa";
        case GROUP_BUILDING_HOUSE_INSULA_1: return "House_Insula_1";
        case GROUP_BUILDING_HOUSE_INSULA_2: return "House_Insula_2";
        case GROUP_BUILDING_HOUSE_VILLA_1: return "House_Villa_1";
        case GROUP_BUILDING_HOUSE_VILLA_2: return "House_Villa_2";
        case GROUP_BUILDING_HOUSE_PALACE_1: return "House_Palace_1";
        case GROUP_BUILDING_HOUSE_PALACE_2: return "House_Palace_2";
        case GROUP_BUILDING_HOUSE_VACANT_LOT: return "House_Vacant_Lot";
        case GROUP_BUILDING_FARM_HOUSE: return "Farm_House";
        case GROUP_BUILDING_MARBLE_QUARRY: return "Marble_Quarry";
        case GROUP_BUILDING_IRON_MINE: return "Iron_Mine";
        case GROUP_BUILDING_CLAY_PIT: return "Clay_Pit";
        case GROUP_BUILDING_SCHOOL: return "School";
        case GROUP_BUILDING_LIBRARY: return "Library";
        case GROUP_BUILDING_ACADEMY: return "Academy";
        case GROUP_BUILDING_WINE_WORKSHOP: return "Wine_Workshop";
        case GROUP_BUILDING_AMPHITHEATER: return "Amphitheater";
        case GROUP_BUILDING_THEATER: return "Theater";
        case GROUP_BUILDING_COLOSSEUM: return "Colosseum";
        case GROUP_BUILDING_GLADIATOR_SCHOOL: return "Gladiator_School";
        case GROUP_BUILDING_LION_HOUSE: return "Lion_House";
        case GROUP_BUILDING_ACTOR_COLONY: return "Actor_Colony";
        case GROUP_BUILDING_CHARIOT_MAKER: return "Chariot_Maker";
        case GROUP_BUILDING_FOUNTAIN_4: return "Fountain_4";
        case GROUP_BUILDING_FOUNTAIN_1: return "Fountain_1";
        case GROUP_BUILDING_FOUNTAIN_2: return "Fountain_2";
        case GROUP_BUILDING_FOUNTAIN_3: return "Fountain_3";
        case GROUP_BUILDING_WORKSHOP_RAW_MATERIAL: return "Workshop_Raw_Material";
        case GROUP_BUILDING_STATUE: return "Statue";
        case GROUP_BUILDING_SENATE: return "Senate";
        case GROUP_BUILDING_FORUM: return "Forum";
        case GROUP_BUILDING_PREFECTURE: return "Prefecture";
        case GROUP_BUILDING_TIMBER_YARD: return "Timber_Yard";
        case GROUP_BUILDING_FORT: return "Fort";
        case GROUP_BUILDING_BARBER: return "Barber";
        case GROUP_BUILDING_DOCTOR: return "Doctor";
        case GROUP_BUILDING_BATHHOUSE_WATER: return "Bathhouse_Water";
        case GROUP_BUILDING_HOSPITAL: return "Hospital";
        case GROUP_BUILDING_TEMPLE_CERES: return "Temple_Ceres";
        case GROUP_BUILDING_TEMPLE_NEPTUNE: return "Temple_Neptune";
        case GROUP_BUILDING_TEMPLE_MERCURY: return "Temple_Mercury";
        case GROUP_BUILDING_TEMPLE_MARS: return "Temple_Mars";
        case GROUP_BUILDING_TEMPLE_VENUS: return "Temple_Venus";
        case GROUP_BUILDING_ORACLE: return "Oracle";
        case GROUP_BUILDING_SHIPYARD: return "Shipyard";
        case GROUP_BUILDING_DOCK_1: return "Dock_1";
        case GROUP_BUILDING_WHARF: return "Wharf";
        case GROUP_BUILDING_ENGINEERS_POST: return "Engineers_Post";
        case GROUP_BUILDING_WAREHOUSE: return "Warehouse";
        case GROUP_BUILDING_WAREHOUSE_STORAGE_EMPTY: return "Warehouse_Storage_Empty";
        case GROUP_BUILDING_WAREHOUSE_STORAGE_FILLED: return "Warehouse_Storage_Filled";
        case GROUP_BUILDING_GOVERNORS_HOUSE: return "Governors_House";
        case GROUP_BUILDING_GOVERNORS_VILLA: return "Governors_Villa";
        case GROUP_BUILDING_GOVERNORS_PALACE: return "Governors_Palace";
        case GROUP_BUILDING_GRANARY: return "Granary";
        case GROUP_BUILDING_FARM_CROPS: return "Farm_Crops";
        case GROUP_BUILDING_OIL_WORKSHOP: return "Oil_Workshop";
        case GROUP_BUILDING_WEAPONS_WORKSHOP: return "Weapons_Workshop";
        case GROUP_BUILDING_FURNITURE_WORKSHOP: return "Furniture_Workshop";
        case GROUP_BUILDING_POTTERY_WORKSHOP: return "Pottery_Workshop";
        case GROUP_BUILDING_BRIDGE: return "Bridge";
        case GROUP_BUILDING_BARRACKS: return "Barracks";
        case GROUP_BUILDING_DOCK_2: return "Dock_2";
        case GROUP_BUILDING_DOCK_3: return "Dock_3";
        case GROUP_BUILDING_DOCK_4: return "Dock_4";
        case GROUP_BUILDING_DOCK_DOCKERS: return "Dock_Dockers";
        case GROUP_BUILDING_NATIVE: return "Native";
        case GROUP_BUILDING_MISSION_POST: return "Mission_Post";
        case GROUP_BUILDING_BATHHOUSE_NO_WATER: return "Bathhouse_No_Water";
        case GROUP_BUILDING_THEATER_SHOW: return "Theater_Show";
        case GROUP_BUILDING_AMPHITHEATER_SHOW: return "Amphitheater_Show";
        case GROUP_BUILDING_COLOSSEUM_SHOW: return "Colosseum_Show";
        case GROUP_BUILDING_AQUEDUCT_NO_WATER: return "Aqueduct_No_Water";
        case GROUP_BUILDING_MILITARY_ACADEMY: return "Military_Academy";
        case GROUP_BUILDING_FORT_JAVELIN: return "Fort_Javelin";
        case GROUP_BUILDING_FORT_LEGIONARY: return "Fort_Legionary";
        case GROUP_BUILDING_TRIUMPHAL_ARCH: return "Triumphal_Arch";
        case GROUP_BUILDING_MARKET_FANCY: return "Market_Fancy";
        case GROUP_BUILDING_BATHHOUSE_FANCY_WATER: return "Bathhouse_Fancy_Water";
        case GROUP_BUILDING_BATHHOUSE_FANCY_NO_WATER: return "Bathhouse_Fancy_No_Water";
        case GROUP_BUILDING_HIPPODROME_1: return "Hippodrome_1";
        case GROUP_BUILDING_HIPPODROME_2: return "Hippodrome_2";
        case GROUP_BUILDING_SENATE_FANCY: return "Senate_Fancy";
        case GROUP_BUILDING_TRADE_CENTER_FLAG: return "Trade_Center_Flag";
        case GROUP_BUILDING_GATEHOUSE: return "Gatehouse";

        default:
            return make_unknown_group_folder_name(group_id);
    }
}

static std::string make_family_root_directory(const LegacyFamily &family)
{
    return std::string(mod_manager_get_julius_graphics_path()) + family.folder_name;
}

static std::string make_group_directory(const LegacyFamily &family, int group_id)
{
    return make_family_root_directory(family) + "/" + make_group_folder_name(group_id);
}

static std::string make_group_assetlist_name(const LegacyFamily &family, int group_id)
{
    return std::string(family.folder_name) + "\\" + make_group_folder_name(group_id);
}

static std::string make_group_xml_path(const LegacyFamily &family, int group_id)
{
    return make_family_root_directory(family) + "/" + make_group_folder_name(group_id) + ".xml";
}

static std::string make_stamp_path(void)
{
    return std::string(mod_manager_get_julius_graphics_path()) + ".legacy_extract.stamp";
}

static std::string make_manifest_path(void)
{
    return std::string(mod_manager_get_julius_graphics_path()) + ".legacy_extract.manifest";
}

static void ensure_directory(const std::string &path)
{
    if (!path.empty()) {
        platform_file_manager_create_directory(path.c_str(), 0, 1);
    }
}

static bool read_text_file(const std::string &path, std::string &contents)
{
    FILE *file = file_open(path.c_str(), "rb");
    if (!file) {
        contents.clear();
        return false;
    }

    char buffer[128];
    const size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, file);
    buffer[bytes_read] = '\0';
    contents.assign(buffer);
    file_close(file);
    return true;
}

static bool write_text_file(const std::string &path, const std::string &contents)
{
    FILE *file = file_open(path.c_str(), "wb");
    if (!file) {
        return false;
    }

    const size_t bytes_written = fwrite(contents.data(), 1, contents.size(), file);
    file_close(file);
    return bytes_written == contents.size();
}

static void hash_stamp_bytes(uint64_t &hash, const void *data, size_t size)
{
    const uint8_t *bytes = static_cast<const uint8_t *>(data);
    const uint64_t fnv_prime = 1099511628211ull;
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= fnv_prime;
    }
}

static void hash_stamp_string(uint64_t &hash, const char *text)
{
    if (!text) {
        return;
    }
    hash_stamp_bytes(hash, text, strlen(text));
}

static void hash_stamp_int(uint64_t &hash, int value)
{
    hash_stamp_bytes(hash, &value, sizeof(value));
}

static uint64_t calculate_source_fingerprint(
    const image *images,
    int image_count,
    const uint16_t *group_image_ids,
    int group_count,
    const char *source_name,
    const image_atlas_data *atlas_data)
{
    uint64_t hash = 1469598103934665603ull;
    hash_stamp_string(hash, source_name);
    hash_stamp_int(hash, image_count);
    hash_stamp_int(hash, group_count);

    if (group_image_ids && group_count > 0) {
        hash_stamp_bytes(hash, group_image_ids, static_cast<size_t>(group_count) * sizeof(uint16_t));
    }

    for (int image_id = 0; image_id < image_count; ++image_id) {
        const image *img = &images[image_id];
        if (!is_exportable_main_image(img)) {
            continue;
        }

        hash_stamp_int(hash, image_id);
        hash_stamp_int(hash, img->x_offset);
        hash_stamp_int(hash, img->y_offset);
        hash_stamp_int(hash, img->width);
        hash_stamp_int(hash, img->height);
        hash_stamp_int(hash, img->original.width);
        hash_stamp_int(hash, img->original.height);
        hash_stamp_int(hash, img->is_isometric);
        hash_stamp_int(hash, img->atlas.id);
        hash_stamp_int(hash, img->atlas.x_offset);
        hash_stamp_int(hash, img->atlas.y_offset);
        hash_stamp_int(hash, img->top ? 1 : 0);
        if (img->animation) {
            hash_stamp_int(hash, img->animation->num_sprites);
            hash_stamp_int(hash, img->animation->sprite_offset_x);
            hash_stamp_int(hash, img->animation->sprite_offset_y);
            hash_stamp_int(hash, img->animation->can_reverse);
            hash_stamp_int(hash, img->animation->speed_id);
            hash_stamp_int(hash, img->animation->start_offset);
        }
    }

    if (atlas_data) {
        hash_stamp_int(hash, atlas_data->num_images);
        for (int atlas_index = 0; atlas_index < atlas_data->num_images; ++atlas_index) {
            const int width = atlas_data->image_widths[atlas_index];
            const int height = atlas_data->image_heights[atlas_index];
            hash_stamp_int(hash, width);
            hash_stamp_int(hash, height);
            if (atlas_data->buffers[atlas_index] && width > 0 && height > 0) {
                hash_stamp_bytes(
                    hash,
                    atlas_data->buffers[atlas_index],
                    static_cast<size_t>(width) * height * sizeof(color_t));
            }
        }
    }

    return hash;
}

static std::string make_stamp_contents(
    const image *images,
    int image_count,
    const uint16_t *group_image_ids,
    int group_count,
    const char *source_name,
    const image_atlas_data *atlas_data)
{
    const uint64_t fingerprint = calculate_source_fingerprint(
        images, image_count, group_image_ids, group_count, source_name, atlas_data);
    char buffer[160];
    snprintf(
        buffer,
        sizeof(buffer),
        "%s%s:%016llx",
        kExtractionStampPrefix,
        make_source_stem(source_name).c_str(),
        static_cast<unsigned long long>(fingerprint));
    return buffer;
}

static bool has_current_stamp(
    const image *images,
    int image_count,
    const uint16_t *group_image_ids,
    int group_count,
    const char *source_name,
    const image_atlas_data *atlas_data)
{
    std::string contents;
    return read_text_file(make_stamp_path(), contents) &&
        contents == make_stamp_contents(images, image_count, group_image_ids, group_count, source_name, atlas_data);
}

static std::vector<LegacyGroupRange> build_group_ranges(
    const uint16_t *group_image_ids,
    int group_count,
    int image_count)
{
    std::vector<LegacyGroupRange> ranges;
    for (int group_id = 1; group_id < group_count; ++group_id) {
        const int first_image_id = group_image_ids[group_id];
        if (first_image_id <= 0 || first_image_id >= image_count) {
            continue;
        }

        int next_first_image_id = image_count;
        for (int next_group_id = 1; next_group_id < group_count; ++next_group_id) {
            const int candidate = group_image_ids[next_group_id];
            if (candidate > first_image_id && candidate < next_first_image_id) {
                next_first_image_id = candidate;
            }
        }

        if (next_first_image_id <= first_image_id) {
            continue;
        }

        ranges.push_back({ group_id, first_image_id, next_first_image_id - 1 });
    }
    return ranges;
}

static std::vector<std::string> read_manifest_entries(void)
{
    std::vector<std::string> entries;
    std::string contents;
    if (!read_text_file(make_manifest_path(), contents) || contents.empty()) {
        return entries;
    }

    size_t start = 0;
    while (start <= contents.size()) {
        const size_t end = contents.find('\n', start);
        std::string line = contents.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            entries.push_back(line);
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return entries;
}

static bool write_manifest_entries(const std::vector<std::string> &entries)
{
    std::string manifest;
    for (const std::string &entry : entries) {
        manifest += entry;
        manifest += "\n";
    }
    return write_text_file(make_manifest_path(), manifest);
}

static void clear_existing_output(void)
{
    std::vector<std::string> entries = read_manifest_entries();
    for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
        if (it->size() < 3 || (*it)[1] != '|') {
            continue;
        }
        const char type = (*it)[0];
        const char *path = it->c_str() + 2;
        if (type == 'D') {
            platform_file_manager_remove_directory(path);
        } else if (type == 'F') {
            platform_file_manager_remove_file(path);
        }
    }
    platform_file_manager_remove_file(make_manifest_path().c_str());
    platform_file_manager_remove_file(make_stamp_path().c_str());
}

static const LegacyFamily &family_for_building_group(int group_id)
{
    switch (group_id) {
        case GROUP_BUILDING_TOWER:
        case GROUP_BUILDING_WALL:
        case GROUP_BUILDING_FORT:
        case GROUP_BUILDING_BARRACKS:
        case GROUP_BUILDING_MILITARY_ACADEMY:
        case GROUP_BUILDING_FORT_JAVELIN:
        case GROUP_BUILDING_FORT_LEGIONARY:
        case GROUP_BUILDING_GATEHOUSE:
            return kMilitary;

        case GROUP_BUILDING_MARBLE_QUARRY:
        case GROUP_BUILDING_IRON_MINE:
        case GROUP_BUILDING_CLAY_PIT:
        case GROUP_BUILDING_FARM_HOUSE:
        case GROUP_BUILDING_WINE_WORKSHOP:
        case GROUP_BUILDING_CHARIOT_MAKER:
        case GROUP_BUILDING_WORKSHOP_RAW_MATERIAL:
        case GROUP_BUILDING_TIMBER_YARD:
        case GROUP_BUILDING_SHIPYARD:
        case GROUP_BUILDING_DOCK_1:
        case GROUP_BUILDING_WHARF:
        case GROUP_BUILDING_WAREHOUSE:
        case GROUP_BUILDING_WAREHOUSE_STORAGE_EMPTY:
        case GROUP_BUILDING_WAREHOUSE_STORAGE_FILLED:
        case GROUP_BUILDING_GRANARY:
        case GROUP_BUILDING_FARM_CROPS:
        case GROUP_BUILDING_OIL_WORKSHOP:
        case GROUP_BUILDING_WEAPONS_WORKSHOP:
        case GROUP_BUILDING_FURNITURE_WORKSHOP:
        case GROUP_BUILDING_POTTERY_WORKSHOP:
        case GROUP_BUILDING_DOCK_2:
        case GROUP_BUILDING_DOCK_3:
        case GROUP_BUILDING_DOCK_4:
        case GROUP_BUILDING_DOCK_DOCKERS:
        case GROUP_BUILDING_TRADE_CENTER_FLAG:
            return kIndustry;

        case GROUP_BUILDING_SCHOOL:
        case GROUP_BUILDING_LIBRARY:
        case GROUP_BUILDING_ACADEMY:
        case GROUP_BUILDING_AMPHITHEATER:
        case GROUP_BUILDING_THEATER:
        case GROUP_BUILDING_COLOSSEUM:
        case GROUP_BUILDING_GLADIATOR_SCHOOL:
        case GROUP_BUILDING_LION_HOUSE:
        case GROUP_BUILDING_ACTOR_COLONY:
        case GROUP_BUILDING_BARBER:
        case GROUP_BUILDING_DOCTOR:
        case GROUP_BUILDING_BATHHOUSE_WATER:
        case GROUP_BUILDING_HOSPITAL:
        case GROUP_BUILDING_TEMPLE_CERES:
        case GROUP_BUILDING_TEMPLE_NEPTUNE:
        case GROUP_BUILDING_TEMPLE_MERCURY:
        case GROUP_BUILDING_TEMPLE_MARS:
        case GROUP_BUILDING_TEMPLE_VENUS:
        case GROUP_BUILDING_ORACLE:
        case GROUP_BUILDING_BATHHOUSE_NO_WATER:
        case GROUP_BUILDING_THEATER_SHOW:
        case GROUP_BUILDING_AMPHITHEATER_SHOW:
        case GROUP_BUILDING_COLOSSEUM_SHOW:
        case GROUP_BUILDING_BATHHOUSE_FANCY_WATER:
        case GROUP_BUILDING_BATHHOUSE_FANCY_NO_WATER:
        case GROUP_BUILDING_HIPPODROME_1:
        case GROUP_BUILDING_HIPPODROME_2:
            return kHealthCulture;

        case GROUP_BUILDING_AQUEDUCT:
        case GROUP_BUILDING_MARKET:
        case GROUP_BUILDING_WELL:
        case GROUP_BUILDING_RESERVOIR:
        case GROUP_BUILDING_FOUNTAIN_1:
        case GROUP_BUILDING_FOUNTAIN_2:
        case GROUP_BUILDING_FOUNTAIN_3:
        case GROUP_BUILDING_FOUNTAIN_4:
        case GROUP_BUILDING_SENATE:
        case GROUP_BUILDING_FORUM:
        case GROUP_BUILDING_PREFECTURE:
        case GROUP_BUILDING_ENGINEERS_POST:
        case GROUP_BUILDING_GOVERNORS_HOUSE:
        case GROUP_BUILDING_GOVERNORS_VILLA:
        case GROUP_BUILDING_GOVERNORS_PALACE:
        case GROUP_BUILDING_BRIDGE:
        case GROUP_BUILDING_MISSION_POST:
        case GROUP_BUILDING_AQUEDUCT_NO_WATER:
        case GROUP_BUILDING_MARKET_FANCY:
        case GROUP_BUILDING_SENATE_FANCY:
            return kAdminLogistics;

        case GROUP_BUILDING_HOUSE_TENT:
        case GROUP_BUILDING_HOUSE_SHACK:
        case GROUP_BUILDING_HOUSE_HOVEL:
        case GROUP_BUILDING_HOUSE_CASA:
        case GROUP_BUILDING_HOUSE_INSULA_1:
        case GROUP_BUILDING_HOUSE_INSULA_2:
        case GROUP_BUILDING_HOUSE_VILLA_1:
        case GROUP_BUILDING_HOUSE_VILLA_2:
        case GROUP_BUILDING_HOUSE_PALACE_1:
        case GROUP_BUILDING_HOUSE_PALACE_2:
        case GROUP_BUILDING_HOUSE_VACANT_LOT:
        case GROUP_BUILDING_STATUE:
        case GROUP_BUILDING_NATIVE:
            return kAesthetics;

        case GROUP_BUILDING_TRIUMPHAL_ARCH:
            return kMonuments;

        default:
            return kBuildings;
    }
}

static const LegacyFamily &family_for_figure_group(int group_id)
{
    switch (group_id) {
        case GROUP_FIGURE_EXPLOSION:
            return kFx;

        case GROUP_FIGURE_SHIP:
        case GROUP_FIGURE_FLOTSAM_0:
        case GROUP_FIGURE_FLOTSAM_1:
        case GROUP_FIGURE_FLOTSAM_2:
        case GROUP_FIGURE_FLOTSAM_3:
        case GROUP_FIGURE_SHIPWRECK:
        case GROUP_FIGURE_FLOTSAM_SHEEP:
            return kShips;

        case GROUP_FIGURE_FORT_FLAGS:
        case GROUP_FIGURE_FORT_STANDARD_ICONS:
        case GROUP_FIGURE_FORT_STANDARD_POLE:
        case GROUP_FIGURE_TOWER_SENTRY:
        case GROUP_FIGURE_MISSILE:
        case GROUP_FIGURE_BALLISTA:
        case GROUP_FIGURE_FORT_MOUNTED:
        case GROUP_FIGURE_GLADIATOR:
        case GROUP_FIGURE_CAESAR_LEGIONARY:
            return kWarriors;

        case GROUP_FIGURE_MAP_FLAG_FLAGS:
        case GROUP_FIGURE_MAP_FLAG_ICONS:
            return kPaperMap;

        case GROUP_FIGURE_SEAGULLS:
        case GROUP_FIGURE_SHEEP:
        case GROUP_FIGURE_WOLF:
        case GROUP_FIGURE_ZEBRA:
        case GROUP_FIGURE_LION:
            return kEnvironment;

        default:
            return kWalkers;
    }
}

static const LegacyFamily &family_for_other_group(int group_id)
{
    switch (group_id) {
        case GROUP_FONT:
            return kFonts;

        case GROUP_MAIN_MENU_BACKGROUND:
        case GROUP_WIN_GAME_BACKGROUND:
        case GROUP_LOADING_SCREEN:
        case GROUP_INTERMEZZO_BACKGROUND:
            return kLoadingScreens;

        case GROUP_EMPIRE_MAP:
        case GROUP_EMPIRE_PANELS:
        case GROUP_EMPIRE_RESOURCES:
        case GROUP_EMPIRE_CITY:
        case GROUP_EMPIRE_CITY_TRADE:
        case GROUP_EMPIRE_CITY_DISTANT_ROMAN:
        case GROUP_EMPIRE_BATTLE:
        case GROUP_EMPIRE_ROMAN_ARMY:
        case GROUP_EMPIRE_TRADE_ROUTE_TYPE:
        case GROUP_EMPIRE_BORDER_EDGE:
        case GROUP_EMPIRE_TRADE_LAND:
        case GROUP_EMPIRE_TRADE_SEA:
        case GROUP_EMPIRE_FOREIGN_CITY:
        case GROUP_EMPIRE_ENEMY_ARMY:
        case GROUP_SELECT_MISSION_BACKGROUND:
        case GROUP_SELECT_MISSION:
        case GROUP_CCK_BACKGROUND:
        case GROUP_SCENARIO_IMAGE:
            return kPaperMap;

        case GROUP_MINIMAP_EMPTY_LAND:
        case GROUP_MINIMAP_WATER:
        case GROUP_MINIMAP_TREE:
        case GROUP_MINIMAP_ROCK:
        case GROUP_MINIMAP_MEADOW:
        case GROUP_MINIMAP_ROAD:
        case GROUP_MINIMAP_HOUSE:
        case GROUP_MINIMAP_BUILDING:
        case GROUP_MINIMAP_WALL:
        case GROUP_MINIMAP_AQUEDUCT:
        case GROUP_MINIMAP_BLACK:
            return kMap;

        case GROUP_BIG_PEOPLE:
            return kPortraits;

        case GROUP_GOD_BOLT:
        case GROUP_PLAGUE_SKULL:
            return kFx;

        case GROUP_FORT_FORMATIONS:
        case GROUP_FORT_ICONS:
            return kWarriors;

        default:
            return kUi;
    }
}

static const LegacyFamily &family_for_group(int group_id)
{
    switch (group_id) {
        case GROUP_TERRAIN_BLACK:
        case GROUP_TERRAIN_SHRUB:
        case GROUP_TERRAIN_UGLY_GRASS:
        case GROUP_TERRAIN_TREE:
        case GROUP_TERRAIN_WATER:
        case GROUP_TERRAIN_EARTHQUAKE:
        case GROUP_TERRAIN_GRASS_2:
        case GROUP_TERRAIN_ELEVATION_ROCK:
        case GROUP_TERRAIN_ELEVATION:
        case GROUP_TERRAIN_GRASS_1:
        case GROUP_TERRAIN_OVERLAY:
        case GROUP_TERRAIN_FLAT_TILE:
        case GROUP_TERRAIN_PLAZA:
        case GROUP_TERRAIN_GARDEN:
        case GROUP_TERRAIN_ROAD:
        case GROUP_TERRAIN_RUBBLE:
        case GROUP_TERRAIN_RUBBLE_TENT:
        case GROUP_TERRAIN_RUBBLE_GENERAL:
        case GROUP_TERRAIN_DESIRABILITY:
        case GROUP_TERRAIN_MEADOW:
        case GROUP_TERRAIN_WATER_SHORE:
        case GROUP_TERRAIN_ACCESS_RAMP:
        case GROUP_TERRAIN_ROCK:
        case GROUP_TERRAIN_ENTRY_EXIT_FLAGS:
            return kTerrainMaps;

        case GROUP_BUILDING_TOWER:
        case GROUP_BUILDING_AQUEDUCT:
        case GROUP_BUILDING_MARKET:
        case GROUP_BUILDING_WELL:
        case GROUP_BUILDING_WALL:
        case GROUP_BUILDING_RESERVOIR:
        case GROUP_BUILDING_HOUSE_TENT:
        case GROUP_BUILDING_HOUSE_SHACK:
        case GROUP_BUILDING_HOUSE_HOVEL:
        case GROUP_BUILDING_HOUSE_CASA:
        case GROUP_BUILDING_HOUSE_INSULA_1:
        case GROUP_BUILDING_HOUSE_INSULA_2:
        case GROUP_BUILDING_HOUSE_VILLA_1:
        case GROUP_BUILDING_HOUSE_VILLA_2:
        case GROUP_BUILDING_HOUSE_PALACE_1:
        case GROUP_BUILDING_HOUSE_PALACE_2:
        case GROUP_BUILDING_HOUSE_VACANT_LOT:
        case GROUP_BUILDING_FARM_HOUSE:
        case GROUP_BUILDING_MARBLE_QUARRY:
        case GROUP_BUILDING_IRON_MINE:
        case GROUP_BUILDING_CLAY_PIT:
        case GROUP_BUILDING_SCHOOL:
        case GROUP_BUILDING_LIBRARY:
        case GROUP_BUILDING_ACADEMY:
        case GROUP_BUILDING_WINE_WORKSHOP:
        case GROUP_BUILDING_AMPHITHEATER:
        case GROUP_BUILDING_THEATER:
        case GROUP_BUILDING_COLOSSEUM:
        case GROUP_BUILDING_GLADIATOR_SCHOOL:
        case GROUP_BUILDING_LION_HOUSE:
        case GROUP_BUILDING_ACTOR_COLONY:
        case GROUP_BUILDING_CHARIOT_MAKER:
        case GROUP_BUILDING_FOUNTAIN_4:
        case GROUP_BUILDING_FOUNTAIN_1:
        case GROUP_BUILDING_FOUNTAIN_2:
        case GROUP_BUILDING_FOUNTAIN_3:
        case GROUP_BUILDING_WORKSHOP_RAW_MATERIAL:
        case GROUP_BUILDING_STATUE:
        case GROUP_BUILDING_SENATE:
        case GROUP_BUILDING_FORUM:
        case GROUP_BUILDING_PREFECTURE:
        case GROUP_BUILDING_TIMBER_YARD:
        case GROUP_BUILDING_FORT:
        case GROUP_BUILDING_BARBER:
        case GROUP_BUILDING_DOCTOR:
        case GROUP_BUILDING_BATHHOUSE_WATER:
        case GROUP_BUILDING_HOSPITAL:
        case GROUP_BUILDING_TEMPLE_CERES:
        case GROUP_BUILDING_TEMPLE_NEPTUNE:
        case GROUP_BUILDING_TEMPLE_MERCURY:
        case GROUP_BUILDING_TEMPLE_MARS:
        case GROUP_BUILDING_TEMPLE_VENUS:
        case GROUP_BUILDING_ORACLE:
        case GROUP_BUILDING_SHIPYARD:
        case GROUP_BUILDING_DOCK_1:
        case GROUP_BUILDING_WHARF:
        case GROUP_BUILDING_ENGINEERS_POST:
        case GROUP_BUILDING_WAREHOUSE:
        case GROUP_BUILDING_WAREHOUSE_STORAGE_EMPTY:
        case GROUP_BUILDING_WAREHOUSE_STORAGE_FILLED:
        case GROUP_BUILDING_GOVERNORS_HOUSE:
        case GROUP_BUILDING_GOVERNORS_VILLA:
        case GROUP_BUILDING_GOVERNORS_PALACE:
        case GROUP_BUILDING_GRANARY:
        case GROUP_BUILDING_FARM_CROPS:
        case GROUP_BUILDING_OIL_WORKSHOP:
        case GROUP_BUILDING_WEAPONS_WORKSHOP:
        case GROUP_BUILDING_FURNITURE_WORKSHOP:
        case GROUP_BUILDING_POTTERY_WORKSHOP:
        case GROUP_BUILDING_BRIDGE:
        case GROUP_BUILDING_BARRACKS:
        case GROUP_BUILDING_DOCK_2:
        case GROUP_BUILDING_DOCK_3:
        case GROUP_BUILDING_DOCK_4:
        case GROUP_BUILDING_DOCK_DOCKERS:
        case GROUP_BUILDING_NATIVE:
        case GROUP_BUILDING_MISSION_POST:
        case GROUP_BUILDING_BATHHOUSE_NO_WATER:
        case GROUP_BUILDING_THEATER_SHOW:
        case GROUP_BUILDING_AMPHITHEATER_SHOW:
        case GROUP_BUILDING_COLOSSEUM_SHOW:
        case GROUP_BUILDING_AQUEDUCT_NO_WATER:
        case GROUP_BUILDING_MILITARY_ACADEMY:
        case GROUP_BUILDING_FORT_JAVELIN:
        case GROUP_BUILDING_FORT_LEGIONARY:
        case GROUP_BUILDING_TRIUMPHAL_ARCH:
        case GROUP_BUILDING_MARKET_FANCY:
        case GROUP_BUILDING_BATHHOUSE_FANCY_WATER:
        case GROUP_BUILDING_BATHHOUSE_FANCY_NO_WATER:
        case GROUP_BUILDING_HIPPODROME_1:
        case GROUP_BUILDING_HIPPODROME_2:
        case GROUP_BUILDING_SENATE_FANCY:
        case GROUP_BUILDING_TRADE_CENTER_FLAG:
        case GROUP_BUILDING_GATEHOUSE:
            return family_for_building_group(group_id);

        case GROUP_FIGURE_LABOR_SEEKER:
        case GROUP_FIGURE_BATHHOUSE_WORKER:
        case GROUP_FIGURE_PRIEST:
        case GROUP_FIGURE_CARTPUSHER_CART:
        case GROUP_FIGURE_ACTOR:
        case GROUP_FIGURE_LION_TAMER:
        case GROUP_FIGURE_EXPLOSION:
        case GROUP_FIGURE_TAX_COLLECTOR:
        case GROUP_FIGURE_SCHOOL_CHILD:
        case GROUP_FIGURE_MARKET_LADY:
        case GROUP_FIGURE_CARTPUSHER:
        case GROUP_FIGURE_MIGRANT:
        case GROUP_FIGURE_LION_TAMER_WHIP:
        case GROUP_FIGURE_ENGINEER:
        case GROUP_FIGURE_GLADIATOR:
        case GROUP_FIGURE_CRIMINAL:
        case GROUP_FIGURE_BARBER:
        case GROUP_FIGURE_PREFECT:
        case GROUP_FIGURE_HOMELESS:
        case GROUP_FIGURE_PREFECT_WITH_BUCKET:
        case GROUP_FIGURE_FORT_FLAGS:
        case GROUP_FIGURE_FORT_STANDARD_ICONS:
        case GROUP_FIGURE_TRADE_CARAVAN:
        case GROUP_FIGURE_MIGRANT_CART:
        case GROUP_FIGURE_MAP_FLAG_FLAGS:
        case GROUP_FIGURE_MAP_FLAG_ICONS:
        case GROUP_FIGURE_FLOTSAM_0:
        case GROUP_FIGURE_FLOTSAM_1:
        case GROUP_FIGURE_FLOTSAM_2:
        case GROUP_FIGURE_FLOTSAM_3:
        case GROUP_FIGURE_LION:
        case GROUP_FIGURE_SHIP:
        case GROUP_FIGURE_TOWER_SENTRY:
        case GROUP_FIGURE_MISSILE:
        case GROUP_FIGURE_BALLISTA:
        case GROUP_FIGURE_SEAGULLS:
        case GROUP_FIGURE_DELIVERY_BOY:
        case GROUP_FIGURE_CHARIOTEER:
        case GROUP_FIGURE_HIPPODROME_HORSE_1:
        case GROUP_FIGURE_HIPPODROME_HORSE_2:
        case GROUP_FIGURE_HIPPODROME_CART_1:
        case GROUP_FIGURE_HIPPODROME_CART_2:
        case GROUP_FIGURE_SHIPWRECK:
        case GROUP_FIGURE_DOCTOR_SURGEON:
        case GROUP_FIGURE_PATRICIAN:
        case GROUP_FIGURE_MISSIONARY:
        case GROUP_FIGURE_TEACHER_LIBRARIAN:
        case GROUP_FIGURE_FORT_MOUNTED:
        case GROUP_FIGURE_SHEEP:
        case GROUP_FIGURE_WOLF:
        case GROUP_FIGURE_ZEBRA:
        case GROUP_FIGURE_CAESAR_LEGIONARY:
        case GROUP_FIGURE_CARTPUSHER_CART_MULTIPLE_FOOD:
        case GROUP_FIGURE_FORT_STANDARD_POLE:
        case GROUP_FIGURE_FLOTSAM_SHEEP:
        case GROUP_FIGURE_CARTPUSHER_CART_MULTIPLE_RESOURCE:
            return family_for_figure_group(group_id);

        default:
            break;
    }

    return family_for_other_group(group_id);
}

static std::vector<color_t> extract_full_canvas(const image *img, const image_atlas_data *atlas_data)
{
    if (!is_exportable_main_image(img) || !atlas_data) {
        return {};
    }

    const int output_width = img->original.width > 0 ? img->original.width : img->width;
    const int output_height = img->original.height > 0 ? img->original.height : img->height;
    if (output_width <= 0 || output_height <= 0) {
        return {};
    }

    const int atlas_index = img->atlas.id & IMAGE_ATLAS_BIT_MASK;
    if (atlas_index < 0 || atlas_index >= atlas_data->num_images) {
        return {};
    }

    const color_t *source = atlas_data->buffers[atlas_index];
    const int source_width = atlas_data->image_widths[atlas_index];
    if (!source || source_width <= 0) {
        return {};
    }

    std::vector<color_t> pixels(static_cast<size_t>(output_width) * output_height, ALPHA_TRANSPARENT);
    for (int y = 0; y < img->height; ++y) {
        color_t *destination = &pixels[static_cast<size_t>(img->y_offset + y) * output_width + img->x_offset];
        const color_t *source_row = &source[static_cast<size_t>(img->atlas.y_offset + y) * source_width + img->atlas.x_offset];
        memcpy(destination, source_row, static_cast<size_t>(img->width) * sizeof(color_t));
    }
    return pixels;
}

struct FootprintExportData {
    std::vector<color_t> pixels;
    int height = 0;
    int trimmed_bottom_rows = 0;
};

static int count_trailing_transparent_bottom_rows(const color_t *pixels, int width, int height)
{
    if (!pixels || width <= 0 || height <= 0) {
        return 0;
    }

    for (int y = height - 1; y >= 0; --y) {
        const color_t *row = &pixels[static_cast<size_t>(y) * width];
        for (int x = 0; x < width; ++x) {
            if ((row[x] & COLOR_CHANNEL_ALPHA) != ALPHA_TRANSPARENT) {
                return height - y - 1;
            }
        }
    }

    return 0;
}

static FootprintExportData normalize_isometric_footprint_export(
    const std::vector<color_t> &pixels,
    int width,
    int height)
{
    FootprintExportData normalized;
    normalized.pixels = pixels;
    normalized.height = height;
    normalized.trimmed_bottom_rows = count_trailing_transparent_bottom_rows(
        pixels.data(),
        width,
        height);

    if (normalized.trimmed_bottom_rows <= 0 || normalized.trimmed_bottom_rows >= height) {
        normalized.trimmed_bottom_rows = 0;
        return normalized;
    }

    normalized.height = height - normalized.trimmed_bottom_rows;
    normalized.pixels.resize(static_cast<size_t>(width) * normalized.height);
    return normalized;
}

static bool write_png(const std::string &path, const color_t *pixels, int width, int height)
{
    if (!pixels || width <= 0 || height <= 0) {
        return false;
    }

    FILE *file = file_open(path.c_str(), "wb");
    if (!file) {
        return false;
    }

    spng_ctx *ctx = spng_ctx_new(SPNG_CTX_ENCODER);
    if (!ctx) {
        file_close(file);
        return false;
    }

    int result = spng_set_option(ctx, SPNG_IMG_COMPRESSION_LEVEL, 1);
    if (result == 0) {
        result = spng_set_png_file(ctx, file);
    }

    spng_ihdr ihdr = {};
    ihdr.width = static_cast<uint32_t>(width);
    ihdr.height = static_cast<uint32_t>(height);
    ihdr.bit_depth = 8;
    ihdr.color_type = SPNG_COLOR_TYPE_TRUECOLOR_ALPHA;

    if (result == 0) {
        result = spng_set_ihdr(ctx, &ihdr);
    }
    if (result == 0) {
        result = spng_encode_image(ctx, 0, 0, SPNG_FMT_PNG, SPNG_ENCODE_PROGRESSIVE | SPNG_ENCODE_FINALIZE);
    }

    std::vector<uint8_t> scanline(static_cast<size_t>(width) * 4);
    for (int y = 0; result == 0 && y < height; ++y) {
        uint8_t *destination = scanline.data();
        const color_t *source_row = &pixels[static_cast<size_t>(y) * width];
        for (int x = 0; x < width; ++x) {
            const color_t color = source_row[x];
            destination[0] = static_cast<uint8_t>(COLOR_COMPONENT(color, COLOR_BITSHIFT_RED));
            destination[1] = static_cast<uint8_t>(COLOR_COMPONENT(color, COLOR_BITSHIFT_GREEN));
            destination[2] = static_cast<uint8_t>(COLOR_COMPONENT(color, COLOR_BITSHIFT_BLUE));
            destination[3] = static_cast<uint8_t>(COLOR_COMPONENT(color, COLOR_BITSHIFT_ALPHA));
            destination += 4;
        }
        const int scanline_result = spng_encode_scanline(ctx, scanline.data(), scanline.size());
        if (scanline_result != SPNG_OK && scanline_result != SPNG_EOI) {
            result = scanline_result;
        }
    }

    spng_ctx_free(ctx);
    file_close(file);
    return result == 0 || result == SPNG_EOI;
}

static void append_indent(std::string &xml, int depth)
{
    xml.append(static_cast<size_t>(depth) * 4, ' ');
}

static void append_attribute(std::string &xml, const char *name, const std::string &value)
{
    xml += " ";
    xml += name;
    xml += "=\"";
    xml += value;
    xml += "\"";
}

static void append_attribute(std::string &xml, const char *name, int value)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", value);
    append_attribute(xml, name, std::string(buffer));
}

static void append_image_xml(
    std::string &xml,
    int &exported_images,
    const std::string &id,
    const std::string &src,
    const std::string &top_src,
    const image *img,
    int image_height,
    int footprint_layer_y)
{
    const bool has_animation = img->animation && img->animation->num_sprites > 0;
    const bool is_layered = img->is_isometric || img->top != nullptr;

    append_indent(xml, 1);
    xml += "<image";
    append_attribute(xml, "id", id);
    if (!is_layered) {
        append_attribute(xml, "src", src);
    }
    if (img->is_isometric) {
        append_attribute(xml, "width", img->original.width > 0 ? img->original.width : img->width);
        append_attribute(xml, "height", image_height);
        append_attribute(xml, "isometric", "true");
    }

    if (!has_animation && !is_layered) {
        xml += "/>\n";
        exported_images++;
        return;
    }

    xml += ">\n";
    if (is_layered) {
        append_indent(xml, 2);
        xml += "<layer";
        append_attribute(xml, "src", src);
        if (img->is_isometric) {
            if (footprint_layer_y > 0) {
                append_attribute(xml, "y", footprint_layer_y);
            }
            append_attribute(xml, "part", "footprint");
        }
        xml += "/>\n";

        if (img->top) {
            append_indent(xml, 2);
            xml += "<layer";
            append_attribute(xml, "src", top_src);
            append_attribute(xml, "part", "top");
            xml += "/>\n";
        }
    }

    if (has_animation) {
        append_indent(xml, 2);
        xml += "<animation";
        append_attribute(xml, "frames", img->animation->num_sprites);
        if (img->animation->speed_id > 0) {
            append_attribute(xml, "speed", img->animation->speed_id);
        }
        if (img->animation->can_reverse) {
            append_attribute(xml, "reversible", "true");
        }
        if (img->animation->sprite_offset_x != 0) {
            append_attribute(xml, "x", img->animation->sprite_offset_x);
        }
        if (img->animation->sprite_offset_y != 0) {
            append_attribute(xml, "y", img->animation->sprite_offset_y);
        }
        xml += "/>\n";
    }

    append_indent(xml, 1);
    xml += "</image>\n";
    exported_images++;
}

static bool export_group(
    const image *images,
    const LegacyGroupRange &range,
    const image_atlas_data *atlas_data,
    std::vector<std::string> &manifest_entries,
    ExtractionStats &stats)
{
    const LegacyFamily &family = family_for_group(range.group_id);
    const std::string group_directory = make_group_directory(family, range.group_id);
    ensure_directory(group_directory);

    const std::string assetlist_name = make_group_assetlist_name(family, range.group_id);
    const std::string xml_path = make_group_xml_path(family, range.group_id);

    std::string xml = "<?xml version=\"1.0\"?>\n<!DOCTYPE assetlist>\n<assetlist";
    append_attribute(xml, "name", assetlist_name);
    xml += ">\n";
    int exported_images = 0;

    for (int image_id = range.first_image_id; image_id <= range.last_image_id; ++image_id) {
        if (is_placeholder_main_image(image_id)) {
            continue;
        }

        const image *img = &images[image_id];
        if (!is_exportable_main_image(img)) {
            continue;
        }

        const int local_index = image_id - range.first_image_id;
        char image_name_buffer[32];
        snprintf(image_name_buffer, sizeof(image_name_buffer), "Image_%04d", local_index);

        const std::string image_name = image_name_buffer;
        const std::vector<color_t> pixels = extract_full_canvas(img, atlas_data);
        if (pixels.empty()) {
            continue;
        }

        const int width = img->original.width > 0 ? img->original.width : img->width;
        const int height = img->original.height > 0 ? img->original.height : img->height;
        FootprintExportData footprint_export;
        if (img->is_isometric) {
            footprint_export = normalize_isometric_footprint_export(pixels, width, height);
        } else {
            footprint_export.pixels = pixels;
            footprint_export.height = height;
        }
        const int exported_image_height =
            (img->is_isometric && !img->top && footprint_export.trimmed_bottom_rows > 0)
            ? footprint_export.height
            : height;
        const int footprint_layer_y =
            (img->is_isometric && img->top && footprint_export.trimmed_bottom_rows > 0)
            ? footprint_export.trimmed_bottom_rows
            : 0;
        const std::string image_path = group_directory + "/" + image_name + ".png";
        if (!write_png(image_path, footprint_export.pixels.data(), width, footprint_export.height)) {
            log_error("Failed to write Julius PNG", image_path.c_str(), 0);
            return false;
        }
        manifest_entries.push_back("F|" + image_path);
        stats.pngs_written++;

        if (img->top) {
            const std::vector<color_t> top_pixels = extract_full_canvas(img->top, atlas_data);
            const int top_width = img->top->original.width > 0 ? img->top->original.width : img->top->width;
            const int top_height = img->top->original.height > 0 ? img->top->original.height : img->top->height;
            const std::string top_path = group_directory + "/" + image_name + "_Top.png";
            if (top_pixels.empty() || !write_png(top_path, top_pixels.data(), top_width, top_height)) {
                log_error("Failed to write Julius top PNG", top_path.c_str(), 0);
                return false;
            }
            manifest_entries.push_back("F|" + top_path);
            stats.pngs_written++;
        }

        append_image_xml(
            xml,
            exported_images,
            image_name,
            image_name,
            image_name + "_Top",
            img,
            exported_image_height,
            footprint_layer_y);
    }

    xml += "</assetlist>\n";
    if (exported_images <= 0) {
        platform_file_manager_remove_file(xml_path.c_str());
        platform_file_manager_remove_directory(group_directory.c_str());
        return true;
    }
    if (!write_text_file(xml_path, xml)) {
        log_error("Failed to write Julius assetlist", xml_path.c_str(), 0);
        return false;
    }
    manifest_entries.push_back("D|" + group_directory);
    manifest_entries.push_back("F|" + xml_path);
    stats.groups_exported++;
    stats.images_exported += exported_images;
    return true;
}

} // namespace

extern "C" int legacy_image_extractor_extract_climate(
    const image *images,
    int image_count,
    const uint16_t *group_image_ids,
    int group_count,
    const char *source_name,
    const image_atlas_data *atlas_data)
{
    if (!images || image_count <= 0 || !group_image_ids || group_count <= 0 || !source_name || !*source_name || !atlas_data) {
        return 0;
    }

    std::string existing_stamp;
    const int has_existing_stamp = read_text_file(make_stamp_path(), existing_stamp);
    const std::string expected_stamp = make_stamp_contents(
        images, image_count, group_image_ids, group_count, source_name, atlas_data);

    if (has_existing_stamp && existing_stamp == expected_stamp) {
        return 1;
    }

    if (!has_existing_stamp) {
        log_info("Extracting Julius graphics because no extraction stamp was found", source_name, 0);
    } else {
        log_info("Extracting Julius graphics because the legacy source fingerprint or XML metadata version changed", source_name, 0);
    }

    ensure_directory(mod_manager_get_julius_graphics_path());
    for (const LegacyFamily *family : kAllFamilies) {
        ensure_directory(make_family_root_directory(*family));
    }

    log_info("Starting Julius legacy graphics extraction", source_name, 0);
    clear_existing_output();

    const std::vector<LegacyGroupRange> ranges = build_group_ranges(group_image_ids, group_count, image_count);
    std::vector<std::string> manifest_entries;
    ExtractionStats stats;
    for (const LegacyGroupRange &range : ranges) {
        if (!export_group(images, range, atlas_data, manifest_entries, stats)) {
            log_error("Julius graphics extraction failed", source_name, 0);
            return 0;
        }
    }

    if (!write_manifest_entries(manifest_entries)) {
        log_error("Failed to write Julius extraction manifest", make_manifest_path().c_str(), 0);
        return 0;
    }
    if (!write_text_file(make_stamp_path(), expected_stamp)) {
        log_error("Failed to write Julius extraction stamp", make_stamp_path().c_str(), 0);
        return 0;
    }

    char summary[256];
    snprintf(
        summary,
        sizeof(summary),
        "%d groups, %d images, %d pngs",
        stats.groups_exported,
        stats.images_exported,
        stats.pngs_written);
    log_info("Julius legacy graphics extraction completed", summary, 0);
    return 1;
}

extern "C" int legacy_image_extractor_get_group_key(int group_id, char *buffer, size_t buffer_size)
{
    if (!buffer || buffer_size == 0) {
        return 0;
    }

    const LegacyFamily &family = family_for_group(group_id);
    const std::string key = make_group_assetlist_name(family, group_id);
    if (key.empty() || key.size() + 1 > buffer_size) {
        return 0;
    }

    snprintf(buffer, buffer_size, "%s", key.c_str());
    return 1;
}

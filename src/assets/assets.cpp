#include "assets.h"

extern "C" {
#include "assets/group.h"
#include "assets/image.h"
#include "assets/xml.h"
#include "core/dir.h"
#include "core/file.h"
#include "core/log.h"
#include "core/png_read.h"
#include "game/mod_manager.h"
#include "graphics/renderer.h"
}

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static struct {
    int roadblock_image_id;
    asset_image *roadblock_image;
    int asset_lookup[ASSET_MAX_KEY];
    int font_lookup[ASSET_FONT_MAX_KEY];
    char failure_reason[512];
} data;

static void set_failure_reason(const char *reason, const char *detail)
{
    if (detail && *detail) {
        snprintf(data.failure_reason, sizeof(data.failure_reason), "%s\n\n%s", reason, detail);
    } else {
        snprintf(data.failure_reason, sizeof(data.failure_reason), "%s", reason);
    }
}

static int load_assetlists_from_source(const char *path, xml_asset_source source)
{
    const dir_listing *xml_files = dir_find_files_with_extension(path, "xml");
    for (int i = 0; i < xml_files->num_files; ++i) {
        if (!xml_process_assetlist_file_from_source(xml_files->files[i].name, source)) {
            char detail[FILE_NAME_MAX + 64];
            snprintf(detail, sizeof(detail), "Asset list: %s", xml_files->files[i].name);
            set_failure_reason("Failed to parse or load an asset list.", detail);
            return 0;
        }
    }
    return 1;
}

int assets_init(int force_reload, color_t **main_images, int *main_image_widths)
{
    data.failure_reason[0] = '\0';

    if (graphics_renderer()->has_image_atlas(ATLAS_EXTRA_ASSET) && !force_reload) {
        asset_image_reload_climate();
        return 1;
    }

    graphics_renderer()->free_image_atlas(ATLAS_EXTRA_ASSET);

    const dir_listing *root_xml_files = dir_find_files_with_extension(ASSETS_DIRECTORY "/" ASSETS_IMAGE_PATH, "xml");
    int total_xml_files = root_xml_files->num_files;
    const dir_listing *mod_xml_files = dir_find_files_with_extension(mod_manager_get_graphics_path(), "xml");
    total_xml_files += mod_xml_files->num_files;
    const dir_listing *augustus_xml_files = dir_find_files_with_extension(mod_manager_get_augustus_graphics_path(), "xml");
    total_xml_files += augustus_xml_files->num_files;
    const dir_listing *julius_xml_files = dir_find_files_with_extension(mod_manager_get_julius_graphics_path(), "xml");
    total_xml_files += julius_xml_files->num_files;

    if (!group_create_all(total_xml_files) || !asset_image_init_array()) {
        log_error("Not enough memory to initialize extra assets. The game will probably crash.", 0, 0);
        set_failure_reason("Not enough memory to initialize extra assets.", 0);
        return 0;
    }

    xml_init();

    if (!load_assetlists_from_source(mod_manager_get_graphics_path(), XML_ASSET_SOURCE_MOD)) {
        return 0;
    }
    if (!load_assetlists_from_source(mod_manager_get_augustus_graphics_path(), XML_ASSET_SOURCE_AUGUSTUS)) {
        return 0;
    }
    if (!load_assetlists_from_source(mod_manager_get_julius_graphics_path(), XML_ASSET_SOURCE_JULIUS)) {
        return 0;
    }
    if (!load_assetlists_from_source(ASSETS_DIRECTORY "/" ASSETS_IMAGE_PATH, XML_ASSET_SOURCE_ROOT)) {
        return 0;
    }

    xml_finish();

    if (!asset_image_load_all(main_images, main_image_widths)) {
        set_failure_reason("Failed to build or upload extra asset graphics.", 0);
        return 0;
    }

    group_set_for_external_files();

    // By default, if the requested image is not found, the roadblock image will be shown.
    // This ensures compatibility with previous release versions of Augustus, which only had roadblocks
    data.roadblock_image_id = assets_get_group_id("Admin_Logistics");
    data.roadblock_image = asset_image_get_from_id(data.roadblock_image_id - IMAGE_MAIN_ENTRIES);
    data.asset_lookup[ASSET_HIGHWAY_BASE_START] = assets_get_image_id("Admin_Logistics", "Highway_Base_Start");
    data.asset_lookup[ASSET_HIGHWAY_BARRIER_START] = assets_get_image_id("Admin_Logistics", "Highway_Barrier_Start");
    data.asset_lookup[ASSET_AQUEDUCT_WITH_WATER] = assets_get_image_id("Admin_Logistics", "Aqueduct_Bridge_Left_Water");
    data.asset_lookup[ASSET_AQUEDUCT_WITHOUT_WATER] = assets_get_image_id("Admin_Logistics", "Aqueduct_Bridge_Left_Empty");
    data.asset_lookup[ASSET_GOLD_SHIELD] = assets_get_image_id("UI", "GoldShield");
    data.asset_lookup[ASSET_HAGIA_SOPHIA_FIX] = assets_get_image_id("UI", "H Sophia Fix");
    data.asset_lookup[ASSET_FIRST_ORNAMENT] = assets_get_image_id("UI", "First Ornament");
    data.asset_lookup[ASSET_CENTER_CAMERA_ON_BUILDING] = assets_get_image_id("UI", "Center Camera Button");
    data.asset_lookup[ASSET_OX] = assets_get_image_id("Walkers", "Ox_Portrait");
    data.asset_lookup[ASSET_UI_RISKS] = assets_get_image_id("UI", "Risk_Widget_Collapse");
    data.asset_lookup[ASSET_UI_SELECTION_CHECKMARK] = assets_get_image_id("UI", "Selection_Checkmark");
    data.asset_lookup[ASSET_UI_VERTICAL_EMPIRE_PANEL] = assets_get_image_id("UI", "Empire_panel_texture_vertical");
    data.asset_lookup[ASSET_UI_GEAR_ICON] = assets_get_image_id("UI", "gear_icon");
    data.asset_lookup[ASSET_UI_COPY_ICON] = assets_get_image_id("UI", "copy_icon");
    data.asset_lookup[ASSET_UI_PASTE_ICON] = assets_get_image_id("UI", "paste_icon");
    data.asset_lookup[ASSET_UI_ASCEPIUS] = assets_get_image_id("UI", "Asclepius Button");
    // empire icons
    data.asset_lookup[ASSET_UI_EMP_ICON_1] = assets_get_image_id("UI", "Empire_Icon_Roman_01");         // tr_town
    data.asset_lookup[ASSET_UI_EMP_ICON_2] = assets_get_image_id("UI", "Empire_Icon_Roman_02");         // ro_town
    data.asset_lookup[ASSET_UI_EMP_ICON_3] = assets_get_image_id("UI", "Empire_Icon_Roman_03");         // tr_village
    data.asset_lookup[ASSET_UI_EMP_ICON_4] = assets_get_image_id("UI", "Empire_Icon_Roman_04");         // ro_village
    data.asset_lookup[ASSET_UI_EMP_ICON_5] = assets_get_image_id("UI", "Empire_Icon_Roman_05");         // ro_capital
    data.asset_lookup[ASSET_UI_EMP_ICON_6] = assets_get_image_id("UI", "Empire_Icon_Distant_01");       // dis_town
    data.asset_lookup[ASSET_UI_EMP_ICON_7] = assets_get_image_id("UI", "Empire_Icon_Distant_02");       // dis_village
    data.asset_lookup[ASSET_UI_EMP_ICON_8] = assets_get_image_id("UI", "Empire_Icon_Construction_01");  // construction
    data.asset_lookup[ASSET_UI_EMP_ICON_9] = assets_get_image_id("UI", "Empire_Icon_Resource_01");      // res_food
    data.asset_lookup[ASSET_UI_EMP_ICON_10] = assets_get_image_id("UI", "Empire_Icon_Resource_02");     // res_goods
    data.asset_lookup[ASSET_UI_EMP_ICON_11] = assets_get_image_id("UI", "Empire_Icon_Resource_03");     // res_sea
    data.asset_lookup[ASSET_UI_EMP_ICON_12] = assets_get_image_id("UI", "Empire_Icon_Trade_01");        // tr_sea
    data.asset_lookup[ASSET_UI_EMP_ICON_13] = assets_get_image_id("UI", "Empire_Icon_Trade_02");        // tr_land
    data.asset_lookup[ASSET_UI_EMP_ICON_OLD_WATCHTOWER] = assets_get_image_id("UI", "Empire_Icon_Watchtower"); // tower
    data.asset_lookup[ASSET_UI_RESERVOIR_RANGE] = assets_get_image_id("UI", "Reservoir_Range_Overlay_Icon");
    // font assets - keep last
    data.font_lookup[ASSET_FONT_SQ_BRACKET_LEFT] = assets_get_image_id("UI", "leftbracket_white_l");
    data.font_lookup[ASSET_FONT_SQ_BRACKET_RIGHT] = assets_get_image_id("UI", "rightbracket_white_l");
    data.font_lookup[ASSET_FONT_CRLY_BRACKET_LEFT] = assets_get_image_id("UI", "curlybracket_white_left");
    data.font_lookup[ASSET_FONT_CRLY_BRACKET_RIGHT] = assets_get_image_id("UI", "curlybracket_white_right");

    return 1;
}

int assets_load_single_group(const char *file_name, color_t **main_images, int *main_image_widths)
{
    data.failure_reason[0] = '\0';
    if (!group_create_all(1) || !asset_image_init_array()) {
        log_error("Not enough memory to initialize extra assets. The game will probably crash.", 0, 0);
        set_failure_reason("Not enough memory to initialize extra assets.", 0);
        return 0;
    }
    xml_init();
    graphics_renderer()->free_image_atlas(ATLAS_EXTRA_ASSET);
    if (!xml_process_assetlist_file(file_name)) {
        char detail[FILE_NAME_MAX + 64];
        snprintf(detail, sizeof(detail), "Asset list: %s", file_name);
        set_failure_reason("Failed to parse or load an asset list.", detail);
        return 0;
    }
    if (!asset_image_load_all(main_images, main_image_widths)) {
        set_failure_reason("Failed to build or upload extra asset graphics.", 0);
        return 0;
    }
    return 1;
}

int assets_get_group_id(const char *assetlist_name)
{
    image_groups *group = group_get_from_name(assetlist_name);
    if (group) {
        return group->first_image_index + IMAGE_MAIN_ENTRIES;
    }
    log_info("Asset group not found: ", assetlist_name, 0);
    return data.roadblock_image_id;
}

int assets_get_image_id(const char *assetlist_name, const char *image_name)
{
    if (!image_name || !*image_name) {
        return data.roadblock_image_id;
    }
    image_groups *group = group_get_from_name(assetlist_name);
    if (!group) {
        log_info("Asset group not found: ", assetlist_name, 0);
        return data.roadblock_image_id;
    }
    const asset_image *img = asset_image_get_from_id(group->first_image_index);
    while (img && img->index <= (unsigned int) group->last_image_index) {
        if (img->id && strcmp(img->id, image_name) == 0) {
            return img->index + IMAGE_MAIN_ENTRIES;
        }
        img = asset_image_get_from_id(img->index + 1);
    }
    log_info("Asset image not found: ", image_name, 0);
    log_info("Asset group is: ", assetlist_name, 0);
    return data.roadblock_image_id;
}

int assets_get_external_image(const char *path, int force_reload)
{
    if (!path || !*path) {
        return 0;
    }
    image_groups *group = group_get_from_name(ASSET_EXTERNAL_FILE_LIST);
    asset_image *img = asset_image_get_from_id(group->first_image_index);
    int was_found = 0;
    while (img && img->index <= (unsigned int) group->last_image_index) {
        if (img->id && strcmp(img->id, path) == 0) {
            if (!force_reload) {
                return img->index + IMAGE_MAIN_ENTRIES;
            } else {
                was_found = 1;
                break;
            }
        }
        img = asset_image_get_from_id(img->index + 1);
    }
    if (was_found) {
        graphics_renderer()->free_unpacked_image(&img->img);
        asset_image_unload(img);
    }
    const asset_image *new_img = asset_image_create_external(path);
    if (!new_img) {
        return 0;
    }
    if (group->first_image_index == -1) {
        group->first_image_index = new_img->index;
    }
    group->last_image_index = new_img->index;
    return new_img->index + IMAGE_MAIN_ENTRIES;
}

int assets_lookup_image_id(asset_id id)
{
    return data.asset_lookup[id];
}

const image *assets_get_image(int image_id)
{
    asset_image *img = asset_image_get_from_id(image_id - IMAGE_MAIN_ENTRIES);
    if (!img) {
        img = data.roadblock_image;
    }
    if (!img) {
        return image_get(0);
    }
    return &img->img;
}

const image *assets_get_font_image(int letter_id)
{
    const image *img = image_get(data.font_lookup[letter_id - IMAGE_FONT_CUSTOM_OFFSET]);
    return img; // offset used to differentiate from normal letters
}

void assets_load_unpacked_asset(int image_id)
{
    asset_image *img = asset_image_get_from_id(image_id - IMAGE_MAIN_ENTRIES);
    if (!img || img->img.resource_handle) {
        return;
    }
    const color_t *pixels;
    if (img->is_reference) {
        asset_image *referenced_asset =
            asset_image_get_from_id(img->first_layer.calculated_image_id - IMAGE_MAIN_ENTRIES);
        pixels = referenced_asset->data;
    } else {
        pixels = img->data;
    }
    graphics_renderer()->load_unpacked_image(&img->img, pixels);
}

const char *assets_get_failure_reason(void)
{
    return data.failure_reason;
}

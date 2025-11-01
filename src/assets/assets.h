#ifndef ASSETS_H
#define ASSETS_H

#include "core/image.h"

#define ASSETS_IMAGE_PATH "Graphics"

#define ASSET_EXTERNAL_FILE_LIST "***EXTERNAL_FILES***"

#define PATH_ROTATE_OFFSET 56

typedef enum {
	ASSET_HIGHWAY_BASE_START,
	ASSET_HIGHWAY_BARRIER_START,
	ASSET_AQUEDUCT_WITH_WATER,
	ASSET_AQUEDUCT_WITHOUT_WATER,
	ASSET_GOLD_SHIELD,
	ASSET_HAGIA_SOPHIA_FIX,
	ASSET_FIRST_ORNAMENT,
	ASSET_CENTER_CAMERA_ON_BUILDING,
	ASSET_OX,
	ASSET_UI_RISKS,
	ASSET_UI_SELECTION_CHECKMARK,
	ASSET_UI_VERTICAL_EMPIRE_PANEL,
	ASSET_UI_GEAR_ICON,
	ASSET_UI_COPY_ICON,
	ASSET_UI_PASTE_ICON,
	ASSET_UI_ASCEPIUS,
	ASSET_MAX_KEY
} asset_id;

typedef enum {
	ASSET_FONT_NONE, // 14000 - equal to IMAGE_FONT_CUSTOM_OFFSET
	ASSET_FONT_SQ_BRACKET_LEFT,
	ASSET_FONT_SQ_BRACKET_RIGHT,
	ASSET_FONT_MAX_KEY
} asset_font_id;

void assets_init(int force_reload, color_t **main_images, int *main_image_widths);

int assets_load_single_group(const char *file_name, color_t **main_images, int *main_image_widths);

int assets_get_group_id(const char *assetlist_name);

int assets_get_image_id(const char *assetlist_name, const char *image_name);

int assets_get_external_image(const char *path, int force_reload);

int assets_lookup_image_id(asset_id id);

const image *assets_get_image(int image_id);

const image *assets_get_font_image(int letter_id);

void assets_load_unpacked_asset(int image_id);

#endif // ASSETS_H

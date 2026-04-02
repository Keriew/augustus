#ifndef ASSETS_XML_H
#define ASSETS_XML_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    XML_ASSET_SOURCE_AUTO = 0,
    XML_ASSET_SOURCE_MOD = 1,
    XML_ASSET_SOURCE_ROOT = 2
} xml_asset_source;

void xml_init(void);
int xml_process_assetlist_file(const char *xml_file_name);
int xml_process_assetlist_file_from_source(const char *xml_file_name, xml_asset_source source);
void xml_finish(void);
void xml_get_full_image_path(char *full_path, const char *image_file_name);
void xml_get_full_group_image_path(char *full_path, const char *group_name);

#ifdef __cplusplus
}
#endif

#endif // ASSETS_XML_H

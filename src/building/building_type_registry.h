#ifndef BUILDING_BUILDING_TYPE_REGISTRY_H
#define BUILDING_BUILDING_TYPE_REGISTRY_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct building building;

void building_type_registry_set_mod_name(const char *mod_name);
const char *building_type_registry_get_mod_name(void);
const char *building_type_registry_get_mod_path(void);
const char *building_type_registry_get_building_type_path(void);

int building_type_registry_validate_mod(void);
int building_type_registry_load(void);

void building_type_registry_reset_runtime_instances(void);
void building_type_registry_apply_graphic(building *b);

#ifdef __cplusplus
}
#endif

#endif // BUILDING_BUILDING_TYPE_REGISTRY_H

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void mod_manager_set_mod_name(const char *mod_name);
const char *mod_manager_get_mod_name(void);
const char *mod_manager_get_mod_path(void);
const char *mod_manager_get_graphics_path(void);
const char *mod_manager_get_augustus_graphics_path(void);
const char *mod_manager_get_julius_graphics_path(void);
int mod_manager_validate_mod_path(void);
int mod_manager_validate_graphics_path(void);

#ifdef __cplusplus
}
#endif


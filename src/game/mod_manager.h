#ifndef GAME_MOD_MANAGER_H
#define GAME_MOD_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

void mod_manager_set_mod_name(const char *mod_name);
const char *mod_manager_get_mod_name(void);
const char *mod_manager_get_mod_path(void);
int mod_manager_validate_mod_path(void);

#ifdef __cplusplus
}
#endif

#endif // GAME_MOD_MANAGER_H

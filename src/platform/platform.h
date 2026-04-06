#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int platform_sdl_version_at_least(int major, int minor, int patch);
char *platform_get_logging_path(void);
char *platform_get_pref_path(void);
void exit_with_status(int status);

#ifdef __cplusplus
}
#endif

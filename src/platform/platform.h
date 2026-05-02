#ifndef PLATFORM_PLATFORM_H
#define PLATFORM_PLATFORM_H

int platform_sdl_version_at_least(int major, int minor, int patch);
const char *platform_get_logging_path(void);
const char *platform_get_pref_path(void);
const char *platform_get_base_path(void);
void exit_with_status(int status);

#endif // PLATFORM_PLATFORM_H

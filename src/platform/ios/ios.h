#ifndef PLATFORM_IOS_H
#define PLATFORM_IOS_H

#if defined(TARGET_OS_IOS) && !defined(TARGET_OS_MACCATALYST)

const char *ios_show_c3_path_dialog(int again);
void c3_path_chosen(char *new_path);
const char *ios_get_base_path(void);

#endif // defined(TARGET_OS_IOS) && !defined(TARGET_OS_MACCATALYST)

#endif // PLATFORM_IOS_H

#ifndef PLATFORM_IOS_H
#define PLATFORM_IOS_H

#if defined(__IPHONEOS__) || defined(SDL_PLATFORM_IOS)

const char *ios_show_c3_path_dialog(int again);
void c3_path_chosen(char *new_path);
const char *ios_get_base_path(void);

#endif // defined(__IPHONEOS__) || defined(SDL_PLATFORM_IOS)

#endif // PLATFORM_IOS_H

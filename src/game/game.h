#ifndef GAME_GAME_H
#define GAME_GAME_H

#ifdef __cplusplus
extern "C" {
#endif

int game_pre_init(void);

int game_init(void);
const char *game_get_init_failure_message(void);

int game_init_editor(void);

int game_reload_language(void);

void game_run(void);

void game_draw(void);

void game_display_fps(int fps);

void game_exit_editor(void);

void game_exit(void);

#ifdef __cplusplus
}
#endif

#endif // GAME_GAME_H

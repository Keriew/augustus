#ifndef CORE_TERMINAL_H
#define CORE_TERMINAL_H

void terminal_init(void);
void terminal_add_line(const char *text);
void terminal_toggle(void);
int terminal_is_active(void);
void terminal_draw(void);
void terminal_scroll_up(void);
void terminal_scroll_down(void);
void terminal_submit(void);
void terminal_handle_key_down(int scancode, int sym, int mod);
void terminal_handle_text(const char *text_utf8);

#endif // CORE_TERMINAL_H

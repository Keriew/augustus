#include "generic_button.h"

#include "graphics/lang_text.h"

static unsigned int get_button(const mouse *m, int x, int y, generic_button *buttons, unsigned int num_buttons)
{
    for (unsigned int i = 0; i < num_buttons; i++) {
        if (x + buttons[i].x <= m->x &&
            x + buttons[i].x + buttons[i].width > m->x &&
            y + buttons[i].y <= m->y &&
            y + buttons[i].y + buttons[i].height > m->y) {
            return i + 1;
        }
    }
    return 0;
}

int generic_buttons_handle_mouse(const mouse *m, int x, int y, generic_button *buttons, unsigned int num_buttons,
    unsigned int *focus_button_id)
{
    unsigned int button_id = get_button(m, x, y, buttons, num_buttons);
    if (focus_button_id) {
        *focus_button_id = button_id;
    }
    if (!button_id) {
        return 0;
    }
    generic_button *button = &buttons[button_id - 1];
    if (m->left.went_up) {
        button->left_click_handler(button->parameter1, button->parameter2);
        return button->left_click_handler != button_none;
    } else if (m->right.went_up) {
        button->right_click_handler(button->parameter1, button->parameter2);
        return button->right_click_handler != button_none;
    } else {
        return 0;
    }
}

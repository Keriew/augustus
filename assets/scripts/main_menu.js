function button_none() {}

var focus_button_id = 0

var buttons = [
    {x:192, y:130, w:256, h:25, text_group: 30, text_id: 1,
        on_click: window_new_career_show, on_rclick: button_none},
    {x:192, y:170, w:256, h:25, text_group: 30, text_id: 2,
        on_click: function() {window_file_dialog_show(FILE_TYPE_SAVED_GAME, FILE_DIALOG_LOAD)}, on_rclick: button_none},
    {x:192, y:210, w:256, h:25, text_group: 30, text_id: 3,
        on_click: window_cck_selection_show, on_rclick: button_none},
    {x:192, y:250, w:256, h:25, text_group: 9, text_id: 8,
        on_click: window_mission_editor_show, on_rclick: button_none},
    {x:192, y:290, w:256, h:25, text_group: 2, text_id: 0,
        on_click: window_config_show, on_rclick: button_none},
    {x:192, y:330, w:256, h:25, text_group: 30, text_id: 5,
        on_click: window_popup_dialog_show, on_rclick: button_none},
]

function get_button(mx, my, x, y, btns) {
    for (var i=0; i < btns.length; ++i) {
        var btn = btns[i]
        if (x + btn.x <= mx && x + btn.x + btn.w > mx &&
            y + btn.y <= my && y + btn.y + btn.h > my) {
            return i + 1;
        }
    }
    return 0;
}


function buttons_handle_mouse(mx, my, x, y, btns) {
    var button_id = get_button(mx, my, x, y, btns);
    focus_button_id = button_id;

    if (!button_id) {
        return 0;
    }

    var button = btns[button_id - 1];
    if (mouse.left_went_up()) {
        button.on_click()
        return button.on_click != button_none
    } else if (mouse.right_went_up()) {
        button.on_rclick()
        return button.on_rclick != button_none
    }

    return 0
}

function main_menu_foreground() {
    for (var i=0; i < buttons.length; ++i) {
        var btn = buttons[i];
        draw_button(btn.x, btn.y, btn.w, (focus_button_id == i + 1) ? 1 : 0, btn.text_group, btn.text_id)
    }
}

function main_menu_handle_input(mx, my) {
    if (buttons_handle_mouse(mx, my, 0, 0, buttons)) {
        return;
    }
    if (hotkey.escape_pressed()) {
        show_escape_dialog();
    }
    if (hotkey.load_file()) {
        window_file_dialog_show(FILE_TYPE_SAVED_GAME, FILE_DIALOG_LOAD);
    }
}
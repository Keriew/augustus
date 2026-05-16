#include "platform/joystick.h"

#include "input/touch.h"
#include "platform/platform.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

typedef struct {
    /* SDL binding kind for this entry (button, axis, hat, etc.). */
    SDL_GamepadBindingType type;
    /* SDL identifier for the bound control, such as axis/button index. */
    union {
        SDL_GamepadAxis axis;
        SDL_GamepadButton button;
    } id;
    /* Direction or position associated with the binding when applicable. */
    joystick_axis_position position;
} gamepad_mapping_element;

/*
 * Stores the SDL gamepad bindings associated with a mapping action.
 *
 * Unlike mapping_element, which describes a generic joystick input in terms of
 * this project's JOYSTICK_ELEMENT_* representation, gamepad_mapping keeps
 * the SDL_Gamepad-derived binding information returned by SDL for each
 * action so it can be translated into the internal mapping format.
 */
typedef struct {
    /* Action that the listed controller bindings should trigger. */
    mapping_action action;
    gamepad_mapping_element element[JOYSTICK_MAPPING_ELEMENTS_MAX];
} gamepad_mapping;

static int enabled;

#ifdef __vita__
#include "platform/vita/pad.h"

static mapping_element default_mapping[] = {
    {MAPPING_ACTION_MOUSE_CURSOR_UP, {{JOYSTICK_ELEMENT_AXIS, VITA_LEFT_ANALOG_Y, JOYSTICK_AXIS_NEGATIVE}}},
    {MAPPING_ACTION_MOUSE_CURSOR_LEFT, {{JOYSTICK_ELEMENT_AXIS, VITA_LEFT_ANALOG_X, JOYSTICK_AXIS_NEGATIVE}}},
    {MAPPING_ACTION_MOUSE_CURSOR_DOWN, {{JOYSTICK_ELEMENT_AXIS, VITA_LEFT_ANALOG_Y, JOYSTICK_AXIS_POSITIVE}}},
    {MAPPING_ACTION_MOUSE_CURSOR_RIGHT, {{JOYSTICK_ELEMENT_AXIS, VITA_LEFT_ANALOG_X, JOYSTICK_AXIS_POSITIVE}}},
    {MAPPING_ACTION_FASTER_MOUSE_CURSOR_SPEED, {{JOYSTICK_ELEMENT_BUTTON, VITA_PAD_L}}},
    {MAPPING_ACTION_SLOWER_MOUSE_CURSOR_SPEED, {{JOYSTICK_ELEMENT_BUTTON, VITA_PAD_R}}},
    {MAPPING_ACTION_LEFT_MOUSE_BUTTON, {{JOYSTICK_ELEMENT_BUTTON, VITA_PAD_CROSS}}},
    {MAPPING_ACTION_RIGHT_MOUSE_BUTTON, {{JOYSTICK_ELEMENT_BUTTON, VITA_PAD_CIRCLE}}},
    {MAPPING_ACTION_SCROLL_WINDOW_UP, {{JOYSTICK_ELEMENT_AXIS, VITA_RIGHT_ANALOG_Y, JOYSTICK_AXIS_NEGATIVE}}},
    {MAPPING_ACTION_SCROLL_WINDOW_DOWN, {{JOYSTICK_ELEMENT_AXIS, VITA_RIGHT_ANALOG_Y, JOYSTICK_AXIS_POSITIVE}}},
    {MAPPING_ACTION_SCROLL_MAP_UP, {{JOYSTICK_ELEMENT_AXIS, VITA_RIGHT_ANALOG_Y, JOYSTICK_AXIS_NEGATIVE}}},
    {MAPPING_ACTION_SCROLL_MAP_LEFT, {{JOYSTICK_ELEMENT_AXIS, VITA_RIGHT_ANALOG_X, JOYSTICK_AXIS_NEGATIVE}}},
    {MAPPING_ACTION_SCROLL_MAP_DOWN, {{JOYSTICK_ELEMENT_AXIS, VITA_RIGHT_ANALOG_Y, JOYSTICK_AXIS_POSITIVE}}},
    {MAPPING_ACTION_SCROLL_MAP_RIGHT, {{JOYSTICK_ELEMENT_AXIS, VITA_RIGHT_ANALOG_X, JOYSTICK_AXIS_POSITIVE}}},
    {MAPPING_ACTION_INCREASE_GAME_SPEED, {{JOYSTICK_ELEMENT_BUTTON, VITA_PAD_TRIANGLE}}},
    {MAPPING_ACTION_DECREASE_GAME_SPEED, {{JOYSTICK_ELEMENT_BUTTON, VITA_PAD_SQUARE}}},
    {MAPPING_ACTION_SHOW_VIRTUAL_KEYBOARD, {{JOYSTICK_ELEMENT_BUTTON, VITA_PAD_START}}},
    {MAPPING_ACTION_CYCLE_TOUCH_TYPE, {{JOYSTICK_ELEMENT_BUTTON, VITA_PAD_SELECT}}},
    {MAPPING_ACTION_MOUSE_CURSOR_UP, {{JOYSTICK_ELEMENT_BUTTON, VITA_PAD_UP}}},
    {MAPPING_ACTION_MOUSE_CURSOR_LEFT, {{JOYSTICK_ELEMENT_BUTTON, VITA_PAD_LEFT}}},
    {MAPPING_ACTION_MOUSE_CURSOR_DOWN, {{JOYSTICK_ELEMENT_BUTTON, VITA_PAD_DOWN}}},
    {MAPPING_ACTION_MOUSE_CURSOR_RIGHT, {{JOYSTICK_ELEMENT_BUTTON, VITA_PAD_RIGHT}}},
    {MAPPING_ACTION_RESET_MAPPING, {
        {JOYSTICK_ELEMENT_BUTTON, VITA_PAD_L}, {JOYSTICK_ELEMENT_BUTTON, VITA_PAD_R}
    }},
};
#elif defined(__SWITCH__)
#include "platform/switch/pad.h"

static mapping_element default_mapping[] = {
    {MAPPING_ACTION_MOUSE_CURSOR_UP, {{JOYSTICK_ELEMENT_AXIS, SWITCH_LEFT_ANALOG_Y, JOYSTICK_AXIS_NEGATIVE}}},
    {MAPPING_ACTION_MOUSE_CURSOR_LEFT, {{JOYSTICK_ELEMENT_AXIS, SWITCH_LEFT_ANALOG_X, JOYSTICK_AXIS_NEGATIVE}}},
    {MAPPING_ACTION_MOUSE_CURSOR_DOWN, {{JOYSTICK_ELEMENT_AXIS, SWITCH_LEFT_ANALOG_Y, JOYSTICK_AXIS_POSITIVE}}},
    {MAPPING_ACTION_MOUSE_CURSOR_RIGHT, {{JOYSTICK_ELEMENT_AXIS, SWITCH_LEFT_ANALOG_X, JOYSTICK_AXIS_POSITIVE}}},
    {MAPPING_ACTION_FASTER_MOUSE_CURSOR_SPEED, {{JOYSTICK_ELEMENT_BUTTON, SWITCH_PAD_L}}},
    {MAPPING_ACTION_SLOWER_MOUSE_CURSOR_SPEED, {{JOYSTICK_ELEMENT_BUTTON, SWITCH_PAD_R}}},
    {MAPPING_ACTION_LEFT_MOUSE_BUTTON, {{JOYSTICK_ELEMENT_BUTTON, SWITCH_PAD_A}}},
    {MAPPING_ACTION_RIGHT_MOUSE_BUTTON, {{JOYSTICK_ELEMENT_BUTTON, SWITCH_PAD_B}}},
    {MAPPING_ACTION_SCROLL_WINDOW_UP, {{JOYSTICK_ELEMENT_AXIS, SWITCH_RIGHT_ANALOG_Y, JOYSTICK_AXIS_NEGATIVE}}},
    {MAPPING_ACTION_SCROLL_WINDOW_DOWN, {{JOYSTICK_ELEMENT_AXIS, SWITCH_RIGHT_ANALOG_Y, JOYSTICK_AXIS_POSITIVE}}},
    {MAPPING_ACTION_SCROLL_MAP_UP, {{JOYSTICK_ELEMENT_AXIS, SWITCH_RIGHT_ANALOG_Y, JOYSTICK_AXIS_NEGATIVE}}},
    {MAPPING_ACTION_SCROLL_MAP_LEFT, {{JOYSTICK_ELEMENT_AXIS, SWITCH_RIGHT_ANALOG_X, JOYSTICK_AXIS_NEGATIVE}}},
    {MAPPING_ACTION_SCROLL_MAP_DOWN, {{JOYSTICK_ELEMENT_AXIS, SWITCH_RIGHT_ANALOG_Y, JOYSTICK_AXIS_POSITIVE}}},
    {MAPPING_ACTION_SCROLL_MAP_RIGHT, {{JOYSTICK_ELEMENT_AXIS, SWITCH_RIGHT_ANALOG_X, JOYSTICK_AXIS_POSITIVE}}},
    {MAPPING_ACTION_ROTATE_MAP_LEFT, {{JOYSTICK_ELEMENT_BUTTON, SWITCH_PAD_ZL}}},
    {MAPPING_ACTION_ROTATE_MAP_RIGHT, {{JOYSTICK_ELEMENT_BUTTON, SWITCH_PAD_ZR}}},
    {MAPPING_ACTION_INCREASE_GAME_SPEED, {{JOYSTICK_ELEMENT_BUTTON, SWITCH_PAD_X}}},
    {MAPPING_ACTION_DECREASE_GAME_SPEED, {{JOYSTICK_ELEMENT_BUTTON, SWITCH_PAD_Y}}},
    {MAPPING_ACTION_TOGGLE_PAUSE, {{JOYSTICK_ELEMENT_BUTTON, SWITCH_RSTICK}}},
    {MAPPING_ACTION_CYCLE_LEGION, {{JOYSTICK_ELEMENT_BUTTON, SWITCH_LSTICK}}},
    {MAPPING_ACTION_SHOW_VIRTUAL_KEYBOARD, {{JOYSTICK_ELEMENT_BUTTON, SWITCH_PAD_PLUS}}},
    {MAPPING_ACTION_CYCLE_TOUCH_TYPE, {{JOYSTICK_ELEMENT_BUTTON, SWITCH_PAD_MINUS}}},
    {MAPPING_ACTION_MOUSE_CURSOR_UP, {{JOYSTICK_ELEMENT_BUTTON, SWITCH_PAD_UP}}},
    {MAPPING_ACTION_MOUSE_CURSOR_LEFT, {{JOYSTICK_ELEMENT_BUTTON, SWITCH_PAD_LEFT}}},
    {MAPPING_ACTION_MOUSE_CURSOR_DOWN, {{JOYSTICK_ELEMENT_BUTTON, SWITCH_PAD_DOWN}}},
    {MAPPING_ACTION_MOUSE_CURSOR_RIGHT, {{JOYSTICK_ELEMENT_BUTTON, SWITCH_PAD_RIGHT}}},
    {MAPPING_ACTION_RESET_MAPPING, {
        {JOYSTICK_ELEMENT_BUTTON, SWITCH_PAD_L}, {JOYSTICK_ELEMENT_BUTTON, SWITCH_PAD_R}
    }}
};
#else
static mapping_element default_mapping[] = {
    {MAPPING_ACTION_MOUSE_CURSOR_UP, {{JOYSTICK_ELEMENT_AXIS, 1, JOYSTICK_AXIS_NEGATIVE}}},
    {MAPPING_ACTION_MOUSE_CURSOR_LEFT, {{JOYSTICK_ELEMENT_AXIS, 0, JOYSTICK_AXIS_NEGATIVE}}},
    {MAPPING_ACTION_MOUSE_CURSOR_DOWN, {{JOYSTICK_ELEMENT_AXIS, 1, JOYSTICK_AXIS_POSITIVE}}},
    {MAPPING_ACTION_MOUSE_CURSOR_RIGHT, {{JOYSTICK_ELEMENT_AXIS, 0, JOYSTICK_AXIS_POSITIVE}}},
    {MAPPING_ACTION_FASTER_MOUSE_CURSOR_SPEED, {{JOYSTICK_ELEMENT_AXIS, 5, JOYSTICK_AXIS_POSITIVE}}},
    {MAPPING_ACTION_SLOWER_MOUSE_CURSOR_SPEED, {{JOYSTICK_ELEMENT_AXIS, 2, JOYSTICK_AXIS_POSITIVE}}},
    {MAPPING_ACTION_LEFT_MOUSE_BUTTON, {{JOYSTICK_ELEMENT_BUTTON, 0}}},
    {MAPPING_ACTION_RIGHT_MOUSE_BUTTON, {{JOYSTICK_ELEMENT_BUTTON, 1}}},
    {MAPPING_ACTION_SCROLL_WINDOW_UP, {{JOYSTICK_ELEMENT_AXIS, 4, JOYSTICK_AXIS_NEGATIVE}}},
    {MAPPING_ACTION_SCROLL_WINDOW_DOWN, {{JOYSTICK_ELEMENT_AXIS, 4, JOYSTICK_AXIS_POSITIVE}}},
    {MAPPING_ACTION_SCROLL_MAP_UP, {{JOYSTICK_ELEMENT_AXIS, 4, JOYSTICK_AXIS_NEGATIVE}}},
    {MAPPING_ACTION_SCROLL_MAP_LEFT, {{JOYSTICK_ELEMENT_AXIS, 3, JOYSTICK_AXIS_NEGATIVE}}},
    {MAPPING_ACTION_SCROLL_MAP_DOWN, {{JOYSTICK_ELEMENT_AXIS, 4, JOYSTICK_AXIS_POSITIVE}}},
    {MAPPING_ACTION_SCROLL_MAP_RIGHT, {{JOYSTICK_ELEMENT_AXIS, 3, JOYSTICK_AXIS_POSITIVE}}},
    {MAPPING_ACTION_ROTATE_MAP_LEFT, {{JOYSTICK_ELEMENT_BUTTON, 5}}},
    {MAPPING_ACTION_ROTATE_MAP_RIGHT, {{JOYSTICK_ELEMENT_BUTTON, 4}}},
    {MAPPING_ACTION_INCREASE_GAME_SPEED, {{JOYSTICK_ELEMENT_BUTTON, 2}}},
    {MAPPING_ACTION_DECREASE_GAME_SPEED, {{JOYSTICK_ELEMENT_BUTTON, 3}}},
    {MAPPING_ACTION_MOUSE_CURSOR_UP, {{JOYSTICK_ELEMENT_HAT, 0, JOYSTICK_HAT_UP}}},
    {MAPPING_ACTION_MOUSE_CURSOR_LEFT, {{JOYSTICK_ELEMENT_HAT, 0, JOYSTICK_HAT_LEFT}}},
    {MAPPING_ACTION_MOUSE_CURSOR_DOWN, {{JOYSTICK_ELEMENT_HAT, 0, JOYSTICK_HAT_DOWN}}},
    {MAPPING_ACTION_MOUSE_CURSOR_RIGHT, {{JOYSTICK_ELEMENT_HAT, 0, JOYSTICK_HAT_RIGHT}}}
};
#endif

static gamepad_mapping gamepad_mappings[] = {
    {.action = MAPPING_ACTION_MOUSE_CURSOR_UP,
        .element = {{.type = SDL_GAMEPAD_BINDTYPE_AXIS, .id.axis = SDL_GAMEPAD_AXIS_LEFTY, .position = JOYSTICK_AXIS_NEGATIVE}}},
    {.action = MAPPING_ACTION_MOUSE_CURSOR_LEFT,
        .element = {{.type = SDL_GAMEPAD_BINDTYPE_AXIS, .id.axis = SDL_GAMEPAD_AXIS_LEFTX, .position = JOYSTICK_AXIS_NEGATIVE}}},
    {.action = MAPPING_ACTION_MOUSE_CURSOR_DOWN,
        .element = {{.type = SDL_GAMEPAD_BINDTYPE_AXIS, .id.axis = SDL_GAMEPAD_AXIS_LEFTY, .position = JOYSTICK_AXIS_POSITIVE}}},
    {.action = MAPPING_ACTION_MOUSE_CURSOR_RIGHT,
        .element = {{.type = SDL_GAMEPAD_BINDTYPE_AXIS, .id.axis = SDL_GAMEPAD_AXIS_LEFTX, .position = JOYSTICK_AXIS_POSITIVE}}},
    {.action = MAPPING_ACTION_FASTER_MOUSE_CURSOR_SPEED,
        .element = {{.type = SDL_GAMEPAD_BINDTYPE_AXIS, .id.axis = SDL_GAMEPAD_AXIS_LEFT_TRIGGER, .position = JOYSTICK_AXIS_POSITIVE}}},
    {.action = MAPPING_ACTION_SLOWER_MOUSE_CURSOR_SPEED,
        .element = {{.type = SDL_GAMEPAD_BINDTYPE_AXIS, .id.axis = SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, .position = JOYSTICK_AXIS_POSITIVE}}},
    {.action = MAPPING_ACTION_LEFT_MOUSE_BUTTON,
        .element = {{.type = SDL_GAMEPAD_BINDTYPE_BUTTON, .id.button = SDL_GAMEPAD_BUTTON_SOUTH}}},
    {.action = MAPPING_ACTION_RIGHT_MOUSE_BUTTON,
        .element = {{.type = SDL_GAMEPAD_BINDTYPE_BUTTON, .id.button = SDL_GAMEPAD_BUTTON_EAST}}},
    {.action = MAPPING_ACTION_SCROLL_WINDOW_UP,
        .element = {{.type = SDL_GAMEPAD_BINDTYPE_AXIS, .id.axis = SDL_GAMEPAD_AXIS_RIGHTY, .position = JOYSTICK_AXIS_NEGATIVE}}},
    {.action = MAPPING_ACTION_SCROLL_WINDOW_DOWN,
        .element = {{.type = SDL_GAMEPAD_BINDTYPE_AXIS, .id.axis = SDL_GAMEPAD_AXIS_RIGHTY, .position = JOYSTICK_AXIS_POSITIVE}}},
    {.action = MAPPING_ACTION_SCROLL_MAP_UP,
        .element = {{.type = SDL_GAMEPAD_BINDTYPE_AXIS, .id.axis = SDL_GAMEPAD_AXIS_RIGHTY, .position = JOYSTICK_AXIS_NEGATIVE}}},
    {.action = MAPPING_ACTION_SCROLL_MAP_LEFT,
        .element = {{.type = SDL_GAMEPAD_BINDTYPE_AXIS, .id.axis = SDL_GAMEPAD_AXIS_RIGHTX, .position = JOYSTICK_AXIS_NEGATIVE}}},
    {.action = MAPPING_ACTION_SCROLL_MAP_DOWN,
        .element = {{.type = SDL_GAMEPAD_BINDTYPE_AXIS, .id.axis = SDL_GAMEPAD_AXIS_RIGHTY, .position = JOYSTICK_AXIS_POSITIVE}}},
    {.action = MAPPING_ACTION_SCROLL_MAP_RIGHT,
        .element = {{.type = SDL_GAMEPAD_BINDTYPE_AXIS, .id.axis = SDL_GAMEPAD_AXIS_RIGHTX, .position = JOYSTICK_AXIS_POSITIVE}}},
    {.action = MAPPING_ACTION_ROTATE_MAP_LEFT,
        .element = {{.type = SDL_GAMEPAD_BINDTYPE_BUTTON, .id.button = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER}}},
    {.action = MAPPING_ACTION_ROTATE_MAP_RIGHT,
        .element = {{.type = SDL_GAMEPAD_BINDTYPE_BUTTON, .id.button = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER}}},
    {.action = MAPPING_ACTION_INCREASE_GAME_SPEED,
        .element = {{.type = SDL_GAMEPAD_BINDTYPE_BUTTON, .id.button = SDL_GAMEPAD_BUTTON_WEST}}},
    {.action = MAPPING_ACTION_DECREASE_GAME_SPEED,
        .element = {{.type = SDL_GAMEPAD_BINDTYPE_BUTTON, .id.button = SDL_GAMEPAD_BUTTON_NORTH}}},
    {.action = MAPPING_ACTION_MOUSE_CURSOR_UP,
        .element = {{.type = SDL_GAMEPAD_BINDTYPE_BUTTON, .id.button = SDL_GAMEPAD_BUTTON_DPAD_UP}}},
    {.action = MAPPING_ACTION_MOUSE_CURSOR_LEFT,
        .element = {{.type = SDL_GAMEPAD_BINDTYPE_BUTTON, .id.button = SDL_GAMEPAD_BUTTON_DPAD_LEFT}}},
    {.action = MAPPING_ACTION_MOUSE_CURSOR_DOWN,
        .element = {{.type = SDL_GAMEPAD_BINDTYPE_BUTTON, .id.button = SDL_GAMEPAD_BUTTON_DPAD_DOWN}}},
    {.action = MAPPING_ACTION_MOUSE_CURSOR_RIGHT,
        .element = {{.type = SDL_GAMEPAD_BINDTYPE_BUTTON, .id.button = SDL_GAMEPAD_BUTTON_DPAD_RIGHT}}}
};

#define NUM_GAMEPAD_MAPPINGS (sizeof(gamepad_mappings) / sizeof(gamepad_mapping))

static SDL_GamepadBinding *get_binding_for_element(SDL_GamepadBinding **bindings, int total_bindings,
    const gamepad_mapping_element *element)
{
    for (int i = 0; i < total_bindings; i++) {
        SDL_GamepadBinding *binding = bindings[i];
        if (binding->output_type != element->type) {
            continue;
        }
        switch (binding->output_type) {
            case SDL_GAMEPAD_BINDTYPE_AXIS:
                if (binding->output.axis.axis == element->id.axis) {
                    return binding;
                }
                break;
            case SDL_GAMEPAD_BINDTYPE_BUTTON:
                if (binding->output.button == element->id.button) {
                    return binding;
                }
                break;
            default:
                break;
        }
    }
    return 0;
}

static void set_default_controller_mapping(joystick_model *model, SDL_Gamepad *gamepad)
{
    int total_bindings;
    SDL_GamepadBinding **bindings = SDL_GetGamepadBindings(gamepad, &total_bindings);

    if (!bindings) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not get gamepad bindings: %s", SDL_GetError());
        return;
    }

    for (int i = 0; i < NUM_GAMEPAD_MAPPINGS; i++) {
        const gamepad_mapping *mapping = &gamepad_mappings[i];

        SDL_GamepadBinding *binds[JOYSTICK_MAPPING_ELEMENTS_MAX] = { 0 };

        int bind_failed = 0;
        for (int j = 0; j < JOYSTICK_MAPPING_ELEMENTS_MAX; j++) {
            if (mapping->element[j].type == SDL_GAMEPAD_BINDTYPE_NONE) {
                continue;
            }
            binds[j] = get_binding_for_element(bindings, total_bindings, &mapping->element[j]);
            if (!binds[j]) {
                bind_failed = 1;
                break;
            }
        }
        if (bind_failed) {
            continue;
        }
        mapping_element *action = &model->mapping[model->num_mappings];
        action->action = mapping->action;
        for (int j = 0; j < JOYSTICK_MAPPING_ELEMENTS_MAX; j++) {
            if (!binds[j]) {
                continue;
            }
            switch (binds[j]->input_type) {
                case SDL_GAMEPAD_BINDTYPE_AXIS:
                    action->element[j].type = JOYSTICK_ELEMENT_AXIS;
                    action->element[j].id = binds[j]->input.axis.axis;
                    action->element[j].position = mapping->element[j].position;
                    break;
                case SDL_GAMEPAD_BINDTYPE_BUTTON:
                    action->element[j].type = JOYSTICK_ELEMENT_BUTTON;
                    action->element[j].id = binds[j]->input.button;
                    break;
                case SDL_GAMEPAD_BINDTYPE_HAT:
                    action->element[j].type = JOYSTICK_ELEMENT_HAT;
                    action->element[j].id = binds[j]->input.hat.hat;
                    action->element[j].position = platform_joystick_convert_hat_position(binds[j]->input.hat.hat_mask);
                    break;
                default:
                    break;
            }
        }
        model->num_mappings++;
    }
    SDL_free(bindings);
}

static void create_new_model(const char *guid, const char *name, int instance_id, SDL_Gamepad *gamepad)
{
    joystick_model model;
    memset(&model, 0, sizeof(model));
    snprintf(model.guid, JOYSTICK_MAX_GUID, "%s", guid);
    if (!name) {
        snprintf(model.name, JOYSTICK_MAX_NAME, "Joystick %d", instance_id);
    } else {
        snprintf(model.name, JOYSTICK_MAX_NAME, "Joystick %s", name);
    }
    if (gamepad) {
        set_default_controller_mapping(&model, gamepad);
    } else {
        size_t size = sizeof(default_mapping);
        if (size > sizeof(model.mapping)) {
            size = sizeof(model.mapping);
        }
        memcpy(model.mapping, default_mapping, size);
        model.num_mappings = size / sizeof(mapping_element);
    }
    joystick_add_model(&model);
}

static void add_joystick(SDL_JoystickID instance_id)
{
    SDL_Gamepad *gamepad = 0;
    SDL_Joystick *joystick = 0;

    if (SDL_IsGamepad(instance_id)) {
        gamepad = SDL_OpenGamepad(instance_id);
        joystick = SDL_GetGamepadJoystick(gamepad);
        SDL_Log("Game controller found. Setting default gamepad mapping.");
    } else {
        joystick = SDL_OpenJoystick(instance_id);
    }
    if (joystick_is_active(instance_id)) {
        if (gamepad) {
            SDL_CloseGamepad(gamepad);
        } else {
            SDL_CloseJoystick(joystick);
        }
        return;
    }

    static char guid[JOYSTICK_MAX_GUID];
    SDL_GUIDToString(SDL_GetJoystickGUID(joystick), guid, JOYSTICK_MAX_GUID);
    if (!joystick_has_model(guid)) {
        create_new_model(guid, SDL_GetJoystickName(joystick), instance_id, gamepad);
    }
    if (!joystick_add(instance_id, guid)) {
        if (gamepad) {
            SDL_CloseGamepad(gamepad);
        } else {
            SDL_CloseJoystick(joystick);
        }
    }
}

static void remove_joystick(SDL_JoystickID instance_id)
{
    if (!joystick_is_active(instance_id)) {
        return;
    }
    SDL_Gamepad *gamepad = 0;
    SDL_Joystick *joystick = 0;
    gamepad = SDL_GetGamepadFromID(instance_id);
    if (!gamepad) {
        joystick = SDL_GetJoystickFromID(instance_id);
    }

    if (gamepad) {
        SDL_CloseGamepad(gamepad);
    } else if (joystick) {
        SDL_CloseJoystick(joystick);
    }
    joystick_remove(instance_id);
}

int platform_joystick_is_enabled(void)
{
#if defined(__vita__) || defined(__SWITCH__) || defined(__ANDROID__)
    return 1;
#else
    return enabled;
#endif
}

void platform_joystick_init(int force_enable)
{
    enabled = force_enable;

    if (!platform_joystick_is_enabled()) {
        return;
    }
    SDL_SetJoystickEventsEnabled(true);
    int count;
    SDL_JoystickID *instance_ids = SDL_GetJoysticks(&count);
    for (int i = 0; i < count; ++i) {
        add_joystick(instance_ids[i]);
    }
    SDL_free(instance_ids);
}

void platform_joystick_device_changed(long long id, int is_connected)
{
    if (!platform_joystick_is_enabled()) {
        return;
    }
    if (is_connected) {
        add_joystick((SDL_JoystickID) id);
    } else {
        remove_joystick((SDL_JoystickID) id);
    }
}

joystick_hat_position platform_joystick_convert_hat_position(int value)
{
    joystick_hat_position position = JOYSTICK_HAT_CENTERED;
    if (value & SDL_HAT_UP) {
        position |= JOYSTICK_HAT_UP;
    }
    if (value & SDL_HAT_DOWN) {
        position |= JOYSTICK_HAT_DOWN;
    }
    if (value & SDL_HAT_LEFT) {
        position |= JOYSTICK_HAT_LEFT;
    }
    if (value & SDL_HAT_RIGHT) {
        position |= JOYSTICK_HAT_RIGHT;
    }
    return position;
}

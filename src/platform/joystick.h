#ifndef PLATFORM_JOYSTICK_H
#define PLATFORM_JOYSTICK_H

#include "input/joystick.h"

int platform_joystick_is_enabled(void);
void platform_joystick_init(int force_enable);
void platform_joystick_device_changed(long long id, int is_connected);
joystick_hat_position platform_joystick_convert_hat_position(int value);

#endif // PLATFORM_JOYSTICK_H

#ifndef GRAPHICS_WEATHER_H
#define GRAPHICS_WEATHER_H

typedef enum {
    WEATHER_NONE,
    WEATHER_RAIN,
    WEATHER_SNOW,
    WEATHER_SAND,
} weather_type;

void weather_stop(void);
void update_weather(void);
void city_weather_update(int month);

#endif // GRAPHICS_WEATHER_H

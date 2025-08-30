#ifndef GRAPHICS_WEATHER_H
#define GRAPHICS_WEATHER_H

typedef enum {
    WEATHER_NONE,
    WEATHER_RAIN,
    WEATHER_SNOW,
    WEATHER_SAND,
} weather_type;

void weather_reset(void);
void set_weather(int active, int intensity, weather_type type);
void update_weather(void);
void city_weather_update(int month);

#endif // GRAPHICS_WEATHER_H

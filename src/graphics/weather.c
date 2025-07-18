#include "weather.h"

#include "core/config.h"
#include "core/dir.h"
#include "core/file.h"
#include "core/random.h"
#include "graphics/color.h"
#include "graphics/graphics.h"
#include "graphics/screen.h"
#include "scenario/property.h"
#include "sound/speech.h"
#include "time.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define DRIFT_DIRECTION_RIGHT 1
#define DRIFT_DIRECTION_LEFT -1

typedef struct {
    int x;
    int y;

    // For rain
    int length;
    int speed;
    int wind_variation;

    // For snow
    int drift_offset;
    int drift_direction;

    // For sand
    int offset;
} weather_element;

static struct {
    int weather_initialized;
    int lightning_timer;
    int lightning_cooldown;
    int wind_angle;
    int wind_speed;
    int overlay_alpha;
    int overlay_target;
    int overlay_color;
    int overlay_fadeout;

    weather_element *elements;

    struct {
        int active;
        int intensity;
        int dx;
        int dy;
        int type;
    } weather_config;
} data = {
    .wind_speed = 1,
    .overlay_target = 100,
    .weather_config = {
        .intensity = 200,
        .dx = 1,
        .dy = 5,
        .type = WEATHER_NONE,
    }
};

void init_weather_element(weather_element *e, int type)
{
    e->x = random_from_stdlib() % screen_width();
    e->y = random_from_stdlib() % screen_height();

    switch (type) {
        case WEATHER_RAIN:
            e->length = 10 + random_from_stdlib() % 10;
            e->speed = 4 + random_from_stdlib() % 5;
            if (data.weather_config.intensity < 500) {
                e->wind_variation = (random_from_stdlib() % 2) - 0; // 0 or 1
            } else {
                e->wind_variation = (random_from_stdlib() % 3) - 1; // -1, 0 or 1
            }
            break;
        case WEATHER_SNOW:
            e->drift_offset = random_from_stdlib() % 100;
            e->speed = 1 + random_from_stdlib() % 2;
            e->drift_direction = (random_from_stdlib() % 2 == 0) ? DRIFT_DIRECTION_RIGHT : DRIFT_DIRECTION_LEFT;
            break;
        case WEATHER_SAND:
            e->speed = 1 + (random_from_stdlib() % 2);
            e->offset = random_between_from_stdlib(0, 1000);
            break;
    }
}

void weather_stop(void)
{
    data.weather_config.active = 0;

    if (data.elements) {
        free(data.elements);
        data.elements = 0;
    }

    data.weather_initialized = 0;
}

static uint32_t apply_alpha(uint32_t color, uint8_t alpha)
{
    return (color & 0x00FFFFFF) | (alpha << 24);
}

static int chance_percent(int percent)
{
    return (random_between_from_stdlib(0, 99) < percent) ? 1 : 0;
}

static void update_lightning(void)
{
    if (data.lightning_cooldown <= 0 && (random_from_stdlib() % 500) == 0) {
        data.lightning_timer = 5 + random_from_stdlib() % 5; // flash duration
        data.lightning_cooldown = 300 + random_from_stdlib() % 400; // gap between flashes
    } else if (data.lightning_cooldown > 0) {
        data.lightning_cooldown--;
    }

    if (data.lightning_timer > 0) {
        color_t white_flash = apply_alpha(COLOR_WHITE, 128);
        graphics_fill_rect(0, 0, screen_width(), screen_height(), white_flash);
        data.lightning_timer--;
    }

    if (data.lightning_timer == 5) {
        char thunder_path[FILE_NAME_MAX];
        int thunder_num = random_between_from_stdlib(1, 2);
        snprintf(thunder_path, sizeof(thunder_path), ASSETS_DIRECTORY "/Sounds/Thunder%d.mp3", thunder_num);
        sound_speech_play_file(thunder_path);
    }
}

static void update_wind(void)
{
    data.wind_angle += data.wind_speed;
    data.weather_config.dx = ((data.wind_angle / 10) % 5) - 2;
}

static void update_overlay_alpha(void)
{
    int speed = 1;

    if (data.overlay_alpha < data.overlay_target) { // fadein
        data.overlay_alpha += speed;
        if (data.overlay_alpha > data.overlay_target) {
            data.overlay_alpha = data.overlay_target;
        }
    } else if (data.overlay_alpha > data.overlay_target) { // fadeout
        data.overlay_alpha -= speed;
        if (data.overlay_alpha < data.overlay_target) {
            data.overlay_alpha = data.overlay_target;
            data.overlay_fadeout = 0;
        }
    }
}

static void render_weather_overlay(void)
{
    update_overlay_alpha();

    if (data.overlay_fadeout == 0 && data.weather_config.active == 0) {
        return;
    }

    int alpha_factor = 40;
    if (data.weather_config.type == WEATHER_SNOW ||
        (data.weather_config.type == WEATHER_RAIN && data.weather_config.intensity < 900)) {
        alpha_factor = 20;
    }

    // no overlay for light rain
    if (data.weather_config.type == WEATHER_RAIN && data.weather_config.intensity < 800) {
        alpha_factor = 0;
    }

    uint8_t alpha = (uint8_t)(((alpha_factor * data.overlay_alpha) / 100) * 255 / 100);

    // update overlay color based on weather type
    if (data.weather_config.type == WEATHER_RAIN) {
        data.overlay_color = COLOR_WEATHER_RAIN;
    } else if (data.weather_config.type == WEATHER_SNOW) {
        data.overlay_color = COLOR_WEATHER_SNOW;
    } else if (data.weather_config.type == WEATHER_SAND) {
        data.overlay_color = COLOR_WEATHER_SAND;
    }

    graphics_fill_rect(0, 0, screen_width(), screen_height(),
        apply_alpha(data.overlay_color, alpha));
}

static void draw_snow(void)
{
    for (int i = 0; i < data.weather_config.intensity; ++i) {
        int drift = ((data.elements[i].y + data.elements[i].drift_offset) % 10) - 5;
        data.elements[i].x += (drift / 10) * data.elements[i].drift_direction;
        data.elements[i].y += data.elements[i].speed;

        graphics_draw_line(
            data.elements[i].x,
            data.elements[i].x + 1,
            data.elements[i].y,
            data.elements[i].y,
            COLOR_WEATHER_SNOWFLAKE);

        graphics_draw_line(
            data.elements[i].x,
            data.elements[i].x,
            data.elements[i].y,
            data.elements[i].y + 1,
            COLOR_WEATHER_SNOWFLAKE);

        if (data.elements[i].y >= screen_height() || data.elements[i].x <= 0 || data.elements[i].x >= screen_width()) {
            init_weather_element(&data.elements[i], data.weather_config.type);
            data.elements[i].y = 0;
        }
    }
}

static void draw_sandstorm(void)
{
    for (int i = 0; i < data.weather_config.intensity; ++i) {
        int wave = ((data.elements[i].y + data.elements[i].offset) % 10) - 5;
        data.elements[i].x += data.elements[i].speed + (wave / 10);

        graphics_draw_line(
            data.elements[i].x,
            data.elements[i].x + 1,
            data.elements[i].y,
            data.elements[i].y + 1,
            COLOR_WEATHER_SAND_PARTICLE);

        if (data.elements[i].x > screen_width()) {
            init_weather_element(&data.elements[i], data.weather_config.type);
            data.elements[i].x = 0;
        }
    }
}

static void draw_rain(void)
{
    if (data.weather_config.intensity < 500) {
        update_wind();
    }

    int wind_strength = abs(data.weather_config.dx);
    int base_speed = 3 + wind_strength + (data.weather_config.intensity / 300);

    for (int i = 0; i < data.weather_config.intensity; ++i) {
        int dx = data.weather_config.dx + data.elements[i].wind_variation;

        graphics_draw_line(
            data.elements[i].x,
            data.elements[i].x + dx * 2,
            data.elements[i].y,
            data.elements[i].y + data.elements[i].length,
            COLOR_WEATHER_DROPS);

        data.elements[i].x += dx;

        int dy = base_speed + data.elements[i].speed + (((data.elements[i].x + data.elements[i].y) % 10) / 10);
        data.elements[i].y += dy;

        if (data.elements[i].y >= screen_height() || data.elements[i].x <= 0 || data.elements[i].x >= screen_width()) {
            init_weather_element(&data.elements[i], data.weather_config.type);
            data.elements[i].y = 0;
        }
    }

    if (data.weather_config.intensity > 800) {
        update_lightning();
    }
}

void update_weather()
{
    render_weather_overlay();

    if (!config_get(CONFIG_UI_DRAW_WEATHER) || data.weather_config.type == WEATHER_NONE || data.weather_config.active == 0) {
        weather_stop();
        return;
    }

    // init
    if (!data.weather_initialized && data.weather_config.active == 1) {
        data.elements = malloc(sizeof(weather_element) * data.weather_config.intensity);
        for (int i = 0; i < data.weather_config.intensity; ++i) {
            init_weather_element(&data.elements[i], data.weather_config.type);
        }
        data.weather_initialized = 1;
    }

    // SNOW
    if (data.weather_config.type == WEATHER_SNOW) {
        draw_snow();
        return;
    }

    // SANDSTORM
    if (data.weather_config.type == WEATHER_SAND) {
        draw_sandstorm();
        return;
    }

    // RAIN
    if (data.weather_config.type == WEATHER_RAIN) {
        draw_rain();
    }

}

static void set_weather(int active, int intensity, weather_type type)
{
    weather_stop();
    if (data.weather_config.active && active == 0) {
        data.overlay_fadeout = 1;
    }

    data.weather_config.active = active;
    data.weather_config.intensity = intensity;
    data.weather_config.type = type;

    if (active == 0) {
        data.overlay_target = 0;
    } else {
        data.overlay_target = 100;
    }
}

void city_weather_update(int month)
{
    static int weather_months_left = 0;
    static int last_active = 0;
    static int last_intensity = 0;
    static weather_type last_type = WEATHER_NONE;

    int active;
    int intensity;
    weather_type type;

    if (weather_months_left > 0) {
        // keep last month's weather but reduce intensity
        active = last_active;
        intensity = last_intensity / 2;
        
        if (last_type == WEATHER_RAIN && intensity < 250) {
            intensity = 250;
        } else if (intensity < 1000) {
            intensity = 1000;
        }
        
        type = last_type;
        weather_months_left--;
    } else {
        active = chance_percent(20);
        type = WEATHER_RAIN;

        if (scenario_property_climate() == CLIMATE_DESERT) {
            active = chance_percent(10);
            intensity = 5000;
            type = WEATHER_SAND;
        } else {
            if (month == 10 || month == 11 || month == 0 || month == 1 || month == 2) {
                type = (random_from_stdlib() % 2 == 0) ? WEATHER_RAIN : WEATHER_SNOW;
            }
            if (WEATHER_RAIN == type) {
                intensity = random_between_from_stdlib(250, 1000);
            } else {
                intensity = random_between_from_stdlib(1000, 5000);
            }
        }

        if (active == 0) {
            intensity = 0;
            type = WEATHER_NONE;
        } else {
            weather_months_left = 1;
        }

        last_active = active;
        last_intensity = intensity;
        last_type = type;
    }

    set_weather(active, intensity, type);
}

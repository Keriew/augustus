#include "weather.h"

#include "core/config.h"
#include "core/dir.h"
#include "core/file.h"
#include "core/random.h"
#include "game/settings.h"
#include "graphics/color.h"
#include "graphics/graphics.h"
#include "graphics/screen.h"
#include "graphics/window.h"
#include "scenario/property.h"
#include "sound/device.h"
#include "sound/speech.h"
#include "time.h"
#include "window/city.h"

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
    int displayed_intensity;
    int last_elements_count;
    int last_intensity;
    int last_active;
    int is_sound_playing;
    weather_type displayed_type;
    weather_type last_type;

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
    .overlay_target = 0,
    .overlay_alpha = 0,
    .last_elements_count = 0,
    .last_active = 0,
    .is_sound_playing = 0,
    .last_intensity = 0,
    .displayed_type = WEATHER_NONE,
    .last_type = WEATHER_NONE,
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
            e->length = config_get(CONFIG_WT_RAIN_LENGTH) + random_from_stdlib() % 10;
            e->speed = config_get(CONFIG_WT_RAIN_SPEED) + random_from_stdlib() % 5;
            if (data.weather_config.intensity < 600) {
                e->wind_variation = 0;
            } else {
                e->wind_variation = (random_from_stdlib() % 3) - 1; // -1, 0 or 1
            }
            break;
        case WEATHER_SNOW:
            e->drift_offset = random_from_stdlib() % 100;
            e->speed = config_get(CONFIG_WT_SNOW_SPEED) + random_from_stdlib() % 2;
            e->drift_direction = (random_from_stdlib() % 2 == 0) ? DRIFT_DIRECTION_RIGHT : DRIFT_DIRECTION_LEFT;
            break;
        case WEATHER_SAND:
            e->speed = config_get(CONFIG_WT_SANDSTORM_SPEED) + (random_from_stdlib() % 2);
            e->offset = random_between_from_stdlib(0, 1000);
            break;
    }
}

static void weather_stop(void)
{
    if (data.elements) {
        free(data.elements);
        data.elements = 0;
    }

    data.weather_config.active = 0;
    data.weather_initialized = 0;
    data.last_elements_count = 0;
    data.displayed_intensity = 0;
    data.last_active = 0;
    data.last_type = WEATHER_NONE;
    data.last_intensity = 0;
    data.overlay_target = 0;
    data.overlay_alpha = 0;
    data.displayed_type = WEATHER_NONE;
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
        snprintf(thunder_path, sizeof(thunder_path), ASSETS_DIRECTORY "/Sounds/Thunder%d.ogg", thunder_num);
        sound_speech_play_file(thunder_path);
    }
}

static void update_wind(void)
{
    data.wind_angle += data.wind_speed;
    data.weather_config.dx = ((data.wind_angle / 10) % 5) - 2;
}

static void update_displayed_intensity(void)
{
    int target = data.weather_config.active ? data.weather_config.intensity : 0;
    int duration = 48;
    int diff = abs(target - data.displayed_intensity);

    int speed = (diff > 0) ? (diff + duration - 1) / duration : 1;

    if (data.displayed_intensity < target) {
        data.displayed_intensity += speed;
        if (data.displayed_intensity > target)
            data.displayed_intensity = target;
    } else if (data.displayed_intensity > target) {
        data.displayed_intensity -= speed;
        if (data.displayed_intensity < target)
            data.displayed_intensity = target;
    }
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
        }
    }
}

static void render_weather_overlay(void)
{
    if (data.displayed_type == WEATHER_RAIN && data.last_intensity < 800) {
        return; // no overlay for light rain
    }

    update_overlay_alpha();

    if (data.overlay_alpha == 0) {
        return;
    }

    int alpha_factor = 40;
    if (data.displayed_type == WEATHER_SNOW) {
        alpha_factor = config_get(CONFIG_WT_SNOW_INTENSITY);
    } else if (data.displayed_type == WEATHER_RAIN && data.last_intensity < 900) {
        alpha_factor = config_get(CONFIG_WT_RAIN_INTENSITY);
    } else if (data.displayed_type == WEATHER_SAND) {
        alpha_factor = config_get(CONFIG_WT_SANDSTORM_INTENSITY);
    }

    uint8_t alpha = (uint8_t) (((alpha_factor * data.overlay_alpha) / 100) * 255 / 100);

    // update overlay color based on weather type
    if (data.displayed_type == WEATHER_RAIN) {
        data.overlay_color = COLOR_WEATHER_RAIN;
    } else if (data.displayed_type == WEATHER_SNOW) {
        data.overlay_color = COLOR_WEATHER_SNOW;
    } else if (data.displayed_type == WEATHER_SAND) {
        data.overlay_color = COLOR_WEATHER_SAND;
    }

    graphics_fill_rect(0, 0, screen_width(), screen_height(),
        apply_alpha(data.overlay_color, alpha));
}

static void draw_snow(void)
{
    if (!data.elements || data.displayed_intensity == 0) {
        return;
    }

    int max_particles = data.last_elements_count;
    int count = data.displayed_intensity;
    if (count > max_particles) {
        count = max_particles;
    }
    for (int i = 0; i < count; ++i) {
        if (window_city_is_window_cityview()) {
            int drift = ((data.elements[i].y + data.elements[i].drift_offset) % 10) - 5;
            data.elements[i].x += (drift / 10) * data.elements[i].drift_direction;
            data.elements[i].y += data.elements[i].speed;
        }

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
    if (!data.elements || data.displayed_intensity == 0) {
        return;
    }

    int max_particles = data.last_elements_count;
    int count = data.displayed_intensity;
    if (count > max_particles) {
        count = max_particles;
    }

    for (int i = 0; i < count; ++i) {
        if (window_city_is_window_cityview()) {
            int wave = ((data.elements[i].y + data.elements[i].offset) % 10) - 5;
            data.elements[i].x += data.elements[i].speed + (wave / 10);
        }

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
    if (!data.elements || data.displayed_intensity == 0) {
        return;
    }

    if (data.weather_config.intensity < 600 && window_city_is_window_cityview()) {
        update_wind();
    }

    int wind_strength = abs(data.weather_config.dx);
    int base_speed = 3 + wind_strength + (data.weather_config.intensity / 300);

    int max_particles = data.last_elements_count;
    int count = data.displayed_intensity;
    if (count > max_particles) {
        count = max_particles;
    }

    for (int i = 0; i < count; ++i) {
        int dx;
        if (data.displayed_intensity < 600) {
            dx = data.weather_config.dx;
        } else {
            dx = data.weather_config.dx + data.elements[i].wind_variation;
        }

        graphics_draw_line(
            data.elements[i].x,
            data.elements[i].x + dx * 2,
            data.elements[i].y,
            data.elements[i].y + data.elements[i].length,
            COLOR_WEATHER_DROPS);

        if (window_city_is_window_cityview()) {
            data.elements[i].x += dx;

            int dy = base_speed + data.elements[i].speed + (((data.elements[i].x + data.elements[i].y) % 10) / 10);
            data.elements[i].y += dy;
        }

        if (data.elements[i].y >= screen_height() || data.elements[i].x <= 0 || data.elements[i].x >= screen_width()) {
            init_weather_element(&data.elements[i], data.weather_config.type);
            data.elements[i].y = 0;
        }
    }

    if (data.displayed_intensity > 900) {
        update_lightning();
    }
}

void update_weather()
{

    render_weather_overlay();
    update_displayed_intensity();

    if (!config_get(CONFIG_UI_DRAW_WEATHER)) {
        if (data.is_sound_playing) {
            sound_device_stop_type(SOUND_TYPE_EFFECTS);
            data.is_sound_playing = 0;
        }
        weather_stop();
        return;
    }

    int target_count = data.weather_config.intensity;
    if (target_count != data.last_elements_count && target_count > 0) {
        if (data.elements) {
            free(data.elements);
            data.elements = 0;
        }
        data.elements = malloc(sizeof(weather_element) * target_count);
        for (int i = 0; i < target_count; ++i) {
            init_weather_element(&data.elements[i], data.weather_config.type);
        }
        data.last_elements_count = target_count;
    } else if (target_count == 0 && data.displayed_intensity == 0) {
        if (data.elements) {
            free(data.elements);
            data.elements = 0;
        }
        data.last_elements_count = 0;
    }

    if ((data.weather_config.type == WEATHER_NONE || data.weather_config.active == 0) && data.displayed_intensity == 0) {
        if (data.is_sound_playing) {
            sound_device_stop_type(SOUND_TYPE_EFFECTS);
            data.is_sound_playing = 0;
        }
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
    if (data.displayed_type == WEATHER_SNOW) {
        draw_snow();
        return;
    }

    // SANDSTORM
    if (data.displayed_type == WEATHER_SAND) {
        draw_sandstorm();
        return;
    }

    // RAIN
    if (data.displayed_type == WEATHER_RAIN) {
        draw_rain();
    }

}

void set_weather(int active, int intensity, weather_type type)
{
    data.weather_config.active = active;
    data.weather_config.intensity = intensity;
    data.weather_config.type = type;

    if (active == 0) {
        data.overlay_target = 0;
    } else {
        data.overlay_target = 100;
        data.displayed_type = type;
    }
}

void weather_reset(void)
{
    weather_stop();
    sound_device_stop_type(SOUND_TYPE_EFFECTS);
}

void city_weather_update(int month)
{
    static int weather_months_left = 0;

    int active;
    int intensity;
    weather_type type;

    if (weather_months_left > 0) {
        active = data.last_active;
        intensity = data.last_intensity;
        type = data.last_type;
        weather_months_left--;
    } else {
        active = chance_percent(15);
        type = WEATHER_RAIN;

        if (scenario_property_climate() == CLIMATE_DESERT) {
            active = chance_percent(8);
            intensity = 5000;
            type = WEATHER_SAND;
        } else {
            if (month == 10 || month == 11 || month == 0 || month == 1) {
                if (config_get(CONFIG_UI_WT_ENABLE_SNOW_CENTRAL)) {
                    type = (random_from_stdlib() % 2 == 0) ? WEATHER_RAIN : WEATHER_SNOW;
                } else {
                    type = WEATHER_RAIN;
                }

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
            // Play sounds only if weather is enabled
            if (config_get(CONFIG_UI_DRAW_WEATHER)) {
                if (WEATHER_RAIN == type) {
                    if (intensity > 800) {
                        sound_device_play_file_on_channel_panned(ASSETS_DIRECTORY "/Sounds/HeavyRain.ogg",
                            SOUND_TYPE_EFFECTS, setting_sound(SOUND_TYPE_EFFECTS)->volume, 100, 100, 1);
                    } else {
                        sound_device_play_file_on_channel_panned(ASSETS_DIRECTORY "/Sounds/LightRain.ogg",
                            SOUND_TYPE_EFFECTS, setting_sound(SOUND_TYPE_EFFECTS)->volume, 100, 100, 1);
                    }
                } else if (WEATHER_SAND == type) {
                    sound_device_play_file_on_channel_panned(ASSETS_DIRECTORY "/Sounds/SandStorm.ogg",
                        SOUND_TYPE_EFFECTS, setting_sound(SOUND_TYPE_EFFECTS)->volume, 100, 50, 1);
                } else if (WEATHER_SNOW == type) {
                    sound_device_play_file_on_channel_panned(ASSETS_DIRECTORY "/Sounds/Snow.ogg",
                        SOUND_TYPE_EFFECTS, setting_sound(SOUND_TYPE_EFFECTS)->volume, 100, 100, 1);
                }
                data.is_sound_playing = 1;
            }
        }

        data.last_active = active;
        data.last_intensity = intensity;
        data.last_type = type;
    }

    set_weather(active, intensity, type);
}

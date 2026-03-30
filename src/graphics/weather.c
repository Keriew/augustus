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




static const int PARTICLE_SIZES_RAIN[] = { 1, 8, 15, 23, 30 }; //sets of arbitrary values for a noticeable difference
static const int PARTICLE_SIZES_SAND[] = { 1, 6, 10, 15, 20 }; //denoted as: minmum, small, regular, large, maximum
static const int PARTICLE_SIZES_SNOW[] = { 2, 3, 4, 6, 8 };
static const int PARTICLE_SPEEDS_RAIN[] = { 1, 4, 8, 13, 20 }; //sets of arbitrary values for a noticeable difference
static const int PARTICLE_SPEEDS_SNOW[] = { 1, 2, 4, 6, 10 };  //denoted as: minimum, slow, regular, fast, maximum
static const int PARTICLE_SPEEDS_SAND[] = { 1, 3, 5, 8, 12 };
static const int WEATHER_MAX_DURATION[] = { 1, 3, 6 }; // expressed in months, doesn't apply to thunderstorms

static int get_particle_size(const int *table, int idx)
{
    if (idx < 0) idx = 0;
    if (idx > 4) idx = 4;
    return table[idx];
}

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
    int current_particle_count;
    int last_elements_count;
    int last_intensity;
    int last_active;
    int is_sound_playing;
    int weather_duration_left;
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
    e->x = random_from_stdlib() % screen_pixel_width();
    e->y = random_from_stdlib() % screen_pixel_height();

    switch (type) {
        case WEATHER_RAIN:
            e->length = get_particle_size(PARTICLE_SIZES_RAIN, config_get(CONFIG_WT_RAIN_LENGTH)) + random_from_stdlib() % 10;
            e->speed = get_particle_size(PARTICLE_SPEEDS_RAIN, config_get(CONFIG_WT_RAIN_SPEED)) + random_from_stdlib() % 5;
            if (data.weather_config.intensity < 600) {
                e->wind_variation = 0;
            } else {
                e->wind_variation = (random_from_stdlib() % 3) - 1; // -1, 0 or 1
            }
            break;
        case WEATHER_SNOW:
            e->drift_offset = random_from_stdlib() % 100;
            e->speed = get_particle_size(PARTICLE_SPEEDS_SNOW, config_get(CONFIG_WT_SNOW_SPEED)) + random_from_stdlib() % 4;
            e->drift_direction = (random_from_stdlib() % 2 == 0) ? DRIFT_DIRECTION_RIGHT : DRIFT_DIRECTION_LEFT;
            break;
        case WEATHER_SAND:
            e->speed = get_particle_size(PARTICLE_SPEEDS_SAND, config_get(CONFIG_WT_SANDSTORM_SPEED)) + (random_from_stdlib() % 2);
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
    data.current_particle_count = 0;
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
        graphics_fill_rect(0, 0, screen_pixel_width(), screen_pixel_height(), white_flash);
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

static void update_current_particle_count(void)
{
    int target = data.weather_config.active ? data.weather_config.intensity : 0;
    int duration = 48;
    int diff = abs(target - data.current_particle_count);

    int speed = (diff > 0) ? (diff + duration - 1) / duration : 1;

    if (data.current_particle_count < target) {
        data.current_particle_count += speed;
        if (data.current_particle_count > target)
            data.current_particle_count = target;
    } else if (data.current_particle_count > target) {
        data.current_particle_count -= speed;
        if (data.current_particle_count < target)
            data.current_particle_count = target;
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
    if (data.displayed_type == WEATHER_RAIN && data.current_particle_count < 800) {
        return; // no overlay for light rain
    }

    update_overlay_alpha();

    if (data.overlay_alpha == 0) {
        return;
    }

    int alpha_factor = 40;
    if (data.displayed_type == WEATHER_SNOW) {
        alpha_factor = config_get(CONFIG_WT_SNOW_INTENSITY);
    } else if (data.displayed_type == WEATHER_RAIN && data.current_particle_count > 800) {
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

    graphics_fill_rect(0, 0, screen_pixel_width(), screen_pixel_height(),
        apply_alpha(data.overlay_color, alpha));
}

static void draw_snow(void)
{
    if (!data.elements || data.current_particle_count == 0) {
        return;
    }

    int max_particles = data.last_elements_count;
    int count = data.current_particle_count;
    if (count > max_particles) {
        count = max_particles;
    }
    for (int i = 0; i < count; ++i) {
        if (window_city_is_window_cityview() || window_city_simulated_weather(WEATHER_SNOW)) {
            int drift = ((data.elements[i].y + data.elements[i].drift_offset) % 10) - 5;
            data.elements[i].x += (drift / 10) * data.elements[i].drift_direction;
            data.elements[i].y += data.elements[i].speed;
        }

        int sf_size = get_particle_size(PARTICLE_SIZES_SNOW, config_get(CONFIG_UI_WT_SNOWFLAKE_SIZE));
        int sf_half = sf_size / 2;
        // horizontal arm of cross
        graphics_draw_line(
            data.elements[i].x,
            data.elements[i].x + sf_size,
            data.elements[i].y + sf_half,
            data.elements[i].y + sf_half,
            COLOR_WEATHER_SNOWFLAKE);
        // vertical arm of cross
        graphics_draw_line(
            data.elements[i].x + sf_half,
            data.elements[i].x + sf_half,
            data.elements[i].y,
            data.elements[i].y + sf_size,
            COLOR_WEATHER_SNOWFLAKE);

        if (data.elements[i].y >= screen_pixel_height() ||
            data.elements[i].x <= 0 || data.elements[i].x >= screen_pixel_width()) {
            init_weather_element(&data.elements[i], data.weather_config.type);
            data.elements[i].y = -(int) (random_from_stdlib() % 30);
        }
    }
}

static void draw_sandstorm(void)
{
    if (!data.elements || data.current_particle_count == 0) {
        return;
    }

    int max_particles = data.last_elements_count;
    int count = data.current_particle_count;
    if (count > max_particles) {
        count = max_particles;
    }

    for (int i = 0; i < count; ++i) {
        if (window_city_is_window_cityview() || window_city_simulated_weather(WEATHER_SAND)) {
            int wave = ((data.elements[i].y + data.elements[i].offset) % 10) - 5;
            data.elements[i].x += data.elements[i].speed + (wave / 10);
        }

        int sd_size = get_particle_size(PARTICLE_SIZES_SAND, config_get(CONFIG_UI_WT_SANDSTORM_SIZE));
        graphics_draw_line(
            data.elements[i].x,
            data.elements[i].x + sd_size,
            data.elements[i].y,
            data.elements[i].y + sd_size,
            COLOR_WEATHER_SAND_PARTICLE);

        if (data.elements[i].x > screen_pixel_width()) {
            init_weather_element(&data.elements[i], data.weather_config.type);
            data.elements[i].x = 0;
        }
    }
}

static void draw_rain(void)
{
    if (!data.elements || data.current_particle_count == 0) {
        return;
    }

    if (data.weather_config.intensity < 600 &&
        (window_city_is_window_cityview() || window_city_simulated_weather(WEATHER_RAIN))) {
        update_wind();
    }

    int wind_strength = abs(data.weather_config.dx);
    int base_speed = 3 + wind_strength + (data.weather_config.intensity / 300);

    int max_particles = data.last_elements_count;
    int count = data.current_particle_count;
    if (count > max_particles) {
        count = max_particles;
    }

    for (int i = 0; i < count; ++i) {
        int dx;
        if (data.current_particle_count < 600) {
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

        if (window_city_is_window_cityview() || window_city_simulated_weather(WEATHER_RAIN)) {
            data.elements[i].x += dx;

            int dy = base_speed + data.elements[i].speed + (((data.elements[i].x + data.elements[i].y) % 10) / 10);
            data.elements[i].y += dy;
        }

        if (data.elements[i].y >= screen_pixel_height() ||
            data.elements[i].x <= 0 || data.elements[i].x >= screen_pixel_width()) {
            init_weather_element(&data.elements[i], data.weather_config.type);
            data.elements[i].y = 0;
        }
    }

    if (data.current_particle_count > 900) {
        update_lightning();
    }
}

void update_weather(void)
{

    render_weather_overlay();
    update_current_particle_count();
    if (window_is(WINDOW_CONFIG)) { //preview weather in config menu
        if (config_get(CONFIG_UI_WT_PREVIEW_RAIN)) {
            data.weather_config.type = WEATHER_RAIN;
            data.weather_config.active = 1;
            // Use default intensity for preview
            set_weather(1, 600, WEATHER_RAIN);
        } else if (config_get(CONFIG_UI_WT_PREVIEW_HEAVY_RAIN)) {
            data.weather_config.type = WEATHER_RAIN;
            data.weather_config.active = 1;
            // Use high intensity for heavy rain preview to show lightning
            set_weather(1, 999, WEATHER_RAIN);
        } else if (config_get(CONFIG_UI_WT_PREVIEW_SNOW)) {
            data.weather_config.type = WEATHER_SNOW;
            data.weather_config.active = 1;
            set_weather(1, 3000, WEATHER_SNOW);
        } else if (config_get(CONFIG_UI_WT_PREVIEW_SANDSTORM)) {
            data.weather_config.type = WEATHER_SAND;
            data.weather_config.active = 1;
            set_weather(1, 3000, WEATHER_SAND);
        } else {
            data.weather_config.type = WEATHER_NONE;
            data.weather_config.active = 0;
            weather_stop();
        }
    }

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
        data.weather_initialized = 1;
    } else if (target_count == 0 && data.current_particle_count == 0) {
        if (data.elements) {
            free(data.elements);
            data.elements = 0;
        }
        data.last_elements_count = 0;
    }

    if ((data.weather_config.type == WEATHER_NONE || data.weather_config.active == 0) && data.current_particle_count == 0) {
        if (data.is_sound_playing) {
            sound_device_stop_type(SOUND_TYPE_EFFECTS);
            data.is_sound_playing = 0;
        }
        weather_stop();
        return;
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
    int active;
    int intensity;
    weather_type type;

    if (data.weather_duration_left > 0) {
        active = data.last_active;
        intensity = data.last_intensity;
        type = data.last_type;
        data.weather_duration_left--;
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
            int duration_setting = config_get(CONFIG_UI_WT_WEATHER_DURATION);
            data.weather_duration_left = random_between_from_stdlib(1, WEATHER_MAX_DURATION[duration_setting]);
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

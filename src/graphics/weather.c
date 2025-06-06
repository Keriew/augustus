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

#define MAX_WEATHER_SEGMENTS 10000

static line_segment line_buffer[MAX_WEATHER_SEGMENTS];

typedef struct {
    float x;
    float y;

    // For rain
    int length;
    int speed;
    float wind_variation;

    // For snow
    float drift_offset;
    int drift_direction;

    // For sand
    float offset;
} weather_element;

static struct {
    int weather_initialized;
    int lightning_timer;
    int lightning_cooldown;
    float wind_angle;
    float wind_speed;
    float overlay_alpha;
    float overlay_target;
    int overlay_color;
    int overlay_fadeout;

    weather_element* elements;

    struct {
        int active;
        int intensity;
        int dx;
        int dy;
        int type;
    } weather_config;
} data = {
    .weather_initialized = 0,
    .lightning_timer = 0,
    .lightning_cooldown = 0,
    .wind_angle = 0.0f,
    .wind_speed = 0.05f,
    .overlay_alpha = 0.0f,
    .overlay_target = 1.0f,
    .overlay_fadeout = 0,
    .elements = 0,
    .weather_config = {
        .active = 0,
        .intensity = 200,
        .dx = 1,
        .dy = 5,
        .type = WEATHER_NONE,
    }
};

void init_weather_element(weather_element* e, int type) 
{
    e->x = random_from_stdlib() % screen_width();
    e->y = random_from_stdlib() % screen_height();

    switch (type) {
        case WEATHER_RAIN:
            e->length = 10 + random_from_stdlib() % 10;
            e->speed = 4 + random_from_stdlib() % 5;
            e->wind_variation = ((random_from_stdlib() % 100) / 100.0f - 0.5f) * 1.5f;
            break;
        case WEATHER_SNOW:
            e->drift_offset = (float)(random_from_stdlib() % 100);
            e->speed = 1 + random_from_stdlib() % 2;
            e->drift_direction = (random_from_stdlib() % 2 == 0) ? DRIFT_DIRECTION_RIGHT : DRIFT_DIRECTION_LEFT;
            break;
        case WEATHER_SAND:
            e->speed = 1.5f + (random_from_stdlib() % 100) / 50.0f;
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
    data.weather_config.dx = (int)(2 * sinf(data.wind_angle)); // between -2 & 2
}

static void update_overlay_alpha(void) 
{
    float speed = 0.01f;

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

    float alpha_factor  = 0.4f;
    if (data.weather_config.type == WEATHER_SNOW ||
        (data.weather_config.type == WEATHER_RAIN && data.weather_config.intensity < 900)) {
        alpha_factor = 0.2f;
    }

    // no overlay for light rain
    if (data.weather_config.type == WEATHER_RAIN && data.weather_config.intensity < 800) {
        alpha_factor = 0.0f;
    }

    uint8_t alpha = (uint8_t)(fminf(alpha_factor * data.overlay_alpha, 1.0f) * 255);
    
    // update overlay color based on weather type
    if (data.weather_config.type == WEATHER_RAIN) {
        data.overlay_color = COLOR_RAIN;
    } else if (data.weather_config.type == WEATHER_SNOW) {
        data.overlay_color = COLOR_SNOW;
    } else if (data.weather_config.type == WEATHER_SAND) {
        data.overlay_color = COLOR_SAND;
    }

    graphics_fill_rect(0, 0, screen_width(), screen_height(),
        apply_alpha(data.overlay_color, alpha));
}

static void draw_snow(void)
{
    int count = 0;

    for (int i = 0; i < data.weather_config.intensity; ++i) {
        float drift = sinf((data.elements[i].y + data.elements[i].drift_offset) * 0.02f);
        data.elements[i].x += drift * 0.5f * data.elements[i].drift_direction;
        data.elements[i].y += (int)(data.elements[i].speed);

        line_buffer[count++] = (line_segment){
            .x1 = data.elements[i].x,
            .y1 = data.elements[i].y,
            .x2 = data.elements[i].x + 1,
            .y2 = data.elements[i].y
        };

        line_buffer[count++] = (line_segment){
            .x1 = data.elements[i].x,
            .y1 = data.elements[i].y,
            .x2 = data.elements[i].x,
            .y2 = data.elements[i].y + 1
        };
        
        if (data.elements[i].y >= screen_height() || data.elements[i].x <= 0 || data.elements[i].x >= screen_width()) {
            init_weather_element(&data.elements[i], data.weather_config.type);
            data.elements[i].y = 0;
        }
    }

    graphics_draw_lines(line_buffer, count, COLOR_SNOWFLAKE);
}

static void draw_sandstorm(void) 
{

    int count = 0;

    for (int i = 0; i < data.weather_config.intensity; ++i) {
        float wave = sinf((data.elements[i].y + data.elements[i].offset) * 0.03f);
        data.elements[i].x += data.elements[i].speed + wave;

        line_buffer[count++] = (line_segment){
            .x1 = (int)data.elements[i].x,
            .y1 = (int)data.elements[i].y,
            .x2 = (int)data.elements[i].x + 1,
            .y2 = (int)data.elements[i].y + 1
        };

        if (data.elements[i].x > screen_width()) {
            init_weather_element(&data.elements[i], data.weather_config.type);
            data.elements[i].x = 0;
        }
    }
    graphics_draw_lines(line_buffer, count, COLOR_SAND_PARTICLE);
}

static void draw_rain(void) 
{
    if (data.weather_config.intensity < 500) {
        update_wind();
    }

    int wind_strength = abs(data.weather_config.dx);
    int base_speed = 3 + wind_strength + (data.weather_config.intensity / 300);

    int count = 0;

    for (int i = 0; i < data.weather_config.intensity; ++i) {
        float dx = data.weather_config.dx + data.elements[i].wind_variation;

        line_buffer[count].x1 = (int)(data.elements[i].x);
        line_buffer[count].y1 = (int)(data.elements[i].y);
        line_buffer[count].x2 = (int)(data.elements[i].x + dx * 2);
        line_buffer[count].y2 = (int)(data.elements[i].y + data.elements[i].length);
        count++;

        data.elements[i].x += dx;

        float dy = base_speed + data.elements[i].speed + (sinf((data.elements[i].x + data.elements[i].y) * 0.05f) * 0.5f);
        data.elements[i].y += dy;

        if (data.elements[i].y >= screen_height() || data.elements[i].x <= 0 || data.elements[i].x >= screen_width()) {
            init_weather_element(&data.elements[i], data.weather_config.type);
            data.elements[i].y = 0;
        }
    }

    graphics_draw_lines(line_buffer, count, COLOR_DROPS);

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
        data.overlay_target = 0.0f;
    } else {
         data.overlay_target = 1.0f;
    }
}

void city_weather_update(int month) 
{
    int active = chance_percent(20);
    int intensity;
    weather_type type = WEATHER_RAIN;

    if (scenario_property_climate() == CLIMATE_DESERT) {
        active = chance_percent(10);
        intensity = 5000;
        type = WEATHER_SAND;
    } else {
        if (month == 10 || month == 11 || month == 0 || month == 1 || month == 2) {
            type = (random_from_stdlib() % 2 == 0) ? WEATHER_RAIN : WEATHER_SNOW;
        }
        if (WEATHER_RAIN == type) {
            intensity = random_between_from_stdlib(100, 1000);
        } else {
            intensity = random_between_from_stdlib(1000, 5000);
        }
    }        

    if (active == 0) {
        intensity = 0;
        type = WEATHER_NONE;
    }
    
    set_weather(active, intensity, type);
}

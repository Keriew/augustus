#include "weather.h"

#include "core/config.h"
#include "core/dir.h"
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
    int length;
    int speed;
    float wind_variation; 
} drop;

typedef struct {
    int x;
    int y;
    float speed;
    float drift_offset;
    int drift_direction;
} snowflake;

typedef struct {
    float x;
    float y;
    float speed;
    float offset;
} sand_particle;

drop *drops = 0;
snowflake *flakes = 0;
sand_particle *sand_particles = 0;

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
    .weather_config = {
        .active = 0,
        .intensity = 200,
        .dx = 1,
        .dy = 5,
        .type = WEATHER_NONE,
    }
};

void init_drop(drop *drop) {
    drop->x = random_from_stdlib() % screen_width();
    drop->y = random_from_stdlib() % screen_height();
    drop->length = 10 + random_from_stdlib() % 10;
    drop->speed = 4 + random_from_stdlib() % 5;
    drop->wind_variation = ((random_from_stdlib() % 100) / 100.0f - 0.5f) * 1.5f;
}

void init_snowflake(snowflake *flake) {
    flake->x = random_from_stdlib() % screen_width();
    flake->y = random_from_stdlib() % screen_height();
    flake->drift_offset = (float)(random_from_stdlib() % 100);
    flake->speed = 1 + random_from_stdlib() % 2;
    flake->drift_direction = (random_from_stdlib() % 2 == 0) ? DRIFT_DIRECTION_RIGHT : DRIFT_DIRECTION_LEFT;
}

void init_sand_particle(sand_particle *sand) {
    sand->x = random_from_stdlib() % screen_width();
    sand->y = random_from_stdlib() % screen_height();
    sand->speed = 1.5f + (random_from_stdlib() % 100) / 50.0f;
    sand->offset = random_between_from_stdlib(0, 1000);
}

void weather_stop(void) {
    data.weather_config.active = 0;
    if (drops) {
        free(drops);
        drops = NULL;
    }
    if (flakes) {
        free(flakes);
        flakes = NULL;
    }
    if (sand_particles) {
        free(sand_particles);
        sand_particles = NULL;
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
        uint32_t white_flash = apply_alpha(COLOR_WHITE, 128);
        graphics_fill_rect(0, 0, screen_width(), screen_height(), white_flash);
        data.lightning_timer--;
    }

    if (data.lightning_timer == 5) {        
        char thunder_path[128];
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
    for (int i = 0; i < data.weather_config.intensity; ++i) {
        float drift = sinf((flakes[i].y + flakes[i].drift_offset) * 0.02f);
        flakes[i].x += drift * 0.5f * flakes[i].drift_direction;
        flakes[i].y += (int)(flakes[i].speed);

        graphics_draw_line(flakes[i].x, flakes[i].x + 1, flakes[i].y, flakes[i].y, COLOR_SNOWFLAKE);
        graphics_draw_line(flakes[i].x, flakes[i].x, flakes[i].y, flakes[i].y + 1, COLOR_SNOWFLAKE);
        
        if (flakes[i].y >= screen_height() || flakes[i].x <= 0 || flakes[i].x >= screen_width()) {
            init_snowflake(&flakes[i]);
            flakes[i].y = 0;
        }
    }
}

static void draw_sandstorm(void) 
{
    for (int i = 0; i < data.weather_config.intensity; ++i) {
        float wave = sinf((sand_particles[i].y + sand_particles[i].offset) * 0.03f);
        sand_particles[i].x += sand_particles[i].speed + wave;

        graphics_draw_line(
            (int)sand_particles[i].x,
            (int)sand_particles[i].x + 1,
            (int)sand_particles[i].y,
            (int)sand_particles[i].y + 1,
            COLOR_SAND_PARTICLE);

        if (sand_particles[i].x > screen_width()) {
            init_sand_particle(&sand_particles[i]);
            sand_particles[i].x = 0;
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
        float dx = data.weather_config.dx + drops[i].wind_variation;
        graphics_draw_line(
            (int)(drops[i].x), 
            (int)(drops[i].x + dx * 2),
            (int)(drops[i].y),
            (int)(drops[i].y + drops[i].length),
            COLOR_DROPS);

        drops[i].x += dx;

        float dy = base_speed + drops[i].speed + (sinf((drops[i].x + drops[i].y) * 0.05f) * 0.5f);
        drops[i].y += dy;

        if (drops[i].y >= screen_height() || drops[i].x <= 0 || drops[i].x >= screen_width()) {
            init_drop(&drops[i]);
            drops[i].y = 0;
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
        if (data.weather_config.type == WEATHER_RAIN) {
            drops = malloc(sizeof(drop) * data.weather_config.intensity);
            for (int i = 0; i < data.weather_config.intensity; ++i) {
                init_drop(&drops[i]);
            }
        } else if (data.weather_config.type == WEATHER_SNOW) {
            flakes = malloc(sizeof(snowflake) * data.weather_config.intensity);
            for (int i = 0; i < data.weather_config.intensity; ++i) {
                init_snowflake(&flakes[i]);                
            }
        } else if (data.weather_config.type == WEATHER_SAND) {
            sand_particles = malloc(sizeof(sand_particle) * data.weather_config.intensity);
            for (int i = 0; i < data.weather_config.intensity; ++i) {
                init_sand_particle(&sand_particles[i]);     
            }   
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

void set_weather(int active, int intensity, weather_type type) 
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

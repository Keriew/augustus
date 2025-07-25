#include "sound/device.h"

#include "core/calc.h"
#include "core/config.h"
#include "core/file.h"
#include "core/log.h"
#include "core/time.h"
#include "game/campaign.h"
#include "game/settings.h"
#include "platform/platform.h"
#include "platform/vita/vita.h"

#include "SDL.h"
#include "SDL_mixer.h"

#include <stdlib.h>
#include <string.h>

#ifdef __vita__
#include <psp2/io/fcntl.h>
#endif

#define AUDIO_RATE 22050
#define AUDIO_FORMAT AUDIO_S16
#define AUDIO_CHANNELS 2
#define AUDIO_BUFFERS 1024

#define NO_CHANNEL -1

#if SDL_VERSION_ATLEAST(2, 0, 7)
#define USE_SDL_AUDIOSTREAM
#endif
#define HAS_AUDIOSTREAM() (platform_sdl_version_at_least(2, 0, 7))

#ifdef __vita__
static struct {
    char filename[FILE_NAME_MAX];
    char *buffer;
    int size;
} vita_music_data;
#endif

typedef struct {
    char filename[FILE_NAME_MAX];
    Mix_Chunk *chunk;
    time_millis last_played;
} sound_channel;

static struct {
    int initialized;
    uint8_t *custom_music;
    Mix_Music *music;
    sound_channel *channels;
    unsigned int total_channels;
    void (*sound_finished_callback)(sound_type);
} data;

static struct {
    int start;
    int total;
} sound_type_to_channels[SOUND_TYPE_MAX] = {
    [SOUND_TYPE_SPEECH]  = { .total = 1  },
    [SOUND_TYPE_EFFECTS] = { .total = 10 },
    [SOUND_TYPE_CITY]    = { .total = 5  }
};

static struct {
    SDL_AudioFormat format;
    SDL_AudioFormat dst_format;
#ifdef USE_SDL_AUDIOSTREAM
    SDL_AudioStream *stream;
    int use_audiostream;
#endif
    SDL_AudioCVT cvt;
    unsigned char *buffer;
    int buffer_size;
    int cur_read;
    int cur_write;
} custom_music;

static int percentage_to_volume(int percentage)
{
    int master_percentage = config_get(CONFIG_GENERAL_MASTER_VOLUME);
    return calc_adjust_with_percentage(percentage, master_percentage) * SDL_MIX_MAXVOLUME / 100;
}

static void init_channels(void)
{
    if (data.initialized) {
        return;
    }

    for (sound_type type = 0; type < SOUND_TYPE_MAX; type++) {
        if (!sound_type_to_channels[type].total) {
            continue;
        }
        sound_type_to_channels[type].start = data.total_channels;
        data.total_channels += sound_type_to_channels[type].total;
    }

    data.channels = malloc(sizeof(sound_channel) * data.total_channels);

    if (!data.channels) {
        log_error("Unable to create sound channels. The game may crash.", 0, 0);
        data.total_channels = 0;
        return;
    }

    memset(data.channels, 0, sizeof(sound_channel) * data.total_channels);
    data.initialized = 1;
}

void sound_device_open(void)
{
#ifdef USE_SDL_AUDIOSTREAM
    custom_music.use_audiostream = HAS_AUDIOSTREAM();
#endif
    // Windows: use directsound by default, as wasapi has issues
#ifdef __WINDOWS__
    SDL_AudioInit("directsound");
#endif
    if (0 == Mix_OpenAudio(AUDIO_RATE, AUDIO_FORMAT, AUDIO_CHANNELS, AUDIO_BUFFERS)) {
        SDL_Log("Using default audio driver: %s", SDL_GetCurrentAudioDriver());
        init_channels();
        return;
    }
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Sound failed to initialize using default driver: %s", Mix_GetError());
    // Try to work around SDL choosing the wrong driver on Windows sometimes
    for (int i = 0; i < SDL_GetNumAudioDrivers(); i++) {
        const char *driver_name = SDL_GetAudioDriver(i);
        if (SDL_strcmp(driver_name, "disk") == 0 || SDL_strcmp(driver_name, "dummy") == 0) {
            // Skip "write-to-disk" and dummy drivers
            continue;
        }
        if (0 == SDL_AudioInit(driver_name) &&
            0 == Mix_OpenAudio(AUDIO_RATE, AUDIO_FORMAT, AUDIO_CHANNELS, AUDIO_BUFFERS)) {
            SDL_Log("Using audio driver: %s", driver_name);
            init_channels();
            return;
        } else {
            SDL_Log("Not using audio driver %s, reason: %s", driver_name, SDL_GetError());
        }
    }
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Sound failed to initialize: %s", Mix_GetError());
    int max = SDL_GetNumAudioDevices(0);
    SDL_Log("Number of audio devices: %d", max);
    for (int i = 0; i < max; i++) {
        SDL_Log("Audio device: %s", SDL_GetAudioDeviceName(i, 0));
    }
}

static void stop_channel(int channel)
{
    if (!data.initialized) {
        return;
    }
    sound_channel *ch = &data.channels[channel];
    if (ch->chunk) {
        Mix_HaltChannel(channel);
        Mix_FreeChunk(ch->chunk);
        ch->chunk = 0;
    }
    ch->filename[0] = 0;
    ch->last_played = 0;
}

void sound_device_close(void)
{
    if (!data.initialized) {
        return;
    }
    for (int i = 0; i < data.total_channels; i++) {
        stop_channel(i);
    }
    Mix_ChannelFinished(NULL);
    Mix_CloseAudio();
    free(data.channels);
    data.channels = 0;
    data.total_channels = 0;
    data.initialized = 0;
}

static Mix_Chunk *load_chunk(const char *filename)
{
    if (!filename || !*filename) {
        return 0;
    }
    size_t size;
    uint8_t *audio_data = game_campaign_load_file(filename, &size);
    if (audio_data) {
        SDL_RWops *sdl_memory = SDL_RWFromMem(audio_data, (int) size);
        return Mix_LoadWAV_RW(sdl_memory, SDL_TRUE);
    }
    filename = dir_get_file(filename, MAY_BE_LOCALIZED);
    if (!filename) {
        return 0;
    }
#if defined(__vita__) || defined(__ANDROID__)
    FILE *fp = file_open(filename, "rb");
    if (!fp) {
        return NULL;
    }
    SDL_RWops *sdl_fp = SDL_RWFromFP(fp, SDL_TRUE);
    return Mix_LoadWAV_RW(sdl_fp, 1);
#else
    return Mix_LoadWAV(filename);
#endif
}

static void callback_for_audio_finished(int channel)
{
    if (!data.sound_finished_callback) {
        return;
    }
    for (sound_type type = SOUND_TYPE_MIN; type < SOUND_TYPE_MAX; type++) {
        if (channel >= sound_type_to_channels[type].start &&
            channel - sound_type_to_channels[type].start < sound_type_to_channels[type].total) {
            data.sound_finished_callback(type);
            return;
        }
    }
    data.sound_finished_callback(NO_CHANNEL);
}

void sound_device_init_channels(void)
{
    if (!data.initialized) {
        return;
    }
    Mix_AllocateChannels(data.total_channels);
    log_info("Loading audio files", 0, 0);
    for (int i = 0; i < data.total_channels; i++) {
        data.channels[i].chunk = 0;
        data.channels[i].filename[0] = 0;
        data.channels[i].last_played = 0;
    }
    Mix_ChannelFinished(callback_for_audio_finished);
}

static int get_channel_for_filename(const char *filename, sound_type type)
{
    for (int i = 0; i < sound_type_to_channels[type].total; i++) {
        int channel = i + sound_type_to_channels[type].start;
        if (strcmp(filename, data.channels[channel].filename) == 0) {
            return channel;
        }
    }
    return NO_CHANNEL;
}

int sound_device_is_file_playing_on_channel(const char *filename, sound_type type)
{
    if (!data.initialized || !filename) {
        return 0;
    }
    int channel = get_channel_for_filename(filename, type);
    if (channel == NO_CHANNEL) {
        return 0;
    }
    return Mix_Playing(channel);
}

void sound_device_set_music_volume(int volume_pct)
{
    if (!data.initialized) {
        return;
    }
    Mix_VolumeMusic(percentage_to_volume(volume_pct));
}

void sound_device_set_volume_for_type(sound_type type, int volume_pct)
{
    if (!data.initialized) {
        return;
    }
    for (int i = 0; i < sound_type_to_channels[type].total; i++) {
        int channel = i + sound_type_to_channels[type].start;
        if (data.channels[channel].chunk) {
            Mix_VolumeChunk(data.channels[channel].chunk, percentage_to_volume(volume_pct));
        }
    }
}

#ifdef __vita__
static void load_music_for_vita(const char *filename)
{
    if (vita_music_data.buffer) {
        if (strcmp(filename, vita_music_data.filename) == 0) {
            return;
        }
        free(vita_music_data.buffer);
        vita_music_data.buffer = 0;
    }
    snprintf(vita_music_data.filename, FILE_NAME_MAX, "%s", filename);
    FILE *fp = file_open(filename, "rb");
    if (!fp) {
        return;
    }
    fseek(fp, 0, SEEK_END);
    vita_music_data.size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    vita_music_data.buffer = malloc(sizeof(char) * vita_music_data.size);
    if (!vita_music_data.buffer) {
        file_close(fp);
        return;
    }
    fread(vita_music_data.buffer, sizeof(char), (size_t) vita_music_data.size, fp);
    file_close(fp);
}
#endif

int sound_device_play_music(const char *filename, int volume_pct, int loop)
{
    if (data.initialized && config_get(CONFIG_GENERAL_ENABLE_AUDIO)) {
        sound_device_stop_music();
        if (!filename) {
            return 0;
        }
        size_t size;
        data.custom_music = game_campaign_load_file(filename, &size);
        if (data.custom_music) {
            SDL_RWops *sdl_music = SDL_RWFromMem(data.custom_music, (int) size);
            data.music = Mix_LoadMUS_RW(sdl_music, SDL_TRUE);
        } else {
#ifdef __vita__
            load_music_for_vita(filename);
            if (!vita_music_data.buffer) {
                return 0;
            }
            SDL_RWops *sdl_music = SDL_RWFromMem(vita_music_data.buffer, vita_music_data.size);
            data.music = Mix_LoadMUS_RW(sdl_music, SDL_TRUE);
#elif defined(__ANDROID__)
            FILE *fp = file_open(filename, "rb");
            if (!fp) {
                return 0;
            }
            SDL_RWops *sdl_fp = SDL_RWFromFP(fp, SDL_TRUE);
            data.music = Mix_LoadMUS_RW(sdl_fp, SDL_TRUE);
#else
            data.music = Mix_LoadMUS(filename);
#endif
        }
        if (!data.music) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "Error opening music file '%s'. Reason: %s", filename, Mix_GetError());
        } else {
            if (Mix_PlayMusic(data.music, loop ? -1 : 0) == -1) {
                data.music = 0;
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Error playing music file '%s'. Reason: %s", filename, Mix_GetError());
            } else {
                sound_device_set_music_volume(volume_pct);
            }
        }
        return data.music ? 1 : 0;
    }
    return 0;
}
static void (*track_finished_callback)(void) = NULL;

static void internal_on_track_finished(void)
{
    if (track_finished_callback) {
        track_finished_callback();
    }
}

int sound_device_play_track(const char *filename, int volume_pct, void (*on_finish)(void))
{
    if (!data.initialized || !config_get(CONFIG_GENERAL_ENABLE_AUDIO)) {
        return 0;
    }

    sound_device_stop_music();  // stop any current track

    if (!filename) {
        return 0;
    }

    size_t size;
    data.custom_music = game_campaign_load_file(filename, &size);
    if (data.custom_music) {
        SDL_RWops *sdl_music = SDL_RWFromMem(data.custom_music, (int) size);
        data.music = Mix_LoadMUS_RW(sdl_music, SDL_TRUE);
    } else {
        data.music = Mix_LoadMUS(filename);
    }

    if (!data.music) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Error opening music file '%s'. Reason: %s", filename, Mix_GetError());
        return 0;
    }

    track_finished_callback = on_finish;
    Mix_HookMusicFinished(internal_on_track_finished);

    if (Mix_PlayMusic(data.music, 0) == -1) {
        data.music = 0;
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Error playing music file '%s'. Reason: %s", filename, Mix_GetError());
        return 0;
    }

    sound_device_set_music_volume(volume_pct);
    return 1;
}

static int get_available_channel(sound_type type)
{
    int oldest_channel = NO_CHANNEL;
    for (int i = 0; i < sound_type_to_channels[type].total; i++) {
        int channel = i + sound_type_to_channels[type].start;
        if (!Mix_Playing(channel)) {
            if (oldest_channel == NO_CHANNEL ||
                data.channels[channel].last_played < data.channels[oldest_channel].last_played) {
                oldest_channel = channel;
            }
        }
    }
    return oldest_channel;
}

int sound_device_play_file_on_channel_panned(const char *filename, sound_type type,
    int volume_pct, int left_pct, int right_pct, int loop)
{
    if (!data.initialized || !config_get(CONFIG_GENERAL_ENABLE_AUDIO)) {
        return 0;
    }

    if (!setting_sound_is_enabled(type)) {
        return 0;
    }
    
    int channel = get_channel_for_filename(filename, type);
    if (channel == NO_CHANNEL) {
        channel = get_available_channel(type);
        if (channel == NO_CHANNEL) {
            return 0;
        }
        stop_channel(channel);
        data.channels[channel].chunk = load_chunk(filename);
        if (!data.channels[channel].chunk) {
            return 0;
        }
        snprintf(data.channels[channel].filename, FILE_NAME_MAX, "%s", filename);
    }
    Mix_SetPanning(channel, left_pct * 255 / 100, right_pct * 255 / 100);
    Mix_VolumeChunk(data.channels[channel].chunk, percentage_to_volume(volume_pct));
    int result = Mix_PlayChannel(channel, data.channels[channel].chunk, loop ? -1 : 0); // -1 = loop
    if (result == -1) {
        return 0;
    }
    data.channels[channel].last_played = time_get_millis();
    return 1;
}

int sound_device_play_file_on_channel(const char *filename, sound_type type, int volume_pct)
{
    return sound_device_play_file_on_channel_panned(filename, type, volume_pct, 100, 100, 0);
}

void sound_device_on_audio_finished(void (*callback)(sound_type))
{
    if (!data.initialized) {
        return;
    }
    data.sound_finished_callback = callback;
}

void sound_device_fadeout_music(int milisseconds)
{
    if (!data.initialized) {
        return;
    }
    Mix_FadeOutMusic(milisseconds);
}

int sound_device_pause_music(void)
{
    if (data.initialized && Mix_PlayingMusic()) {
        Mix_PauseMusic();
        return 1;
    }
    return 0;
}

int sound_device_resume_music(void)
{
    if (data.initialized && Mix_PausedMusic()) {
        Mix_ResumeMusic();
        SDL_Log("Resuming paused music.");
        return 1;
    }
    return 0;
}

void sound_device_stop_music(void)
{
    if (data.initialized) {
        if (data.music) {
            Mix_HaltMusic();
            Mix_FreeMusic(data.music);
            data.music = 0;
        }
        free(data.custom_music);
        data.custom_music = 0;
    }
}

void sound_device_stop_type(sound_type type)
{
    for (int i = 0; i < sound_type_to_channels[type].total; i++) {
        stop_channel(i + sound_type_to_channels[type].start);
    }
}

static void free_custom_audio_stream(void)
{
#ifdef USE_SDL_AUDIOSTREAM
    if (custom_music.use_audiostream) {
        if (custom_music.stream) {
            SDL_FreeAudioStream(custom_music.stream);
            custom_music.stream = 0;
        }
        return;
    }
#endif

    if (custom_music.buffer) {
        free(custom_music.buffer);
        custom_music.buffer = 0;
    }
}

static int create_custom_audio_stream(SDL_AudioFormat src_format, Uint8 src_channels, int src_rate,
    SDL_AudioFormat dst_format, Uint8 dst_channels, int dst_rate)
{
    free_custom_audio_stream();

    custom_music.dst_format = dst_format;

#ifdef USE_SDL_AUDIOSTREAM
    if (custom_music.use_audiostream) {
        custom_music.stream = SDL_NewAudioStream(
            src_format, src_channels, src_rate,
            dst_format, dst_channels, dst_rate
        );
        return custom_music.stream != 0;
    }
#endif

    int result = SDL_BuildAudioCVT(
        &custom_music.cvt, src_format, src_channels, src_rate,
        dst_format, dst_channels, dst_rate
    );
    if (result < 0) {
        return 0;
    }

    // Allocate buffer large enough for 2 seconds of 16-bit audio
    custom_music.buffer_size = dst_rate * dst_channels * 2 * 2;
    custom_music.buffer = malloc(custom_music.buffer_size);
    if (!custom_music.buffer) {
        return 0;
    }
    custom_music.cur_read = 0;
    custom_music.cur_write = 0;
    return 1;
}

static int custom_audio_stream_active(void)
{
#ifdef USE_SDL_AUDIOSTREAM
    if (custom_music.use_audiostream) {
        return custom_music.stream != 0;
    }
#endif
    return custom_music.buffer != 0;
}

static int put_custom_audio_stream(const void *audio_data, int len)
{
    if (!audio_data || len <= 0 || !custom_audio_stream_active()) {
        return 0;
    }

#ifdef USE_SDL_AUDIOSTREAM
    if (custom_music.use_audiostream) {
        return SDL_AudioStreamPut(custom_music.stream, audio_data, len) == 0;
    }
#endif

    // Convert audio to SDL format
    custom_music.cvt.buf = (Uint8 *) malloc((size_t) (len * custom_music.cvt.len_mult));
    if (!custom_music.cvt.buf) {
        return 0;
    }
    memcpy(custom_music.cvt.buf, audio_data, len);
    custom_music.cvt.len = len;
    SDL_ConvertAudio(&custom_music.cvt);
    int converted_len = custom_music.cvt.len_cvt;

    // Copy data to circular buffer
    if (converted_len + custom_music.cur_write <= custom_music.buffer_size) {
        memcpy(&custom_music.buffer[custom_music.cur_write], custom_music.cvt.buf, converted_len);
    } else {
        int end_len = custom_music.buffer_size - custom_music.cur_write;
        memcpy(&custom_music.buffer[custom_music.cur_write], custom_music.cvt.buf, end_len);
        memcpy(custom_music.buffer, &custom_music.cvt.buf[end_len], converted_len - end_len);
    }
    custom_music.cur_write = (custom_music.cur_write + converted_len) % custom_music.buffer_size;

    // Clean up
    free(custom_music.cvt.buf);
    custom_music.cvt.buf = 0;
    custom_music.cvt.len = 0;

    return 1;
}

static void custom_music_callback(void *dummy, Uint8 *dst, int len)
{
    // Write silence
    memset(dst, 0, len);

    int volume = config_get(CONFIG_GENERAL_ENABLE_AUDIO) && config_get(CONFIG_GENERAL_ENABLE_VIDEO_SOUND) ?
        percentage_to_volume(config_get(CONFIG_GENERAL_VIDEO_VOLUME)) : 0;

    if (!dst || len <= 0 || volume == 0 || !custom_audio_stream_active()) {
        return;
    }
    int bytes_copied = 0;

    // Mix audio to sound effect volume
    Uint8 *mix_buffer = (Uint8 *) malloc(len);
    if (!mix_buffer) {
        return;
    }
    memset(mix_buffer, 0, len);

#ifdef USE_SDL_AUDIOSTREAM
    if (custom_music.use_audiostream) {
        bytes_copied = SDL_AudioStreamGet(custom_music.stream, mix_buffer, len);
        if (bytes_copied <= 0) {
            free(mix_buffer);
            return;
        }
    } else {
#endif
        if (custom_music.cur_read < custom_music.cur_write) {
            int bytes_available = custom_music.cur_write - custom_music.cur_read;
            int bytes_to_copy = bytes_available < len ? bytes_available : len;
            memcpy(mix_buffer, &custom_music.buffer[custom_music.cur_read], bytes_to_copy);
            bytes_copied = bytes_to_copy;
        } else {
            int bytes_available = custom_music.buffer_size - custom_music.cur_read;
            int bytes_to_copy = bytes_available < len ? bytes_available : len;
            memcpy(mix_buffer, &custom_music.buffer[custom_music.cur_read], bytes_to_copy);
            bytes_copied = bytes_to_copy;
            if (bytes_copied < len) {
                int second_part_len = len - bytes_copied;
                bytes_available = custom_music.cur_write;
                bytes_to_copy = bytes_available < second_part_len ? bytes_available : second_part_len;
                memcpy(&mix_buffer[bytes_copied], custom_music.buffer, bytes_to_copy);
                bytes_copied += bytes_to_copy;
            }
        }
        custom_music.cur_read = (custom_music.cur_read + bytes_copied) % custom_music.buffer_size;
#ifdef USE_SDL_AUDIOSTREAM
    }
#endif

    SDL_MixAudioFormat(dst, mix_buffer, custom_music.dst_format, bytes_copied, volume);
    free(mix_buffer);
}

void sound_device_use_custom_music_player(int bitdepth, int num_channels, int rate, const void *audio_data, int len)
{
    if (!data.initialized) {
        return;
    }
    SDL_AudioFormat format;
    if (bitdepth == 8) {
        format = AUDIO_U8;
    } else if (bitdepth == 16) {
        format = AUDIO_S16SYS;
    } else if (bitdepth == 32) {
        format = AUDIO_F32;
    } else {
        log_error("Custom music bitdepth not supported:", 0, bitdepth);
        return;
    }
    int device_rate;
    Uint16 device_format;
    int device_channels;
    Mix_QuerySpec(&device_rate, &device_format, &device_channels);
    custom_music.format = format;

    int result = create_custom_audio_stream(
        format, num_channels, rate,
        device_format, device_channels, device_rate
    );
    if (!result) {
        return;
    }

    sound_device_write_custom_music_data(audio_data, len);

    Mix_HookMusic(custom_music_callback, 0);
}

void sound_device_write_custom_music_data(const void *audio_data, int len)
{
    if (!data.initialized || !audio_data || len <= 0 || !custom_audio_stream_active()) {
        return;
    }
    put_custom_audio_stream(audio_data, len);
}

void sound_device_use_default_music_player(void)
{
    if (!data.initialized) {
        return;
    }
    Mix_HookMusic(0, 0);
    free_custom_audio_stream();
}

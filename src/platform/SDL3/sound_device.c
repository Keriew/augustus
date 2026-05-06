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

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <stdlib.h>
#include <string.h>

#ifdef __vita__
#include <psp2/io/fcntl.h>
#endif

#define AUDIO_RATE 22050
#define AUDIO_FORMAT SDL_AUDIO_S16LE
#define AUDIO_CHANNELS 2
#define AUDIO_BUFFERS 1024

#define NO_CHANNEL -1

#ifdef __vita__
static struct {
    char filename[FILE_NAME_MAX];
    char *buffer;
    int size;
} vita_music_data;
#endif

typedef struct {
    char filename[FILE_NAME_MAX];
    MIX_Track *track;
    time_millis last_played;
    sound_type type;
} sound_channel;

static struct {
    MIX_Mixer *mixer;
    int initialized;
    struct {
        MIX_Track *track;
        SDL_AudioStream *stream;
    } custom_music;
    sound_channel *channels;
    unsigned int total_channels;
    void (*sound_finished_callback)(sound_type);
    void (*music_finished_callback)(void);
} data;

static struct {
    int start;
    int total;
} sound_type_to_channels[SOUND_TYPE_MAX] = {
    [SOUND_TYPE_SPEECH]  = { .total = 1  },
    [SOUND_TYPE_EFFECTS] = { .total = 10 },
    [SOUND_TYPE_CITY]    = { .total = 5  },
    [SOUND_TYPE_MUSIC]   = { .total = 1  },
};

static float percentage_to_volume(int percentage)
{
    int master_percentage = config_get(CONFIG_GENERAL_MASTER_VOLUME);
    return calc_adjust_with_percentage(percentage, master_percentage) / 100.0f;
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

    data.channels[sound_type_to_channels[SOUND_TYPE_MUSIC].start].type = SOUND_TYPE_MUSIC;
    data.initialized = 1;
}

void sound_device_open(void)
{
    if (!MIX_Init()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize SDL_mixer: %s", SDL_GetError());
        return;
    }

    SDL_AudioSpec desired_spec = { AUDIO_FORMAT, AUDIO_CHANNELS, AUDIO_RATE };

    data.mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired_spec);

    if (data.mixer) {
        SDL_Log("Using default audio driver: %s", SDL_GetCurrentAudioDriver());
        init_channels();
        return;
    }
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Sound failed to initialize using default driver: %s", SDL_GetError());
    // Try to work around SDL choosing the wrong driver on Windows sometimes
    for (int i = 0; i < SDL_GetNumAudioDrivers(); i++) {
        const char *driver_name = SDL_GetAudioDriver(i);
        if (SDL_strcmp(driver_name, "disk") == 0 || SDL_strcmp(driver_name, "dummy") == 0) {
            // Skip "write-to-disk" and dummy drivers
            continue;
        }

        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, driver_name);

        if (SDL_InitSubSystem(SDL_INIT_AUDIO) &&
            (data.mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired_spec))) {
            SDL_Log("Using audio driver: %s", driver_name);
            init_channels();
            return;
        } else {
            SDL_Log("Not using audio driver %s, reason: %s", driver_name, SDL_GetError());
        }
    }
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Sound failed to initialize: %s", SDL_GetError());
}

static void stop_channel(int channel)
{
    if (!data.initialized) {
        return;
    }
    sound_channel *ch = &data.channels[channel];
    if (ch->track) {
        MIX_DestroyTrack(ch->track);
        ch->track = 0;
    }
    ch->filename[0] = 0;
    ch->last_played = 0;
}

static uint8_t *load_file(const char *filename, size_t *size)
{
    filename = dir_get_file(filename, MAY_BE_LOCALIZED);
    if (!filename) {
        return 0;
    }
    FILE *fp = file_open(filename, "rb");
    if (!fp) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open audio file '%s'. Reason: %s",
            filename, SDL_GetError());
        return 0;
    }
    fseek(fp, 0, SEEK_END);
    *size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    uint8_t *audio_data = malloc(*size);
    if (!audio_data) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to allocate memory for audio file '%s'.", filename);
        file_close(fp);
        return 0;
    }
    if (fread(audio_data, 1, *size, fp) != *size) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to read audio file '%s'.", filename);
        free(audio_data);
        file_close(fp);
        return 0;
    }
    file_close(fp);
    return audio_data;
}

static MIX_Track *create_track_from_memory(uint8_t *buffer, size_t size, const char *filename, bool free_when_done)
{
    if (!buffer || !size) {
        if (free_when_done) {
            free(buffer);
        }
        return 0;
    }

    MIX_Audio *audio = MIX_LoadAudioNoCopy(data.mixer, buffer, size, free_when_done);
    if (!audio) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "Failed to load audio from SDL_IOStream for file '%s'. Reason: %s", filename, SDL_GetError());
        return 0;
    }
    
    MIX_Track *track = MIX_CreateTrack(data.mixer);
    if (!track) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "Failed to create track for file '%s'. Reason: %s", filename, SDL_GetError());
        MIX_DestroyAudio(audio);
        return 0;
    }

    MIX_SetTrackAudio(track, audio);

    return track;
}

static MIX_Track *load_track(const char *filename)
{
    if (!filename || !*filename) {
        return 0;
    }
    size_t size;
    uint8_t *audio_data = game_campaign_load_file(filename, &size);
    if (audio_data) {
        return create_track_from_memory(audio_data, size, filename, false);
    }

    audio_data = load_file(filename, &size);
    if (!audio_data) {
        return 0;
    }

    return create_track_from_memory(audio_data, size, filename, true);
}

static void callback_for_sound_finished(void *userdata, MIX_Track *track)
{
    if (!data.sound_finished_callback) {
        return;
    }
    sound_channel *channel = (sound_channel *) userdata;

    if (channel->type == SOUND_TYPE_MUSIC) {
        if (data.music_finished_callback) {
            data.music_finished_callback();
        }
    } else {
        data.sound_finished_callback(channel->type);
    }
}

void sound_device_init_channels(void)
{
    if (!data.initialized) {
        return;
    }
    log_info("Loading audio files", 0, 0);
    for (unsigned int i = 0; i < data.total_channels; i++) {
        data.channels[i].track = 0;
        data.channels[i].filename[0] = 0;
        data.channels[i].last_played = 0;
        data.channels[i].type = NO_CHANNEL;
    }
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
    return MIX_TrackPlaying(data.channels[channel].track);
}

void sound_device_set_music_volume(int volume_pct)
{
    if (!data.initialized) {
        return;
    }
    sound_channel *music_channel = &data.channels[sound_type_to_channels[SOUND_TYPE_MUSIC].start];
    if (!music_channel->track) {
        return;
    }
    MIX_SetTrackGain(music_channel->track, percentage_to_volume(volume_pct));
}

void sound_device_set_volume_for_type(sound_type type, int volume_pct)
{
    if (!data.initialized) {
        return;
    }
    for (int i = 0; i < sound_type_to_channels[type].total; i++) {
        int channel = i + sound_type_to_channels[type].start;
        if (data.channels[channel].track) {
            MIX_SetTrackGain(data.channels[channel].track, percentage_to_volume(volume_pct));
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

static int play_music_from_file(const char *filename, int volume_pct, int loop, void (*on_finish)(void))
{
    if (!data.initialized || !filename || !*filename || !config_get(CONFIG_GENERAL_ENABLE_AUDIO)) {
        return 0;
    }
    sound_device_stop_music();
    data.music_finished_callback = on_finish;
    return sound_device_play_file_on_channel_panned(filename, SOUND_TYPE_MUSIC, volume_pct, 100, 100, loop);
}

int sound_device_play_music(const char *filename, int volume_pct, int loop)
{
    return play_music_from_file(filename, volume_pct, loop, 0);
}

int sound_device_play_track(const char *filename, int volume_pct, void (*on_finish)(void))
{
    return play_music_from_file(filename, volume_pct, 0, on_finish);
}

static int get_available_channel(sound_type type)
{
    int oldest_channel = NO_CHANNEL;
    for (int i = 0; i < sound_type_to_channels[type].total; i++) {
        int channel = i + sound_type_to_channels[type].start;
        if (!MIX_TrackPlaying(data.channels[channel].track)) {
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
        data.channels[channel].track = load_track(filename);
        if (!data.channels[channel].track) {
            return 0;
        }
        data.channels[channel].type = type;
        snprintf(data.channels[channel].filename, FILE_NAME_MAX, "%s", filename);
    }
    MIX_StereoGains gains = { left_pct / 100.0f, right_pct / 100.0f };
    MIX_SetTrackStereo(data.channels[channel].track, &gains);
    MIX_SetTrackGain(data.channels[channel].track, percentage_to_volume(volume_pct));
    MIX_SetTrackStoppedCallback(data.channels[channel].track, callback_for_sound_finished, &data.channels[channel]);

    if (!MIX_PlayTrack(data.channels[channel].track, 0)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Error playing audio file '%s'. Reason: %s",
            filename, SDL_GetError());
        return 0;
    }

    if (loop) {
        MIX_SetTrackLoops(data.channels[channel].track, -1);
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
    sound_channel *music_channel = &data.channels[sound_type_to_channels[SOUND_TYPE_MUSIC].start];
    MIX_StopTrack(music_channel->track, MIX_TrackMSToFrames(music_channel->track, milisseconds));
}

int sound_device_pause_music(void)
{
    if (!data.initialized) {
        return 0;
    }

    sound_channel *music_channel = &data.channels[sound_type_to_channels[SOUND_TYPE_MUSIC].start];
    return MIX_PauseTrack(music_channel->track);
}

int sound_device_resume_music(void)
{
    if (!data.initialized) {
        return 0;
    }
    sound_channel *music_channel = &data.channels[sound_type_to_channels[SOUND_TYPE_MUSIC].start];
    if (!MIX_TrackPaused(music_channel->track)) {
        return 0;
    }
    return MIX_ResumeTrack(music_channel->track);
}

static void free_custom_audio_stream(void)
{
    MIX_DestroyTrack(data.custom_music.track);
    data.custom_music.track = 0;
    SDL_DestroyAudioStream(data.custom_music.stream);
    data.custom_music.stream = 0;
}

void sound_device_stop_music(void)
{
    if (!data.initialized) {
        return;
    }
    stop_channel(sound_type_to_channels[SOUND_TYPE_MUSIC].start);
    free_custom_audio_stream();
}

void sound_device_stop_type(sound_type type)
{
    for (int i = 0; i < sound_type_to_channels[type].total; i++) {
        stop_channel(i + sound_type_to_channels[type].start);
    }
}

static int custom_audio_stream_active(void)
{
    return data.custom_music.track != 0;
}

void sound_device_use_custom_music_player(int bitdepth, int num_channels, int rate, const void *audio_data, int len)
{
    if (!data.initialized) {
        return;
    }

    free_custom_audio_stream();

    SDL_AudioFormat format;
    if (bitdepth == 8) {
        format = SDL_AUDIO_U8;
    } else if (bitdepth == 16) {
        format = SDL_AUDIO_S16;
    } else if (bitdepth == 32) {
        format = SDL_AUDIO_F32;
    } else {
        log_error("Custom music bitdepth not supported:", 0, bitdepth);
        return;
    }

    SDL_AudioSpec src_spec = { format, num_channels, rate };
    SDL_AudioSpec dst_spec;
    MIX_GetMixerFormat(data.mixer, &dst_spec);

   data.custom_music.stream = SDL_CreateAudioStream(&src_spec, &dst_spec);

    if (!data.custom_music.stream) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "Failed to create audio stream for custom music player. Reason: %s", SDL_GetError());
        return;
    }

    data.custom_music.track = MIX_CreateTrack(data.mixer);
    if (!data.custom_music.track) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "Failed to create track for custom music player. Reason: %s", SDL_GetError());
        free_custom_audio_stream();
        return;
    }

    if (!MIX_SetTrackAudioStream(data.custom_music.track, data.custom_music.stream)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "Failed to set audio stream for custom music player. Reason: %s", SDL_GetError());
        free_custom_audio_stream();
        return;
    }  

    sound_device_write_custom_music_data(audio_data, len);
    SDL_PropertiesID options = SDL_CreateProperties();
    if (options) {
        SDL_SetBooleanProperty(options, MIX_PROP_PLAY_HALT_WHEN_EXHAUSTED_BOOLEAN, false);
    }
    MIX_PlayTrack(data.custom_music.track, options);
}

void sound_device_write_custom_music_data(const void *audio_data, int len)
{
    if (!data.initialized || !audio_data || len <= 0 || !custom_audio_stream_active()) {
        return;
    }

    float volume = config_get(CONFIG_GENERAL_ENABLE_AUDIO) && config_get(CONFIG_GENERAL_ENABLE_VIDEO_SOUND) ?
        percentage_to_volume(config_get(CONFIG_GENERAL_VIDEO_VOLUME)) : 0;
    MIX_SetTrackGain(data.custom_music.track, volume);

    SDL_PutAudioStreamData(data.custom_music.stream, audio_data, len);
}

void sound_device_use_default_music_player(void)
{
    if (!data.initialized) {
        return;
    }
    free_custom_audio_stream();
}

void sound_device_close(void)
{
    if (!data.initialized) {
        return;
    }
    free_custom_audio_stream();
    MIX_DestroyMixer(data.mixer);
    free(data.channels);
    data.channels = 0;
    data.total_channels = 0;
    data.initialized = 0;
}

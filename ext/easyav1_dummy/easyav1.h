#ifndef EASYAV1_H
#define EASYAV1_H

#include <stdio.h>

/*
 * Dummy structures and types for easyav1.
 */
typedef struct easyav1_t easyav1_t;

typedef enum {
    EASYAV1_FALSE = 0,
    EASYAV1_TRUE = 1
} easyav1_bool;

typedef enum {
    EASYAV1_STATUS_ERROR = 0
} easyav1_status;

typedef struct easyav1_settings {
    easyav1_bool enable_audio;
    easyav1_bool close_handle_on_destroy;
} easyav1_settings;

typedef struct easyav1_video_frame {
    const void *data[3];
    size_t stride[3];
} easyav1_video_frame;

typedef struct easyav1_audio_frame {
    size_t bytes;
    union {
        const float **deinterlaced;
        const float *interlaced;
    } pcm;
} easyav1_audio_frame;



/**
 * Dummy easyav1 header file for compatibility when there is no av1 support built.
 * 
 */

easyav1_t *easyav1_init_from_memory(const void *data, size_t size, const easyav1_settings *settings) { return 0; }
easyav1_t *easyav1_init_from_file(FILE *f, const easyav1_settings *settings) { return 0; }
easyav1_settings easyav1_default_settings(void) { easyav1_settings settings = { 0 }; return settings; }

easyav1_bool easyav1_has_video_track(easyav1_t *easyav1) { return EASYAV1_FALSE; }
easyav1_bool easyav1_has_audio_track(easyav1_t *easyav1) { return EASYAV1_FALSE; }
unsigned int easyav1_get_video_width(easyav1_t *easyav1) { return 0; }
unsigned int easyav1_get_video_height(easyav1_t *easyav1) { return 0; }
unsigned int easyav1_get_video_fps(easyav1_t *easyav1) { return 0; }
unsigned int easyav1_get_audio_sample_rate(easyav1_t *easyav1) { return 0; }
unsigned int easyav1_get_audio_channels(easyav1_t *easyav1) { return 0; }
unsigned int easyav1_get_total_video_tracks(easyav1_t *easyav1) { return 0; }

easyav1_bool easyav1_has_video_frame(easyav1_t *easyav1) { return EASYAV1_FALSE; }
easyav1_bool easyav1_has_audio_frame(easyav1_t *easyav1) { return EASYAV1_FALSE; }
easyav1_bool easyav1_is_finished(easyav1_t *easyav1) { return EASYAV1_TRUE; }
easyav1_audio_frame *easyav1_get_audio_frame(easyav1_t *easyav1) { return 0; }
easyav1_video_frame *easyav1_get_video_frame(easyav1_t *easyav1) { return 0; }

easyav1_status easyav1_play(easyav1_t *easyav1) { return EASYAV1_STATUS_ERROR; }
void easyav1_stop(easyav1_t *easyav1) {}
void easyav1_destroy(easyav1_t **easyav1) {}

 /**
 * Start playing the video
 * @param filename Video file
 * @return True if the video could be loaded
 */

 #endif // EASYAV1_H
#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct ALCdevice_struct ALCdevice;

extern bool utox_audio_thread_init;

enum {
    UTOXAUDIO_KILL,

    UTOXAUDIO_CHANGE_MIC,
    UTOXAUDIO_CHANGE_SPEAKER,

    UTOXAUDIO_START_FRIEND,
    UTOXAUDIO_STOP_FRIEND,

    UTOXAUDIO_GROUPCHAT_START,
    UTOXAUDIO_GROUPCHAT_STOP,

    UTOXAUDIO_START_PREVIEW,
    UTOXAUDIO_STOP_PREVIEW,

    UTOXAUDIO_PLAY_RINGTONE,
    UTOXAUDIO_STOP_RINGTONE,

    UTOXAUDIO_PLAY_NOTIFICATION,
    UTOXAUDIO_STOP_NOTIFICATION,

    UTOXAUDIO_NEW_AV_INSTANCE,
};

enum {
    NOTIFY_TONE_NONE,
    NOTIFY_TONE_FRIEND_ONLINE,
    NOTIFY_TONE_FRIEND_OFFLINE,
    NOTIFY_TONE_FRIEND_NEW_MSG,
    NOTIFY_TONE_FRIEND_REQUEST,
};

#define UTOX_DEFAULT_BITRATE_A 32
#define UTOX_DEFAULT_FRAME_A 20
#define UTOX_DEFAULT_SAMPLE_RATE_A 48000
#define UTOX_DEFAULT_AUDIO_CHANNELS 1

/* Check self */
#define UTOX_SENDING_AUDIO(f_number) (!!(get_friend(f_number)->call_state_self & TOXAV_FRIEND_CALL_STATE_SENDING_A))

/* Check friend */
#define UTOX_AVAILABLE_AUDIO(f_number) \
        (!!(get_friend(f_number)->call_state_friend & TOXAV_FRIEND_CALL_STATE_SENDING_A))

/* Check both */
#define UTOX_SEND_AUDIO(f_number) \
    (   !!(get_friend(f_number)->call_state_self & TOXAV_FRIEND_CALL_STATE_SENDING_A) \
     && !!(get_friend(f_number)->call_state_friend & TOXAV_FRIEND_CALL_STATE_ACCEPTING_A))

void utox_audio_in_device_open(void);
void utox_audio_in_device_close(void);
void utox_audio_in_listen(void);
void utox_audio_in_ignore(void);
void utox_audio_in_device_set(ALCdevice *new_device);
ALCdevice *utox_audio_in_device_get(void);

void utox_audio_out_device_open(void);
void utox_audio_out_device_close(void);
void utox_audio_out_device_set(ALCdevice *new_device);

void sourceplaybuffer(
        unsigned i,
        const int16_t *data,
        int samples,
        uint8_t channels,
        unsigned sample_rate);

// Send a message to the audio thread.
void postmessage_audio(uint8_t msg, uint32_t param1, uint32_t param2, void *data);

void utox_audio_thread(void *args);

#endif

#include "utox_av.h"

#include "audio.h"
#include "video.h"

#include "../flist.h"
#include "../friend.h"
#include "../groups.h"
#include "../inline_video.h"
#include "../macros.h"
#include "../tox.h"
#include "../utox.h"

#include "../native/audio.h"
#include "../native/thread.h"

#include <stdlib.h>

bool utox_av_ctrl_init = false;

static bool toxav_thread_msg = 0;

void postmessage_utoxav(uint8_t msg, uint32_t param1, uint32_t param2, void *data) {
    while (toxav_thread_msg && utox_av_ctrl_init) { /* I'm not convinced this is the best way */
        yieldcpu(1);
    }

    toxav_msg.msg    = msg;
    toxav_msg.param1 = param1;
    toxav_msg.param2 = param2;
    toxav_msg.data   = data;

    toxav_thread_msg = 1;
}

void utox_av_ctrl_thread(void *UNUSED(args)) {
    ToxAV *av = NULL;

    utox_av_ctrl_init = 1;

    volatile uint32_t call_count = 0;
    volatile bool     audio_in   = 0;
    // volatile bool video_on  = 0;

    while (1) {
        if (toxav_thread_msg) {
            TOX_MSG *msg = &toxav_msg;
            if (msg->msg == UTOXAV_KILL) {
                break;
            } else if (msg->msg == UTOXAV_NEW_TOX_INSTANCE) {
                if (av) { /* toxcore restart */
                    toxav_kill(av);
                    postmessage_audio(UTOXAUDIO_NEW_AV_INSTANCE, 0, 0, msg->data);
                    postmessage_video(UTOXVIDEO_NEW_AV_INSTANCE, 0, 0, msg->data);
                } else {
                    thread(utox_audio_thread, msg->data);
                    thread(utox_video_thread, msg->data);
                }

                av = msg->data;
                set_av_callbacks(av);
            }

            if (!utox_audio_thread_init || !utox_video_thread_init) {
                yieldcpu(10);
            }

            switch (msg->msg) {
                case UTOXAV_INCOMING_CALL_PENDING: {
                    call_count++;
                    postmessage_audio(UTOXAUDIO_PLAY_RINGTONE, msg->param1, msg->param2, NULL);
                    break;
                }

                case UTOXAV_INCOMING_CALL_ANSWER: {                    FRIEND *f = get_friend(msg->param1);
                    postmessage_audio(UTOXAUDIO_STOP_RINGTONE, msg->param1, msg->param2, NULL);
                    postmessage_audio(UTOXAUDIO_START_FRIEND, msg->param1, msg->param2, NULL);
                    f->call_state_self = (TOXAV_FRIEND_CALL_STATE_SENDING_A | TOXAV_FRIEND_CALL_STATE_ACCEPTING_A);
                    if (msg->param2) {
                        utox_video_start(0);
                        f->call_state_self |= (TOXAV_FRIEND_CALL_STATE_SENDING_V | TOXAV_FRIEND_CALL_STATE_ACCEPTING_V);
                    }
                    break;
                }

                case UTOXAV_INCOMING_CALL_REJECT: {
                    call_count--;
                    postmessage_audio(UTOXAUDIO_STOP_RINGTONE, msg->param1, msg->param2, NULL);
                    break;
                }

                case UTOXAV_OUTGOING_CALL_PENDING: {
                    call_count++;
                    postmessage_audio(UTOXAUDIO_PLAY_RINGTONE, msg->param1, msg->param2, NULL);
                    FRIEND *f = get_friend(msg->param1);
                    f->call_state_self = (TOXAV_FRIEND_CALL_STATE_SENDING_A | TOXAV_FRIEND_CALL_STATE_ACCEPTING_A);
                    if (msg->param2) {
                        utox_video_start(0);
                        f->call_state_self |= (TOXAV_FRIEND_CALL_STATE_SENDING_V | TOXAV_FRIEND_CALL_STATE_ACCEPTING_V);
                    }
                    break;
                }

                case UTOXAV_OUTGOING_CALL_ACCEPTED:
                    postmessage_audio(UTOXAUDIO_START_FRIEND, msg->param1, msg->param2, NULL);
                    // Intentional fall-through.
                case UTOXAV_OUTGOING_CALL_REJECTED: {
                    postmessage_audio(UTOXAUDIO_STOP_RINGTONE, msg->param1, msg->param2, NULL);
                    break;
                }

                case UTOXAV_CALL_END: {
                    call_count--;
                    FRIEND *f = get_friend(msg->param1);
                    if (f
                        && f->call_state_self & (TOXAV_FRIEND_CALL_STATE_SENDING_V
                                                 | TOXAV_FRIEND_CALL_STATE_ACCEPTING_V))
                    {
                        utox_video_stop(false);
                    }

                    postmessage_audio(UTOXAUDIO_STOP_FRIEND, msg->param1, msg->param2, NULL);
                    postmessage_audio(UTOXAUDIO_STOP_RINGTONE, msg->param1, msg->param2, NULL);
                    break;
                }

                case UTOXAV_GROUPCALL_START: {
                    call_count++;
                    postmessage_audio(UTOXAUDIO_GROUPCHAT_START, msg->param1, msg->param2, NULL);
                    break;
                }

                case UTOXAV_GROUPCALL_END: {
                    GROUPCHAT *g = get_group(msg->param1);
                    if (!g || !call_count) {
                        break;
                    }

                    postmessage_audio(UTOXAUDIO_GROUPCHAT_STOP, msg->param1, msg->param2, NULL);
                    call_count--;
                    break;
                }

                case UTOXAV_START_AUDIO: {
                    call_count++;
                    if (msg->param1) {
                        /* Start audio preview */
                        call_count++;
                        postmessage_audio(UTOXAUDIO_START_PREVIEW, 0, 0, NULL);
                    }
                    break;
                }

                case UTOXAV_STOP_AUDIO: {
                    if (!call_count) {
                        break;
                    }

                    if (msg->param1) {
                        call_count--;
                        postmessage_audio(UTOXAUDIO_STOP_PREVIEW, 0, 0, NULL);
                    }
                    break;
                }

                case UTOXAV_START_VIDEO: {
                    if (msg->param2) {
                        utox_video_start(1);
                    } else {
                        utox_video_start(0);
                        toxav_bit_rate_set(av, msg->param1, UTOX_DEFAULT_BITRATE_V, 0, NULL);
                    }
                    break;
                }

                case UTOXAV_STOP_VIDEO: {
                    if (msg->param2) {
                        utox_video_stop(1);
                    } else {
                        utox_video_stop(0);
                        toxav_bit_rate_set(av, msg->param1, -1, 0, NULL);
                    }
                    postmessage_utox(AV_CLOSE_WINDOW, msg->param1, 0, NULL);
                    break;
                }

                case UTOXAV_SET_AUDIO_IN: {
                    if (audio_in) {
                        postmessage_audio(UTOXAUDIO_CHANGE_MIC, 0, 0, NULL);
                    }

                    utox_audio_in_device_set(msg->data);

                    if (msg->data != utox_audio_in_device_get()) {
                        audio_in   = 0;
                        call_count = 0;
                        break;
                    }

                    // TODO get a count in audio.c and allow count restore
                    // if (audio_in) {
                    //     utox_audio_in_device_open();
                    //     utox_audio_in_listen();
                    // }
                    break;
                }

                case UTOXAV_SET_AUDIO_OUT: {
                    postmessage_audio(UTOXAUDIO_CHANGE_SPEAKER, 0, 0, NULL);
                    utox_audio_out_device_set(msg->data);
                    break;
                }

                case UTOXAV_SET_VIDEO_IN: {
                    utox_video_change_device(msg->param1);
                    break;
                }

                case UTOXAV_SET_VIDEO_OUT: {
                    break;
                }
            }
        }

        toxav_thread_msg = false;

        if (av) {
            toxav_iterate(av);
            yieldcpu(toxav_iteration_interval(av));
        } else {
            yieldcpu(10);
        }
    }


    postmessage_audio(UTOXAUDIO_KILL, 0, 0, NULL);
    postmessage_video(UTOXVIDEO_KILL, 0, 0, NULL);

    // Wait for all a/v threads to return 0
    while (utox_audio_thread_init || utox_video_thread_init) {
        yieldcpu(1);
    }

    toxav_thread_msg  = false;
    utox_av_ctrl_init = false;

    toxav_kill(av);
}

static void utox_av_incoming_call(ToxAV *UNUSED(av), uint32_t friend_number,
                                  bool audio, bool video, void *UNUSED(userdata))
{
        FRIEND *f = get_friend(friend_number);
    if (!f) {
                return;
    }

    f->call_state_self   = 0;
    f->call_state_friend = (audio << 2 | video << 3 | audio << 4 | video << 5);
        postmessage_utoxav(UTOXAV_INCOMING_CALL_PENDING, friend_number, 0, NULL);
    postmessage_utox(AV_CALL_INCOMING, friend_number, video, NULL);
}

static void utox_av_remote_disconnect(ToxAV *UNUSED(av), int32_t friend_number) {
    FRIEND *f = get_friend(friend_number);
    if (!f) {
        return;
    }

    postmessage_utoxav(UTOXAV_CALL_END, friend_number, 0, NULL);
    f->call_state_self   = 0;
    f->call_state_friend = 0;
    postmessage_utox(AV_CLOSE_WINDOW, friend_number + 1, 0, NULL);
    postmessage_utox(AV_CALL_DISCONNECTED, friend_number, 0, NULL);
}

void utox_av_local_disconnect(ToxAV *av, int32_t friend_number) {
    if (av) {
        /* TODO HACK: tox_callbacks doesn't have access to toxav, so it just sets it as NULL, this is bad! */
        toxav_call_control(av, friend_number, TOXAV_CALL_CONTROL_CANCEL, NULL);
    }

    FRIEND *f = get_friend(friend_number);
    if (!f) {
        return;
    }

    f->call_state_self   = 0;
    f->call_state_friend = 0;
    postmessage_utox(AV_CLOSE_WINDOW, friend_number + 1, 0, NULL); /* TODO move all of this into a static function in that
                                                                 file !*/
    postmessage_utox(AV_CALL_DISCONNECTED, friend_number, 0, NULL);
    postmessage_utoxav(UTOXAV_CALL_END, friend_number, 0, NULL);
}

void utox_av_local_call_control(ToxAV *av, uint32_t friend_number, TOXAV_CALL_CONTROL control) {
    TOXAV_ERR_CALL_CONTROL err = 0;
    toxav_call_control(av, friend_number, control, &err);
    if (err) {
        return;
    }

    FRIEND *f = get_friend(friend_number);
    if (!f) {
        return;
    }

    switch (control) {
        case TOXAV_CALL_CONTROL_HIDE_VIDEO: {
            toxav_bit_rate_set(av, friend_number, -1, 0, NULL);
            postmessage_utoxav(UTOXAV_STOP_VIDEO, friend_number, 0, NULL);
            f->call_state_self &= (0xFF ^ TOXAV_FRIEND_CALL_STATE_SENDING_V);
            break;
        }

        case TOXAV_CALL_CONTROL_SHOW_VIDEO: {
            toxav_bit_rate_set(av, friend_number, -1, UTOX_DEFAULT_BITRATE_V, NULL);
            postmessage_utoxav(UTOXAV_START_VIDEO, friend_number, 0, NULL);
            f->call_state_self |= TOXAV_FRIEND_CALL_STATE_SENDING_V;
            break;
        }

        default: {
        }
        // TODO
        // TOXAV_CALL_CONTROL_RESUME,
        // TOXAV_CALL_CONTROL_PAUSE,
        // TOXAV_CALL_CONTROL_CANCEL,
        // TOXAV_CALL_CONTROL_MUTE_AUDIO,
        // TOXAV_CALL_CONTROL_UNMUTE_AUDIO,
    }
}

// responds to a audio frame call back from toxav
static void utox_av_incoming_frame_a(ToxAV *UNUSED(av), uint32_t friend_number, const int16_t *pcm, size_t sample_count,
                                     uint8_t channels, uint32_t sample_rate, void *UNUSED(userdata))
{
    sourceplaybuffer(friend_number, pcm, sample_count, channels, sample_rate);
}

static void utox_av_incoming_frame_v(ToxAV *UNUSED(toxAV), uint32_t friend_number, uint16_t width, uint16_t height,
                                     const uint8_t *y, const uint8_t *u, const uint8_t *v, int32_t ystride,
                                     int32_t ustride, int32_t vstride, void *UNUSED(user_data))
{
    /* copy the vpx_image */
    /* 4 bits for the H*W, then a pixel for each color * size */
    FRIEND *f = get_friend(friend_number);
    if (!f) {
        return;
    }
    f->video_width  = width;
    f->video_height = height;
    size_t size     = width * height * 4;

    UTOX_FRAME_PKG *frame = calloc(1, sizeof(UTOX_FRAME_PKG));
    frame->w    = width;
    frame->h    = height;
    frame->size = size;
    frame->img  = malloc(size);

    yuv420tobgr(width, height, y, u, v, ystride, ustride, vstride, frame->img);
    if (f->video_inline) {
        inline_set_frame(width, height, size, frame->img);

        postmessage_utox(AV_INLINE_FRAME, friend_number, 0, NULL);
        free(frame->img);
        free(frame);
    } else {
        postmessage_utox(AV_VIDEO_FRAME, friend_number, 0, (void *)frame);
    }
}

static void utox_audio_friend_accepted(ToxAV *av, uint32_t friend_number, uint32_t state) {
    /* First accepted call back */
    get_friend(friend_number)->call_state_friend = state;
    if (SELF_SEND_VIDEO(friend_number) && !FRIEND_ACCEPTING_VIDEO(friend_number)) {
        utox_av_local_call_control(av, friend_number, TOXAV_CALL_CONTROL_HIDE_VIDEO);
    }
    postmessage_utoxav(UTOXAV_OUTGOING_CALL_ACCEPTED, friend_number, 0, NULL);
    postmessage_utox(AV_CALL_ACCEPTED, friend_number, 0, NULL);
}

/** respond to a Audio Video state change call back from toxav */
static void utox_callback_av_change_state(ToxAV *av, uint32_t friend_number, uint32_t state, void *UNUSED(userdata)) {
    FRIEND *f = get_friend(friend_number);
    if (state == 1) {
        // handle error
        utox_av_remote_disconnect(av, friend_number);
        return;
    }

    if (state == 2) {
        utox_av_remote_disconnect(av, friend_number);
        message_add_type_notice(&f->msg, "Friend Has Ended the call!", 26, 0); /* TODO localization with S() SLEN() */
        return;
    }

    if (!f->call_state_friend) {
        utox_audio_friend_accepted(av, friend_number, state);
    }

    if (get_friend(friend_number)->call_state_friend ^ (state & TOXAV_FRIEND_CALL_STATE_SENDING_V)) {
        if (!(state & TOXAV_FRIEND_CALL_STATE_SENDING_V)) {
            flist_reselect_current();
        }
    }

    get_friend(friend_number)->call_state_friend = state;
}

static void utox_incoming_rate_change(ToxAV *AV, uint32_t f_num, uint32_t UNUSED(a_bitrate),
                                      uint32_t v_bitrate, void *UNUSED(ud))
{
    /* Just accept what toxav wants the bitrate to be... */
    if (v_bitrate > (uint32_t)UTOX_MIN_BITRATE_VIDEO) {
        toxav_bit_rate_set(AV, f_num, -1, v_bitrate, NULL);
    }
}

void set_av_callbacks(ToxAV *av) {
    /* Friend update callbacks */
    toxav_callback_call(av, &utox_av_incoming_call, NULL);
    toxav_callback_call_state(av, &utox_callback_av_change_state, NULL);

    /* Incoming data callbacks */
    toxav_callback_audio_receive_frame(av, &utox_av_incoming_frame_a, NULL);
    toxav_callback_video_receive_frame(av, &utox_av_incoming_frame_v, NULL);

    /* Data type change callbacks. */
    toxav_callback_bit_rate_status(av, &utox_incoming_rate_change, NULL);
}

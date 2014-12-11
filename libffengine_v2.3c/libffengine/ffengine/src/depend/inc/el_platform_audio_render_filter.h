
#ifndef _EL_AUDIO_RENDER_FILTER_H_
#define _EL_AUDIO_RENDER_FILTER_H_

#include "el_std.h"

#define EL_AUDIO_MAX_VOLUME   15

//~ 尚未使用
typedef enum tagELAudioChannels
{
    EL_AUDIO_CHANNELS_MONO = 0x01,  // 单声道
    EL_AUDIO_CHANNELS_STEREO        // 立体声，没有增加对杜比的支持
} ELAudioChannels_e;

//~ 尚未使用
typedef struct tagELAudioRenderAudioParms
{
    enum AVCodecID          eAudioSampleFmt;
    EL_UINT                 uiSampleRate;
    EL_UINT                 uiBitsPerSample;
    ELAudioChannels_e       iChannelCount;
} ELAudioRenderAudioParms_s;

typedef EL_BOOL (*el_audio_get_sample_cb)(EL_BYTE** ppSampleData, EL_INT* pDataLen);
typedef EL_VOID (*el_audio_consume_sample_cb)();
typedef EL_VOID (*el_audio_set_sample_first_play_time_cb)(int64_t tm);

typedef EL_VOID (*el_audio_render_filter_init_func)(el_audio_get_sample_cb get_cb,
                                                    el_audio_consume_sample_cb consume_cb,
                                                    el_audio_set_sample_first_play_time_cb set_sample_first_play_time_cb);
typedef EL_VOID (*el_audio_render_filter_uninit_func)();

typedef EL_VOID (*el_audio_render_filter_set_param_func)(ELAudioRenderAudioParms_s a_AudioRenderAudioPams);
typedef EL_VOID (*el_audio_render_filter_get_param_func)(ELAudioRenderAudioParms_s *a_pAudioParams);

typedef EL_VOID (*el_audio_render_filter_set_volume_func)(EL_SIZE a_nVolume); // [0, MAX]
typedef EL_VOID (*el_audio_render_filter_get_volume_func)(EL_SIZE *a_nVolume);

typedef EL_VOID (*el_audio_render_filter_stop_func)();
typedef EL_VOID (*el_audio_render_filter_pause_func)();
typedef EL_VOID (*el_audio_render_filter_resume_func)();
typedef EL_VOID (*el_audio_render_filter_reset_func)();

// 音频结构，和设备层分离开，靠这个方法(跨平台)
typedef struct tagELAudioRenderFilter
{
    el_audio_render_filter_init_func             filter_init;
    el_audio_render_filter_uninit_func           filter_unint;
    el_audio_render_filter_set_param_func        filter_set_param;     // 不设置，不工作
    el_audio_render_filter_get_param_func        filter_get_param;     // 未实现
    el_audio_render_filter_set_volume_func       filter_set_volume;    // 设置声音值
    el_audio_render_filter_get_volume_func       filter_get_volume;
    el_audio_render_filter_stop_func             filter_stop;
    el_audio_render_filter_pause_func            filter_pause;
    el_audio_render_filter_resume_func           filter_resume;
    el_audio_render_filter_reset_func            filter_reset;
} ELAudioRenderFilter_t;

EL_STATIC ELAudioRenderFilter_t* el_player_get_audio_render_filter();



#endif

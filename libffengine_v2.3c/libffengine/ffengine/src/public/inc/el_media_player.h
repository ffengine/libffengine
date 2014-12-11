#ifndef _EL_MEDIA_PLAYER_H_
#define _EL_MEDIA_PLAYER_H_

#include <stdint.h>

#include "el_std.h"

typedef struct tagELVideoDesc
{
    enum AVCodecID                  eVideoCodecID;
    EL_UINT                         nWidth;
    EL_UINT                         nHeight;

    EL_UINT                         nBitRate;
    EL_UINT                         nFrameRate;
    EL_UINT                         nSizeImage;         // 一帧的Image大小
}ELVideoDesc_t;

typedef struct tagELAudioDesc
{
    enum AVCodecID                  eAudioCodecID;    // avcodec_find_decoder(codecID);
    EL_INT                          eChannelCount;      // 声道数
    EL_INT                          euiFreq;
    EL_UINT                         uiBitCount;
    EL_UINT                         uiBitsPerSample;    // 采样率
    EL_CHAR                         strSongName[32];    // demuxer, 放mp3的时候会有用
    EL_CHAR                         strSpecialName[32];
    EL_CHAR                         strSingerName[32];
    EL_UINT                         uiKbps;             // 码流
}ELAudioDesc_t;

typedef struct tagELMediaMuxerDesc
{
    enum AVCodecID                  eVideoCodecID;       // 枚举值,参考ffmpeg头文件 *
    enum AVCodecID                  eAudioCodecID;        // 枚举值,参考ffmpeg头文件 *
    EL_UINT                         uiVideoSurfaceCount;    // Video Package数目
    EL_UINT                         uiAudioSampleCount;     // 获取出来可能是0
    int64_t                         duration;               // 总时长 s * 1000 * 1000 // AVFormatContext->duration
    EL_BOOL                         is_network_media;       // 是不是网络的
    EL_BOOL                         is_live_media;          // 是不是实时流媒体, get出来只是一个参考 * (不准)
} ELMediaMuxerDesc_t;

// 发送给UI界面层的消息
typedef enum tagEL_player_ui_message
{
    EL_PLAYER_MESSAGE_NONE = 0,        
    EL_PLAYER_MESSAGE_MEDIA_CURRENT_POS,    // 当前播放位置
    EL_PLAYER_MESSAGE_BUFFERING_START,      // 开始缓冲
    EL_PLAYER_MESSAGE_BUFFERING_PERCENT,    // 缓冲百分比
    EL_PLAYER_MESSAGE_READY_TO_PLAY,        // 可以播放了
    EL_PLAYER_MESSAGE_OPEN_SUCCESS,         // 打开文件成功
    EL_PLAYER_MESSAGE_OPEN_FAILED,          // 打开文件失败
    EL_PLAYER_MESSAGE_END_OF_FILE,          // 到文件结尾
    EL_PLAYER_MESSAGE_PAUSE_RESULT,         // 暂停是否成功
    EL_PLAYER_MESSAGE_SEEK_THUMBNAIL,       // 查找缩略图
    EL_PLAYER_MESSAGE_MEDIA_CACHED_POS,     // 缓冲到了哪里
    EL_PLAYER_MESSAGE_NOTIFICATION,         // 通知消息
    EL_PLAYER_MESSAGE_DISPLAY_FRAME,        // 显示画面
    EL_PLAYER_MESSAGE_END,                  // 枚举结尾
} EL_PLAYER_MESSAGE_e;

// 底层给上层的接口

EL_STATIC EL_BOOL el_media_player_init();       // 初始化控制层
EL_STATIC EL_BOOL el_media_player_uninit();     // 同步接口

EL_STATIC EL_BOOL el_media_player_open(EL_CONST EL_UTF8* a_pszUrlOrPathname); // 可以使用http/rtsp/rtp/file等格式的。, 异步接口，比如等待网络
EL_STATIC EL_BOOL el_media_player_close();      // 下次打开文件之前，必须把上次文件close掉。

EL_STATIC EL_BOOL el_media_player_start();      // 从头开始播,后面没有调用start,再次调用会被忽略
EL_STATIC EL_BOOL el_media_player_stop();       // 文件播完了,会被stop。  stop之后,底下是一个很复杂的状态机,stop之后，可以调restart/start。

EL_STATIC EL_BOOL el_media_player_pause();      //
EL_STATIC EL_BOOL el_media_player_play();       // 相当于resume的功能

EL_STATIC EL_INT  el_media_player_seek_to(EL_SIZE a_SeekPos);  // seekTo(0)->play,相当于restart

EL_STATIC EL_BOOL el_media_player_get_description(ELMediaMuxerDesc_t *aMediaMuxerDesc, ELVideoDesc_t *aVideoDesc, ELAudioDesc_t *aAudioDesc);

typedef void (* el_ui_message_receiver_func_t)(int nMessage, void* wParam,void* lParam);

typedef struct __el_dynamic_reference_table_t__
{
    el_ui_message_receiver_func_t el_ui_message_receiver_func;
} el_dynamic_reference_table_t;

extern EL_VOID el_media_player_set_dynamic_reference_table(el_dynamic_reference_table_t tbl);

EL_VOID el_player_send_message_to_ui(EL_INT nMessage, EL_VOID* wParam,EL_VOID* lParam);


#endif  // _EL_MEDIA_PLAYER_H_


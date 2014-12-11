#ifndef _EL_MEDIA_PLAYER_ENGINE_H_
#define _EL_MEDIA_PLAYER_ENGINE_H_

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavcodec/audioconvert.h"

#include "el_std.h"
#include "el_log.h"

#include "el_media_player.h"
#include "el_platform_sys_info.h"

#include "el_list.h"
#include "el_ipc.h"

#include "el_platform_audio_render_filter.h"
#include "el_platform_video_render_filter.h"

// log字符串定义
#ifdef EL_DBG_DECODER

#define EL_DBG_POS \
do{ \
EL_DBG_LOG("decoder: pos %s() line %d\n",__FUNCTION__,__LINE__); \
}while(0)
#define EL_DBG_FUNC_ENTER \
do{ \
EL_DBG_LOG("decoder: enter %s() line %d\n",__FUNCTION__,__LINE__); \
}while(0)
#define EL_DBG_FUNC_EXIT \
do{ \
EL_DBG_LOG("decoder: exit %s() line %d\n",__FUNCTION__,__LINE__); \
}while(0)

#else

#define EL_DBG_POS
#define EL_DBG_FUNC_ENTER
#define EL_DBG_FUNC_EXIT

#endif

// 
typedef struct tagELAVSample
{
	EL_UTF8*                                  pBuffer;
	EL_UINT                                   nBufferSize;
	int64_t                                  dwTS;
	EL_INT width;
	EL_INT height;
	EL_BOOL 								bVolumeAdjusted;
} ELAVSample_t;

/* 播放引擎状态机 */
typedef enum _el_player_engine_state_e_
{
	EL_ENGINE_STATE_UNKNOWN = -1,
    
	EL_ENGINE_CENTRAL_STATE_IDLE,       // 0 待命状态
	EL_ENGINE_CENTRAL_STATE_PREPARE,    // 准备打开
	EL_ENGINE_CENTRAL_STATE_OPENED,     // 打开成功
	EL_ENGINE_CENTRAL_STATE_BUFFERING,  // 正在缓冲
	EL_ENGINE_CENTRAL_STATE_PLAY_WAIT,  // 等待播放
	EL_ENGINE_CENTRAL_STATE_PLAYING,	// 5 正在播放
	EL_ENGINE_CENTRAL_STATE_PLAY_FILE_END,  // 播放到文件结尾了
	EL_ENGINE_CENTRAL_STATE_PAUSED,     // 暂停
	EL_ENGINE_CENTRAL_STATE_SEEK_WAIT,  // 搜寻暂停
	EL_ENGINE_CENTRAL_STATE_PLAY_COMPLETE,  // 播放完毕
	EL_ENGINE_CENTRAL_STATE_STOPPED,	// 10 播放停止
    
	/* state below is used for decoding state machine*/
	EL_ENGINE_DECODE_STATE_START,		// start state of the decode thread
	EL_ENGINE_DECODE_STATE_WAIT,        // 等待解码 ??
	EL_ENGINE_DECODE_STATE_SEEK_WAIT,	// 搜寻等待
	EL_ENGINE_DECODE_STATE_WORK,        // 解码线程工作中
    
	EL_ENGINE_VIDEO_SYNC_STATE_IDLE,	// 15 视频同步线程待命
	EL_ENGINE_VIDEO_SYNC_STATE_WAIT,    // 视频同步线程等待中
	EL_ENGINE_VIDEO_SYNC_STATE_WORK,    // 视频同步线程工作中

    /* 解码级别 */
	EL_ENGINE_DECODE_CTRL_START,        // 开始解码
	EL_ENGINE_DECODE_CTRL_NORMAL,       // 正常速率解码
	EL_ENGINE_DECODE_CTRL_L1,           // 20 一级丢帧
	EL_ENGINE_DECODE_CTRL_L2,           // 二级丢帧
    
	EL_ENGINE_STATE_END,                // 定义结束
} el_player_engine_state_e;

/* 播放引擎事件 */
typedef enum _el_player_engine_event_e_
{
	EL_ENGINE_EVT_MAC_GO_ON = 0,        // not external event
	EL_ENGINE_EVT_DECODE_MAC_GO_ON,     // 继续处理解码
	EL_ENGINE_EVT_VIDEO_SYNC_MAC_GO_ON,	// 继续处理视频同步
	EL_ENGINE_EVT_OPEN,                 // 打开,打开网络文件,本地文件等媒体文件，会发送此消息
	EL_ENGINE_EVT_CLOSE,                // 关闭,主动关闭文件
	EL_ENGINE_EVT_START,                // 开始,文件打开后,开始播放, Start + Play
	EL_ENGINE_EVT_PAUSE,                // 暂停
	EL_ENGINE_EVT_PLAY,                 // 播放,相当于Resume
	EL_ENGINE_EVT_SEEK_PAUSE,           // 搜索暂停
	EL_ENGINE_EVT_SEEK,                 // 搜索
	EL_ENGINE_EVT_READY_TO_SEEK,        // 可以搜索了
	EL_ENGINE_EVT_SEEK_DONE,            // 搜索完毕
	EL_ENGINE_EVT_STOP,                 // 停止
	EL_ENGINE_EVT_BUFFERING,            // 开始缓冲
	EL_ENGINE_EVT_STOP_DECODE_WAIT,     // 停止解码等待
	EL_ENGINE_EVT_RESTART,              // 重新启动
	EL_ENGINE_EVT_PLAY_COMPLETE,        // 播放完毕
	EL_ENGINE_EVT_EXIT_THREAD,          // 线程推出 this event is used for quitting  thread
	EL_ENGINE_EVT_END
} el_player_engine_evt_e;

// 当前文件类型
typedef enum _el_player_engine_AV_support_e_
{
	EL_ENGINE_HAS_NONE = 0,
	EL_ENGINE_HAS_AUDIO,	// has audio
	EL_ENGINE_HAS_VIDEO,	// has video
	EL_ENGINE_HAS_BOTH,		// has all
	EL_ENGINE_HAS_END
} el_player_engine_AV_support_e;

// 媒体类型
typedef enum _el_player_media_type_e_
{
	EL_MEDIA_VIDEO = 0,     // 仅仅包含视频
	EL_MEDIA_AUDIO,         // 仅仅包含音频
	EL_MEDIA_END            // 音视频都有
} el_player_media_type_e;

// 数据包类型
typedef struct _el_packet_qnode_t_
{
	AVPacket packet;
	struct list_node list;
} el_ffmpeg_packet_qnode_t;


typedef struct _el_frame_qnode_t_
{
	ELAVSample_t frm_sample;
	unsigned long real_buf_size;
	struct list_node list;
} el_ffmpeg_frame_qnode_t;

typedef enum _el_player_engine_state_mahine_type_e_
{
	el_state_machine_engine = 1,
	el_state_machine_video_decoder,
	el_state_machine_audio_decoder,
	el_state_machine_video_syncer,
	el_state_machine_decoder_controller,
	el_state_machine_max
}el_player_engine_state_machine_type_e;

typedef struct _el_player_engine_state_machine_t_
{
	el_player_engine_state_e state;
	el_player_engine_state_e old_state;
	el_player_engine_state_machine_type_e  type;
	el_ipc_msg_q_name_e ipc_fd;
}el_player_engine_state_machine_t;

typedef struct _el_player_network_error
{
	EL_BOOL berror;
	EL_INT	msg;
	EL_INT	msg1;
}el_player_network_error;

typedef struct _el_player_engine_decoded_file_obj_t_
{
	AVFormatContext *format_context;
	el_player_engine_state_machine_t engine;
	el_player_engine_state_machine_t video_decoder;
	el_player_engine_state_machine_t audio_decoder;
	el_player_engine_state_machine_t video_syncer;
	el_player_engine_state_machine_t decoder_controller;
	el_player_engine_evt_e event;	// event that can be received
	el_player_engine_AV_support_e av_support;
	EL_UTF8 *pszUrlOrPathname; // File name to be demuxed and decoded
	AVStream *audio_stream;
	AVStream *video_stream;	
	int stream_index[AVMEDIA_TYPE_NB];	// stream index
	int audio_channels_cnt;        // audio channels
	struct q_head video_pkt_q;	// raw packet q that not be decoded
	struct q_head audio_pkt_q;
	struct q_head video_frm_q;  // frame q that has already be decoded
	struct q_head audio_frm_q;
	struct q_head free_pkt_q;	// free packet q that not be decoded
	struct q_head free_video_frm_q;  // free frame q that has already be decoded
	struct q_head free_audio_frm_q;  // free frame q that has already be decoded
	void *mutex;				// mutex lock for this object
	int64_t seekpos;
	int64_t duration;
	int64_t buffering_time;   // in ms unit
	int64_t play_buffering_time;
	int64_t current_video_pts;
	int buffering_percent;
	int video_frm_q_max_cnt;    // max video frame q count
    
    // 2012.10.08
	// EL_BOOL has_been_closed;
    EL_BOOL abort_request;
	
    EL_BOOL is_mms_file;			// file is an mms based url
	EL_BOOL is_network_media;      // source media is from network 
	EL_BOOL is_live_media;			// whether the meida playing is live
	EL_BOOL is_file_eof;

	int         frame_count;
	int         avg_frame_time;
	int64_t	start_time;
	int64_t	first_video_ts;
	int64_t	cur_video_ts;
	int64_t	av_sync_time;
	int64_t    audio_start_node_pts;
	int64_t    audio_current_node_pts;
	int64_t    video_consume_time;
	int64_t    audio_consume_time;
	int64_t    delta_consume;
	int64_t    delta_consume_last;
	int64_t    delta_adjusted_sys_time;

	int		consume_count;
	int		av_seeking;
	unsigned long video_frame_size;
	EL_INT nAudioVolume;
	EL_BOOL  key_frame;

	el_player_network_error neterror;

	// for renders
	ELVideoRenderFilter_t   *video_render;
	ELAudioRenderFilter_t   *audio_render;
	ELVideoDesc_t           video_desc;
	ELAudioDesc_t           audio_desc;
	ELMediaMuxerDesc_t      sELMediaMuxerDesc;
	EL_BOOL					bHaveDesc;

	// for avsync
	int64_t 				base_sys_time;
	int64_t				dwVideoPlayDelta;
	int64_t                first_video_pts_per_sync;
	int64_t                first_audio_pts_per_sync;
	int64_t                i64CurrentPos;		// current play-back position, audio or video
	int64_t				beginning_video_pts;
	int64_t				beginning_audio_pts;
	EL_LONG 				nEmptyCount;		// audio empty count
	EL_LONG 				video_sync_sleep_time;
	EL_BOOL					is_video_syncing_first_frame;
	EL_BOOL					is_audio_syncing_first_frame;
	EL_BOOL					should_base_time_be_updated;

	int64_t current_video_decoded_ts;
	int64_t first_video_decoded_ts;
	int64_t avg_video_decoded_time;
	int64_t drop_frame_time_base;
	EL_INT read_retry_count;
	EL_BOOL	video_pkt_q_exceed_space_limit;
    


}el_player_engine_decoded_file_obj_t;

typedef struct _el_player_engine_obj_t_
{
	pthread_t engine_tid;
	pthread_t decode_video_tid;
	pthread_t decode_audio_tid;
	pthread_t video_sync_tid;
    
	float price_for_audio_syncer;
	float price_for_video_syncer;
	float price_for_central_engine;
	
    AVFrame *video_frame;    // frame for video decoder
	
    // 2012.10.08
    int16_t *audio_decode_buf;		 // buffer for audio decoder
	int16_t *audio_decode_buf2;		// used for convert audio sample format
    
//    AVFrame *audio_frame;
//    AVFrame *audio_frame2;
    
    EL_BOOL need_notify_ready;
	EL_BOOL notified_ready2;
	
    EL_BOOL need_sending_upper_msg;
	
    el_dynamic_reference_table_t dynamic_reference_table;
	
    EL_BOOL has_init;
} el_player_engine_obj_t;


typedef struct _el_upper_layer_msg_t_
{
	long msg;
	long arg1;
	long arg2;
	long arg3;
}el_upper_layer_msg_t;

typedef enum _el_q_id_e_
{
	EL_Q_ID_NONE_ID = 0,
	EL_Q_ID_PKT_VIDEO,
	EL_Q_ID_PKT_AUDIO,
	EL_Q_ID_PKT_FREE,
	EL_Q_ID_FRM_VIDEO,
	EL_Q_ID_FRM_AUDIO,
	EL_Q_ID_FRM_FREE_VIDEO,
	EL_Q_ID_FRM_FREE_AUDIO,
	EL_Q_ID_MAX
}el_q_id_e;

#define el_min(a,b) (((a) < (b)) ? (a):(b))
#define el_max(a,b) (((a) > (b)) ? (a):(b))
#define el_abs(a) (((a) >= 0) ? (a) : -(a))

#define EL_MS(sec) ((sec) * 1000)
#define EL_MS_2_US(msec) ((msec) * 1000)

#define EL_MAX_PKT_Q_TS 	EL_MS(0.6)  			// in ms unit
#define EL_MAX_PKT_Q_NETWORK_TS EL_MS(40)	// in ms unit
#define EL_MAX_PKT_Q_NETWORK_FIRST_BUFFERING_TS EL_MS(6)	// in s unit
// buffering time. Not for the initial buffering
#define EL_MAX_PKT_Q_NETWORK_BUFFERING_TS EL_MS(12)	// in ms unit

#define EL_MAX_FRM_VIDEO_Q_NODE_CNT 20	// max frm q video frame count

#define EL_MAX_FRM_Q_TIME_LIMIT (0.7) // s

#define EL_MAX_FRM_AUDIO_Q_NODE_CNT 40	// max frm q audio frame count

#define EL_MEGA_SIZE(t) ((t) * 1024 * 1024)
#define EL_MAX_FRM_VIDEO_Q_MEM_SPACE  EL_MEGA_SIZE(40)  // 40M space size

#define EL_MAX_PKT_VIDEO_Q_MEM_SPACE  EL_MEGA_SIZE(10)  // 10M space size
#define EL_MAX_PKT_AUDIO_Q_MEM_SPACE  EL_MEGA_SIZE(5)  // 5M space size

#define EL_MAX_READ_CNT_IN_ONE_READ_MSG 30

#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio
#define EL_MAX_AUDIO_FRAME_SIZE (AVCODEC_MAX_AUDIO_FRAME_SIZE * 2)

EL_STATIC el_player_engine_decoded_file_obj_t el_media_file_blk;
EL_STATIC el_player_engine_obj_t	el_media_player_blk;

EL_STATIC EL_BOOL el_player_engine_init();
EL_STATIC EL_BOOL el_player_engine_uninit();
EL_STATIC EL_BOOL el_player_engine_open_media(EL_CONST EL_UTF8* a_pszUrlOrPathname);
EL_STATIC EL_BOOL el_player_engine_close_media();
EL_STATIC EL_BOOL el_player_engine_get_description(ELMediaMuxerDesc_t *aMediaMuxerDesc,ELVideoDesc_t *aVideoDesc,ELAudioDesc_t *aAudioDesc);

EL_STATIC EL_BOOL el_player_engine_start(void);
EL_STATIC EL_BOOL el_player_engine_stop(void);
EL_STATIC EL_BOOL el_player_engine_pause(void);
EL_STATIC EL_INT  el_player_engine_seek_to(EL_SIZE a_SeekPos);
EL_STATIC EL_BOOL el_player_engine_play(void);
void el_player_send_message_to_ui(EL_INT nMessage, EL_VOID* wParam,EL_VOID* lParam);


EL_STATIC EL_VOID el_player_engine_notify(EL_INT a_nNotifyMsg, EL_VOID* wParam,EL_VOID* lParam);
EL_STATIC EL_BOOL el_decoder_get_video_sample(ELAVSample_t** aELVideoSample);
EL_STATIC EL_BOOL el_decoder_get_audio_sample(ELAVSample_t** aELAudioSample);
EL_STATIC EL_BOOL el_decoder_consume_video_sample(ELAVSample_t** aELVideoSample);
EL_STATIC EL_BOOL el_decoder_consume_audio_sample(ELAVSample_t** aELAudioSample);
EL_STATIC EL_VOID el_decoder_free_audio_buffer(ELAVSample_t *audio_sample);
EL_STATIC EL_VOID el_decoder_free_video_buffer(ELAVSample_t *video_samp);
EL_STATIC void el_do_close_central_engine(void);


EL_STATIC void el_adjust_ffmpeg_mms_url(int type,char *url);
EL_STATIC void el_player_engine_central_state_machine(el_player_engine_state_machine_t *mac,el_player_engine_evt_e evt);
EL_STATIC void el_state_machine_change_state(el_player_engine_state_machine_t *mac,
	el_player_engine_state_e new_state);
EL_STATIC void *el_video_sync_thread(void *arg);
EL_STATIC void el_player_engine_decoder_state_machine(el_player_engine_state_machine_t *mac,el_player_engine_evt_e evt);

EL_STATIC void el_frm_q_full_wait_available_cb(void *mac);
EL_STATIC int sync_av_after_seek(int64_t* av_sync_time);
EL_STATIC void el_video_frm_q_empty_func_cb(void *mac);
EL_STATIC void el_pkt_q_empty_goto_buffering_cb(void *mac);
EL_STATIC void el_launch_cmo_buffering(void);
EL_STATIC void packet_node_free(el_ffmpeg_packet_qnode_t *pqnode);

EL_STATIC void el_pkt_q_push_cb(void *q,struct list_node *new_item);
EL_STATIC void el_pkt_q_pop_cb(void *q,struct list_node *poped_item);

EL_STATIC void el_decoder_controller_state_machine(el_player_engine_state_machine_t * mac, el_player_engine_evt_e evt);
EL_STATIC int el_decode_one_video_packet(el_ffmpeg_packet_qnode_t *pkt_qnode);
EL_STATIC void el_reset_decoder_controller(void);
EL_STATIC void el_resume_decoder_controller(void);


EL_STATIC EL_BOOL el_engine_start_render();
EL_STATIC void el_engine_stop_render();
EL_STATIC void el_engine_pause_render();
EL_STATIC void el_engine_resume_render();
EL_STATIC void el_engine_reset_render();

// player decoder media mm collection system
EL_STATIC  el_ffmpeg_packet_qnode_t *el_allocate_packet_node(void);
EL_STATIC  void el_free_packet_node(el_ffmpeg_packet_qnode_t *free_node);
EL_STATIC  el_ffmpeg_frame_qnode_t *el_allocate_frm_node(el_player_media_type_e type,
	unsigned long required_size);
EL_STATIC  void el_free_frm_node(el_player_media_type_e type,el_ffmpeg_frame_qnode_t *free_node);
EL_STATIC  void el_init_free_qs(void);
EL_STATIC  void el_destroy_free_qs(void);

#endif // _EL_MEDIA_PLAYER_ENGINE_H_


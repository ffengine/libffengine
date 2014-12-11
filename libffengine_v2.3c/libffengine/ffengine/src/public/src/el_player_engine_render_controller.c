#include "el_ipc.h"
#include "el_media_player_engine.h"
#include "el_log.h"

#include <sys/time.h>

// 20120928
#define USE_MARKET_PRICE

#ifdef USE_MARKET_PRICE
float el_get_video_syncer_price(void)
{
	int r = el_media_file_blk.video_frm_q.node_nr;
	int n = el_media_file_blk.video_frm_q.max_node_nr;
	if(r != 0 && n != 0)
	{
		float p = ((float)n * n / 4) / (r * r);
		//el_media_player_blk.price_for_video_syncer = p;
		return p;
	}
	return 1.0f;
}

EL_STATIC float el_get_audio_syncer_price(void)
{
	int r = el_media_file_blk.audio_frm_q.node_nr;
	int n = el_media_file_blk.audio_frm_q.max_node_nr;
	if(r != 0 && n != 0)
	{
		float p = ((float)n * n / 4) / (r * r);
		//el_media_player_blk.price_for_audio_syncer = p;
		//EL_DBG_LOG("video:price = %f, r = %d, n = %d\n", p,r,n);
		return p;
	}
	return 1.0f;
}
#endif

//
// return value:
// -1 error occured, e.g. q is empty
// 0 no drop, av synchronized
// n  drop count
EL_STATIC int sync_av_after_seek(int64_t* av_sync_time)
{
	EL_DBG_FUNC_ENTER;

	ELAVSample_t* pAudioSample = EL_NULL;
	ELAVSample_t* pVideoSample = EL_NULL;

	int64_t start_time = 0;
	int drop_count = 0;

	// get first audio sample
	if (!el_decoder_get_audio_sample(&pAudioSample))
	{
		EL_DBG_FUNC_EXIT;
		return -1;
	}

//	EL_DBG_LOG("sync_av_after_seek, the first audio TS: %lld", pAudioSample->dwTS);

	// get first video sample
	if (!el_decoder_get_video_sample(&pVideoSample))
	{
		EL_DBG_FUNC_EXIT;
		return -1;
	}

//	EL_DBG_LOG("sync_av_after_seek, the first video TS: %lld", pVideoSample->dwTS);

	if (*av_sync_time == 0)
	{
		// set start-time to maximum of audio and video
		start_time = pAudioSample->dwTS;

		if (start_time < pVideoSample->dwTS)
			start_time = pVideoSample->dwTS;

		EL_DBG_LOG("sync_av_after_seek, start_time set to: %lld", start_time);
		*av_sync_time = start_time;
	}
	else
	{
		start_time = *av_sync_time;
	}

	// drop audio if necessary
	while (pAudioSample->dwTS < start_time)
	{
		if (el_decoder_consume_audio_sample(&pAudioSample))
		{
			EL_DBG_LOG("sync_av_after_seek, drop a audio sample, TS: %lld", pAudioSample->dwTS);
			el_decoder_free_audio_buffer(pAudioSample);
			pAudioSample = EL_NULL;
			drop_count++;
		}
		else
        {
			break;
        }

		if (!el_decoder_get_audio_sample(&pAudioSample))
        {
			break;
        }
	}

	// drop video if necessary
	while (pVideoSample->dwTS < start_time)
	{
		if (el_decoder_consume_video_sample(&pVideoSample))
		{
			EL_DBG_LOG("sync_av_after_seek, drop a video sample, TS: %lld", pVideoSample->dwTS);
			el_decoder_free_video_buffer(pVideoSample);
			pVideoSample = EL_NULL;
			drop_count++;
		}
		else
		{
			EL_DBG_LOG("sync_av_after_seek, consume_video_sample failed!");
			break;
		}

		if (!el_decoder_get_video_sample(&pVideoSample))
			break;
	}

	EL_DBG_LOG("drop count = %d\n",drop_count);
	EL_DBG_FUNC_EXIT;
	return drop_count;
}

EL_STATIC void trans_audio(void *samples, int audio_volume, int decoded_data_size, int sample_fmt)
{
	int i = 0;

    // preprocess audio (volume)
    if (audio_volume != 256) {
        switch (sample_fmt) {
        case AV_SAMPLE_FMT_U8:
        {
            uint8_t *volp = samples;
            for (i = 0; i < (decoded_data_size / sizeof(*volp)); i++) {
                int v = (((*volp - 128) * audio_volume + 128) >> 8) + 128;
                *volp++ = av_clip_uint8(v);
            }
            break;
        }
        case AV_SAMPLE_FMT_S16:
        {
            int16_t *volp = samples;
            for (i = 0; i < (decoded_data_size / sizeof(*volp)); i++) {
                int v = ((*volp) * audio_volume + 128) >> 8;
                *volp++ = av_clip_int16(v);
            }
            break;
        }
        case AV_SAMPLE_FMT_S32:
        {
            int32_t *volp = samples;
            for (i = 0; i < (decoded_data_size / sizeof(*volp)); i++) {
                int64_t v = (((int64_t)*volp * audio_volume + 128) >> 8);
                *volp++ = av_clipl_int32(v);
            }
            break;
        }
        case AV_SAMPLE_FMT_FLT:
        {
            float *volp = samples;
            float scale = audio_volume / 256.f;
            for (i = 0; i < (decoded_data_size / sizeof(*volp)); i++) {
                *volp++ *= scale;
            }
            break;
        }
        case AV_SAMPLE_FMT_DBL:
        {
            double *volp = samples;
            double scale = audio_volume / 256.f;
            for (i = 0; i < (decoded_data_size / sizeof(*volp)); i++) {
                *volp++ *= scale;
            }
            break;
        }
        default:
			av_log(NULL, AV_LOG_FATAL,
				   "Audio volume adjustment on sample format %s is not supported.\n",
				   av_get_sample_fmt_name(sample_fmt));
        }
    }
}


EL_STATIC void el_player_audio_set_sample_first_play_time_cb(int64_t tm)
{
	if(el_media_file_blk.should_base_time_be_updated)
	{
		if(tm > el_media_file_blk.base_sys_time)
		{
			//printf("audio syncer: set new time base = %lld,diff = %lld" ,
			//	tm,tm - el_media_file_blk.base_sys_time);
			el_media_file_blk.base_sys_time = tm;
		}
		el_media_file_blk.should_base_time_be_updated = EL_FALSE;
	}
}

#define EL_AUDIO_SYNCER_DELAY_PER_GET EL_MS_2_US(10)  // in us
EL_STATIC EL_BOOL el_player_audio_get_sample_cb(EL_BYTE** ppSampleData, EL_INT* pDataLen)
{
	ELAVSample_t* pAudioSample = EL_NULL;
	EL_BOOL ret;

	if (el_decoder_get_audio_sample(&pAudioSample)
		&& pAudioSample != EL_NULL)
	{
		if(el_media_file_blk.is_audio_syncing_first_frame)
		{
			el_media_file_blk.base_sys_time = el_get_sys_time_ms();
			EL_DBG_LOG("audio sync: resume time = %lld\n",
				el_media_file_blk.base_sys_time);
			el_media_file_blk.first_audio_pts_per_sync = pAudioSample->dwTS;
/*
			if(el_media_file_blk.is_audio_beginning_pts_in_file)
			{
				el_media_file_blk.beginning_audio_pts = el_media_file_blk.first_audio_pts_per_sync;
				el_media_file_blk.is_audio_beginning_pts_in_file = EL_FALSE;
				EL_DBG_LOG("audio sync: beginning frame of file pts = %lld\n",el_media_file_blk.beginning_audio_pts);
			}
*/

			el_media_file_blk.is_audio_syncing_first_frame = EL_FALSE;			
		}

		if (!pAudioSample->bVolumeAdjusted)
		{
			if (1 < el_media_file_blk.nAudioVolume)
			{
				// adjust audio volume
				trans_audio(pAudioSample->pBuffer, 256*el_media_file_blk.nAudioVolume, pAudioSample->nBufferSize, AV_SAMPLE_FMT_S16);
				EL_DBG_LOG("el_player_audio_volume, buffer %x adjusted to %d",
						pAudioSample->pBuffer, el_media_file_blk.nAudioVolume);
			}

			pAudioSample->bVolumeAdjusted = EL_TRUE;
		}

		*ppSampleData = pAudioSample->pBuffer;
		*pDataLen = pAudioSample->nBufferSize;
		ret = EL_TRUE;
		goto pay_the_goods;
	}
	else
	{
		ret = EL_FALSE;
		goto pay_the_goods;
	}
pay_the_goods:

    // 20120928
#if 0
#ifdef USE_MARKET_PRICE
	{
		float p;
		int t;
		p = el_get_audio_syncer_price();
		t = (int)(p * EL_AUDIO_SYNCER_DELAY_PER_GET);
		if(t > EL_MS_2_US(150))
		{
			t = EL_MS_2_US(150);
		}
		//EL_DBG_LOG("t = %d\n",t);
		usleep(t);
	}
#endif
#endif
    
	return ret;
}

#define DELTA_CONSUME_CENTRAL 0
#define DELTA_CONSUME_UPPER (DELTA_CONSUME_CENTRAL + 300) 
#define DELTA_CONSUME_LOWER (DELTA_CONSUME_CENTRAL - 300)


EL_STATIC EL_VOID el_player_audio_consume_sample_cb()
{
	ELAVSample_t* pAudioSample = EL_NULL;

	if (el_decoder_consume_audio_sample(&pAudioSample)
		&& pAudioSample != EL_NULL)
	{
/*	
		if (el_media_file_blk.is_audio_syncing_first_frame)
		{
			el_media_file_blk.first_audio_pts_per_sync = pAudioSample->dwTS;
			el_media_file_blk.is_audio_syncing_first_frame = EL_FALSE;
			int64_t tm = el_get_sys_time_ms();
			EL_DBG_LOG("audio sync: consume time = %lld\n",tm);
		}
*/
		
		el_media_file_blk.audio_consume_time = pAudioSample->dwTS;

		if(el_media_file_blk.consume_count < 36)
		{		   
		   if(el_media_file_blk.video_consume_time != 0
		   		&& el_media_file_blk.audio_consume_time != 0)
		   {
           		el_media_file_blk.delta_consume += el_media_file_blk.audio_consume_time -
                	 el_media_file_blk.video_consume_time;
				el_media_file_blk.consume_count++;	
		   }
		}
		else
		{
			el_media_file_blk.delta_consume /= el_media_file_blk.consume_count;

		   	EL_DBG_LOG("cosume time video = %lld, audio = %lld, diff = %lld,delta = %lld, last = %lld\n",
                        el_media_file_blk.video_consume_time,
                        el_media_file_blk.audio_consume_time,
                        el_media_file_blk.audio_consume_time - el_media_file_blk.video_consume_time,
                        el_media_file_blk.delta_consume,
                        el_media_file_blk.delta_consume_last);
			el_media_file_blk.consume_count = 0;
			el_media_file_blk.delta_consume_last = el_media_file_blk.delta_consume;

			// make sure delta is within a good range
			if(el_media_file_blk.delta_consume > 5000)
			{
				el_media_file_blk.delta_consume = 5000;
			}

			if(el_media_file_blk.delta_consume < -5000)
			{
				el_media_file_blk.delta_consume = -5000;
			}
			
			if( el_media_file_blk.delta_consume > DELTA_CONSUME_UPPER)
			{
				el_media_file_blk.delta_adjusted_sys_time += (int64_t)(el_media_file_blk.delta_consume - DELTA_CONSUME_UPPER);
				EL_DBG_LOG("change adjusted delta = %lld,delta consume = %lld\n",
					el_media_file_blk.delta_adjusted_sys_time,
					el_media_file_blk.delta_consume);
			}
			else if(el_media_file_blk.delta_consume < DELTA_CONSUME_LOWER)
			{
				el_media_file_blk.delta_adjusted_sys_time += (int64_t)(el_media_file_blk.delta_consume - DELTA_CONSUME_LOWER);
				EL_DBG_LOG("change adjusted minus delta = %lld, delta consume = %lld\n",
					el_media_file_blk.delta_adjusted_sys_time,
					el_media_file_blk.delta_consume);
			}
			else
			{
				EL_DBG_LOG("adjusted delta = %lld\n",el_media_file_blk.delta_adjusted_sys_time);
			}

			el_media_file_blk.delta_consume = 0;
		}
		
		if (pAudioSample->dwTS != 0)
		{
			el_media_file_blk.i64CurrentPos = pAudioSample->dwTS - el_media_file_blk.beginning_audio_pts;
			el_player_engine_notify(EL_PLAYER_MESSAGE_MEDIA_CURRENT_POS,	(EL_VOID*)el_media_file_blk.i64CurrentPos, 0);
		}
	//	EL_DBG_LOG("EL_PLAYER_MESSAGE_MEDIA_CURRENT_POS: %d", dwAudioPlaytime);
		el_decoder_free_audio_buffer(pAudioSample);
	}
}

/////////////////////////////////////////////////////////


EL_STATIC void el_player_do_video_sync(void)
{
	ELAVSample_t*                     pVideoFrame      = EL_NULL;
	int64_t                               dwCurrentTime    = 0;

    // 判断是否需要sleep，可能video解码时间较快，所以需要等待，默认为10ms
	el_media_file_blk.video_sync_sleep_time = 10;

    // 如果没有视频流，直接退出88
	if (!(el_media_file_blk.av_support & EL_ENGINE_HAS_VIDEO))
    {
		return;
    }

    // 是第一次同步，或者seek后第一次同步
	if (el_media_file_blk.is_video_syncing_first_frame)
	{
		el_ffmpeg_frame_qnode_t *frm_node;  // 视频帧
		EL_BOOL got_first_frame = EL_FALSE; // 是否获取到了第一帧
		el_media_file_blk.base_sys_time = el_get_sys_time_ms(); // base time

		EL_DBG_LOG("video sync: resume time = %lld\n",el_media_file_blk.base_sys_time);

		el_mutex_lock(el_media_file_blk.video_frm_q.mutex); // 锁定视频帧流

        // 从视频帧里面拿依次数据
		list_for_each_entry(frm_node,&el_media_file_blk.video_frm_q.lst_node,list)
		{
			got_first_frame = EL_TRUE;
			pVideoFrame = &frm_node->frm_sample;
			el_media_file_blk.first_video_pts_per_sync = pVideoFrame->dwTS;

			if(el_media_file_blk.seekpos == 0)
			{
				EL_DBG_LOG("video sync: first_video_pts_per_sync set to %lld\n",el_media_file_blk.first_video_pts_per_sync);
				break;
			}

			if(el_media_file_blk.first_video_pts_per_sync != 0)
			{
				EL_DBG_LOG("video sync: after seek,first_video_pts_per_sync set to %lld\n",el_media_file_blk.first_video_pts_per_sync);
				break;
			}
		}

		el_mutex_unlock(el_media_file_blk.video_frm_q.mutex);

		if(!got_first_frame)
		{

			EL_DBG_LOG("video sync: warning! video original queue empty at start!\n");
		}
	
		el_media_file_blk.is_video_syncing_first_frame = EL_FALSE;
	}

    dwCurrentTime = el_get_sys_time_ms();
//	dwCurrentTime = el_get_adjusted_sys_time_ms();
	int64_t time_line = dwCurrentTime - el_media_file_blk.base_sys_time;

	//EL_DBG_LOG("el_player_media_engine, video time_line: %lld, base_sys_time = %lld\n", time_line,el_media_file_blk.base_sys_time);

    // xw, el_player_do_video_sync
	while (el_decoder_get_video_sample(&pVideoFrame) && pVideoFrame != EL_NULL)
	{
		int64_t ts_line = pVideoFrame->dwTS - el_media_file_blk.first_video_pts_per_sync;
		int64_t dwVideoPlayDelta = ts_line - time_line;
		el_media_file_blk.dwVideoPlayDelta = dwVideoPlayDelta;

		//EL_DBG_LOG("el_player_media_engine g_dropNonkey, ts_line: %lld\n", ts_line);
		//EL_DBG_LOG("el_player_media_engine g_dropNonkey, pVideoFrame->dwTS: %lld\n", pVideoFrame->dwTS);
		//EL_DBG_LOG("el_player_media_engine g_dropNonkey, dwVideoPlayDelta: %lld\n", dwVideoPlayDelta);
        
        if (1800000 < dwVideoPlayDelta)
        {
            EL_DBG_LOG("el_player_media_engine, first_video_pts_per_sync error, update to: %lld", pVideoFrame->dwTS);
            el_media_file_blk.first_video_pts_per_sync = pVideoFrame->dwTS;
            dwVideoPlayDelta = 10;
        }

		if (0 < dwVideoPlayDelta)
		{
			el_media_file_blk.video_sync_sleep_time = (int)dwVideoPlayDelta;
			break;
		}

		if (!el_decoder_consume_video_sample(&pVideoFrame) || (pVideoFrame == EL_NULL))
		{
			// code should not go here, because get-video-sample is OK
			//printf("el_player_media_engine, error! consume video queue failed");
			continue;
		}

        // xw 开始绘制
		//printf("av sync display frame\n");
		el_media_file_blk.video_render->filter_display_frame(pVideoFrame->pBuffer, pVideoFrame->nBufferSize,pVideoFrame->width, pVideoFrame->height);

		el_media_file_blk.video_consume_time = pVideoFrame->dwTS;

		if (!(el_media_file_blk.av_support & EL_ENGINE_HAS_AUDIO))
		{
			el_media_file_blk.i64CurrentPos = pVideoFrame->dwTS - el_media_file_blk.beginning_video_pts;
			el_player_engine_notify(EL_PLAYER_MESSAGE_MEDIA_CURRENT_POS, (EL_VOID*)el_media_file_blk.i64CurrentPos, 0);
		}

		el_decoder_free_video_buffer(pVideoFrame);
        
        // xw 绘制完毕
        
		break;
	}

//	pVideoFrame = EL_NULL;
}

EL_STATIC void el_do_process_video_sync_idle(el_player_engine_state_machine_t *mac,el_player_engine_evt_e evt)
{
	switch(evt)
	{
		case EL_ENGINE_EVT_START:
		{
			EL_DBG_LOG("video syncer: idle state recv evt open\n");
//			el_media_file_blk.is_video_beginning_pts_in_file = EL_TRUE;
//			el_media_file_blk.is_audio_beginning_pts_in_file = EL_TRUE;
			el_state_machine_change_state(mac,EL_ENGINE_VIDEO_SYNC_STATE_WAIT);
			return;
		}
		default:
		{
			EL_DBG_POS;
			return;
		}
	}
	return;
}

EL_STATIC void el_do_process_video_sync_wait(el_player_engine_state_machine_t *mac,el_player_engine_evt_e evt)
{
	switch(evt)
	{
		case EL_ENGINE_EVT_PLAY:
		{
			el_state_machine_change_state(mac,EL_ENGINE_VIDEO_SYNC_STATE_WORK);
			el_media_file_blk.is_video_syncing_first_frame = EL_TRUE;
			el_media_file_blk.base_sys_time = 0;
			el_media_file_blk.is_audio_syncing_first_frame = EL_TRUE;
			el_ipc_send(EL_ENGINE_EVT_VIDEO_SYNC_MAC_GO_ON,mac->ipc_fd);			
			return;
		}
		case EL_ENGINE_EVT_STOP:
		{
			el_state_machine_change_state(mac,EL_ENGINE_VIDEO_SYNC_STATE_IDLE);
			return;
		}
		default:
		{
			EL_DBG_POS;
			return;
		}
	}
	return;
}

EL_STATIC void el_do_process_video_sync_work(el_player_engine_state_machine_t *mac,el_player_engine_evt_e evt)
{
	switch(evt)
	{
		case EL_ENGINE_EVT_VIDEO_SYNC_MAC_GO_ON: 
		{
			el_player_do_video_sync();
/*			
			if(el_media_file_blk.video_sync_sleep_time > 100)
			{
				el_media_file_blk.video_sync_sleep_time = 100;
			}
*/
            // 20120928
#ifdef USE_MARKET_PRICE
 			float p = el_get_video_syncer_price();
			int t = (int)(p * el_media_file_blk.video_sync_sleep_time*1000);
			//EL_DBG_LOG("t = %d, sleep time = %d, p = %f\n",t,el_media_file_blk.video_sync_sleep_time,p);
			//printf("t = %d, sleep time = %d, p = %f\n",t,el_media_file_blk.video_sync_sleep_time,p);
			if(t > EL_MS_2_US(300))
			{
				t = EL_MS_2_US(300);
			}
			if(t < el_media_file_blk.video_sync_sleep_time)
			{
				t = EL_MS_2_US(el_media_file_blk.video_sync_sleep_time);
			}
            
			usleep(t);
#endif
		
			el_ipc_send(EL_ENGINE_EVT_VIDEO_SYNC_MAC_GO_ON,mac->ipc_fd);

			return;
		}
		case EL_ENGINE_EVT_PAUSE:
		{
			el_state_machine_change_state(mac,EL_ENGINE_VIDEO_SYNC_STATE_WAIT);
			return;
		}
		case EL_ENGINE_EVT_STOP:
		{
			el_state_machine_change_state(mac,EL_ENGINE_VIDEO_SYNC_STATE_IDLE);
			return;
		}
		case EL_ENGINE_EVT_PLAY_COMPLETE:
		{
			el_state_machine_change_state(mac,EL_ENGINE_VIDEO_SYNC_STATE_WAIT);
			return;
		}
		default:
		{
			EL_DBG_POS;
			return;
		}
	}

	return;
}

EL_STATIC void el_player_video_syncer_state_machine(el_player_engine_state_machine_t *mac,el_player_engine_evt_e evt)
{
	switch(mac->state)
	{
		case EL_ENGINE_VIDEO_SYNC_STATE_IDLE:
		{
			el_do_process_video_sync_idle(mac,evt);
			return;
		}
		case EL_ENGINE_VIDEO_SYNC_STATE_WAIT:
		{
			el_do_process_video_sync_wait(mac,evt);
			return;
		}
		case EL_ENGINE_VIDEO_SYNC_STATE_WORK:
		{
			el_do_process_video_sync_work(mac,evt);
			return;
		}
		default:
		{
			EL_DBG_LOG("Invalid video sync state!\n");
			return;
		}
	}
}

EL_STATIC void *el_video_sync_thread(void *arg)
{
	int msg;
	int ret;
	int64_t tm;

	EL_DBG_FUNC_ENTER;
	// read the packets from media file and buffer them to a queue
	while(1)
	{
        FUNC_DEBUG_START
        
		if((ret = el_ipc_wait(el_msg_q_video_sync,&msg,&tm)) > 0)
		{
			if(msg == EL_ENGINE_EVT_EXIT_THREAD)
			{
				break;
			}

			el_player_video_syncer_state_machine(&el_media_file_blk.video_syncer,msg);
		}
        
        FUNC_DEBUG_END(msg)
	}
    
	EL_DBG_FUNC_EXIT;
	return ((void *)0);
}

/////////////////////////////////////////////////////////

EL_STATIC EL_BOOL el_engine_start_render()
{
	int ret;
	pthread_t video_sync_tid;
	ELAudioRenderAudioParms_s          sAudioRenderAudioParms;

	EL_DBG_FUNC_ENTER;
	//printf("********start render\n");
	
	// start video sync thread
	ret = pthread_create(&video_sync_tid,NULL,el_video_sync_thread,NULL);
	if(ret)
	{
		EL_DBG_LOG("render: fatal err, create video syncer err %d\n",ret);
		goto lbl_err_exit;
	}	
	el_media_player_blk.video_sync_tid = video_sync_tid;

	el_ipc_send(EL_ENGINE_EVT_START,el_msg_q_video_sync);

 	if(el_media_file_blk.av_support & EL_ENGINE_HAS_VIDEO)
	{
		el_media_file_blk.video_render = el_player_get_video_render_filter();
		if (EL_NULL == el_media_file_blk.video_render)
		{
			EL_DBG_LOG("error: player_media_render::get video render failed");
			goto lbl_err_exit;
		}

		el_media_file_blk.video_render->filter_init();

	}

 	if(el_media_file_blk.av_support & EL_ENGINE_HAS_AUDIO)
	{
		el_media_file_blk.audio_render = el_player_get_audio_render_filter();
		if (EL_NULL == el_media_file_blk.audio_render)
		{
			EL_DBG_LOG("error: player_media_render::get audio render failed");
			goto lbl_err_exit;
		}

		el_media_file_blk.audio_render->filter_init(
			el_player_audio_get_sample_cb,
			el_player_audio_consume_sample_cb,
			el_player_audio_set_sample_first_play_time_cb);

		EL_DBG_LOG("info:el_player_media_render:: audio render filter init success");
		el_media_file_blk.should_base_time_be_updated = EL_TRUE;

		sAudioRenderAudioParms.eAudioSampleFmt = el_media_file_blk.audio_desc.eAudioCodecID;
		sAudioRenderAudioParms.iChannelCount   = el_media_file_blk.audio_desc.eChannelCount;
		sAudioRenderAudioParms.uiSampleRate    = el_media_file_blk.audio_desc.euiFreq;
		sAudioRenderAudioParms.uiBitsPerSample = el_media_file_blk.audio_desc.uiBitsPerSample;
		el_media_file_blk.audio_render->filter_set_param(sAudioRenderAudioParms);
	}

	EL_DBG_FUNC_EXIT;
 	return EL_TRUE;

lbl_err_exit:

	EL_DBG_FUNC_EXIT;
	return EL_FALSE;
}

EL_STATIC void el_engine_stop_render()
{
	EL_DBG_FUNC_ENTER;
	el_ipc_send(EL_ENGINE_EVT_STOP,el_msg_q_video_sync);
	el_ipc_send(EL_ENGINE_EVT_EXIT_THREAD,el_msg_q_video_sync);

	EL_DBG_LOG("render: begin join video syncer\n");
	if(el_media_player_blk.video_sync_tid)
	{
		pthread_join(el_media_player_blk.video_sync_tid,NULL);
		el_media_player_blk.video_sync_tid = EL_NULL;
	}
	EL_DBG_LOG("render: after join video syncer\n");

	if (el_media_file_blk.video_render && el_media_file_blk.video_render->filter_uninit)
	{
		el_media_file_blk.video_render->filter_uninit();	
	}

	if (el_media_file_blk.audio_render)
	{
		el_media_file_blk.audio_render->filter_stop();
	}
	
	if (el_media_file_blk.audio_render && el_media_file_blk.audio_render->filter_unint)
	{
		el_media_file_blk.audio_render->filter_unint();
		el_media_file_blk.should_base_time_be_updated = EL_TRUE;
	}

	EL_DBG_FUNC_EXIT;
	return;
}

EL_STATIC void el_engine_pause_render()
{
	EL_DBG_FUNC_ENTER;
	el_ipc_send(EL_ENGINE_EVT_PAUSE,el_msg_q_video_sync);
	if(el_media_file_blk.audio_render && (el_media_file_blk.av_support & EL_ENGINE_HAS_AUDIO))
	{
  		el_media_file_blk.audio_render->filter_pause();
	}
	
	EL_DBG_FUNC_EXIT;
	return;
}

EL_STATIC void el_engine_resume_render()
{
	EL_DBG_FUNC_ENTER;
	el_media_file_blk.is_video_syncing_first_frame = EL_TRUE;
	el_media_file_blk.base_sys_time = 0;
    el_media_file_blk.first_video_decoded_ts = el_media_file_blk.current_video_decoded_ts;
    
	el_ipc_send(EL_ENGINE_EVT_PLAY,el_msg_q_video_sync);
    
	if (    el_media_file_blk.audio_render 
        &&  (el_media_file_blk.av_support & EL_ENGINE_HAS_AUDIO))
	{
		el_media_file_blk.audio_render->filter_resume();
        
		el_media_file_blk.is_audio_syncing_first_frame = EL_TRUE;
		EL_DBG_LOG("audio sync in resume: resume time = %lld\n",
			el_media_file_blk.base_sys_time);			
	}
	
	EL_DBG_FUNC_EXIT;
	return;
}

EL_STATIC void el_engine_reset_render()
{
	EL_DBG_FUNC_ENTER;

	el_media_file_blk.consume_count = 0;
	el_media_file_blk.delta_consume = 0;
	el_media_file_blk.audio_consume_time = 0;
	el_media_file_blk.video_consume_time = 0;

    if (el_media_file_blk.audio_render && (el_media_file_blk.av_support & EL_ENGINE_HAS_AUDIO))
    {
    
		ELAudioRenderAudioParms_s          sAudioRenderAudioParms;
		el_media_file_blk.audio_render->filter_stop();
		el_media_file_blk.audio_render->filter_unint();

		el_media_file_blk.audio_render->filter_init(
			el_player_audio_get_sample_cb,
			el_player_audio_consume_sample_cb,
			el_player_audio_set_sample_first_play_time_cb);
		el_media_file_blk.should_base_time_be_updated = EL_TRUE;

		EL_DBG_LOG("info:el_player_media_render:: audio render filter init success");

		sAudioRenderAudioParms.eAudioSampleFmt = el_media_file_blk.audio_desc.eAudioCodecID;
		sAudioRenderAudioParms.iChannelCount   = el_media_file_blk.audio_desc.eChannelCount;
		sAudioRenderAudioParms.uiSampleRate    = el_media_file_blk.audio_desc.euiFreq;
		sAudioRenderAudioParms.uiBitsPerSample = el_media_file_blk.audio_desc.uiBitsPerSample;
		el_media_file_blk.audio_render->filter_set_param(sAudioRenderAudioParms);
	}

	EL_DBG_FUNC_EXIT;
	return;
}

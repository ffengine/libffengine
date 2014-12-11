
#include "el_media_player_engine.h"
#include "el_log.h"
#include "el_mutex.h"

// global player object which stored lower ffmpeg information
EL_STATIC el_player_engine_decoded_file_obj_t el_media_file_blk = {
	.engine = 
	{
		.state = EL_ENGINE_CENTRAL_STATE_IDLE
	},
	.video_decoder =
	{
		.state = EL_ENGINE_DECODE_STATE_START
	},
	.audio_decoder =
	{
		.state = EL_ENGINE_DECODE_STATE_START
	},
	.video_syncer =
	{
		.state = EL_ENGINE_VIDEO_SYNC_STATE_IDLE
	},
	.decoder_controller = 
	{
		.state = EL_ENGINE_DECODE_CTRL_START
	}
	
};

EL_STATIC el_player_engine_obj_t	el_media_player_blk = {
	.has_init = EL_FALSE,
};

// return 1 means want interrupt
// 2012.10.08
//EL_STATIC int el_url_interrupt(void)
//{
//	return 1;
//}
//
//EL_STATIC int el_url_noninterrupt(void)
//{
//	return 0;
//}

EL_STATIC void *el_engine_thread(void *arg)
{
	int msg,ret;
	int64_t tm;

	EL_DBG_FUNC_ENTER;
	// read the packets from media file and buffer them to a queue
	while(1)
	{	
FUNC_DEBUG_START
        
		if((ret = el_ipc_wait(el_msg_q_engine,&msg,&tm)) > 0)
		{
			if(msg == EL_ENGINE_EVT_EXIT_THREAD)
			{
				break;
			}

			if(msg == EL_ENGINE_EVT_CLOSE)
			{
				// process close
				EL_DBG_LOG("close in read\n");
				// FIX BUG1781, change to idle state as soon as receiving the close event				
				el_state_machine_change_state(&el_media_file_blk.engine,EL_ENGINE_CENTRAL_STATE_IDLE);
				el_ipc_clear(el_msg_q_engine);
				//el_ipc_send(el_media_player_blk.read_socket,EL_ENGINE_EVT_CLOSE,EL_CLOSE_SOCKET_PORT);
				el_ipc_send(EL_ENGINE_EVT_CLOSE,el_msg_q_close);
				continue;  // should use continue
			}

			if(msg == EL_ENGINE_EVT_SEEK)
			{
				el_player_engine_central_state_machine(&el_media_file_blk.engine,EL_ENGINE_EVT_SEEK_PAUSE);
				el_player_engine_central_state_machine(&el_media_file_blk.engine,EL_ENGINE_EVT_SEEK);
			}
			else
			{ 
				el_player_engine_central_state_machine(&el_media_file_blk.engine,msg);
			}
		}
        
FUNC_DEBUG_END(msg)  
	}

	EL_DBG_FUNC_EXIT;
	return ((void *)0);
}


EL_STATIC void *el_decode_video_thread(void *arg)
{
	int msg,ret;
	int64_t tm;

	EL_DBG_FUNC_ENTER;

	while(1)
	{
        FUNC_DEBUG_START

		if((ret = el_ipc_wait(el_msg_q_decode_video,&msg,&tm)) <= 0)
		{
			// step into decode loop loginc, to make it loops, we send message selfly
			el_ipc_send(EL_ENGINE_EVT_DECODE_MAC_GO_ON,el_msg_q_decode_video);
			continue;
		}

		if(msg == EL_ENGINE_EVT_EXIT_THREAD)
		{
			break;
		}

		if(msg == EL_ENGINE_EVT_CLOSE)
		{
			// process close
			//EL_DBG_LOG("close in decode video\n");
			el_state_machine_change_state(&el_media_file_blk.video_decoder,EL_ENGINE_DECODE_STATE_START);
			el_ipc_clear(el_msg_q_decode_video);
			el_ipc_send(EL_ENGINE_EVT_CLOSE,el_msg_q_close);
			continue;  // should use continue	
		}

		if(el_media_file_blk.av_support & EL_ENGINE_HAS_VIDEO)
		{
			if(msg == EL_ENGINE_EVT_SEEK)
			{
				el_player_engine_decoder_state_machine(&el_media_file_blk.video_decoder, EL_ENGINE_EVT_SEEK_PAUSE);
				el_player_engine_decoder_state_machine(&el_media_file_blk.video_decoder, EL_ENGINE_EVT_SEEK);
			}
			else
			{
                // 20120928,这里非常重要,否则会导致这个线程占用资源严重的现象,不停的go on, go on
                if( msg == EL_ENGINE_EVT_MAC_GO_ON || msg == EL_ENGINE_EVT_DECODE_MAC_GO_ON)
                {
                    usleep(10000);
                }

				el_player_engine_decoder_state_machine(&el_media_file_blk.video_decoder, msg);
			}
		}
        
        FUNC_DEBUG_END(msg)
	}

	EL_DBG_FUNC_EXIT;
	return ((void *)0);
}

EL_STATIC void *el_decode_audio_thread(void *arg)
{
	int msg,ret;
	int64_t tm;

	EL_DBG_FUNC_ENTER;

	while(1)
	{
        FUNC_DEBUG_START
        
		if((ret = el_ipc_wait(el_msg_q_decode_audio,&msg,&tm)) <= 0)
		{
			// step into decode loop loginc, to make it loops, we send message selfly
			el_ipc_send(EL_ENGINE_EVT_DECODE_MAC_GO_ON,el_msg_q_decode_audio);
			continue;
		}

		if(msg == EL_ENGINE_EVT_EXIT_THREAD)
		{
			break;
		}

		if(msg == EL_ENGINE_EVT_CLOSE)
		{
			// process close
			//EL_DBG_LOG("close in decode audio\n");
			el_state_machine_change_state(&el_media_file_blk.audio_decoder,EL_ENGINE_DECODE_STATE_START);
			el_ipc_clear(el_msg_q_decode_audio);
			//el_ipc_send(el_media_player_blk.decode_socket,EL_ENGINE_EVT_CLOSE,EL_CLOSE_SOCKET_PORT);
			el_ipc_send(EL_ENGINE_EVT_CLOSE,el_msg_q_close);
			continue;  // should use continue	
		}

		if(el_media_file_blk.av_support & EL_ENGINE_HAS_AUDIO)
		{
			if(msg == EL_ENGINE_EVT_SEEK)
			{		
				el_player_engine_decoder_state_machine(&el_media_file_blk.audio_decoder,EL_ENGINE_EVT_SEEK_PAUSE);
				el_player_engine_decoder_state_machine(&el_media_file_blk.audio_decoder,EL_ENGINE_EVT_SEEK);
			}
			else
			{
                if( msg == EL_ENGINE_EVT_MAC_GO_ON || msg == EL_ENGINE_EVT_DECODE_MAC_GO_ON)
                {
                    usleep(10000);
                }
                
				el_player_engine_decoder_state_machine(&el_media_file_blk.audio_decoder,msg);
			}
		}
        
        FUNC_DEBUG_END(msg)
	}

	EL_DBG_FUNC_EXIT;
	return ((void *)0);
}

EL_STATIC EL_BOOL el_player_engine_init()
{
	pthread_t engine_tid,decode_tid,decode_audio_tid;
	int ret;

	EL_DBG_FUNC_ENTER;

	/* ignore SIGPIPE to avoid unexpected quit */
	signal(SIGPIPE, SIG_IGN);

	if(el_media_player_blk.has_init)
	{
		EL_DBG_FUNC_EXIT;
		return EL_FALSE;
	}
	
	memset(&el_media_player_blk,0,sizeof(el_player_engine_obj_t));

    // 2012.10.11, 在SDK初始化时，调用一次这些方法
	// Register all formats and codecs
    avcodec_register_all();
	av_register_all();
    avformat_network_init();

	el_media_player_blk.video_frame = avcodec_alloc_frame();
	if(!el_media_player_blk.video_frame)
	{
		EL_DBG_FUNC_EXIT;
		return EL_FALSE;
	}
    
	// NOTE: we must use av_malloc here, when allcating audio deocded buffer
	el_media_player_blk.audio_decode_buf = (int16_t *)av_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE*3/2);
	if(!el_media_player_blk.audio_decode_buf)
	{
		EL_DBG_LOG("prepare audio buf not OK!\n");
		EL_DBG_FUNC_EXIT; 
		goto init_eout6;
	}

	el_media_player_blk.audio_decode_buf2 = (int16_t *)av_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE*3/2);
	if(!el_media_player_blk.audio_decode_buf2)
	{
		EL_DBG_LOG("prepare audio buf2 not OK!\n");
		EL_DBG_FUNC_EXIT; 
		goto init_eout8;
	}		

	el_ipc_create();

	// start read thread
	ret = pthread_create(&engine_tid, NULL, el_engine_thread, NULL);
	if(ret)
	{
		EL_DBG_LOG("create reader thread err %d\n",ret);
		goto init_eout1;
	}

	el_media_player_blk.engine_tid = engine_tid;

	// start decode thread
	ret = pthread_create(&decode_tid, NULL, el_decode_video_thread, NULL);
	if(ret)
	{
		EL_DBG_LOG("create decode thread err %d\n",ret);
		goto init_eout1;
	}

	// start decode thread
	ret = pthread_create(&decode_audio_tid,NULL,el_decode_audio_thread,NULL);
	if(ret)
	{
		EL_DBG_LOG("create decode thread err %d\n",ret);
		goto init_eout1;
	}	

	el_media_player_blk.decode_audio_tid = decode_audio_tid;

	el_media_player_blk.dynamic_reference_table.el_ui_message_receiver_func 
		= el_player_send_message_to_ui;
	
	el_media_player_blk.has_init = EL_TRUE;
	el_media_file_blk.abort_request = EL_TRUE;
	EL_DBG_FUNC_EXIT;
	return EL_TRUE;

init_eout1:
	el_ipc_delete();
	av_free(el_media_player_blk.audio_decode_buf2);
init_eout8:
	av_free(el_media_player_blk.audio_decode_buf);
init_eout6:
	av_free(el_media_player_blk.video_frame);
	EL_DBG_FUNC_EXIT;
	return EL_FALSE;
}

EL_STATIC EL_BOOL el_player_engine_uninit()
{
	EL_DBG_FUNC_ENTER;

	if(!el_media_player_blk.has_init)
	{
		EL_DBG_FUNC_EXIT;
		return EL_FALSE;
	}

	el_media_player_blk.has_init = EL_FALSE;

	el_ipc_send(EL_ENGINE_EVT_EXIT_THREAD,el_msg_q_engine);
	el_ipc_send(EL_ENGINE_EVT_EXIT_THREAD,el_msg_q_decode_video);
	el_ipc_send(EL_ENGINE_EVT_EXIT_THREAD,el_msg_q_decode_audio);

	pthread_join(el_media_player_blk.engine_tid,NULL);
	pthread_join(el_media_player_blk.decode_video_tid,NULL);
	pthread_join(el_media_player_blk.decode_audio_tid,NULL);

	if(el_media_player_blk.video_frame)
	{
		av_free(el_media_player_blk.video_frame);
	}

	if(el_media_player_blk.audio_decode_buf)
	{
		av_free(el_media_player_blk.audio_decode_buf);
	}

	if(el_media_player_blk.audio_decode_buf2)
	{
		av_free(el_media_player_blk.audio_decode_buf2);
	}

	el_ipc_delete();

	EL_DBG_FUNC_EXIT;
	return EL_TRUE;
}

EL_STATIC EL_BOOL el_player_engine_open_media(EL_CONST EL_UTF8* a_pszUrlOrPathname)
{
	int namelen;
	int *st_index;

	EL_DBG_FUNC_ENTER;

	if(!a_pszUrlOrPathname || !(*a_pszUrlOrPathname))
	{
		EL_DBG_FUNC_EXIT;
		return EL_FALSE;
	}

	EL_DBG_LOG("file opened: %s\n",a_pszUrlOrPathname);

    // 2012.10.08
	// url_set_interrupt_cb(el_url_noninterrupt);
    el_media_file_blk.abort_request = EL_FALSE;

	el_ipc_clear(el_msg_q_close);
	el_ipc_clear(el_msg_q_engine);
	el_ipc_clear(el_msg_q_decode_video);
	el_ipc_clear(el_msg_q_decode_audio);
	el_ipc_clear(el_msg_q_video_sync);

	// clear global struct
	memset(&el_media_file_blk,0,sizeof(el_player_engine_decoded_file_obj_t));
	st_index = &el_media_file_blk.stream_index[0];	
	st_index[AVMEDIA_TYPE_VIDEO] = -1;
	st_index[AVMEDIA_TYPE_AUDIO] = -1;

	namelen = strlen((const char *)a_pszUrlOrPathname);
	if(!namelen)
	{
		EL_DBG_FUNC_EXIT;
		return EL_FALSE;		
	}

	// name string len + `\0` + maybe mms ''h" or mms "t" in mms protocol
	el_media_file_blk.pszUrlOrPathname = (EL_UTF8*)malloc(namelen + 2);
	if(!el_media_file_blk.pszUrlOrPathname)
	{
		EL_DBG_FUNC_EXIT;
		return EL_FALSE;	
	}
	
	memcpy(el_media_file_blk.pszUrlOrPathname, a_pszUrlOrPathname,namelen);
	el_media_file_blk.pszUrlOrPathname[namelen] = 0;
	el_media_file_blk.pszUrlOrPathname[namelen + 1] = 0;

	el_media_file_blk.engine.type = el_state_machine_engine;
	el_media_file_blk.engine.state = EL_ENGINE_CENTRAL_STATE_IDLE;
	el_media_file_blk.engine.ipc_fd = el_msg_q_engine;
	el_media_file_blk.video_decoder.type = el_state_machine_video_decoder;
	el_media_file_blk.video_decoder.state = EL_ENGINE_DECODE_STATE_START;
	el_media_file_blk.video_decoder.ipc_fd = el_msg_q_decode_video;
	el_media_file_blk.audio_decoder.type = el_state_machine_audio_decoder;
	el_media_file_blk.audio_decoder.state = EL_ENGINE_DECODE_STATE_START;
	el_media_file_blk.audio_decoder.ipc_fd = el_msg_q_decode_audio;
	el_media_file_blk.video_syncer.type = el_state_machine_video_syncer;
	el_media_file_blk.video_syncer.state = EL_ENGINE_VIDEO_SYNC_STATE_IDLE;
	el_media_file_blk.video_syncer.ipc_fd = el_msg_q_video_sync;
	el_media_file_blk.decoder_controller.type = el_state_machine_decoder_controller;
	el_media_file_blk.decoder_controller.state = EL_ENGINE_DECODE_CTRL_START;
	
	el_media_file_blk.bHaveDesc = EL_FALSE;
	el_media_file_blk.neterror.berror = EL_FALSE;

	// init audio q
	//q_init(&el_media_file_blk.audio_pkt_q,el_q_mode_block_empty);
	q_init(&el_media_file_blk.audio_pkt_q,el_q_mode_unblock);
	q_set_property(&el_media_file_blk.audio_pkt_q,el_q_node_empty_func,
		(void *)el_pkt_q_empty_goto_buffering_cb,
		(void *)&el_media_file_blk.audio_decoder);
	q_set_property(&el_media_file_blk.audio_pkt_q,el_q_node_push_func,
		(void *)el_pkt_q_push_cb,NULL);
	q_set_property(&el_media_file_blk.audio_pkt_q,el_q_node_pop_func,
		(void *)el_pkt_q_pop_cb,NULL);		

	// init video q
	//q_init(&el_media_file_blk.video_pkt_q,el_q_mode_block_empty);
	q_init(&el_media_file_blk.video_pkt_q,el_q_mode_unblock);
	q_set_property(&el_media_file_blk.video_pkt_q,el_q_node_empty_func,
		(void *)el_pkt_q_empty_goto_buffering_cb,
		(void *)&el_media_file_blk.video_decoder);
	q_set_property(&el_media_file_blk.video_pkt_q,el_q_node_push_func,
		(void *)el_pkt_q_push_cb,NULL);
	q_set_property(&el_media_file_blk.video_pkt_q,el_q_node_pop_func,
		(void *)el_pkt_q_pop_cb,NULL);	


	// init audio frame q
	// we do not set audio q empty blocked mode
	//q_init(&el_media_file_blk.audio_frm_q, el_q_mode_block_full);
	q_init(&el_media_file_blk.audio_frm_q, el_q_mode_unblock);
	q_set_property(&el_media_file_blk.audio_frm_q,el_q_node_max_nr,
		(void *)EL_MAX_FRM_AUDIO_Q_NODE_CNT,NULL);
	el_media_file_blk.audio_frm_q.need_reset_time_limit = 1;
	q_set_property(&el_media_file_blk.audio_frm_q,el_q_node_full_func,
		(void *)el_frm_q_full_wait_available_cb,
		(void *)&el_media_file_blk.audio_decoder);
	
	// init video frame q
	//q_init(&el_media_file_blk.video_frm_q,el_q_mode_block_empty | el_q_mode_block_full);
	q_init(&el_media_file_blk.video_frm_q,el_q_mode_unblock);
	q_set_property(&el_media_file_blk.video_frm_q,el_q_node_max_nr,
		(void *)EL_MAX_FRM_VIDEO_Q_NODE_CNT,NULL);
	el_media_file_blk.video_frm_q.need_reset_time_limit = 1;
	q_set_property(&el_media_file_blk.video_frm_q,el_q_node_full_func,
		(void *)el_frm_q_full_wait_available_cb,
		(void *)&el_media_file_blk.video_decoder);
	q_set_property(&el_media_file_blk.video_frm_q,el_q_node_empty_func,
		(void *)el_video_frm_q_empty_func_cb,
		(void *)&el_media_file_blk.video_decoder);

	el_init_free_qs();

	// create mutex lock
	el_media_file_blk.mutex = el_mutex_init();
	if(!el_media_file_blk.mutex)
	{
		goto init_eout;
	}	

	if (strstr((const char *)a_pszUrlOrPathname,"http://") ||
		strstr((const char *)a_pszUrlOrPathname,"rtsp://") ||
		strstr((const char *)a_pszUrlOrPathname,"rtmp://"))
	{
		el_media_file_blk.buffering_time = EL_MAX_PKT_Q_NETWORK_FIRST_BUFFERING_TS;
		el_media_file_blk.play_buffering_time = EL_MAX_PKT_Q_NETWORK_BUFFERING_TS;
		el_media_file_blk.is_network_media = EL_TRUE;
	}
	else if(strstr((const char *)a_pszUrlOrPathname,"mms://"))
	{
		el_media_file_blk.buffering_time = EL_MAX_PKT_Q_NETWORK_FIRST_BUFFERING_TS;
		el_media_file_blk.play_buffering_time = EL_MAX_PKT_Q_NETWORK_BUFFERING_TS;
		el_media_file_blk.is_mms_file = EL_TRUE;
		el_media_file_blk.is_network_media = EL_TRUE;
		el_adjust_ffmpeg_mms_url(0, (EL_CHAR *)el_media_file_blk.pszUrlOrPathname);
	}
	else	
	{
		el_media_file_blk.buffering_time = EL_MAX_PKT_Q_TS;
		el_media_file_blk.play_buffering_time = 2 * EL_MAX_PKT_Q_TS;
	}

	el_ipc_send(EL_ENGINE_EVT_OPEN,el_msg_q_engine);
	EL_DBG_FUNC_EXIT;
    
    el_media_file_blk.abort_request = EL_FALSE;
	
    return EL_TRUE;

init_eout:
	if(el_media_file_blk.pszUrlOrPathname)
	{
		free(el_media_file_blk.pszUrlOrPathname);
		el_media_file_blk.pszUrlOrPathname = NULL;
	}
    
	EL_DBG_FUNC_EXIT;
	
    return EL_FALSE;

}

EL_STATIC EL_BOOL el_player_engine_close_media()
{
	int msg, ret, i;
	int64_t tm;
	EL_DBG_FUNC_ENTER;
    
	if( el_media_file_blk.abort_request )
	{
		EL_DBG_LOG("decoder: close has already been called!");
		EL_DBG_FUNC_EXIT;
		return EL_TRUE;
	}

	el_media_file_blk.abort_request = EL_TRUE;

	el_ipc_send(EL_ENGINE_EVT_CLOSE,el_msg_q_engine);
	el_ipc_send(EL_ENGINE_EVT_CLOSE,el_msg_q_decode_video);
	el_ipc_send(EL_ENGINE_EVT_CLOSE,el_msg_q_decode_audio);
    
	// wait for read and decode thread ready to close
	for(i = 0;i < 3;i++)
	{
		ret = el_ipc_wait(el_msg_q_close,&msg,&tm);
		if(ret <= 0 || msg != EL_ENGINE_EVT_CLOSE)
		{
			EL_DBG_LOG("decoder:close ipc err!\n");
			EL_DBG_FUNC_EXIT;
			return EL_FALSE;
		}
	}
	
    el_ipc_clear(el_msg_q_engine);
	el_ipc_clear(el_msg_q_decode_video);
	el_ipc_clear(el_msg_q_decode_video);	
    
	el_media_file_blk.key_frame = EL_FALSE;

	// We do not call state machine now here, as the previous event processing logic
	// in both reader and decoder thread makes the two state machine as idle or start,
	// we directly call do close method here for simplity.
	el_do_close_central_engine();
	if(el_media_file_blk.pszUrlOrPathname)
	{
		free(el_media_file_blk.pszUrlOrPathname);
		el_media_file_blk.pszUrlOrPathname = NULL;
	}

	EL_DBG_FUNC_EXIT;
	return EL_TRUE;

}


EL_STATIC EL_UINT convert_audio_sample_format(enum AVSampleFormat sample_fmt)
{
	EL_DBG_LOG("sample_fmt = %d\n",sample_fmt);
	switch(sample_fmt)
	{
		case AV_SAMPLE_FMT_U8:
			return 8;
	    case AV_SAMPLE_FMT_S16:
			return 16;
		case AV_SAMPLE_FMT_S32:
			return 32;
		case AV_SAMPLE_FMT_FLT:
		case AV_SAMPLE_FMT_DBL:
			return 16;
		default:
			return 0;
	}
}

EL_STATIC EL_BOOL el_player_engine_get_description(ELMediaMuxerDesc_t *aMediaMuxerDesc, ELVideoDesc_t *aVideoDesc, ELAudioDesc_t *aAudioDesc)
{
	AVFormatContext *fc = el_media_file_blk.format_context;
	int *st_index = &el_media_file_blk.stream_index[0];
	AVCodecContext	*pCodecCtx;

	EL_DBG_FUNC_ENTER;

	if (el_media_file_blk.bHaveDesc)
	{
		memcpy(aMediaMuxerDesc, &el_media_file_blk.sELMediaMuxerDesc, sizeof(ELMediaMuxerDesc_t));
		memcpy(aVideoDesc, &el_media_file_blk.video_desc, sizeof(ELVideoDesc_t));
		memcpy(aAudioDesc, &el_media_file_blk.audio_desc, sizeof(ELAudioDesc_t));
		EL_DBG_LOG("have desc.\n");
		EL_DBG_FUNC_EXIT;

		return EL_TRUE;
	}

	if (EL_NULL == fc)
	{
		EL_DBG_LOG("error:el_media_file_blk.pFormatCtx == NULL\n");
		EL_DBG_FUNC_EXIT;
		return EL_FALSE;
	}
		
	// check if the decoder file opened
	if(el_media_file_blk.engine.state == EL_ENGINE_CENTRAL_STATE_IDLE)
	{
		EL_DBG_LOG("no description got.\n");
		EL_DBG_FUNC_EXIT;
		return EL_FALSE;
	}

	if(!aMediaMuxerDesc || !aVideoDesc || !aAudioDesc)
	{
		EL_DBG_LOG("paramter err.\n");
		EL_DBG_FUNC_EXIT;
		return EL_FALSE;
	}

    aMediaMuxerDesc->duration = fc->duration;
	aMediaMuxerDesc->is_network_media = el_media_file_blk.is_network_media;

	if(st_index[AVMEDIA_TYPE_VIDEO] >= 0)
	{
		AVStream *vst = fc->streams[st_index[AVMEDIA_TYPE_VIDEO]];
		pCodecCtx = vst->codec;
		
		//pCodec =	avcodec_find_decoder(pCodecCtx->codec_id);
		// Is this codec id the right info we found?
		EL_DBG_LOG("video:stream codec id = %d\n",pCodecCtx->codec_id);
		aMediaMuxerDesc->eVideoCodecID = pCodecCtx->codec_id;
		aVideoDesc->eVideoCodecID = aMediaMuxerDesc->eVideoCodecID;
		aVideoDesc->nBitRate = pCodecCtx->bit_rate;
		
		aVideoDesc->nWidth = pCodecCtx->width;
		aVideoDesc->nHeight = pCodecCtx->height;

		aVideoDesc->nFrameRate = (EL_UINT)(av_q2d(vst->avg_frame_rate));

		EL_DBG_LOG("video: bitrate = %d,width = %d,height = %d\n",
			pCodecCtx->bit_rate,pCodecCtx->width,pCodecCtx->height);

		if(vst->start_time != AV_NOPTS_VALUE)
		{
			el_media_file_blk.beginning_video_pts = (int64_t)(vst->start_time * av_q2d(vst->time_base)*1000);
			EL_DBG_LOG("set video begin pts = %lld\n",el_media_file_blk.beginning_video_pts);
		}
	}

	if(st_index[AVMEDIA_TYPE_AUDIO] >= 0)
	{
		AVStream *ast = fc->streams[st_index[AVMEDIA_TYPE_AUDIO]];
		pCodecCtx = ast->codec;
		// Is this codec id the right info we found?
		EL_DBG_LOG("audio:stream codec id = %d\n",pCodecCtx->codec_id);
		aMediaMuxerDesc->eAudioCodecID = pCodecCtx->codec_id;
		aAudioDesc->eAudioCodecID = aMediaMuxerDesc->eAudioCodecID;
		aAudioDesc->eChannelCount = pCodecCtx->channels;
		aAudioDesc->euiFreq = pCodecCtx->sample_rate;
		EL_DBG_LOG("sample_rate = %d\n",pCodecCtx->sample_rate);
		aAudioDesc->uiBitCount = convert_audio_sample_format(pCodecCtx->sample_fmt);
		//aAudioDesc->uiBitsPerSample = pCodecCtx->bits_per_raw_sample; //???
		if(ast->start_time != AV_NOPTS_VALUE)
		{
			el_media_file_blk.beginning_audio_pts = (int64_t)(ast->start_time * av_q2d(ast->time_base)*1000);
			EL_DBG_LOG("set audio begin pts = %lld\n",el_media_file_blk.beginning_audio_pts);
		}		
	}

	if(fc->duration != AV_NOPTS_VALUE)
	{		
		int64_t secs, us;
		secs = fc->duration / AV_TIME_BASE;
		us = fc->duration % AV_TIME_BASE;
		aMediaMuxerDesc->duration = secs * 1000 + us / 1000;	// ms
		el_media_file_blk.duration = aMediaMuxerDesc->duration;
	}
	else
	{
		aMediaMuxerDesc->duration = 0;
	}

	if(el_media_file_blk.is_network_media)
	{
		el_media_file_blk.is_live_media = EL_TRUE;
	}	

	aMediaMuxerDesc->is_live_media = el_media_file_blk.is_live_media;
	EL_DBG_LOG("decoder: live media detect: %d\n", el_media_file_blk.is_live_media);

	EL_DBG_FUNC_EXIT; 
	return EL_TRUE;
}

EL_STATIC EL_BOOL el_player_engine_start(void)
{
    // TODO: Check Function
    
	EL_DBG_FUNC_ENTER;
	el_ipc_send(EL_ENGINE_EVT_START,el_msg_q_engine);
	EL_DBG_FUNC_EXIT;
	return EL_TRUE;
}

EL_STATIC EL_BOOL el_player_engine_stop(void)
{
	EL_DBG_FUNC_ENTER;
	el_ipc_send(EL_ENGINE_EVT_STOP,el_msg_q_engine);
	EL_DBG_FUNC_EXIT;
	return EL_TRUE;
}

EL_STATIC EL_BOOL el_player_engine_pause(void)
{
	EL_DBG_FUNC_ENTER;
	el_ipc_send(EL_ENGINE_EVT_PAUSE,el_msg_q_engine);
	// need pause render
	el_engine_pause_render();	
	EL_DBG_FUNC_EXIT;
	return EL_TRUE;
}

EL_STATIC EL_INT  el_player_engine_seek_to(EL_SIZE a_SeekPos)
{
	EL_DBG_FUNC_ENTER;
	el_media_file_blk.seekpos = a_SeekPos;
	el_ipc_send(EL_ENGINE_EVT_SEEK,el_msg_q_engine);
	EL_DBG_FUNC_EXIT;
	return a_SeekPos;
}


EL_STATIC EL_BOOL el_player_engine_play(void)
{
	EL_DBG_FUNC_ENTER;
	el_ipc_send(EL_ENGINE_EVT_PLAY,el_msg_q_engine);
	EL_DBG_FUNC_EXIT;
	return EL_TRUE;
}

EL_STATIC EL_BOOL el_decoder_get_video_sample(ELAVSample_t** aELVideoSample)
{
	struct list_node *ptr;
	el_ffmpeg_frame_qnode_t *frm_node;

	ptr = q_get_first(&el_media_file_blk.video_frm_q);
	
	if(!ptr)
	{
		return EL_FALSE;
	}

	frm_node = list_entry(ptr,el_ffmpeg_frame_qnode_t,list);
	*aELVideoSample = &frm_node->frm_sample;

//		EL_DBG_LOG("get:video q ,cnt = %d\n",el_media_file_blk.video_frm_q.node_nr);
	return EL_TRUE;
}

EL_STATIC EL_BOOL el_decoder_get_audio_sample(ELAVSample_t** aELAudioSample)
{
	struct list_node *ptr;
	el_ffmpeg_frame_qnode_t *frm_node;

	ptr = q_get_first(&el_media_file_blk.audio_frm_q);
	if(!ptr)
	{
		EL_DBG_LOG("get:audio q empty!,cnt = %d\n",el_media_file_blk.audio_frm_q.node_nr);
		if(el_media_file_blk.is_network_media)
		{
			el_launch_cmo_buffering();
		}
		return EL_FALSE;
	}

	frm_node = list_entry(ptr,el_ffmpeg_frame_qnode_t,list);
	*aELAudioSample = &frm_node->frm_sample;

//		EL_DBG_LOG("get:audio q ,cnt = %d\n",el_media_file_blk.audio_frm_q.node_nr);
	return EL_TRUE;
}

// Note that ELAVSample_t should be freed by the caller
// the pointer of el_ffmpeg_frame_qnode_t is the same as ELAVSample_t
EL_STATIC EL_BOOL el_decoder_consume_video_sample(ELAVSample_t** aELVideoSample)
{
	struct list_node *ptr;
	el_ffmpeg_frame_qnode_t *frm_node;

	ptr = q_pop(&el_media_file_blk.video_frm_q);
	//EL_DBG_LOG("consume:video cnt = %d\n",q_list_count(&el_media_file_blk.video_frm_q));
	if(!ptr)
	{
		//EL_DBG_LOG("consume:video q empty!\n");
		return EL_FALSE;
	}

	frm_node = list_entry(ptr,el_ffmpeg_frame_qnode_t,list);
	*aELVideoSample = &frm_node->frm_sample;
//	EL_DBG_LOG("consume:video timestamp = %lld, size = %d\n",
//		(*aELVideoSample)->dwTS,(*aELVideoSample)->nBufferSize);
	return EL_TRUE;
}

// Note that ELAVSample_t should be freed by the caller
// the pointer of el_ffmpeg_frame_qnode_t is the same as ELAVSample_t
EL_STATIC EL_BOOL el_decoder_consume_audio_sample(ELAVSample_t** aELAudioSample)
{
	struct list_node *ptr;
	el_ffmpeg_frame_qnode_t *frm_node;

	ptr = q_pop(&el_media_file_blk.audio_frm_q);
	if(!ptr)
	{
		//EL_DBG_LOG("consume:audio q empty!,cnt = %d\n",el_media_file_blk.audio_frm_q.node_nr);
		//EL_DBG_LOG("audio cnt = %d\n",q_list_count(&el_media_file_blk.audio_frm_q));
		return EL_FALSE;
	}

	frm_node = list_entry(ptr,el_ffmpeg_frame_qnode_t,list);
	*aELAudioSample = &frm_node->frm_sample;
//	EL_DBG_LOG("consume audio:timestamp = %lld, size = %d\n",
//		(*aELAudioSample)->dwTS,(*aELAudioSample)->nBufferSize);
	return EL_TRUE;
}

EL_STATIC EL_VOID el_decoder_free_audio_buffer(ELAVSample_t *audio_sample)
{
	if(audio_sample)
	{
		el_free_frm_node(EL_MEDIA_AUDIO,(el_ffmpeg_frame_qnode_t * )audio_sample);
	}
}

EL_STATIC EL_VOID el_decoder_free_video_buffer(ELAVSample_t *video_samp)
{
	if(video_samp)
	{
		el_free_frm_node(EL_MEDIA_VIDEO,(el_ffmpeg_frame_qnode_t * )video_samp);
	}
}

EL_STATIC EL_VOID el_player_engine_notify(EL_INT a_nNotifyMsg, EL_VOID* wParam,EL_VOID* lParam)
{
	switch(a_nNotifyMsg)
	{
		case EL_PLAYER_MESSAGE_MEDIA_CURRENT_POS:
			{
				// Notify UI the current position of UI every 1s
				static long last_pos = 0;
				long cur_pos = (unsigned long)wParam;
				if(el_abs(cur_pos - last_pos) >= 1000)
				{
					last_pos = cur_pos;
					el_media_player_blk.dynamic_reference_table.el_ui_message_receiver_func(EL_PLAYER_MESSAGE_MEDIA_CURRENT_POS, wParam, 0);	
				}
				break;
			}		
		case EL_PLAYER_MESSAGE_END_OF_FILE:
			{
				if (el_media_file_blk.neterror.berror)
				{
					el_media_player_blk.dynamic_reference_table.el_ui_message_receiver_func(EL_PLAYER_MESSAGE_NOTIFICATION,
						(void *)el_media_file_blk.neterror.msg,
						(void *)el_media_file_blk.neterror.msg1);
				}
				else
				{
					el_media_player_blk.dynamic_reference_table.el_ui_message_receiver_func(a_nNotifyMsg, wParam, lParam);	
				}
				break;
			}
        case EL_PLAYER_MESSAGE_NOTIFICATION:        
		case EL_PLAYER_MESSAGE_BUFFERING_START:
		case EL_PLAYER_MESSAGE_BUFFERING_PERCENT:	
		case EL_PLAYER_MESSAGE_PAUSE_RESULT:
		case EL_PLAYER_MESSAGE_OPEN_SUCCESS:
		case EL_PLAYER_MESSAGE_OPEN_FAILED:
   		case EL_PLAYER_MESSAGE_READY_TO_PLAY:
		case EL_PLAYER_MESSAGE_SEEK_THUMBNAIL:
			el_media_player_blk.dynamic_reference_table.el_ui_message_receiver_func(a_nNotifyMsg, wParam, lParam);	
			break;
		default:	
			break;
	}
}





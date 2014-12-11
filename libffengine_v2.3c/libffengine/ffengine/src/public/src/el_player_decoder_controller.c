#include "el_media_player_engine.h"

EL_STATIC int64_t el_get_adjusted_sys_time_ms(void)
{
	int64_t adjusted_sys = el_get_sys_time_ms();
	if(el_media_file_blk.delta_adjusted_sys_time)
	{
		adjusted_sys +=  el_media_file_blk.delta_adjusted_sys_time;
	}
	return adjusted_sys;
}

EL_STATIC int64_t el_compute_delay(void)
{
	int64_t delta;
//	int64_t sys = el_get_adjusted_sys_time_ms();
    int64_t sys = el_get_sys_time_ms();
    
	delta = (sys - el_media_file_blk.base_sys_time) + 2 * el_media_file_blk.avg_video_decoded_time
		- (el_media_file_blk.current_video_decoded_ts - el_media_file_blk.first_video_decoded_ts);

	EL_DBG_LOG("cpt delta = %lld,sys = %lld,adjusted = %lld, base_time = %lld, current ts = %lld, first = %lld, avg decoded = %lld,state = %d\n",
               delta,
               sys,
               el_media_file_blk.delta_adjusted_sys_time,
               el_media_file_blk.base_sys_time,
               el_media_file_blk.current_video_decoded_ts,
               el_media_file_blk.first_video_decoded_ts,
               el_media_file_blk.avg_video_decoded_time,
               el_media_file_blk.decoder_controller.state
            );

	return delta;
}


EL_STATIC void el_set_current_decoded_ts(el_ffmpeg_packet_qnode_t *pkt_node)
{
	int64_t pts = pkt_node->packet.dts;
    
	if(pts != AV_NOPTS_VALUE)
	{
		el_media_file_blk.current_video_decoded_ts = (pts * av_q2d(el_media_file_blk.video_stream->time_base)*1000);	
	}
}

EL_STATIC void  el_do_process_decode_ctrl_normal(el_player_engine_state_machine_t *mac,el_player_engine_evt_e evt)
{
    EL_DBG_LOG("%s %d\n", __FUNCTION__, __LINE__);

	struct list_node *ptr;
	el_ffmpeg_packet_qnode_t *pkt_qnode;

	AVCodecContext *videoctx = el_media_file_blk.video_stream->codec;
	if(!videoctx)
	{
		return;
	}

	videoctx->skip_frame = AVDISCARD_DEFAULT;
	videoctx->flags2 &= ~CODEC_FLAG2_FAST;
	videoctx->skip_loop_filter = AVDISCARD_DEFAULT;

	struct q_head *video_pkt_q = &el_media_file_blk.video_pkt_q;
	ptr = q_pop(video_pkt_q);

	if(!ptr)
	{
		EL_DBG_LOG("vpq already empty!\n");
		usleep(20000);
		return;
	}

	pkt_qnode = (el_ffmpeg_packet_qnode_t *)list_entry(ptr, el_ffmpeg_packet_qnode_t, list);

    el_set_current_decoded_ts(pkt_qnode);

	// check if it should enter skip B frame
	int64_t delta = el_compute_delay();

    if(delta > 200)
	{
		el_decode_one_video_packet(pkt_qnode);
		el_state_machine_change_state(mac,EL_ENGINE_DECODE_CTRL_L1);
		EL_DBG_LOG("enter l1, delta = %lld\n",delta);
	}
	else
	{
		el_decode_one_video_packet(pkt_qnode);
	}
}

EL_STATIC int64_t el_find_key_frame_pts(struct q_head *head);

EL_STATIC void  el_do_process_decode_ctrl_l1(el_player_engine_state_machine_t *mac,el_player_engine_evt_e evt)
{
    EL_DBG_LOG("%s %d\n", __FUNCTION__, __LINE__);

	struct list_node *ptr;
	el_ffmpeg_packet_qnode_t *pkt_qnode;
	int64_t key_pts,key_ts;
	AVCodecContext *videoctx = el_media_file_blk.video_stream->codec;
	int64_t delta,dist;
	struct q_head *video_pkt_q = &el_media_file_blk.video_pkt_q;
	
	if(!videoctx)
	{
		return;
	}	

	videoctx->skip_frame = AVDISCARD_NONREF;
	videoctx->flags2 |= CODEC_FLAG2_FAST;
	videoctx->skip_loop_filter = AVDISCARD_NONREF;

	ptr = q_pop(video_pkt_q);

	if(!ptr)
	{
		EL_DBG_LOG("vpkt source already empty!\n");
		usleep(20000);
		return;
	}

	pkt_qnode = (el_ffmpeg_packet_qnode_t *)list_entry(ptr, el_ffmpeg_packet_qnode_t, list);
	el_set_current_decoded_ts(pkt_qnode);

	delta = el_compute_delay();

	if(delta < 201)
	{
		el_state_machine_change_state(mac,EL_ENGINE_DECODE_CTRL_NORMAL);
		el_decode_one_video_packet(pkt_qnode);
		EL_DBG_LOG("enter normal, delta = %lld\n",delta);

		return;
	}

	// before drop, we had better check if we can drop the packet
	// If I frame is far far away from here, we had better decode rather than drop
	key_pts = el_find_key_frame_pts(&el_media_file_blk.video_pkt_q);

    //
	if(key_pts != -1)
	{
		key_ts = (int64_t)(key_pts * av_q2d(el_media_file_blk.video_stream->time_base)*1000);
		dist = key_ts - el_media_file_blk.current_video_decoded_ts;
	}
    
	if ((key_pts != -1  // can not find a key frame
		&& el_media_file_blk.dwVideoPlayDelta < -500
		&& dist < delta))
		
	{	
		packet_node_free(pkt_qnode);
		el_state_machine_change_state(mac,EL_ENGINE_DECODE_CTRL_L2);
		EL_DBG_LOG("enter l2, delta = %lld\n",delta);
	}
	else
	{
        // xw, 可以在这里做decode并且输出
		el_decode_one_video_packet(pkt_qnode);
	}

}

//TODO:需要删除此函数，将这个性能好些的播放器，降级为最普通的播放器
EL_STATIC void  el_do_process_decode_ctrl_l2(el_player_engine_state_machine_t *mac,el_player_engine_evt_e evt)
{
    EL_DBG_LOG("%s %d\n", __FUNCTION__, __LINE__);
    
	int64_t delta;
	struct list_node *ptr;
	struct q_head *video_pkt_q = &el_media_file_blk.video_pkt_q;
	el_ffmpeg_packet_qnode_t *pkt_node;	
	AVCodecContext *videoctx = el_media_file_blk.video_stream->codec;
	
	if(!videoctx)
	{
		return;
	}

	ptr = q_pop(video_pkt_q);
	if(!ptr)
	{
		return;
	}

	pkt_node = (el_ffmpeg_packet_qnode_t *)list_entry(ptr, el_ffmpeg_packet_qnode_t, list);
	if(!pkt_node->packet.flags & AV_PKT_FLAG_KEY)
	{
		goto drop_out;
	}

	// key frame
	el_set_current_decoded_ts(pkt_node);
	delta = el_compute_delay();

	if(delta > 8000)
	{
		EL_DBG_LOG("drop I frame ts = %lld\n",el_media_file_blk.current_video_decoded_ts);
		goto drop_out;
	}

	if(delta < 3000)
	{
		el_media_file_blk.dwVideoPlayDelta = 0;
		el_state_machine_change_state(mac,EL_ENGINE_DECODE_CTRL_L1);
	}

	el_decode_one_video_packet(pkt_node);

    return;

drop_out:

	el_set_current_decoded_ts(pkt_node);

	delta = el_compute_delay();

	EL_DBG_LOG("drop a frame dts = %lld, delta = %lld,frame = %d\n",
               pkt_node->packet.dts,
               delta,
               el_media_file_blk.frame_count);

	packet_node_free(pkt_node);

	return;
}


EL_STATIC void  el_do_process_decode_ctrl_start(el_player_engine_state_machine_t *mac,el_player_engine_evt_e evt)
{
	struct list_node *ptr;
	el_ffmpeg_packet_qnode_t *pkt_qnode;
	AVCodecContext *videoctx = el_media_file_blk.video_stream->codec;
	int64_t start,end;
	struct q_head *video_pkt_q = &el_media_file_blk.video_pkt_q;

	el_media_file_blk.base_sys_time = el_get_sys_time_ms();

	videoctx->skip_frame = AVDISCARD_DEFAULT;
	videoctx->flags2 &= ~CODEC_FLAG2_FAST;      // 正常解码
	videoctx->skip_loop_filter = AVDISCARD_DEFAULT;

	ptr = q_pop(video_pkt_q);   // 拿出来一帧

	if(!ptr)
	{
		EL_DBG_LOG("vpkt source already empty!\n");
		usleep(20000);
		return;
	}

	pkt_qnode = (el_ffmpeg_packet_qnode_t *)list_entry(ptr, el_ffmpeg_packet_qnode_t, list);
	el_set_current_decoded_ts(pkt_qnode);
	el_media_file_blk.first_video_decoded_ts = el_media_file_blk.current_video_decoded_ts;

	start = el_get_sys_time_ms();
	el_decode_one_video_packet(pkt_qnode);
	end = el_get_sys_time_ms();
	el_media_file_blk.avg_video_decoded_time = end - start; // 平均解码时间，先解码一帧试试

	el_state_machine_change_state(mac,EL_ENGINE_DECODE_CTRL_NORMAL);
	// test print I frame table
/*
{
        int i;
		AVStream *st = el_media_file_blk.video_stream;
        AVIndexEntry *index_entries = st->index_entries;
        EL_DBG_LOG("entries: st->nb_index_entries = %d \n",st->nb_index_entries); 
       if(!index_entries)
        {
            EL_DBG_LOG("no entries!\n");
            return;
        }
        for(i = 0; i < st->nb_index_entries; i++)
        {
            EL_DBG_LOG("i = %d, pos: %lld, timestamp: %lld\n",i,index_entries[i].pos,index_entries[i].timestamp);    
        }
}
*/
}

EL_STATIC void el_decoder_controller_state_machine(el_player_engine_state_machine_t *mac,el_player_engine_evt_e evt)
{
	switch(mac->state)
	{
		case EL_ENGINE_DECODE_CTRL_NORMAL:
		{
			el_media_file_blk.frame_count++;
			el_do_process_decode_ctrl_normal(mac,evt);
			return;
		}
		case EL_ENGINE_DECODE_CTRL_L1:
		{
			el_media_file_blk.frame_count++;
			el_do_process_decode_ctrl_l1(mac,evt);
			return;
		}
		case EL_ENGINE_DECODE_CTRL_L2:
		{
			el_media_file_blk.frame_count++;
			el_do_process_decode_ctrl_l2(mac,evt);
			return;
		}
		case EL_ENGINE_DECODE_CTRL_START:
		{
			el_media_file_blk.frame_count = 0;
			el_do_process_decode_ctrl_start(mac,evt);
			return;
		}
		default:
		{
			EL_DBG_POS;
			return;
		}
	}
}

EL_STATIC void el_reset_decoder_controller(void)
{
	el_state_machine_change_state(&el_media_file_blk.decoder_controller,EL_ENGINE_DECODE_CTRL_START);
}

EL_STATIC void el_resume_decoder_controller(void)
{
	struct list_node *ptr;
	el_ffmpeg_packet_qnode_t *pkt_qnode;
	AVCodecContext *videoctx = el_media_file_blk.video_stream->codec;
	int64_t start,end;
	struct q_head *video_pkt_q = &el_media_file_blk.video_pkt_q;

	if(el_media_file_blk.decoder_controller.state == EL_ENGINE_DECODE_CTRL_NORMAL)
	{
		videoctx->skip_frame = AVDISCARD_DEFAULT;
		videoctx->flags2 &= ~CODEC_FLAG2_FAST;
		videoctx->skip_loop_filter = AVDISCARD_DEFAULT;		
	}
	else
	{
		videoctx->skip_frame = AVDISCARD_NONREF;
		videoctx->flags2 |= CODEC_FLAG2_FAST;
		videoctx->skip_loop_filter = AVDISCARD_NONREF;		
	}

	ptr = q_pop(video_pkt_q);

	if(!ptr)
	{
		EL_DBG_LOG("vpkt resume source already empty!\n");
		usleep(20000);
		return;
	}

	pkt_qnode = (el_ffmpeg_packet_qnode_t *)list_entry(ptr, el_ffmpeg_packet_qnode_t, list);
	el_set_current_decoded_ts(pkt_qnode);
	el_media_file_blk.first_video_decoded_ts = el_media_file_blk.current_video_decoded_ts;

	start = el_get_sys_time_ms();
	el_decode_one_video_packet(pkt_qnode);
	end = el_get_sys_time_ms();
	el_media_file_blk.avg_video_decoded_time = end - start;
	
}


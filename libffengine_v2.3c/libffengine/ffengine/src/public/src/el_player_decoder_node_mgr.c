#include "el_media_player_engine.h"

//! 这里牵扯到分配内存空间的复用问题，处理的非常巧妙
EL_STATIC el_ffmpeg_packet_qnode_t *el_allocate_packet_node(void)
{
	el_ffmpeg_packet_qnode_t *pkt_node;
	struct list_node *ptr;

	if(el_media_file_blk.free_pkt_q.node_nr != 0)
	{
		ptr = q_pop(&el_media_file_blk.free_pkt_q);
		if(!ptr)
		{
			goto go_malloc;
		}
		pkt_node = list_entry(ptr,el_ffmpeg_packet_qnode_t,list);
		//EL_DBG_LOG("alloc: nr = %d",el_media_file_blk.free_pkt_q.node_nr);
		return pkt_node;
	}

go_malloc:	
	pkt_node = malloc(sizeof(el_ffmpeg_packet_qnode_t));
	if(!pkt_node)
	{
		EL_DBG_LOG("decoder: alloc el_ffmpeg_packet_qnode_t err!\n");
		return NULL;
	}
	return pkt_node;
}


EL_STATIC void el_free_packet_node(el_ffmpeg_packet_qnode_t *free_node)
{
	q_push(&(free_node->list), &el_media_file_blk.free_pkt_q);
	//EL_DBG_LOG("added! nr = %d\n",el_media_file_blk.free_pkt_q.node_nr);
}

// 这里面还牵扯到了frame node数据的复用，可以尽量少的减少alloc/free的调用，减少内存碎片
EL_STATIC el_ffmpeg_frame_qnode_t *el_allocate_frm_node(el_player_media_type_e type,
	unsigned long required_size)
{
	el_ffmpeg_frame_qnode_t *frm_node;
	struct q_head *free_frm_q;
	struct list_node *ptr;

	if(type == EL_MEDIA_VIDEO)
	{
		free_frm_q = &el_media_file_blk.free_video_frm_q;
	}
	else
	{
		free_frm_q = &el_media_file_blk.free_audio_frm_q;
	}


	if(free_frm_q->node_nr != 0)
	{
		ptr = q_pop(free_frm_q);
		if(!ptr)
		{
			goto go_malloc;
		}
		frm_node = list_entry(ptr,el_ffmpeg_frame_qnode_t,list);
		//EL_DBG_LOG("fetch v free: nr = %d",el_media_file_blk.free_video_frm_q.node_nr);
		if(frm_node->real_buf_size < required_size)
		{
			// do not satisfy your size, free it and goto malloc
			free(frm_node->frm_sample.pBuffer);
			free(frm_node);
			goto go_malloc;
		}
		frm_node->frm_sample.nBufferSize = required_size;
		return frm_node;
	}

go_malloc:	
	frm_node = malloc(sizeof(el_ffmpeg_frame_qnode_t));
	if(!frm_node)
	{
		EL_DBG_LOG("decoder: alloc el_ffmpeg_frame_qnode_t err!\n");
		return NULL;
	}
	frm_node->frm_sample.pBuffer = malloc(required_size);
	if(!frm_node->frm_sample.pBuffer)
	{
		EL_DBG_LOG("decoder: alloc sample buf err!\n");
		free(frm_node);
		return NULL;
	}

	frm_node->frm_sample.nBufferSize = required_size;
	frm_node->real_buf_size = required_size;

	return frm_node;
}


EL_STATIC void el_free_frm_node(el_player_media_type_e type,el_ffmpeg_frame_qnode_t *free_node)
{

	struct q_head *free_frm_q;

	if(type == EL_MEDIA_VIDEO)
	{
		free_frm_q = &el_media_file_blk.free_video_frm_q;
	}
	else
	{
		free_frm_q = &el_media_file_blk.free_audio_frm_q;
	}

	q_push(&(free_node->list), free_frm_q);
	//EL_DBG_LOG("add v free! nr = %d\n",el_media_file_blk.free_video_frm_q.node_nr);
}

EL_STATIC void el_init_free_qs(void)
{
	// init free pkt q
    q_init(&el_media_file_blk.free_pkt_q,el_q_mode_unblock);
	// init free video buf q
    q_init(&el_media_file_blk.free_video_frm_q,el_q_mode_unblock);
	// init free audio buf q
    q_init(&el_media_file_blk.free_audio_frm_q,el_q_mode_unblock);	
}

EL_STATIC void el_destroy_free_qs(void)
{
	struct q_head *q_head;
	// destroy content
	EL_DBG_LOG("decoder: listing all free q nodes count...\n");
	EL_DBG_LOG("decoder: free pkt q = %d, free video frm q = %d,free audio frm q = %d\n",
		el_media_file_blk.free_pkt_q.node_nr,
		el_media_file_blk.free_video_frm_q.node_nr,
		el_media_file_blk.free_audio_frm_q.node_nr);

	// pkt q
	q_head = &el_media_file_blk.free_pkt_q;
	while(!q_is_empty(q_head))
	{
		struct list_node *ptr;
		el_ffmpeg_packet_qnode_t *pkt_node;
		ptr = q_pop(q_head);

		if(!ptr)
		{
			return;
		}
		pkt_node = list_entry(ptr,el_ffmpeg_packet_qnode_t,list);
		free(pkt_node);
	}	

	// frm q
	q_head = &el_media_file_blk.free_video_frm_q;
	while(!q_is_empty(q_head))
	{
		struct list_node *ptr;
		el_ffmpeg_frame_qnode_t *frm_node;
		ptr = q_pop(q_head);

		if(!ptr)
		{
			return;
		}
		frm_node = list_entry(ptr,el_ffmpeg_frame_qnode_t,list);
		free(frm_node->frm_sample.pBuffer);		
		free(frm_node);
	}	

	// frm q
	q_head = &el_media_file_blk.free_audio_frm_q;
	while(!q_is_empty(q_head))
	{
		struct list_node *ptr;
		el_ffmpeg_frame_qnode_t *frm_node;
		ptr = q_pop(q_head);

		if(!ptr)
		{
			return;
		}
		frm_node = list_entry(ptr,el_ffmpeg_frame_qnode_t,list);
		free(frm_node->frm_sample.pBuffer);		
		free(frm_node);
	}		

	// destroy free pkt q
	q_destroy(&el_media_file_blk.free_pkt_q);
	// destroy video buf q
	q_destroy(&el_media_file_blk.free_video_frm_q);
	// destroy audio buf q
	q_destroy(&el_media_file_blk.free_audio_frm_q);		
}




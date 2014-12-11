#include <sys/time.h>

#include "el_ipc.h"
#include "el_media_player_engine.h"
#include "el_log.h"

struct el_sample_fmtinfo
{
    const char *name;
    int bits;
};

/** this table gives more information about formats */
static const struct el_sample_fmtinfo sample_fmt_info[AV_SAMPLE_FMT_NB] = {
    [AV_SAMPLE_FMT_U8]  = { .name = "u8",  .bits = 8 },
    [AV_SAMPLE_FMT_S16] = { .name = "s16", .bits = 16 },
    [AV_SAMPLE_FMT_S32] = { .name = "s32", .bits = 32 },
    [AV_SAMPLE_FMT_FLT] = { .name = "flt", .bits = 32 },
    [AV_SAMPLE_FMT_DBL] = { .name = "dbl", .bits = 64 },
};


EL_STATIC int el_get_bits_per_sample_fmt(enum AVSampleFormat sample_fmt)
{
    return sample_fmt < 0 || sample_fmt >= AV_SAMPLE_FMT_NB ?
        0 : sample_fmt_info[sample_fmt].bits;
}

EL_STATIC const char *el_get_sample_fmt_name(enum AVSampleFormat sample_fmt)
{
    if (sample_fmt < 0 || sample_fmt >= AV_SAMPLE_FMT_NB)
        return NULL;
    return sample_fmt_info[sample_fmt].name;
}

EL_STATIC void el_state_machine_change_state(el_player_engine_state_machine_t *mac,
    el_player_engine_state_e new_state)
{
    EL_DBG_LOG("state change: machine = %d, old = %d, new = %d\n",
        mac->type, mac->state, new_state);
//    mac->old_state = mac->state;
    mac->state = new_state;
}

EL_STATIC void packet_node_free(el_ffmpeg_packet_qnode_t *pqnode)
{
    if (pqnode)
    {
        av_free_packet(&(pqnode->packet));
        //el_free(pqnode);
        el_free_packet_node(pqnode);
    }
}

EL_STATIC void frame_node_free(el_player_media_type_e type,el_ffmpeg_frame_qnode_t *pqnode)
{
    if (pqnode)
    {
        el_free_frm_node(type,pqnode);
    }
}

EL_STATIC void yuv_data_collect(AVFrame *frame, EL_BYTE* videodst, int width, int height)
{
    AVFrame *pELAVFrame = frame;
    if(frame)
    {
        int i, width_2, height_2;
        EL_BYTE* pDst = NULL;
        EL_BYTE* pSrc = NULL;

        if(!pELAVFrame->data[1] || !pELAVFrame->data[2]
            || !pELAVFrame->data[0])
        {
            return;
        }

        width_2  = width >> 1;       // width / 2
        height_2 = height >> 1;     // height / 2
        pDst = videodst;

        //first copy y
        pSrc = pELAVFrame->data[0];

        if (width == pELAVFrame->linesize[0])
        {
            memcpy(pDst, pSrc, width * height);
            pDst += width*height;
        }
        else for(i=0; i<height; i++)
        {
            memcpy(pDst, pSrc, width);
            pDst += width;
            pSrc += pELAVFrame->linesize[0];
        }

        //copy u
        pSrc = pELAVFrame->data[1];
        if (width_2 == pELAVFrame->linesize[1])
        {
            memcpy(pDst, pSrc, width_2 * height_2);
            pDst += width_2 * height_2;
        }
        else for(i=0; i<height_2; i++)
        {
            memcpy(pDst, pSrc, width_2);
            pDst += width_2;
            pSrc += pELAVFrame->linesize[1];
        }
        //copy v
        pSrc = pELAVFrame->data[2];
        if (width_2 == pELAVFrame->linesize[2])
        {
            memcpy(pDst, pSrc, width_2 * height_2);
            pDst += width_2 * height_2;
        }
        else for(i=0; i<height_2; i++)
        {
            memcpy(pDst, pSrc, width_2);
            pDst += width_2;
            pSrc += pELAVFrame->linesize[2];
        }
    }
    return;
}

EL_STATIC void rgb_data_collect(AVFrame *frame, EL_BYTE* videodst, int width, int height)
{
    int i;
    EL_BYTE *src,*dst;
    if(!frame)
    {
        return;
    }

    if(!frame->data[0])
    {
        EL_DBG_LOG("decoder:no rgb data!\n");
        return;
    }

    EL_DBG_LOG("rgb width = %d,linesize %d,%d,%d  \n",width,frame->linesize[0],frame->linesize[1],frame->linesize[2]);
    dst = videodst;
    for(i = 0; i < height; i++)
    {
        src = frame->data[0] + i * frame->linesize[0];
        memcpy(dst,src,width * 3);
        dst += width * 3;
    }
    return;
}


EL_STATIC void packet_q_destruction(struct q_head *q_head)
{
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
        packet_node_free(pkt_node);
    }
}

EL_STATIC void frame_q_destruction(el_player_media_type_e type,struct q_head *q_head)
{
    while(!q_is_empty(q_head))
    {
        struct list_node *ptr;
        el_ffmpeg_frame_qnode_t *frm_node;
        ptr = q_pop(q_head);
        frm_node = list_entry(ptr,el_ffmpeg_frame_qnode_t,list);
        frame_node_free(type,frm_node);
    }
}

EL_STATIC void q_all_destruction(EL_VOID)
{
    EL_DBG_LOG("#############pkt v a q %d,%d\n",el_media_file_blk.video_pkt_q.node_nr,el_media_file_blk.audio_pkt_q.node_nr);
    packet_q_destruction(&el_media_file_blk.audio_pkt_q);
    packet_q_destruction(&el_media_file_blk.video_pkt_q);
    frame_q_destruction(EL_MEDIA_AUDIO,&el_media_file_blk.audio_frm_q);
    frame_q_destruction(EL_MEDIA_VIDEO,&el_media_file_blk.video_frm_q);

}


// if already empty, returns NULL
// get first element
EL_STATIC int64_t  q_pkt_get_first_pts(struct q_head *q_head)
{
    struct list_node *got_item;
    el_ffmpeg_packet_qnode_t *qnode;
    int64_t pts = 0;
    el_mutex_lock(q_head->mutex);
    if(list_empty(&q_head->lst_node))
    {
        el_mutex_unlock(q_head->mutex);
        return 0;
    }
    got_item = q_head->lst_node.next;
    qnode = (el_ffmpeg_packet_qnode_t *)list_entry(got_item, el_ffmpeg_packet_qnode_t, list);
    // use dts instead of pts now.
    pts = qnode->packet.dts;
    if(pts == AV_NOPTS_VALUE)
    {
        pts = 0;
    }
    el_mutex_unlock(q_head->mutex);
    return pts;
}

// if already empty, returns NULL
// get last element (latest element that inserted on the q)
EL_STATIC int64_t q_pkt_get_last_pts(struct q_head *q_head)
{
    struct list_node *got_item;
    el_ffmpeg_packet_qnode_t *qnode;
    int64_t pts = 0;
    el_mutex_lock(q_head->mutex);
    if(list_empty(&q_head->lst_node))
    {
        el_mutex_unlock(q_head->mutex);
        return 0;
    }
    got_item = q_head->lst_node.prev;
    qnode = (el_ffmpeg_packet_qnode_t *)list_entry(got_item, el_ffmpeg_packet_qnode_t, list);
    // use dts instead of pts now.
    pts = qnode->packet.dts;
    if(pts == AV_NOPTS_VALUE)
    {
        pts = 0;
    }    
    el_mutex_unlock(q_head->mutex);
    return pts;
}

EL_STATIC int decode_interrupt_cb(void *ctx)
{
    el_player_engine_decoded_file_obj_t *is = ctx;

    return is->abort_request;
}

/* open a given stream. Return 0 if OK */
//~ 2012.10.08 这两个方法component_open/close, 是从ffplay中借鉴过来的
EL_STATIC int stream_component_open(int stream_index)
{
    AVFormatContext *ic = el_media_file_blk.format_context;
    AVCodecContext *codec_context;
    AVCodec *codec;
 
    if (stream_index < 0 || stream_index >= ic->nb_streams)
    {
        return -1;
    }
    
    codec_context = ic->streams[stream_index]->codec;

    /* prepare audio output */
    if (codec_context->codec_type == AVMEDIA_TYPE_AUDIO) 
    {
        if(codec_context->sample_rate <= 0)
        {
            EL_DBG_LOG("decoder:Invalid sample rate,samplerate = %d,channels = %d\n",codec_context->sample_rate,codec_context->channels);
            codec_context->sample_rate= 8000;
        }

        if(codec_context->channels <= 0)
        {
            EL_DBG_LOG("decoder:Invalid channel count,samplerate = %d,channels = %d\n", codec_context->sample_rate, codec_context->channels);
            codec_context->channels = 1;
        }

        el_media_file_blk.audio_channels_cnt = codec_context->channels;
        
        if (codec_context->channels > 0) 
        {
            codec_context->request_channels = FFMIN(2, codec_context->channels);
        } 
        else 
        {
            codec_context->request_channels = 2;
        }
        
        if (el_media_file_blk.audio_channels_cnt > codec_context->request_channels)
        {
            el_media_file_blk.audio_channels_cnt = codec_context->request_channels;
        }
    }

    codec = avcodec_find_decoder(codec_context->codec_id);
    
    if (!codec || avcodec_open2(codec_context, codec, NULL) < 0)
    {
        return -1;
    }

    ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;

    return 0;
}

EL_STATIC void stream_component_close(int stream_index)
{
    AVFormatContext *ic = el_media_file_blk.format_context;
    AVCodecContext *avctx;
 
    if (stream_index < 0 || stream_index >= ic->nb_streams)
    {
        return;
    }
    
    avctx = ic->streams[stream_index]->codec;
    ic->streams[stream_index]->discard = AVDISCARD_ALL;
    avcodec_close(avctx);
 }

EL_STATIC int64_t get_packet_q_ts(struct q_head *q)
{
    int64_t nfirst = 0;
    int64_t nlast = 0;
    nfirst = q_pkt_get_first_pts(q);
    nlast = q_pkt_get_last_pts(q);
    return ((nlast-nfirst > 0) ? (nlast-nfirst) : 0);
}


EL_STATIC int el_add_packet_to_q(AVPacket *pkt)
{
    AVFormatContext *pFormatCtx;
    struct q_head *audio_pkt_q; 
    struct q_head *video_pkt_q; 
    struct q_head *pq;
    el_ffmpeg_packet_qnode_t *pkt_node;
    int *st_index; 

    audio_pkt_q = &el_media_file_blk.audio_pkt_q;
    video_pkt_q = &el_media_file_blk.video_pkt_q;

    pFormatCtx = el_media_file_blk.format_context;
    st_index = &el_media_file_blk.stream_index[0];
    

    if (pkt->stream_index == st_index[AVMEDIA_TYPE_VIDEO])
    {

        pq = video_pkt_q;
        //EL_DBG_LOG("state_machine: read  v packet,pts = %lld, ts = %lld\n",pkt->dts,
            //(int64_t)(pkt->dts * av_q2d(el_media_file_blk.video_stream->time_base)*1000));
    }
    else if (pkt->stream_index == st_index[AVMEDIA_TYPE_AUDIO]) 
    {

        pq = audio_pkt_q;
        //EL_DBG_LOG("state_machine: read a packet,pts = %lld,ts = %lld\n",pkt->dts,
            //(int64_t)(pkt->dts * av_q2d(el_media_file_blk.audio_stream->time_base)*1000));
    }
    else
    {
        av_free_packet(pkt);
        return -1;
    }

    pkt_node = el_allocate_packet_node();
    if(!pkt_node)
    {
        EL_DBG_LOG("decoder: read pkt alloc err!\n");
        return -1000;
    }
    pkt_node->packet = *pkt;
    av_dup_packet(&pkt_node->packet);
    q_push(&(pkt_node->list), pq);
    return 0;
}

EL_STATIC int el_do_read_one_packet(void)
{
    AVFormatContext *pFormatCtx;
    int *st_index; 
    AVPacket packet;
    int ret;

    struct q_head *audio_pkt_q; 
    struct q_head *video_pkt_q; 
    audio_pkt_q = &el_media_file_blk.audio_pkt_q;
    video_pkt_q = &el_media_file_blk.video_pkt_q;
    pFormatCtx = el_media_file_blk.format_context;
    st_index = &el_media_file_blk.stream_index[0];

/*
    EL_DWORD start,end,diff;
    start = el_get_sys_time_ms();
*/    

    // < 0, error
    if((ret = av_read_frame(pFormatCtx, &packet)) < 0)
    {
//        if(url_feof(pFormatCtx->pb))
//        {
//            EL_DBG_LOG("state_machine:reach eof!\n");
//            return AVERROR_EOF;
//        }

        el_media_file_blk.read_retry_count++;
        EL_DBG_LOG("av_read_frame return: %d\n", ret);
        
        return ret;
    }
    
//    printf("packet.flags = %06x\n", packet.flags);

    //! xw, 加密
//    if (packet.flags == AV_PKT_FLAG_KEY)
//    {
//        if (packet.stream_index == el_media_file_blk.st_index[AVMEDIA_TYPE_VIDEO])
//        {
//            int i = 0;
//            
//            for(i = 20; i < packet.size; i++)
//            {
//                packet.data[i] = ~packet.data[i];
//            }
//        }
//    }

    el_media_file_blk.read_retry_count = 0;

    return el_add_packet_to_q(&packet);
}

EL_STATIC void el_do_close_central_engine(void)
{
    int *st_index = &el_media_file_blk.stream_index[0];

    EL_DBG_LOG("********close here10!");

    //q_all_destruction();

    // Close the codec
    if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) 
    {
        stream_component_close(st_index[AVMEDIA_TYPE_AUDIO]);
    }

    EL_DBG_LOG("********close here3!");

    
    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) 
    {
        stream_component_close(st_index[AVMEDIA_TYPE_VIDEO]);
    }

    EL_DBG_LOG("********close here2!");

    if(el_media_file_blk.format_context)
    {
        // 2012.10.08
        avformat_close_input(&el_media_file_blk.format_context);
        el_media_file_blk.format_context = NULL;
    }

    // before destroy, check if q is already empty
    EL_DBG_LOG("pkt audio video q count %d,%d\n",
        el_media_file_blk.audio_pkt_q.node_nr,
        el_media_file_blk.video_pkt_q.node_nr);

    EL_DBG_LOG("frm audio video q count %d,%d\n",
        el_media_file_blk.audio_frm_q.node_nr,
        el_media_file_blk.video_frm_q.node_nr);

    // destroy audio q
    q_destroy(&el_media_file_blk.audio_pkt_q);
    // destroy video q
    q_destroy(&el_media_file_blk.video_pkt_q);

    // destroy audio frame q
    q_destroy(&el_media_file_blk.audio_frm_q);
    // destroy video frame q
    q_destroy(&el_media_file_blk.video_frm_q);

    el_destroy_free_qs();
    
    if(el_media_file_blk.mutex)
    {
        el_mutex_destroy(el_media_file_blk.mutex);
    }

    return;
}


EL_STATIC inline int64_t  el_pos_to_pts(int64_t pos, AVStream *stream)
{
    double pts,ts;
    ts = (double)pos/1000;
    pts = ts / av_q2d(stream->time_base);
    //EL_DBG_LOG("state_machine: seek start(in sec):%lld, pts = %lld\n", (int64_t)ts,(int64_t)pts);
    return (int64_t)pts;
}

// 搜索
EL_STATIC void el_ffmpeg_do_seek(void)
{
    AVStream *stream;
    int st_index;
    int64_t abs_seek_pos,pts;
    // do seek actions
    int ret,i;
    AVPacket packet;
    
    AVFormatContext *fc = el_media_file_blk.format_context;
    
    q_all_destruction();

    switch(el_media_file_blk.av_support)
    {
        case EL_ENGINE_HAS_BOTH:
        case EL_ENGINE_HAS_VIDEO:
        {
            stream = el_media_file_blk.video_stream;
            st_index = el_media_file_blk.stream_index[AVMEDIA_TYPE_VIDEO];
            break;
        }
        case EL_ENGINE_HAS_AUDIO:
        {
            stream = el_media_file_blk.audio_stream;
            st_index = el_media_file_blk.stream_index[AVMEDIA_TYPE_AUDIO];
            break;
        }
        default:
        {
            return;
        }
    }

    el_media_file_blk.av_seeking = EL_TRUE;
    el_media_file_blk.av_sync_time = 0;
//    EL_DBG_LOG("sync_av_after_seek done, set av_sync_time to 0");

    if(el_media_file_blk.av_support & EL_ENGINE_HAS_AUDIO)
    {
        abs_seek_pos = el_media_file_blk.seekpos + 
            el_media_file_blk.beginning_audio_pts;
    }
    else
    {
        abs_seek_pos = el_media_file_blk.seekpos + 
            el_media_file_blk.beginning_video_pts;
    }

    avcodec_flush_buffers(el_media_file_blk.video_stream->codec);
    avcodec_flush_buffers(el_media_file_blk.audio_stream->codec);

    pts = el_pos_to_pts(abs_seek_pos,stream);

    EL_DBG_LOG("state_machine: seek start(in msec):%lld, pts = %lld\n", (int64_t)abs_seek_pos,(int64_t)pts);
    
    ret = avformat_seek_file(fc, st_index, INT64_MIN, pts, INT64_MAX, 0);

    EL_DBG_LOG("after #############pkt v a q %d,%d\n",el_media_file_blk.video_pkt_q.node_nr,el_media_file_blk.audio_pkt_q.node_nr);

    if(fc->iformat && fc->iformat->name && !strcmp(fc->iformat->name,"applehttp"))
    {
        EL_DBG_LOG("state_machine: after seek applehttp\n");    
        // seek 5s forward
        pts = el_pos_to_pts(abs_seek_pos + 5000,stream);        
        ret = avformat_seek_file(fc, st_index, INT64_MIN, pts, INT64_MAX, 0);
        EL_DBG_LOG("state_machine: seek forward(in msec):%lld, pts = %lld\n", (int64_t)abs_seek_pos + 5000,(int64_t)pts);

        for(i=0;; i++) 
        {
            int ret;
            //do{
            ret = av_read_frame(fc, &packet);   // 在这里从文件读取音视频信息
            //}while(ret == AVERROR(EAGAIN));
            if(ret<0)
                break;
            if(st_index  == packet.stream_index)
            {
                int64_t range = el_pos_to_pts(10000,stream);
                EL_DBG_LOG("pts = %lld,range = %lld\n",pts,range);

                // pts is seek pos, +-10s is OK, [pts - 10s, pts + 10s]
                if((packet.flags & AV_PKT_FLAG_KEY) && (packet.dts >= pts - range) &&
                    (packet.dts <= pts + range))
                {
                    EL_DBG_LOG("after  seek timestamp = %lld,find dts = %lld, ts = %lld\n",pts,packet.dts,
                          (int64_t)(packet.dts * av_q2d(fc->streams[st_index]->time_base) * 1000));
                    el_add_packet_to_q(&packet);    // 添加
                    break;
                }
            }
            
//    free_pkt:
            EL_DBG_LOG("omit type %s, key = %d, packet dts = %lld, ts = %lld\n",
                (packet.stream_index == el_media_file_blk.stream_index[AVMEDIA_TYPE_VIDEO]) ? ("video") :("audio") ,
                    packet.flags,
                    packet.dts,
                    (int64_t)(packet.dts * av_q2d(fc->streams[st_index]->time_base) * 1000));
            av_free_packet(&packet);
        }
    }

    EL_DBG_LOG("state_machine: after avformat_seek_file, ret = %d\n",ret);
    return;
}


EL_STATIC void el_send_upper_layer_msg(long msg,long arg1,long arg2)
{
    static int last_percent = -1;
    
    if(msg == EL_PLAYER_MESSAGE_BUFFERING_PERCENT) 
    {
        if(arg1 < last_percent + 5)
        {
            EL_DBG_LOG("skipping up layer msg %d,arg1 = %d,arg2 = %d ...last_percent = %d\n",msg,arg1,arg2,last_percent);
            return;
        }

        last_percent = arg1;
    }
    else
    {
        last_percent = -1;
    }

    EL_DBG_LOG("msg = %d,need sending upper msg = %d\n",msg,el_media_player_blk.need_sending_upper_msg);
    
    if(el_media_player_blk.need_sending_upper_msg)
    {
        EL_DBG_LOG("sending up msg %d, arg1 = %d,arg2 = %d\n",msg,
            arg1,arg2);        
        el_player_engine_notify(msg,(EL_VOID *)arg1,
            (EL_VOID *)arg2);
    }
}


EL_STATIC void el_notify_buffering_start(void)
{
    if(el_media_file_blk.engine.state > EL_ENGINE_CENTRAL_STATE_OPENED &&
        el_media_file_blk.engine.state < EL_ENGINE_CENTRAL_STATE_PLAY_COMPLETE &&
        el_media_file_blk.engine.state != EL_ENGINE_CENTRAL_STATE_BUFFERING)
    {
            EL_DBG_LOG("state_machine: notify buffering start ...\n");
            el_send_upper_layer_msg(EL_PLAYER_MESSAGE_BUFFERING_START,0,0);
    }
}

EL_STATIC void el_notify_buffering_percent(double buffering_percent)
{
    if(el_media_file_blk.engine.state == EL_ENGINE_CENTRAL_STATE_BUFFERING)
    {
        int percent = (int)(buffering_percent * 100);
        if(el_media_file_blk.buffering_percent < percent)
        {
            {
                EL_DBG_LOG("state_machine: buffering_percent = %d\n",percent);
                el_media_file_blk.buffering_percent = percent;
                if(el_media_file_blk.is_network_media)
                {    
                    el_send_upper_layer_msg(EL_PLAYER_MESSAGE_BUFFERING_PERCENT,percent,0);
                }
            }
        }
    }    
}

EL_STATIC void el_notify_buffering_ready(void)
{
//    EL_DBG_LOG("in notify ready, need = %d\n",el_media_player_blk.need_notify_ready);
    if(el_media_player_blk.need_notify_ready)
    {
        long msg;
        struct q_head *audio_pkt_q = &el_media_file_blk.audio_pkt_q;
        struct q_head *video_pkt_q = &el_media_file_blk.video_pkt_q;
        struct q_head *audio_frm_q = &el_media_file_blk.audio_frm_q;
        struct q_head *video_frm_q = &el_media_file_blk.video_frm_q;    

        if(q_is_empty(audio_pkt_q) && q_is_empty(video_pkt_q) 
            && q_is_empty(audio_frm_q) && q_is_empty(video_frm_q))
        {
            EL_DBG_LOG("state_machine: notify eof and should quit ...\n");
            msg = EL_PLAYER_MESSAGE_END_OF_FILE;
        }
        else
        {
            EL_DBG_LOG("state_machine:notify to play ...\n");
            //up_msg->msg = el_PLAYER_NOTIFY_BUFFERING_READY;
            msg = EL_PLAYER_MESSAGE_READY_TO_PLAY;
        }
        el_send_upper_layer_msg(msg,0,0);
        el_media_player_blk.need_notify_ready = EL_FALSE;
    }
}

EL_STATIC void el_pkt_q_push_cb(void *q,struct list_node *new_item)
{
    struct q_head *pkt_q = (struct q_head *)q;
//    el_q_id_e id = (el_q_id_e)pkt_q->priv;
    el_ffmpeg_packet_qnode_t *qnode = (el_ffmpeg_packet_qnode_t *)list_entry(new_item, el_ffmpeg_packet_qnode_t, list);
    el_mutex_lock(pkt_q->mutex);
    pkt_q->data_size += qnode->packet.size;
    el_mutex_unlock(pkt_q->mutex);
    //EL_DBG_LOG("push cb: id = %d,packet size = %ld, total = %ld,drop i = %d, key = %d\n",
        //id,qnode->packet.size,pkt_q->data_size,__built_for_internalincrementpkt,qnode->packet.flags & AV_PKT_FLAG_KEY);    
    return;
}

EL_STATIC void el_pkt_q_pop_cb(void *q,struct list_node *poped_item)
{
    struct q_head *pkt_q = (struct q_head *)q;
//    el_q_id_e id = (el_q_id_e)pkt_q->priv;
    el_ffmpeg_packet_qnode_t *qnode = (el_ffmpeg_packet_qnode_t *)list_entry(poped_item, el_ffmpeg_packet_qnode_t, list);
    el_mutex_lock(pkt_q->mutex);
    pkt_q->data_size -= qnode->packet.size;
    el_mutex_unlock(pkt_q->mutex);
    //EL_DBG_LOG("pop cb: id = %d,packet size = %ld, total = %ld, drop i = %d, key = %d\n",
        //id,qnode->packet.size,pkt_q->data_size,__built_for_internalincrementpkt,qnode->packet.flags & AV_PKT_FLAG_KEY);    
    return;
}

EL_STATIC EL_BOOL is_pkt_q_full(int64_t base_time)
{
    int64_t q_v_ts = -1;
    int64_t q_a_ts = -1;
    double buffering_percent = 0.1;
    
    struct q_head *audio_pkt_q = NULL;
    struct q_head *video_pkt_q = NULL;

    AVFormatContext *fc = el_media_file_blk.format_context;
    int *st_index = &el_media_file_blk.stream_index[0];
    video_pkt_q = &el_media_file_blk.video_pkt_q;
    audio_pkt_q = &el_media_file_blk.audio_pkt_q;

    if(st_index[AVMEDIA_TYPE_VIDEO] >= 0)
    {
        AVStream *vst = fc->streams[st_index[AVMEDIA_TYPE_VIDEO]];
        q_v_ts = get_packet_q_ts(video_pkt_q) * av_q2d(vst->time_base)*1000;
    }

    if(st_index[AVMEDIA_TYPE_AUDIO] >= 0)
    {
        AVStream *vst = fc->streams[st_index[AVMEDIA_TYPE_AUDIO]];
        q_a_ts = get_packet_q_ts(audio_pkt_q) * av_q2d(vst->time_base)*1000;
    }

    EL_DBG_LOG("state_machine:PKT Q,q_v_ts = %lld, q_a_ts = %lld, v node_nr = %d, a node_nr = %d, base_time = %lld\n",
               q_v_ts, q_a_ts, video_pkt_q->node_nr, audio_pkt_q->node_nr, base_time);

    switch(el_media_file_blk.av_support)
    {
        case EL_ENGINE_HAS_BOTH:
        {        
            if(el_media_file_blk.video_pkt_q.data_size >= EL_MAX_PKT_VIDEO_Q_MEM_SPACE)
            {
                EL_DBG_LOG("pkt video q exceed limit space, q full\n");

                goto is_full;
            }

            //if(q_v_ts > el_media_file_blk.buffering_time && q_a_ts > el_media_file_blk.buffering_time)
            if(q_v_ts > base_time && q_a_ts > base_time)
            {
                EL_DBG_LOG("state_machine: q/a > base time, is full\n");
                
                goto is_full;
            }

            buffering_percent = el_min(q_v_ts,q_a_ts) / (double)base_time;
            goto is_not_full;
            //return EL_FALSE;
        }
        case EL_ENGINE_HAS_AUDIO:
        {
            if(q_a_ts > base_time)
            {
                EL_DBG_LOG("PKT Q,q_a_ts = %lld a node_nr = %d\n",
                    q_a_ts,audio_pkt_q->node_nr);            
                goto is_full;
            }
            
            buffering_percent = q_a_ts / (double)base_time;
            goto is_not_full;
        }
        case EL_ENGINE_HAS_VIDEO: 
        {
            if(el_media_file_blk.video_pkt_q.data_size >= EL_MAX_PKT_VIDEO_Q_MEM_SPACE)
            {
                goto is_full;
            }
            
            if(q_v_ts > base_time)
            {
                EL_DBG_LOG("PKT Q,q_v_ts = %lld,v node_nr = %d\n",
                    q_v_ts,video_pkt_q->node_nr);            
                goto is_full;
            }
            
            buffering_percent = q_v_ts / (double)base_time;
            goto is_not_full;
            //return EL_FALSE;
        }
        default:
        {
            return EL_FALSE;
        }
    }
    
is_full:
    el_media_file_blk.buffering_percent = 0;
    return EL_TRUE;

is_not_full:
    el_notify_buffering_percent(buffering_percent);
    return EL_FALSE;
}


//
// try sequence mmst to mmsh
// @ type : 0 mmst, 1 mmsh
EL_STATIC void el_adjust_ffmpeg_mms_url(int type,char *url)
{
    int urllen;
//    char *ptr;
    char ch;
    char *proto = strstr((const char *)url,"mms://");

    if(type)
    {
        // mmsh
        ch = 'h';        
    }
    else
    {
        // mmst
        ch = 't';
    }

    if(proto)
    {
        urllen = strlen(proto + 3);
                memmove(proto + 4, proto + 3, urllen);
                proto[3] = ch;
        // Make sure that the url is with '0' tailed
        proto[4 + urllen] = 0;
        goto adjusted_ok;
    }
    
    proto = strstr((const char *)url,"mmst://");
    if(proto)
    {
        proto[3] = ch;
        goto adjusted_ok;
    }
    
    proto = strstr((const char *)url,"mmsh://");
    if(proto)
    {
        proto[3] = ch;
        goto adjusted_ok;
    }
    
    EL_DBG_LOG("state_machine: err in adjusting mms url %s\n",url);     
    return;
adjusted_ok:
    EL_DBG_LOG("state_machine: adjust mms url is %s\n",url);     
    return;
}


EL_STATIC EL_VOID el_player_engine_do_open(EL_BOOL aSuccess)
{
    if(aSuccess)
    {
        EL_DBG_LOG("video_desc.nHeight: %d", el_media_file_blk.video_desc.nHeight);
        EL_DBG_LOG("video_desc.nWidth: %d", el_media_file_blk.video_desc.nWidth);
        EL_DBG_LOG("audio_desc.eChannelCount: %d", el_media_file_blk.audio_desc.eChannelCount);
        EL_DBG_LOG("audio_desc.euiFreq: %d", el_media_file_blk.audio_desc.euiFreq);

        // 20120928
        el_player_engine_notify(EL_PLAYER_MESSAGE_OPEN_SUCCESS, (EL_VOID*)el_media_file_blk.video_desc.nWidth, (EL_VOID*)el_media_file_blk.video_desc.nHeight);
    }
    else
    {
        EL_DBG_LOG("el_player_demux_decoder_open err");
        el_player_engine_notify(EL_PLAYER_MESSAGE_OPEN_FAILED, 0, 0);
    }
}


EL_STATIC void el_do_stop_central_engine(void)
{
    el_media_player_blk.need_sending_upper_msg = EL_FALSE;
    el_ipc_send(EL_ENGINE_EVT_STOP,el_msg_q_decode_video);
    el_ipc_send(EL_ENGINE_EVT_STOP,el_msg_q_decode_audio);    
    q_all_destruction();
    el_engine_stop_render();
}

EL_STATIC void el_do_start_central_engine(void)
{
    el_media_player_blk.need_sending_upper_msg = EL_TRUE;
    el_ipc_send(EL_ENGINE_EVT_START,el_msg_q_decode_video);
    el_ipc_send(EL_ENGINE_EVT_START,el_msg_q_decode_audio);
    el_engine_start_render();
}

EL_STATIC void el_do_pause_central_engine(void)
{
    AVFormatContext *fc = el_media_file_blk.format_context;
    // do pause actions
    av_read_pause(fc);
    el_ipc_send(EL_ENGINE_EVT_PAUSE,el_msg_q_decode_video);
    el_ipc_send(EL_ENGINE_EVT_PAUSE,el_msg_q_decode_audio);    
    el_engine_pause_render();
}

EL_STATIC void el_do_resume_central_engine(void)
{
    AVFormatContext *fc = el_media_file_blk.format_context;
    el_media_file_blk.start_time = 0;
    av_read_play(fc);
    
    el_ipc_send(EL_ENGINE_EVT_PLAY,el_msg_q_decode_video);
    el_ipc_send(EL_ENGINE_EVT_PLAY,el_msg_q_decode_audio);
    
    el_engine_resume_render();
    
//    av_read_play(fc);
}

EL_STATIC void el_do_seek_pause_central_engine(void)
{
    AVFormatContext *fc = el_media_file_blk.format_context;
    // do pause actions
    av_read_pause(fc);
    el_ipc_send(EL_ENGINE_EVT_SEEK,el_msg_q_decode_video);
    el_ipc_send(EL_ENGINE_EVT_SEEK,el_msg_q_decode_audio);
    el_engine_pause_render();
    el_engine_reset_render();
}

EL_STATIC void el_do_notify_seek_to_decoder(void)
{
    el_ipc_send(EL_ENGINE_EVT_SEEK,el_msg_q_decode_video);
    el_ipc_send(EL_ENGINE_EVT_SEEK,el_msg_q_decode_audio);
    el_engine_reset_render();    
}

EL_STATIC EL_BOOL el_do_defered_open_file(void)
{
    EL_INT i,ret,codec_rst;
    int retry_count = 0;

    EL_DBG_FUNC_ENTER;

retry_open:
    // Open video file
    // MMS protocol check and probe
    // Note that in mms protocol, we have to try mmsh and mmst according to ffmpeg protocol
    if((ret = avformat_open_input (&(el_media_file_blk.format_context), (const char *)el_media_file_blk.pszUrlOrPathname, NULL, NULL))!=0)
    {
        EL_DBG_LOG("av_open_input_file,ret %d err\n",ret);

        ret = EL_FALSE;
        goto notify_callback;
    }

    
    AVFormatContext *format_context = el_media_file_blk.format_context;
    
    // 2012.10.08
    // set interrup call back
    format_context->interrupt_callback.callback = decode_interrupt_cb;
    format_context->interrupt_callback.opaque = &el_media_file_blk;
    
    el_media_file_blk.av_seeking = EL_TRUE;
    
    // Retrieve stream information
    if((ret = avformat_find_stream_info(el_media_file_blk.format_context, NULL)) < 0)
    {
        EL_DBG_LOG("Couldn't find stream information %d\n",ret);
        if(el_media_file_blk.format_context)
        {
            // 2012.10.08
            // close format_context, and set it to null
            avformat_close_input(&el_media_file_blk.format_context);
            
            el_media_file_blk.format_context = NULL;
        }
        
        ret = EL_FALSE;
        
        goto notify_callback;
    }
    
    EL_DBG_LOG("state_machine:av_find_stream_info called!\n");
    
#ifdef EL_LOG
    // Dump information about file onto standard error    
    av_dump_format(el_media_file_blk.format_context, 0, (const char *)el_media_file_blk.pszUrlOrPathname , 0);
#endif

    int *st_index = &el_media_file_blk.stream_index[0];

    st_index[AVMEDIA_TYPE_SUBTITLE] = -1;
    
    for (i = 0; i < format_context->nb_streams; i++)
    {
        format_context->streams[i]->discard = AVDISCARD_ALL;
    }

    for(i = 0; i < format_context->nb_streams; i++)
    {        
        if(format_context->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) 
        {
            st_index[AVMEDIA_TYPE_VIDEO] = i;
            el_media_file_blk.video_stream = format_context->streams[i];
            el_media_file_blk.av_support |= EL_ENGINE_HAS_VIDEO;
        }

        if(format_context->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO &&
            !(el_media_file_blk.av_support & EL_ENGINE_HAS_AUDIO)) 
        {
            st_index[AVMEDIA_TYPE_AUDIO] = i;
            el_media_file_blk.audio_stream = format_context->streams[i];
            el_media_file_blk.av_support |= EL_ENGINE_HAS_AUDIO;
        }

        if (format_context->streams[i]->codec->codec_type==AVMEDIA_TYPE_SUBTITLE)
        {
            st_index[AVMEDIA_TYPE_SUBTITLE] = i;
        }
    }
    
    el_player_engine_get_description(&el_media_file_blk.sELMediaMuxerDesc, \
        &el_media_file_blk.video_desc, &el_media_file_blk.audio_desc);
    el_media_file_blk.bHaveDesc = EL_TRUE;

    // open the codec
    if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) 
    {
        codec_rst = stream_component_open(st_index[AVMEDIA_TYPE_AUDIO]);
        if(codec_rst)
        {
            EL_DBG_LOG("state_machine: open audio codec err, disable audio support\n");
            el_media_file_blk.av_support &= ~EL_ENGINE_HAS_AUDIO;
            st_index[AVMEDIA_TYPE_AUDIO] = -1;
        }
    }
    
    if (st_index[AVMEDIA_TYPE_SUBTITLE] >= 0)
    {
        codec_rst = stream_component_open(st_index[AVMEDIA_TYPE_SUBTITLE]);

        if(codec_rst)
        {
            EL_DBG_LOG("state_machine: open subtitle codec err\n");
            st_index[AVMEDIA_TYPE_SUBTITLE] = -1;
        }
    }

    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) 
    {
        codec_rst= stream_component_open(st_index[AVMEDIA_TYPE_VIDEO]);
        if(codec_rst)
        {
            EL_DBG_LOG("state_machine: open video codec err, disable video support\n");
            el_media_file_blk.av_support &= ~EL_ENGINE_HAS_VIDEO;
            st_index[AVMEDIA_TYPE_VIDEO] = -1;
        }
    }

    EL_DBG_LOG("stream index video,audio,av_support is %d,%d,%d\n",
        st_index[AVMEDIA_TYPE_VIDEO],
        st_index[AVMEDIA_TYPE_AUDIO],
        el_media_file_blk.av_support);

    if(!el_media_file_blk.av_support)
    {
        ret = EL_FALSE;
        goto notify_callback;
    }

    ret = EL_TRUE;

notify_callback:
    
    // 2012.10.08
    if (el_media_file_blk.abort_request)    //    if(url_interrupt_cb())
    {
        EL_DBG_LOG("state_machine:user cancelled!\n");
        EL_DBG_FUNC_EXIT;

        return ret;
    }

    // check if it is an mms url
    if(!ret && el_media_file_blk.is_mms_file && !retry_count)
    {
        retry_count++;
        el_adjust_ffmpeg_mms_url(1, (EL_CHAR *)el_media_file_blk.pszUrlOrPathname);
        EL_DBG_LOG("state_machine:try to open using mmsh ...\n");
        goto retry_open;
    }

    EL_DBG_LOG("state_machine: defered open ret = %d, call open callback...\n",ret);
    el_player_engine_do_open(ret);

    EL_DBG_FUNC_EXIT;
    return ret;
}

EL_STATIC void el_do_process_idle(el_player_engine_state_machine_t *mac, el_player_engine_evt_e evt)
{
    switch(evt)
    {
        case EL_ENGINE_EVT_OPEN:
        {
            EL_DBG_LOG("state_machine: idle state recv evt open\n");
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_PREPARE);
            el_ipc_send(EL_ENGINE_EVT_MAC_GO_ON,mac->ipc_fd);
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

EL_STATIC void el_do_process_prepare(el_player_engine_state_machine_t *mac,el_player_engine_evt_e evt)
{
    switch(evt)
    {
        case EL_ENGINE_EVT_MAC_GO_ON:
        {
            // do deferred open
            if(el_do_defered_open_file())
            {
                el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_OPENED);
            }
            else
            {
                el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_IDLE);
            }
            return;
        }
        case EL_ENGINE_EVT_STOP:
        {
            // do stop actions
            el_do_stop_central_engine();
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_STOPPED);
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

EL_STATIC void el_do_process_opened(el_player_engine_state_machine_t *mac,el_player_engine_evt_e evt)
{
    switch(evt)
    {
        case EL_ENGINE_EVT_START:
        {
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_BUFFERING);
            el_do_start_central_engine();            
            el_ipc_send(EL_ENGINE_EVT_MAC_GO_ON,mac->ipc_fd);
            return;
        }
        case EL_ENGINE_EVT_SEEK_PAUSE:
        {
            el_do_start_central_engine();            
            el_ipc_send(EL_ENGINE_EVT_SEEK,el_msg_q_decode_video);
            el_ipc_send(EL_ENGINE_EVT_SEEK,el_msg_q_decode_audio);    
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_PAUSED);
            return;
        }
        case EL_ENGINE_EVT_STOP:
        {
            // do stop actions
            el_do_stop_central_engine();
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_STOPPED);
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


EL_STATIC void el_do_process_buffering(el_player_engine_state_machine_t *mac,el_player_engine_evt_e evt)
{
    switch(evt)
    {
        case EL_ENGINE_EVT_MAC_GO_ON:
        {
            int readcnt_in_one_msg = 0;
            
            do
            {
                if (el_do_read_one_packet() == AVERROR_EOF && el_media_file_blk.read_retry_count > 5)
                {
                    EL_DBG_LOG("state_machine:FILE EOF, play wait , vpktq = %d,apktq = %d\n",
                        el_media_file_blk.video_pkt_q.node_nr,
                        el_media_file_blk.audio_pkt_q.node_nr);
                    el_media_file_blk.is_file_eof = EL_TRUE;
                    el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_PLAY_FILE_END);
                    el_ipc_send(EL_ENGINE_EVT_STOP_DECODE_WAIT,el_msg_q_decode_video);
                    el_ipc_send(EL_ENGINE_EVT_STOP_DECODE_WAIT,el_msg_q_decode_audio);
                    el_media_player_blk.need_notify_ready = EL_TRUE;
                    el_notify_buffering_ready();
                    goto next_read_msg;
                }

                el_media_file_blk.is_file_eof = EL_FALSE;

                if(is_pkt_q_full(el_media_file_blk.buffering_time))
                {
                    EL_DBG_LOG("state_machine:buffering OK, play wait... time = %lld, v nr = %d, a nr = %d \n",el_media_file_blk.buffering_time,
                        el_media_file_blk.video_pkt_q.node_nr,
                        el_media_file_blk.audio_pkt_q.node_nr);
                    el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_PLAY_WAIT);


                if(el_media_file_blk.is_network_media &&
                      el_media_file_blk.buffering_time == EL_MAX_PKT_Q_NETWORK_FIRST_BUFFERING_TS)
                {
                      el_media_file_blk.buffering_time = EL_MAX_PKT_Q_NETWORK_BUFFERING_TS;
                      EL_DBG_LOG("state_machine: net buffering time changed to %d\n",EL_MAX_PKT_Q_NETWORK_BUFFERING_TS);
                }

                    // It is the time to force decode thread to work
                    el_media_player_blk.need_notify_ready = EL_TRUE;
                    el_media_player_blk.notified_ready2 = EL_FALSE;
                    el_ipc_send(EL_ENGINE_EVT_STOP_DECODE_WAIT,el_msg_q_decode_video);
                    el_ipc_send(EL_ENGINE_EVT_STOP_DECODE_WAIT,el_msg_q_decode_audio);
                    //el_notify_buffering_ready();
                    goto next_read_msg;
                }
            }while(++readcnt_in_one_msg <= EL_MAX_READ_CNT_IN_ONE_READ_MSG);
next_read_msg:
            
            // TODO: 20120928 这里会不会造成无谓的循环，从 go on 打 go on
            el_ipc_send(EL_ENGINE_EVT_MAC_GO_ON,el_msg_q_engine);
            return;
        }
        case EL_ENGINE_EVT_PAUSE:
        {
            EL_DBG_LOG("state_machine:buffering state recv pause evt!\n");
            el_ipc_send(EL_ENGINE_EVT_MAC_GO_ON,el_msg_q_engine);
            return;
        }
        case EL_ENGINE_EVT_SEEK_PAUSE:
        {
            el_do_seek_pause_central_engine();
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_PAUSED);
            return;
        }
        case EL_ENGINE_EVT_STOP:
        {
            // do stop actions
            el_do_stop_central_engine();
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_STOPPED);
            return;
        }        
        default:
        {
            EL_DBG_POS;
            return;
        }
    }
}


EL_STATIC void el_do_process_play_wait(el_player_engine_state_machine_t *mac,el_player_engine_evt_e evt)
{
    switch(evt)
    {
        // buffering ready is sent to upper layer, then upper layer calls play
        case EL_ENGINE_EVT_PLAY:
        {
            el_do_resume_central_engine();
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_PLAYING);
            el_ipc_send(EL_ENGINE_EVT_MAC_GO_ON,el_msg_q_engine);
            return;
        }
        case EL_ENGINE_EVT_MAC_GO_ON:
        {
            if(!el_media_file_blk.is_file_eof ||
                !el_media_file_blk.neterror.berror)
            {
            //  todo
                if(is_pkt_q_full(el_media_file_blk.play_buffering_time));
                {
                    EL_DBG_LOG("state_machine: play wait pkt full,vpktq = %d,apktq = %d\n",
                        el_media_file_blk.video_pkt_q.node_nr,
                        el_media_file_blk.audio_pkt_q.node_nr);
                    usleep(50000);
                    return;
                }
                
                if(el_do_read_one_packet() == AVERROR_EOF && el_media_file_blk.read_retry_count > 5)
                {
                    el_media_file_blk.is_file_eof = EL_TRUE;
                    EL_DBG_LOG("state_machine: play wait reach file eof\n");
                    el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_PLAY_FILE_END);
                }
                else
                {
                    // 20120928 TODO: 这里要不要Sleep??
                    el_ipc_send(EL_ENGINE_EVT_MAC_GO_ON,el_msg_q_engine);
                }

                if(el_media_file_blk.buffering_time == EL_MAX_PKT_Q_TS)
                {
                    // source is not network
                    usleep(2000);
                }
                return;
            }
            return;
        }
        case EL_ENGINE_EVT_STOP:
        {
            // do stop actions
            el_do_stop_central_engine();
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_STOPPED);
            return;
        }
        case EL_ENGINE_EVT_PAUSE:
        {
            el_do_pause_central_engine();
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_PAUSED);            
            return;
        }
        case EL_ENGINE_EVT_SEEK_PAUSE:
        {
            el_do_seek_pause_central_engine();
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_PAUSED);
            return;
        }
        case EL_ENGINE_EVT_BUFFERING:
        {
            EL_DBG_LOG("******pkt q empty,go to buffering\n");
            el_media_file_blk.buffering_percent = 0;
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_BUFFERING);
            el_ipc_send(EL_ENGINE_EVT_MAC_GO_ON,el_msg_q_engine);
            return;
        }
        default:
        {
            EL_DBG_POS;
            return;
        }
    }
}

EL_STATIC void el_do_process_playing(el_player_engine_state_machine_t *mac,el_player_engine_evt_e evt)
{
    switch(evt)
    {
        case EL_ENGINE_EVT_MAC_GO_ON:
        {
            int ret;

            if (el_do_read_one_packet() == AVERROR_EOF && el_media_file_blk.read_retry_count > 5)
            {
                EL_DBG_LOG("state_machine:PLAYING FILE EOF,vpktq = %d,apktq = %d\n",
                        el_media_file_blk.video_pkt_q.node_nr,
                        el_media_file_blk.audio_pkt_q.node_nr);
                el_media_file_blk.is_file_eof = EL_TRUE;
                el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_PLAY_FILE_END);
                el_ipc_send(EL_ENGINE_EVT_STOP_DECODE_WAIT,el_msg_q_decode_video);
                el_ipc_send(EL_ENGINE_EVT_STOP_DECODE_WAIT,el_msg_q_decode_audio);                
                el_media_player_blk.need_notify_ready = EL_TRUE;

                el_notify_buffering_ready();

                return;
            }

            el_media_file_blk.is_file_eof = EL_FALSE;

            if(el_media_file_blk.is_network_media)    
            {
                if(el_media_file_blk.video_pkt_q.data_size >= EL_MAX_PKT_VIDEO_Q_MEM_SPACE)
                {
                    ret = 1;

                    EL_DBG_LOG("network , pkt q full, %d,%d,%d\n",
                    el_media_file_blk.video_pkt_q.node_nr,
                    el_media_file_blk.audio_pkt_q.node_nr,
                    el_media_file_blk.video_pkt_q.data_size);
                }
            }
            else
            {
                ret = is_pkt_q_full(el_media_file_blk.play_buffering_time);
            }

            // 2012.10.07   // 本地文件等待时间长些，网络文件，等待时间短些
            if(ret)
            {
                usleep(10000);
            }
            else
            {
                usleep(1000);
            }

read_next_msg:
            el_ipc_send(EL_ENGINE_EVT_MAC_GO_ON,el_msg_q_engine);
            return;
        }
        case EL_ENGINE_EVT_BUFFERING:
        {
            EL_DBG_LOG("******pkt q empty,go to buffering\n");
            el_media_file_blk.buffering_percent = 0;
        
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_BUFFERING);
            el_ipc_send(EL_ENGINE_EVT_MAC_GO_ON,el_msg_q_engine);
            return;
        }
        case EL_ENGINE_EVT_PAUSE:
        {
            el_do_pause_central_engine();
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_PAUSED);
            return;
        }
        case EL_ENGINE_EVT_SEEK_PAUSE:
        {
            el_do_seek_pause_central_engine();
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_PAUSED);
            return;
        }
        case EL_ENGINE_EVT_PLAY_COMPLETE:
        {
              EL_DBG_LOG("state_machine: play complete!\n");
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_PLAY_COMPLETE);
            el_ipc_send(EL_ENGINE_EVT_MAC_GO_ON,el_msg_q_engine);
            return;
        }
        case EL_ENGINE_EVT_STOP:
        {
            // do stop actions
            el_do_stop_central_engine();
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_STOPPED);
            return;
        }
        default:
        {
            EL_DBG_POS;
            return;
        }
    }
}

EL_STATIC void el_do_process_play_file_end(el_player_engine_state_machine_t *mac,el_player_engine_evt_e evt)
{
    switch(evt)
    {
        case EL_ENGINE_EVT_PAUSE:
        {
              EL_DBG_LOG("state_machine: pause in play file end\n");
            el_do_pause_central_engine();
            return;
        }
        case EL_ENGINE_EVT_PLAY:
        {
              EL_DBG_LOG("state_machine: resume in play file end\n");
            el_do_resume_central_engine();
            return;
        }
        case EL_ENGINE_EVT_SEEK_PAUSE:
        {
            el_do_seek_pause_central_engine();
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_PAUSED);
            return;
        }
        case EL_ENGINE_EVT_PLAY_COMPLETE:
        {
            EL_DBG_LOG("state_machine: play complete!\n");
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_PLAY_COMPLETE);
            el_ipc_send(EL_ENGINE_EVT_MAC_GO_ON,el_msg_q_engine);
            return;
        }
        case EL_ENGINE_EVT_BUFFERING:
        {
            //if (el_media_file_blk.video_frm_q.node_nr == 0
                  if( el_media_file_blk.audio_frm_q.node_nr == 0
                && el_media_file_blk.video_pkt_q.node_nr == 0
                && el_media_file_blk.audio_pkt_q.node_nr == 0)
            {
                EL_DBG_LOG("state_machine: play complete in file end\n");
                el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_PLAY_COMPLETE);
                el_ipc_send(EL_ENGINE_EVT_MAC_GO_ON,el_msg_q_engine);
            }
            else
            {
                EL_DBG_LOG("state_machine: wait for frm q empty in file end\n");
                usleep(50000);
                el_ipc_send(EL_ENGINE_EVT_BUFFERING,el_msg_q_engine);
            }
            return;
        }
        case EL_ENGINE_EVT_STOP:
        {
            // do stop actions
            el_do_stop_central_engine();
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_STOPPED);
            return;
        }
        default:
        {
            EL_DBG_POS;
            return;
        }
    }
}

EL_STATIC void el_do_process_play_complete(el_player_engine_state_machine_t *mac,el_player_engine_evt_e evt)
{
    switch(evt)
    {
        case EL_ENGINE_EVT_MAC_GO_ON:
        {
            el_player_engine_notify(EL_PLAYER_MESSAGE_END_OF_FILE,0,0);
            return;
        }
        case EL_ENGINE_EVT_STOP: 
        {    
            // do stop actions
            el_do_stop_central_engine();
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_STOPPED);
            return;
        }
        case EL_ENGINE_EVT_SEEK_PAUSE:
        {
            el_do_seek_pause_central_engine();
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_PAUSED);            
            return;
        }
        default:
        {
            //el_player_engine_notify(EL_PLAYER_MESSAGE_END_OF_FILE,0,0);
            EL_DBG_POS;
            return;
        }
    }
}

EL_STATIC void el_do_process_play_stopped(el_player_engine_state_machine_t *mac,el_player_engine_evt_e evt)
{
    switch(evt)
    {
        case EL_ENGINE_EVT_CLOSE:
        {
            EL_DBG_LOG("state_machine: stop recv close evt\n");
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_IDLE);
            return;
        }
        case EL_ENGINE_EVT_RESTART:
        {
            EL_DBG_LOG("state_machine: stop recv restart evt\n");
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_OPENED);
            return;
        }
        default:
        {
            EL_DBG_POS;
            return;
        }
    }
}



EL_STATIC void el_do_process_seek_wait(el_player_engine_state_machine_t *mac,el_player_engine_evt_e evt)
{
    static int catch_ready_to_ready_cnt = 0;
    switch(evt)
    {
        case EL_ENGINE_EVT_READY_TO_SEEK:
        {
            AVFormatContext *fc = el_media_file_blk.format_context;
            // Here we should check more, now there are audio and video decode threads, so we need
            // to check if all ready_to_seek event are catched
            if((el_media_file_blk.av_support == EL_ENGINE_HAS_BOTH)
                && !catch_ready_to_ready_cnt)
            {
                EL_DBG_LOG("state_machine: waiting for next ready_to_seek evt!\n");
                catch_ready_to_ready_cnt++;
                return;
            }

            // change buffering time
            if(el_media_file_blk.is_network_media &&
                    el_media_file_blk.buffering_time == EL_MAX_PKT_Q_NETWORK_BUFFERING_TS)
            {
                    el_media_file_blk.buffering_time = EL_MAX_PKT_Q_NETWORK_FIRST_BUFFERING_TS;
                    EL_DBG_LOG("state_machine: before seek, net buffering time changed to %d\n",EL_MAX_PKT_Q_NETWORK_FIRST_BUFFERING_TS);
            }
            
            catch_ready_to_ready_cnt = 0;

            // Notify UI and engine to send buffering start message 
            el_notify_buffering_start();

            // Note that this event is sent by decode thread when it is in its wait state
            el_ffmpeg_do_seek();

            // after seek, discard packet that comes from last play
            
            
            EL_DBG_LOG("state_machine: sending seek done evt to decoder!\n");
            EL_DBG_LOG("*****@@pkt q, a frm = %d, v frm = %d\n",
                el_media_file_blk.audio_frm_q.node_nr,el_media_file_blk.video_frm_q.node_nr);

            // Notify decoder thread
            el_ipc_send(EL_ENGINE_EVT_SEEK_DONE,el_msg_q_decode_video);
            el_ipc_send(EL_ENGINE_EVT_SEEK_DONE,el_msg_q_decode_audio);
            // do resume actions
            av_read_play(fc);
            
            el_media_file_blk.buffering_percent = 0;
//#ifdef USE_NEW_ENGINE
            el_media_file_blk.start_time = 0;
//#endif
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_BUFFERING);
            // send read next packet message
            el_ipc_send(EL_ENGINE_EVT_MAC_GO_ON,el_msg_q_engine);
            
            return;
        }
        case EL_ENGINE_EVT_STOP:
        {
            // do stop actions
            el_do_stop_central_engine();
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_STOPPED);
            return;
        }
        case EL_ENGINE_EVT_SEEK_PAUSE:
        {
            EL_DBG_LOG("state_machine: seek wait pause\n");
            catch_ready_to_ready_cnt = 0;
            // do pause actions
            el_do_seek_pause_central_engine();
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_PAUSED);
            return;
        }        
        default:
        {
            EL_DBG_POS;
            return;
        }
    }    
}

EL_STATIC void el_do_process_paused(el_player_engine_state_machine_t *mac,el_player_engine_evt_e evt)
{
    switch(evt)
    {
        case EL_ENGINE_EVT_MAC_GO_ON:
        {
            EL_DBG_LOG("state_machine: paused state enter empty loop\n");
            return;
        }
        case EL_ENGINE_EVT_PLAY:
        {
            el_do_resume_central_engine();
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_PLAYING);
            // send read next packet message
            el_ipc_send(EL_ENGINE_EVT_MAC_GO_ON,el_msg_q_engine);
            return;
        }
        case EL_ENGINE_EVT_SEEK_PAUSE:
        {
            el_do_notify_seek_to_decoder();
            return;
        }
        case EL_ENGINE_EVT_SEEK:
        {
            EL_DBG_LOG("begin to seek...\n");
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_SEEK_WAIT);
            return;
        }
        case EL_ENGINE_EVT_STOP:
        {
            // do stop actions
            el_do_stop_central_engine();
            el_state_machine_change_state(mac,EL_ENGINE_CENTRAL_STATE_STOPPED);
            return;
        }
        default:
        {
            EL_DBG_POS;
            return;
        }
    }    
}


// NOTE: call this function after file opened
EL_STATIC void el_player_engine_central_state_machine(el_player_engine_state_machine_t *mac,el_player_engine_evt_e evt)
{
    switch(mac->state)
    {
        case EL_ENGINE_CENTRAL_STATE_IDLE:
        {
            el_do_process_idle(mac,evt);
            return;
        }
        case EL_ENGINE_CENTRAL_STATE_PREPARE:
        {
            el_do_process_prepare(mac,evt);
            return;
        }
        case EL_ENGINE_CENTRAL_STATE_OPENED:
        {
            el_do_process_opened(mac,evt);
            return;
        }        
        case EL_ENGINE_CENTRAL_STATE_BUFFERING:
        {
            el_do_process_buffering(mac,evt);
            return;
        }
        case EL_ENGINE_CENTRAL_STATE_PLAY_WAIT:
        {
            el_do_process_play_wait(mac,evt);
            return;
        }            
        case EL_ENGINE_CENTRAL_STATE_PLAYING:
        {
            el_do_process_playing(mac,evt);
            return;
        }
        case EL_ENGINE_CENTRAL_STATE_PLAY_FILE_END:
        {
            el_do_process_play_file_end(mac,evt);
            return;
        }
        case EL_ENGINE_CENTRAL_STATE_PAUSED:
        {
            el_do_process_paused(mac,evt);
            return;
        }
        case EL_ENGINE_CENTRAL_STATE_SEEK_WAIT: 
        {
            el_do_process_seek_wait(mac,evt);    
            return;
        }
        case EL_ENGINE_CENTRAL_STATE_PLAY_COMPLETE: 
        {
            el_do_process_play_complete(mac,evt);    
            return;
        }
        case EL_ENGINE_CENTRAL_STATE_STOPPED: 
        {
            el_do_process_play_stopped(mac,evt);    
            return;
        }
        default:
        {
            EL_DBG_LOG("Invalid state!\n");
            return;
        }
    }
    
}

/////////////////////////////////////////////////////////////
/* Lines below are decode thread state machine*/
EL_STATIC void el_do_process_decode_wait(el_player_engine_state_machine_t *mac,el_player_engine_evt_e evt)
{
    switch(evt)
    {
        case EL_ENGINE_EVT_STOP_DECODE_WAIT:
        {
            // we should suspend the read thread until next resume is called
            EL_DBG_LOG("decoder:decode thread recv stop wait!\n");
            el_state_machine_change_state(mac,EL_ENGINE_DECODE_STATE_WORK);
            // step into decode loop loginc, to make it loops, we send message selfly
            el_ipc_send(EL_ENGINE_EVT_DECODE_MAC_GO_ON,mac->ipc_fd);
            if(mac->type == el_state_machine_video_decoder)
                el_reset_decoder_controller();
            return;
        }
        case EL_ENGINE_EVT_PLAY:
        {
            EL_DBG_LOG("decoder:decode thread recv resume evt!\n");
            el_state_machine_change_state(mac,EL_ENGINE_DECODE_STATE_WORK);
            // step into decode loop logic, to make it loops, we send message selfly
            el_ipc_send(EL_ENGINE_EVT_DECODE_MAC_GO_ON,mac->ipc_fd);
            if(mac->type == el_state_machine_video_decoder)
                el_resume_decoder_controller();
            return;
        }
        case EL_ENGINE_EVT_SEEK:
        {
            EL_DBG_LOG("decoder: decode thread sent ready to seek evt...\n");
            el_state_machine_change_state(mac,EL_ENGINE_DECODE_STATE_SEEK_WAIT);
            el_ipc_send(EL_ENGINE_EVT_READY_TO_SEEK,el_msg_q_engine);
            return;
        }
        case EL_ENGINE_EVT_STOP:
        {
            el_state_machine_change_state(mac,EL_ENGINE_DECODE_STATE_START);
            EL_DBG_LOG("decoder:decode stoped!\n");
            return;
        }        
        default:
        {
            return;
        }
    }
}


EL_STATIC void el_video_frm_q_empty_func_cb(void *mac)
{
    return;
}

EL_STATIC void el_launch_cmo_buffering(void)
{
    struct q_head *audio_pkt_q = &el_media_file_blk.audio_pkt_q;

    struct q_head *video_pkt_q = &el_media_file_blk.video_pkt_q;
    struct q_head *audio_frm_q = &el_media_file_blk.audio_frm_q;
    struct q_head *video_frm_q = &el_media_file_blk.video_frm_q;

    switch(el_media_file_blk.av_support)
    {
        case EL_ENGINE_HAS_BOTH:
        {
            if(q_is_empty(audio_pkt_q))
            {
                EL_DBG_LOG("****************launch buffering, a frm = %d, v frm = %d, vpkt = %d\n",
                    audio_frm_q->node_nr,video_frm_q->node_nr,video_pkt_q->node_nr);

                // notify buffering start
                el_do_pause_central_engine();
                el_notify_buffering_start();            
                // no need to stop decoder currently
                if(el_media_file_blk.engine.state != EL_ENGINE_CENTRAL_STATE_BUFFERING)
                {
                    el_ipc_send(EL_ENGINE_EVT_BUFFERING,el_msg_q_engine);
                }
            }

            return;
        }
        default:
        {
            return;
        }

    }

}

EL_STATIC void el_pkt_q_empty_goto_buffering_cb(void *mac)
{
    struct q_head *audio_pkt_q = &el_media_file_blk.audio_pkt_q;
    struct q_head *video_pkt_q = &el_media_file_blk.video_pkt_q;
    struct q_head *audio_frm_q = &el_media_file_blk.audio_frm_q;
    struct q_head *video_frm_q = &el_media_file_blk.video_frm_q;

    switch(el_media_file_blk.av_support)
    {
        case EL_ENGINE_HAS_BOTH:
        {
            if(q_is_empty(video_pkt_q) && q_is_empty(audio_pkt_q)) 
            {
                EL_DBG_LOG("****************empty pkt q, a frm = %d, v frm = %d\n",
                    audio_frm_q->node_nr,video_frm_q->node_nr);
                // send buffering to read thread
                // if reader state is buffering, no need to send buffering any more
                if(((el_player_engine_state_machine_t *)mac)->state != EL_ENGINE_DECODE_STATE_WAIT)
                {
                    // notify engine to stop playing
                    el_engine_pause_render();
                    el_notify_buffering_start();
                    el_state_machine_change_state((el_player_engine_state_machine_t *)mac, EL_ENGINE_DECODE_STATE_WAIT);
                }
                
                if(el_media_file_blk.engine.state != EL_ENGINE_CENTRAL_STATE_BUFFERING)
                {
                    el_ipc_send(EL_ENGINE_EVT_BUFFERING,el_msg_q_engine);
                    EL_DBG_LOG("mac 1 state = %d\n",el_media_file_blk.engine.state);
                }
                else
                {
                    EL_DBG_LOG("decoder: reentering pkt q empty call back,sending evt buffering...\n");
            
                }
                return;
            }
            return;
        }
        case EL_ENGINE_HAS_AUDIO:
        {
            EL_DBG_LOG("****************empty pkt q, a frm = %d, v frm = %d\n",
                audio_frm_q->node_nr,video_frm_q->node_nr);
            el_state_machine_change_state((el_player_engine_state_machine_t *)mac,EL_ENGINE_DECODE_STATE_WAIT);
            // send buffering to read thread
            el_ipc_send(EL_ENGINE_EVT_BUFFERING,el_msg_q_engine);
            // notify engine to stop playing
            //el_notify_buffering_start();
            return;
        }
        case EL_ENGINE_HAS_VIDEO: 
        {
            EL_DBG_LOG("****************empty pkt q, a frm = %d, v frm = %d\n",
                audio_frm_q->node_nr,video_frm_q->node_nr);
            el_state_machine_change_state((el_player_engine_state_machine_t *)mac,EL_ENGINE_DECODE_STATE_WAIT);
            // send buffering to read thread
            el_ipc_send(EL_ENGINE_EVT_BUFFERING,el_msg_q_engine);
            // notify engine to stop playing
            //el_notify_buffering_start();
            return;
        }
        default:
        {
            return;
        }
    }
}


EL_STATIC void el_frm_q_full_wait_available_cb(void *mac)
{
//    struct q_head *audio_pkt_q = &el_media_file_blk.audio_pkt_q;
//    struct q_head *video_pkt_q = &el_media_file_blk.video_pkt_q;
    struct q_head *audio_frm_q = &el_media_file_blk.audio_frm_q;
    struct q_head *video_frm_q = &el_media_file_blk.video_frm_q;

    switch(el_media_file_blk.av_support)
    {
        case EL_ENGINE_HAS_BOTH:
        {
            if(audio_frm_q->node_nr >= audio_frm_q->max_node_nr
                && video_frm_q->node_nr >= video_frm_q->max_node_nr)
            {
                int drop_count;
                // sycn A/V samples
                if(el_media_player_blk.need_notify_ready
                    && !el_media_player_blk.notified_ready2)
                {
                    el_media_player_blk.notified_ready2 = EL_TRUE;
                    drop_count = sync_av_after_seek(&el_media_file_blk.av_sync_time);
                    //drop_count = 0; // test for
                    if (drop_count == 0)
                    {
                        //EL_DBG_LOG("sync_av_after_seek done, send buffering ready");
                        el_media_file_blk.av_seeking = EL_FALSE;
                        el_state_machine_change_state(&el_media_file_blk.video_decoder,EL_ENGINE_DECODE_STATE_WAIT);
                        el_state_machine_change_state(&el_media_file_blk.audio_decoder,EL_ENGINE_DECODE_STATE_WAIT);
                        el_notify_buffering_ready();

                    }
                    else if(drop_count == -1)
                    {
                        el_media_player_blk.notified_ready2 = EL_FALSE;
                        EL_DBG_LOG("sync_av_after_seek, prevent buffering ready");
                        if(((el_player_engine_state_machine_t *)mac)->type == el_state_machine_video_decoder)
                            el_reset_decoder_controller();
                    }
                    else
                    {
                        el_media_player_blk.notified_ready2 = EL_FALSE;
                        EL_DBG_LOG("sync_av_after_seek, drop %d frames\n",drop_count);
                        if(((el_player_engine_state_machine_t *)mac)->type == el_state_machine_video_decoder)
                            el_reset_decoder_controller();
                    }
                }
            }
            else if(video_frm_q->node_nr >= video_frm_q->max_node_nr)
            {
                if(el_media_file_blk.audio_pkt_q.node_nr == 0 &&
                    el_media_file_blk.engine.state == EL_ENGINE_CENTRAL_STATE_PLAY_WAIT)
                {
                    el_ipc_send(EL_ENGINE_EVT_BUFFERING,el_msg_q_engine);
                        EL_DBG_LOG("audio frm q request buffering\n");                
                }

            }
            else
            {
                if(el_media_file_blk.video_pkt_q.node_nr == 0 &&
                                        el_media_file_blk.engine.state == EL_ENGINE_CENTRAL_STATE_PLAY_WAIT)
                                {
                                        el_ipc_send(EL_ENGINE_EVT_BUFFERING,el_msg_q_engine);
                                        EL_DBG_LOG("video frm q request buffering\n");
                                }

            }    
            return;
        }
        case EL_ENGINE_HAS_AUDIO:
        {
            //EL_DBG_LOG("decoder:audio frm q full, wait ...\n");
            el_notify_buffering_ready();
            return;
        }
        case EL_ENGINE_HAS_VIDEO: 
        {
            //EL_DBG_LOG("decoder:video frm q full,wait 1ms...\n");
            el_notify_buffering_ready();
            return;
        }
        default:
        {
            return;
        }
    }
}

EL_STATIC void el_frm_q_set_time_limit(struct q_head *frm_q,int64_t pts,
    el_player_media_type_e type)
{
    if(!frm_q->need_reset_time_limit)
    {
        return;
    }
    
    if(frm_q->start_node_pts == 0)
    {
        if(pts)
        {
            frm_q->start_node_pts = pts;
        }
    }
    else
    {
        if(pts)
        {                
            frm_q->current_node_pts = pts;
            if(++frm_q->cal_pts_denominator == 10)
            {
                // calc avg frame gap of time
                int space_limit_frame_cnt;
                int time_limit_frame_cnt;
                int max_frame;
                float avg = 
                    (frm_q->current_node_pts - frm_q->start_node_pts) / 10.0f;
                time_limit_frame_cnt = (int)(EL_MS(EL_MAX_FRM_Q_TIME_LIMIT) / avg + 1);

                if(type == EL_MEDIA_VIDEO)
                {
                    // size limit
                    if(el_media_file_blk.video_frame_size)
                    {
                        space_limit_frame_cnt = (EL_MAX_FRM_VIDEO_Q_MEM_SPACE / el_media_file_blk.video_frame_size) + 1;
                    }

                    max_frame = el_min(time_limit_frame_cnt,space_limit_frame_cnt);
                }
                else
                {
                    max_frame = time_limit_frame_cnt;
                }
                
                if(max_frame < 5)
                {
                    max_frame = 5;
                }
                else if(max_frame > 100)
                {
                    max_frame = 100;
                }

                EL_DBG_LOG("type = %d,avg frame gap = %f,reset max frame count = %d\n",type,avg,max_frame);

                q_set_property(frm_q,el_q_node_max_nr,
                    (void *)max_frame,NULL);                        
                frm_q->need_reset_time_limit = 0;
                // clear pts data
                frm_q->start_node_pts = 0;
                frm_q->current_node_pts = 0;
                frm_q->cal_pts_denominator = 0;
            }
        }
    }
}

EL_STATIC AVFrame *alloc_picture(enum PixelFormat pix_fmt, int width, int height)
{
    AVFrame *picture;
    uint8_t *picture_buf;
    int size;

    picture = avcodec_alloc_frame();
    if (!picture)
        return NULL;
    size = avpicture_get_size(pix_fmt, width, height);
    picture_buf = av_malloc(size);
    if (!picture_buf) {
        av_free(picture);
        return NULL;
    }
    avpicture_fill((AVPicture *)picture, picture_buf,
                   pix_fmt, width, height);
    return picture;
}


EL_STATIC int64_t el_find_key_frame_pts(struct q_head *head)
{
    AVPacket *packet;
    struct list_node *ptr;
    el_ffmpeg_packet_qnode_t *pkt_qnode;
    int64_t dts = -1;

    el_mutex_lock(head->mutex);
    if(list_empty(&head->lst_node))
    {
        goto unlock;
    }

    ptr = head->lst_node.next;

    while (ptr->next != &head->lst_node)
    {
        pkt_qnode = (el_ffmpeg_packet_qnode_t *)list_entry(ptr, el_ffmpeg_packet_qnode_t, list);
        packet = &(pkt_qnode->packet);

        if (packet->flags & AV_PKT_FLAG_KEY)
        {
//            EL_DBG_LOG("el_find_key_frame_pts, found a key frame, ts: %lld", packet->pts);
            //EL_DBG_LOG("list exit1...\n");
            dts = packet->dts;
            goto unlock;
        }
//        else
//            EL_DBG_LOG("el_find_key_frame_pts, found a normal frame, ts: %lld", packet->pts);

        ptr = ptr->next;
    }
unlock:
    el_mutex_unlock(head->mutex);
    //EL_DBG_LOG("list exit2...\n");
    return dts;
}

// 解码一帧数据
EL_STATIC int el_decode_one_video_packet(el_ffmpeg_packet_qnode_t *pkt_qnode)
{
    int64_t pts;
    int frameFinished;
    AVPacket *packet;
    el_ffmpeg_frame_qnode_t *frm_node;
    int nlen;
    unsigned long required_size;
    int64_t start,end;
    static int diff = 0;
    static int n = 0;
    
    AVCodecContext *videoctx = el_media_file_blk.video_stream->codec;
    struct q_head *video_frm_q = &el_media_file_blk.video_frm_q; 

    packet = &(pkt_qnode->packet);

    start = el_get_sys_time_ms();

    // 调用ffmpeg的解码函数，这里计算真正的调用底层的解码时间。如果需要做硬解码，则需要替换这里就OK了。
    nlen = avcodec_decode_video2(videoctx,
                                 el_media_player_blk.video_frame,
                                 &frameFinished,
                                 packet);

    end = el_get_sys_time_ms();
    diff += (end - start);
    if(++n >= 5)
    {
        el_media_file_blk.avg_video_decoded_time = diff/n;
        n = 0;
        diff = 0;
    }

    if(!frameFinished)  // 没有数据可以解码
    {
        packet_node_free(pkt_qnode);    

        return 0;
    }

    if(packet->dts != AV_NOPTS_VALUE) 
    {                    
        pts = (int64_t)(packet->dts * av_q2d(el_media_file_blk.video_stream->time_base)*1000);
    }
    else
    {
        pts = 0;
        EL_DBG_LOG("decoder:meet pts no value!\n");
    }

    if(videoctx->codec_id != CODEC_ID_RAWVIDEO)
    {
        required_size = videoctx->width*videoctx->height
            + (videoctx->width/2)*(videoctx->height/2)*2;
    }
    else
    {
        // raw video, rgb24
        required_size = videoctx->width*videoctx->height*3;
    }

    // xw，解码，并且将解码放到视频队列
#if 1
    el_media_file_blk.video_frame_size = required_size;
    frm_node = el_allocate_frm_node(EL_MEDIA_VIDEO,required_size);
    AVFrame * pFrameData = el_media_player_blk.video_frame;
    if (frm_node)
    {        
        ELAVSample_t *frm_sample = &(frm_node->frm_sample);

        frm_sample->width = videoctx->width;
        frm_sample->height = videoctx->height;
        frm_sample->dwTS = pts;
        el_frm_q_set_time_limit(video_frm_q,pts,EL_MEDIA_VIDEO);


        if(videoctx->codec_id != CODEC_ID_RAWVIDEO)
        {
            yuv_data_collect(pFrameData, frm_sample->pBuffer, videoctx->width, videoctx->height);
            q_push(&(frm_node->list), video_frm_q);
        }
        else
        {
            // is raw video, rgb24
            rgb_data_collect(el_media_player_blk.video_frame, frm_sample->pBuffer, videoctx->width, videoctx->height);
            q_push(&(frm_node->list), video_frm_q);
        }
    }
#endif  // test
    packet_node_free(pkt_qnode);    


//    printf("2-----xxxxxxx--------- decode use time =%lld \n", el_get_sys_time_ms() - beginTime);

    return 0;    

}

// if pkt q is empty, return -1, if can not be decoded, return -2
EL_STATIC int el_decode_one_audio_packet(void)
{
    // decode audio
    int len;
    struct list_node * ptr;
    uint8_t *data_bak;
    int   size_bak;
    el_ffmpeg_frame_qnode_t *frm_node;
    el_ffmpeg_packet_qnode_t *pkt_node;
    AVPacket *packet;
    int data_size;
    EL_BOOL decoded = EL_FALSE;
    AVAudioConvert *reformat_ctx;
    int16_t *av_decoded_audio;
    int64_t pts;
    
    AVCodecContext *ctx = el_media_file_blk.audio_stream->codec;
    struct q_head *audio_frm_q = &el_media_file_blk.audio_frm_q;

    struct q_head *audio_pkt_q = &el_media_file_blk.audio_pkt_q;
    ptr = q_pop(audio_pkt_q);
    if(!ptr)
    {
        EL_DBG_LOG("apkt q already empty, v nr = %d\n",el_media_file_blk.video_pkt_q.node_nr);
        return -1;
    }

    pkt_node = (el_ffmpeg_packet_qnode_t *)list_entry(ptr, el_ffmpeg_packet_qnode_t, list);
    packet = &(pkt_node->packet);

    if(packet->pts != packet->dts)
    {
        EL_DBG_LOG("**********error audio pts != dts****\n");
    }

    if(packet->dts != AV_NOPTS_VALUE)
    {
        pts = (int64_t)(packet->dts * av_q2d(el_media_file_blk.audio_stream->time_base)*1000);
//        EL_DBG_LOG("packet dts: %lld, pts: %lld\n", packet->dts, pts);
    }
    else
    {
        pts = 0;
    }

    data_bak = packet->data;
    size_bak = packet->size;    

#ifdef USE_FINE_ADJUSTING
    if(pts < el_media_file_blk.seekpos)
    {
        EL_DBG_LOG("decoder:adjust a pts = %lld, target = %lld\n",
                pts,el_media_file_blk.seekpos);        
        goto free_pkt;
    }
#endif

    /* NOTE: the audio packet can contain several frames */
    while (packet->size > 0) 
    {
        data_size = AVCODEC_MAX_AUDIO_FRAME_SIZE*3/2;
        
        // Make sure the channels here is what we expected
        ctx->channels = el_media_file_blk.audio_channels_cnt;
        
        // 2012.10.08
        len = avcodec_decode_audio3(ctx,
                                    el_media_player_blk.audio_decode_buf,
                                    &data_size,
                                    packet);

        if (len < 0)
        {
            /* if error, we skip the frame */
            //packet->size = 0;
            EL_DBG_LOG("decode audio pkt not OK %d!\n",len);

            goto free_pkt;
        }
        
        packet->data += len;
        packet->size -= len;

        if (data_size <= 0)
        {
            EL_DBG_LOG("data size err!\n");

            continue;
        }

        //EL_DBG_LOG("data_size = %d,pts = %f \n",data_size,
        //    packet->pts * av_q2d(el_media_file_blk.audio_stream->time_base));

        // need convert to S16
        if(ctx->sample_fmt == AV_SAMPLE_FMT_FLT ||
            ctx->sample_fmt == AV_SAMPLE_FMT_DBL)
        {
            // 2012.10.08
            const void *ibuf[6]= {el_media_player_blk.audio_decode_buf};
            void *obuf[6]= {el_media_player_blk.audio_decode_buf2};
            int istride[6]= {el_get_bits_per_sample_fmt(ctx->sample_fmt)/8};
            int ostride[6]= {2};
            int len= data_size/istride[0];
        
            reformat_ctx = av_audio_convert_alloc(AV_SAMPLE_FMT_S16, 1,
                                                             ctx->sample_fmt, 1, NULL, 0);
            if (!reformat_ctx) 
            {
                //EL_DBG_LOG("create convert ctx err!\n");
                EL_DBG_LOG("Cannot convert %s sample format to %s sample format\n",
                    el_get_sample_fmt_name(ctx->sample_fmt),
                    el_get_sample_fmt_name(AV_SAMPLE_FMT_S16));
                goto free_pkt;
            }

            if (av_audio_convert(reformat_ctx, obuf, ostride, ibuf, istride, len)<0) 
            {
                EL_DBG_LOG("av_audio_convert() failed\n");
                goto free_pkt;
            }

            // free the audio convert context
            av_audio_convert_free(reformat_ctx);
            
            av_decoded_audio = el_media_player_blk.audio_decode_buf2;
            /* FIXME: existing code assume that data_size equals framesize*channels*2
            remove this legacy cruft */
            data_size= len*2;
        }
        else
        {
            av_decoded_audio = el_media_player_blk.audio_decode_buf;
        }

        // a frame decode OK here
        frm_node = el_allocate_frm_node(EL_MEDIA_AUDIO,data_size);
        if(frm_node)
        {    
            ELAVSample_t *frm_sample = &(frm_node->frm_sample);
            //el_sample->nBufferSize = data_size;
            memcpy(frm_sample->pBuffer,av_decoded_audio,data_size);
            //EL_DBG_LOG("pts = %lld\n",packet->pts);
            frm_sample->dwTS = pts;
            el_frm_q_set_time_limit(audio_frm_q,pts,EL_MEDIA_AUDIO);
            frm_sample->bVolumeAdjusted = EL_FALSE;
            // add frame q
            //EL_DBG_LOG("decode audio pts = %lld,size = %d\n",el_sample->dwTS,data_size);
            q_push(&(frm_node->list), audio_frm_q);

            decoded = EL_TRUE;
        }
        else
        {
            goto free_pkt;
        }
    }

free_pkt:
    packet->data = data_bak;
    packet->size = size_bak;
    packet_node_free(pkt_node);
    if(decoded)
    {
        return 0;
    }
    
    return -2;
}

EL_STATIC void el_do_process_decode_seek_wait(el_player_engine_state_machine_t *mac,el_player_engine_evt_e evt)
{
       switch(evt)
       {
            case EL_ENGINE_EVT_SEEK_DONE:
            {
                EL_DBG_LOG("decoder:decode thread recv seek done!\n");
                EL_DBG_LOG("*****@@@pkt q, a frm = %d, v frm = %d\n",
                               el_media_file_blk.audio_frm_q.node_nr,el_media_file_blk.video_frm_q.node_nr);
                el_state_machine_change_state(mac,EL_ENGINE_DECODE_STATE_WAIT);
                return;
            }
            case EL_ENGINE_EVT_SEEK_PAUSE:
            {
                EL_DBG_LOG("decoder:decode thread seek wait recv seek pause!\n");
                el_state_machine_change_state(mac,EL_ENGINE_DECODE_STATE_WAIT);
                return;
            }
            case EL_ENGINE_EVT_STOP:
            {
                   el_state_machine_change_state(mac,EL_ENGINE_DECODE_STATE_START);
                   EL_DBG_LOG("decoder:decode stoped!\n");
                   return;
            }
            default:
            {
                EL_DBG_POS;
                return;
            }
       }       
}

EL_STATIC void el_do_process_decode_start(el_player_engine_state_machine_t *mac,el_player_engine_evt_e evt)
{
    switch(evt)
    {
        case EL_ENGINE_EVT_START:
        {
            EL_DBG_LOG("decoder:decoder recv start event,goto wait!\n");
            el_state_machine_change_state(mac,EL_ENGINE_DECODE_STATE_WAIT);
            return;
        }

        default:
        {
            EL_DBG_POS;
            return;
        }
    }    
}

EL_STATIC void el_do_process_decode_work(el_player_engine_state_machine_t *mac,el_player_engine_evt_e evt)
{
    switch(evt)
    {
        case EL_ENGINE_EVT_DECODE_MAC_GO_ON:
        {
//            struct q_head *audio_pkt_q;
//            struct q_head *video_pkt_q;
            struct q_head *audio_frm_q;
            struct q_head *video_frm_q;
            //EL_BOOL has_audio = EL_FALSE;
            EL_BOOL has_video = EL_FALSE;
            

            if(el_media_file_blk.av_support & EL_ENGINE_HAS_VIDEO)
            {
                if(el_media_file_blk.video_stream->codec->codec)
                {
                    has_video = EL_TRUE;
                }
            }
                
            video_frm_q = &el_media_file_blk.video_frm_q;
            audio_frm_q = &el_media_file_blk.audio_frm_q;
    
            // 20120928 这里会不会有问题，导致速度变慢
            // step into decode loop loginc, to make it loops, we send message selfly
            el_ipc_send(EL_ENGINE_EVT_DECODE_MAC_GO_ON, mac->ipc_fd);

            //EL_DBG_LOG("working mac = %d, before decoding...\n",mac->type);

            if(mac->type == el_state_machine_video_decoder)
            {
                if(has_video)
                {
                    int cnt = 0;

                    while(cnt++ < (EL_MAX_FRM_VIDEO_Q_NODE_CNT >> 1))
                    {
                        if(video_frm_q->node_nr >= video_frm_q->max_node_nr)
                        {
                            video_frm_q->full_func(video_frm_q->full_parm);
                            usleep(50000);
                            return;
                        }

                        el_decoder_controller_state_machine(&el_media_file_blk.decoder_controller, EL_ENGINE_EVT_MAC_GO_ON);

                    } // end while
                }    
            }
            else
            {
                //if(has_audio)
                //{
                int  cnt = 0;
                int ret;
                // audio decoder state machine
                while(cnt++ < 1)// EL_MAX_FRM_AUDIO_Q_NODE_CNT)// && !q_is_empty(audio_pkt_q))
                {    
                    if(audio_frm_q->node_nr >= audio_frm_q->max_node_nr)
                    {
                        audio_frm_q->full_func(audio_frm_q->full_parm);
                        usleep(50000);
                        return;
                    }                    
                    
                    ret = el_decode_one_audio_packet();
                    
                    // 2012.10.08
                    if(ret == -1)
                    {
                        EL_DBG_LOG("decode one audio,pkt q is empty...\n");
                        usleep(2000);
                        return;
                    }
                }
                //} // endif has audio
            }

            //EL_DBG_LOG("working mac = %d, after decoding...\n",mac->type);
            return;
        }
        case EL_ENGINE_EVT_PAUSE:
        case EL_ENGINE_EVT_SEEK_PAUSE:
        {
            EL_DBG_LOG("decoder:mac = %d,decode paused!\n",mac->type);
            el_state_machine_change_state(mac,EL_ENGINE_DECODE_STATE_WAIT);
            return;
        }
        case EL_ENGINE_EVT_STOP:
        {
            el_state_machine_change_state(mac,EL_ENGINE_DECODE_STATE_START);
            EL_DBG_LOG("decoder:decode stoped!\n");
            return;
        }
        default:
        {
            EL_DBG_POS;
            return;
        }
    }
}

// NOTE: call this function after file opened
EL_STATIC void el_player_engine_decoder_state_machine(el_player_engine_state_machine_t *mac,el_player_engine_evt_e evt)
{
    switch(mac->state)
    {
        case EL_ENGINE_DECODE_STATE_WAIT:
        {
            el_do_process_decode_wait(mac,evt);
            return;
        }
        case EL_ENGINE_DECODE_STATE_WORK:
        {
            el_do_process_decode_work(mac,evt);
            return;
        }
        case EL_ENGINE_DECODE_STATE_SEEK_WAIT:
        {
            el_do_process_decode_seek_wait(mac,evt);
            return;
        }
        case EL_ENGINE_DECODE_STATE_START:
        {
            el_do_process_decode_start(mac,evt);
            return;
        }
        default:
        {
            EL_DBG_LOG("Invalid decode state!\n");
            return;
        }
    }
}




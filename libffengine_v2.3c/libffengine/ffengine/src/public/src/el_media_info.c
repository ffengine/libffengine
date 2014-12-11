
#include <stdarg.h>

#include "el_std.h"
#include "el_log.h"
#include "el_platform_sys_info.h"
#include "el_iOS_log.h"
#include "el_media_player.h"

EL_STATIC EL_BOOL el_get_media_description(const char *pszUrlOrPathname,
                                           ELMediaMuxerDesc_t * const aMediaMuxerDesc,
                                           ELVideoDesc_t * const aVideoDesc,
                                           ELAudioDesc_t * const aAudioDesc)
{
    int ret = 0;

    AVFormatContext *pFormatCtx = NULL;
    AVCodecContext	*pCodecCtx = NULL;

    if((ret = avformat_open_input (&pFormatCtx, (const char *)pszUrlOrPathname, NULL, NULL)) != 0)
    {
        EL_DBG_LOG("av_open_input_file,ret %d err\n",ret);

        ret = EL_FALSE;

        goto open_failed;
    }

    // Retrieve stream information
    if((ret = avformat_find_stream_info(pFormatCtx, NULL)) < 0)
    {
    	EL_DBG_LOG("Couldn't find stream information %d\n",ret);

		ret = EL_FALSE;

		goto failed;
    }

    if(!aMediaMuxerDesc || !aVideoDesc || !aAudioDesc)
	{
		EL_DBG_LOG("paramter err.\n");
		EL_DBG_FUNC_EXIT;
        
		ret = EL_FALSE;

		goto failed;
	}
    
    aMediaMuxerDesc->duration = pFormatCtx->duration;
    
    for(int i = 0; i < pFormatCtx->nb_streams; i++)
    {
        AVStream *avStream = pFormatCtx->streams[i];
        pCodecCtx = avStream->codec;

		if(avStream->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
            //pCodec =	avcodec_find_decoder(pCodecCtx->codec_id);
            // Is this codec id the right info we found?
            EL_DBG_LOG("video:stream codec id = %d\n",pCodecCtx->codec_id);
            
            aMediaMuxerDesc->eVideoCodecID = pCodecCtx->codec_id;
            
            aVideoDesc->eVideoCodecID = aMediaMuxerDesc->eVideoCodecID;
            aVideoDesc->nBitRate = pCodecCtx->bit_rate;
            
            aVideoDesc->nWidth = pCodecCtx->width;
            aVideoDesc->nHeight = pCodecCtx->height;
            
            aVideoDesc->nFrameRate = (EL_UINT)(av_q2d(avStream->avg_frame_rate));
            
            EL_DBG_LOG("video: bitrate = %d,width = %d,height = %d\n",
                       pCodecCtx->bit_rate,pCodecCtx->width,pCodecCtx->height);
    	}
        else if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            // Is this codec id the right info we found?
            EL_DBG_LOG("audio:stream codec id = %d\n",pCodecCtx->codec_id);

            aMediaMuxerDesc->eAudioCodecID = pCodecCtx->codec_id;

            aAudioDesc->eAudioCodecID = aMediaMuxerDesc->eAudioCodecID;
            aAudioDesc->eChannelCount = pCodecCtx->channels;
            aAudioDesc->euiFreq = pCodecCtx->sample_rate;
            EL_DBG_LOG("sample_rate = %d\n",pCodecCtx->sample_rate);
            aAudioDesc->uiBitCount = convert_audio_sample_format(pCodecCtx->sample_fmt);
            aAudioDesc->uiBitsPerSample = pCodecCtx->bits_per_raw_sample; //???
        }
	}
    
    goto success;
    

failed:
    
    // 2012.10.08
//    av_close_input_file(pFormatCtx);
    avformat_close_input(&pFormatCtx);

open_failed:
    
    return ret;

success:
    
    return EL_TRUE;
}



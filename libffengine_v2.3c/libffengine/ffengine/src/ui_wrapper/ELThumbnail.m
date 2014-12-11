//
//  ELThumbnail.c
//  ELMediaPlayerLib
//
//  Created by www.e-linkway.com on 12-6-25.
//  Copyright (c) 2012年 www.e-linkway.com. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavcodec/avcodec.h"

#include "ELThumbnail.h"

#define MAX_THUMB_DIMENSION 256         /* max dimension in pixels */
#define MAX_THUMB_BYTES (100*1024)

@implementation ELThumbnail

@synthesize videoPath = _videoPath;

-(id) initWithVideoPath: (NSString *) videoPath
{
    self = [super init];
    
    if ( !videoPath )
    {
        [self release];
        
        return nil;
    }

    self.videoPath = videoPath;
    
    return self;
}

-(void)dealloc
{
    self.videoPath = nil;

    [super dealloc];
}


-(NSData *) pngData
{
    int ret = [self thumbnail_video_read];
    
    if (0 != ret)
    {
        return nil;
    }

    if (!_imageData)
    {
        return nil;
    }

    // 在parseFrame中，为_imageData赋予了值
//    NSData *data = [NSData dataWithBytesNoCopy: _imageData length: _dataSize freeWhenDone: YES];

    NSData *data = [NSData dataWithBytes: _imageData length: _dataSize];
    
    av_free(_imageData);

//    [data writeToFile: [NSString stringWithFormat: @"/tmp/%d.png", _keyFrameIndex] atomically:NO];

    return data;
}


////////////////////////////////////////////////////////////////////////////////////////////////////

static void thumbnail_calculate_dimensions(int src_width, int src_height,
                                           int src_sar_num, int src_sar_den,
                                           int *dst_width, int *dst_height);

static size_t thumbnail_create(int src_width, int src_height, int src_stride[],
                               enum PixelFormat src_pixfmt, const uint8_t * const src_data[],
                               int dst_width, int dst_height,
                               uint8_t **output_data, size_t output_max_size);

////////////////////////////////////////////////////////////////////////////////////////////////////

//
/* open a given stream. Return 0 if OK */
-(int) thumbnail_video_stream_component_open
{
    _codecContext = _formatContext->streams[_videoStreamIndex]->codec;
    _videoCodec = avcodec_find_decoder(  _codecContext->codec_id );

    if (!_videoCodec)
    {
        return -1;
    }

    _codecContext->workaround_bugs   = FF_BUG_AUTODETECT;     // auto detection
    _codecContext->lowres            = 0;  // 缩放最小比例
    if(_codecContext->lowres > _videoCodec->max_lowres)       // 支持的最小缩放比例：　max_lowres
    {
        av_log(_codecContext, AV_LOG_WARNING, "The maximum value for lowres supported by the decoder is %d\n",
               _videoCodec->max_lowres);
        
        _codecContext->lowres= _videoCodec->max_lowres;
    }
    
    _codecContext->idct_algo         = FF_IDCT_AUTO;
    _codecContext->skip_frame        = AVDISCARD_DEFAULT;
    _codecContext->skip_idct         = AVDISCARD_DEFAULT;
    _codecContext->skip_loop_filter  = AVDISCARD_DEFAULT;
    _codecContext->error_concealment = FF_EC_DEBLOCK;
    
    if(_codecContext->lowres)
    {
        _codecContext->flags |= CODEC_FLAG_EMU_EDGE;
    }
    
    if(_videoCodec->capabilities & CODEC_CAP_DR1)
    {
        _codecContext->flags |= CODEC_FLAG_EMU_EDGE;
    }
    
    if (    !_videoCodec
        ||  avcodec_open2(_codecContext, _videoCodec, NULL) < 0)
    {
        return -1;
    }
    
    _formatContext->streams[_videoStreamIndex]->discard = AVDISCARD_DEFAULT;

    return 0;
}

-(int) parseFrame: (AVFrame *) pAVFrame
{
    int thumb_width = 0;
    int thumb_height = 0;

    thumbnail_calculate_dimensions (_codecContext->width, _codecContext->height,
                                    _codecContext->sample_aspect_ratio.num,
                                    _codecContext->sample_aspect_ratio.den,
                                    &thumb_width, &thumb_height);

    int size = thumbnail_create (_codecContext->width,
                                 _codecContext->height,
                                 pAVFrame->linesize,
                                 _codecContext->pix_fmt,
                                 (const uint8_t * const*) pAVFrame->data,
                                 thumb_width,
                                 thumb_height,
                                 &_imageData,
                                 MAX_THUMB_BYTES);
    
    if( size > 0 )
    {
        _dataSize = size;
        
        return 0;
    }
    else
    {
        return -1;
    }
}

-(int) thumbnail_video_read
{
    int ret = -1;

    AVPacket avPacket, *pAVPacket = &avPacket;
    
    _formatContext = NULL;
    
    int _offsetSeconds = 5;

    int err = avformat_open_input(&_formatContext, [_videoPath UTF8String], NULL, NULL);
    if (err < 0)
    {
        goto open_failed;
    }

    err = avformat_find_stream_info(_formatContext, NULL);
    if (err < 0)
    {
        goto find_stream_info_failed;
    }

    int nb_streams = _formatContext->nb_streams;

    if (_formatContext->pb)
    {
        _formatContext->pb->eof_reached = 0; // FIXME hack, ffplay maybe should not use url_feof() to test for the end
    }
    
    for (int i = 0; i < nb_streams; i++)
    {
        _formatContext->streams[i]->discard = AVDISCARD_ALL;
    }
    
    _videoStreamIndex = av_find_best_stream(_formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    
    if (_videoStreamIndex >= 0)
    {
        if ([self thumbnail_video_stream_component_open] < 0)
        {
            goto video_stream_component_open_failed;
        }
    }

    AVFrame *videoFrame = avcodec_alloc_frame();
    
    // TODO: Seek到某个时间
    // 查找这个时间附近的关键帧
    
    int iCurrentKeyFrameCount = 0;
    // 读数据，　直到读完一帧
    for (;;)
    {
        if ( 0 > av_read_frame( _formatContext, pAVPacket) )
        {
            // 读到文件末尾了
            if (    ret == AVERROR_EOF
                ||  url_feof( _formatContext->pb))
            {
                goto read_key_frame_failed;
            }

            // 出错了
            if (    _formatContext->pb
                &&  _formatContext->pb->error)
            {
                goto read_key_frame_failed;
            }

            continue;
        }

        int got_picture = 0;

        avcodec_decode_video2(_codecContext, videoFrame, &got_picture, pAVPacket);
        
        if (!got_picture)
        {
            av_free_packet(pAVPacket);

            continue;
        }

        if(  iCurrentKeyFrameCount < _offsetSeconds )
        {
            iCurrentKeyFrameCount++;
    
            continue;
        }

        // 上面都OK了，获取thumbnail
        // thumbnail放到了成员变量中，明天继续调试
        ret = [self parseFrame: videoFrame];

        break;
    }

read_key_frame_failed:

    avcodec_free_frame(&videoFrame);    // 这里会不会有内存泄露?

video_stream_component_open_failed:
    

    // 关闭流
    if (_videoStreamIndex >= 0)
    {
        _formatContext->streams[_videoStreamIndex]->discard = AVDISCARD_ALL;
        avcodec_close(_codecContext);
    }

find_stream_info_failed:
    avformat_free_context(_formatContext);
    _formatContext = NULL;
    
open_failed:

    return ret;
}


@end


#pragma mark -
/////////////////////////////////////////////////////////////////////////////////////////
/* calculate the thumbnail dimensions, taking pixel aspect into account */
static void thumbnail_calculate_dimensions(int src_width,
                                           int src_height,
                                           int src_sar_num,
                                           int src_sar_den,
                                           int *dst_width,
                                           int *dst_height)
{
    if (src_sar_num <= 0 || src_sar_den <= 0)
    {
        src_sar_num = 1;
        src_sar_den = 1;
    }
    if ((src_width * src_sar_num) / src_sar_den > src_height)
    {
        *dst_width = MAX_THUMB_DIMENSION;
        *dst_height = (*dst_width * src_height) /
        ((src_width * src_sar_num) / src_sar_den);
    }
    else
    {
        *dst_height = MAX_THUMB_DIMENSION;
        *dst_width = (*dst_height *
                      ((src_width * src_sar_num) / src_sar_den)) /
        src_height;
    }
    
    if (*dst_width < 8)
    {
        *dst_width = 8;
    }
    
    if (*dst_height < 1)
    {
        *dst_height = 1;
    }
}

/*
 * Rescale and encode a PNG thumbnail
 * on success, fills in output_data and returns the number of bytes used
 */
static size_t thumbnail_create(
                               int src_width,
                               int src_height,
                               int src_stride[],
                               enum PixelFormat src_pixfmt,
                               const uint8_t * const src_data[],
                               int dst_width,
                               int dst_height,
                               uint8_t **output_data,
                               size_t output_max_size)
{
    AVCodecContext *encoder_codec_ctx = NULL;
    AVCodec *encoder_codec = NULL;
    struct SwsContext *scaler_ctx = NULL;
    int sws_flags = SWS_BILINEAR;
    AVFrame *dst_frame = NULL;
    uint8_t *dst_buffer = NULL;
    uint8_t *encoder_output_buffer = NULL;
    size_t encoder_output_buffer_size;
    int err;

    // 2012.10.11 在SDK init的时候，执行av_register命令
    av_register_all();

    // 寻找png编码器
    encoder_codec = avcodec_find_encoder_by_name ("png");
    if (encoder_codec == NULL)
    {
        return 0;
    }
    
    /* NOTE: the scaler will be used even if the src and dst image dimensions
     * match, because the scaler will also perform colour space conversion */
    scaler_ctx = sws_getContext (src_width, src_height, src_pixfmt,
                                 dst_width, dst_height, PIX_FMT_RGB24,
                                 sws_flags, NULL, NULL, NULL);
    
    if (scaler_ctx == NULL)
    {
        return 0;
    }
    
    dst_frame = avcodec_alloc_frame ();
    if (dst_frame == NULL)
    {
        sws_freeContext(scaler_ctx);

        return 0;
    }

    dst_buffer = av_malloc (avpicture_get_size (PIX_FMT_RGB24, dst_width, dst_height));
    if (dst_buffer == NULL)
    {
        av_free (dst_frame);
        sws_freeContext(scaler_ctx);

        return 0;
    }
    
    avpicture_fill ((AVPicture *) dst_frame, dst_buffer,
                    PIX_FMT_RGB24, dst_width, dst_height);
    
    sws_scale (scaler_ctx,
               src_data, 
               src_stride,
               0, src_height, 
               dst_frame->data, 
               dst_frame->linesize);
    
    encoder_output_buffer_size = output_max_size;
    encoder_output_buffer = av_malloc (encoder_output_buffer_size);
    if (encoder_output_buffer == NULL)
    {
        av_free (dst_buffer);
        av_free (dst_frame);
        sws_freeContext(scaler_ctx);

        return 0;
    }
    
    encoder_codec_ctx = avcodec_alloc_context ();
    if (encoder_codec_ctx == NULL)
    {
        av_free (encoder_output_buffer);
        av_free (dst_buffer);
        av_free (dst_frame);
        sws_freeContext(scaler_ctx);

        return 0;
    }
    
    encoder_codec_ctx->width = dst_width;
    encoder_codec_ctx->height = dst_height;
    encoder_codec_ctx->pix_fmt = PIX_FMT_RGB24;

    if (avcodec_open (encoder_codec_ctx, encoder_codec) < 0)
    {
        av_free (encoder_codec_ctx);
        av_free (encoder_output_buffer);
        av_free (dst_buffer);
        av_free (dst_frame);
        sws_freeContext(scaler_ctx);

        return 0;
    }
    
    err = avcodec_encode_video (encoder_codec_ctx,
                                encoder_output_buffer,
                                encoder_output_buffer_size, dst_frame);
    
    avcodec_close (encoder_codec_ctx);
    av_free (encoder_codec_ctx);
    av_free (dst_buffer);
    av_free (dst_frame);
    sws_freeContext(scaler_ctx);
    
    *output_data = encoder_output_buffer;

    return err < 0 ? 0 : err;
}

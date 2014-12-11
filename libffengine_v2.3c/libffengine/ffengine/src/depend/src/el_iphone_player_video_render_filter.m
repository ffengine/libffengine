#include <pthread.h>
#import <CoreGraphics/CGContext.h>

#include "el_platform_video_render_filter.h"
#include "el_media_player_engine.h"
#include "el_std.h"

#define ENABLE_VIDEO 1

static EL_BYTE *g_pRGBABuffer = NULL;
static CGColorSpaceRef  g_cgColor = NULL;
static ELVideoRenderFilter_t videoRenderfilter = {0};
typedef struct 
{
	int width;
	int height;
}frame_rect_t;

static frame_rect_t frame_rect = {0,0};

EL_STATIC EL_VOID el_iphone_video_render_filter_init(EL_VOID) 
{
    frame_rect.width = frame_rect.height = 0;

	g_cgColor = CGColorSpaceCreateDeviceRGB();
}

EL_STATIC EL_VOID el_iphone_video_render_filter_uninit(EL_VOID) 
{
    EL_DBG_LOG("el_iphone_video_render_filter_uninit\n");

	if (g_pRGBABuffer) 
	{
		free(g_pRGBABuffer);
		g_pRGBABuffer = NULL;
	}
	
	if (g_cgColor) 
	{
		CGColorSpaceRelease(g_cgColor);
		g_cgColor = NULL;
	}

    frame_rect.width = frame_rect.height = 0;
}


#define bytes_per_pixel     2
EL_STATIC void el_rgb565_convert(uint8_t *dst_ptr, const uint8_t *y_ptr, int32_t pic_width, int32_t pic_height);

EL_STATIC EL_VOID el_iphone_video_render_filter_display_frame(EL_BYTE* pDisplayFrame, EL_SIZE a_nFramelen,
                                                          EL_SIZE a_nFrameWidth, EL_SIZE a_nFrameHeight)
    {
#if ENABLE_VIDEO
        if(a_nFrameWidth != frame_rect.width || a_nFrameHeight != frame_rect.height)
	{
		frame_rect.width = a_nFrameWidth;
		frame_rect.height = a_nFrameHeight;
	        if(g_pRGBABuffer)
		{
                free(g_pRGBABuffer);
		} 
        	g_pRGBABuffer = (EL_BYTE *)malloc(frame_rect.height * frame_rect.width * bytes_per_pixel);
		if(!g_pRGBABuffer)
		{
			EL_DBG_LOG("err:alloc rgba buffer err!\n");
			return;
		}
	}


	el_rgb565_convert(g_pRGBABuffer,
                                   pDisplayFrame,
                                   a_nFrameWidth,
                                   a_nFrameHeight
                                   );

	el_player_send_message_to_ui(EL_PLAYER_MESSAGE_DISPLAY_FRAME,g_pRGBABuffer,NULL);
#endif
    }

EL_STATIC ELVideoRenderFilter_t* el_player_get_video_render_filter(EL_VOID) 
{
    ELVideoRenderFilter_t *pFilter    = &videoRenderfilter;
    pFilter->filter_init                = el_iphone_video_render_filter_init;
    pFilter->filter_uninit              = el_iphone_video_render_filter_uninit;
    pFilter->filter_display_frame       = el_iphone_video_render_filter_display_frame;
    
    return pFilter;
}


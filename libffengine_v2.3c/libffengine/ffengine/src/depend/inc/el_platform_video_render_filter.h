
#ifndef _EL_PLATFORM_VIDEO_RENDER_FILTER_H_
#define _EL_PLATFORM_VIDEO_RENDER_FILTER_H_

#include "el_std.h"

typedef void (*el_video_render_filter_init_func)();
typedef void (*el_video_render_filter_uninit_func)();
typedef void (*el_video_render_filter_display_frame_func)(EL_BYTE* pDisplayFrame, EL_SIZE a_nFramelen, EL_SIZE a_nFrameWidth, EL_SIZE a_nFrameHeight);
    
typedef struct tagELVideoRenderFilter 
{
    el_video_render_filter_init_func                 filter_init;
    el_video_render_filter_uninit_func               filter_uninit;
    el_video_render_filter_display_frame_func        filter_display_frame;
} ELVideoRenderFilter_t;

EL_STATIC ELVideoRenderFilter_t* el_player_get_video_render_filter(EL_VOID);


#endif

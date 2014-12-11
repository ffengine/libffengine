//
//  EL
//
//  Created by apple on 11-9-2.
//  Copyright 2011年 EL. All rights reserved.
//

#import <OpenGLES/ES1/gl.h>
#import <OpenGLES/ES1/glext.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/EAGLDrawable.h>

#import "commonDef.h"
#import "ELMediaPlayer.h"
#import "ELVideoEAGLView.h"
#import "ELMessageObject.h"

#import "el_std.h"
#import "el_media_player.h"
#import "el_platform_video_render_filter.h"
#import "el_platform_audio_render_filter.h"
#import "el_log.h"

#import "ELMediaUtil.h"

ELMediaPlayer *g_defaultMediaPlayer;

static bool g_sdkInitOk = NO;
static bool g_playerStoped = NO;

void el_player_send_message_to_ui(int nMessage, void* wParam, void* lParam)
{
    ELMessageObject *obj = [[ELMessageObject alloc] initWithMessage:(int)nMessage arg1:(int)wParam arg2:(int)lParam];
    
    [g_defaultMediaPlayer performSelectorOnMainThread: @selector(player_get_message_send_to_ui:) withObject:obj waitUntilDone:NO];
    
    [obj release];
}

//----------------------------------------------------------------------------------------------------

@interface ELMediaPlayer()

- (BOOL)resetVideoFrame;

@end


@implementation ELMediaPlayer

@synthesize delegate = _delegate;

@synthesize mediaPath = _mediaPath;
@synthesize containerView = _containerView;
@synthesize videoView = _videoView;
@synthesize seekToTime = _seekToTime;
@synthesize screenType = _screenType;
@synthesize isFullscreen = _isFullscreen;
@synthesize openning = _openning;
@synthesize stopped = _stopped;
@synthesize paused = _paused;
@synthesize initOk = _initOk;
@synthesize autoPlay = _autoPlay;
@synthesize buffering = _buffering;

@synthesize shouldUpdateVideoPicture = _shouldUpdateVideoPicture;

//----------------------------------------------------------------------------------------------------
+(ELMediaPlayer *) sharedMediaPlayer
{
    if (!g_defaultMediaPlayer) 
    {
        /* !! 必须先调用 el_media_player_init, 再调用 el_media_player_set_dynamic_reference_table
         * 否则会出现内部线程出错，在App层很难调试!
         */
        
        g_sdkInitOk = el_media_player_init();
        if (!g_sdkInitOk) 
        {
            EL_DBG_LOG("** SDK init failed!!");
        }
        
        // 设置底层回调方法
        {
            el_dynamic_reference_table_t table = {
                el_player_send_message_to_ui,
            };
            
            el_media_player_set_dynamic_reference_table(table);
        }
        
        g_playerStoped = YES;
        g_defaultMediaPlayer = [[ELMediaPlayer alloc] init];
    }
    
    return g_defaultMediaPlayer;
}

+(void) destroySharedMediaPlayer
{
    if (g_sdkInitOk) 
    {
        el_media_player_uninit();
        
        [g_defaultMediaPlayer dealloc];
        
        // 2012.10.08, fix bug: 调用本方法crash
        g_defaultMediaPlayer = nil;
    }
}

+ (UIImage *)image:(UIImage *)image fillSize:(CGSize)viewsize
{	
	UIGraphicsBeginImageContext(viewsize);
	
	CGRect rect = CGRectMake(0, 0, viewsize.width, viewsize.height);
	[image drawInRect:rect];
	
    UIImage *newimg = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();  
	
    return newimg;  
}

//----------------------------------------------------------------------------------------------------

- (id)init 
{
    self = [super init];
    if (self) 
    {
        self.seekToTime = 0;
        self.videoView = [[ELVideoEAGLView alloc] initWithFrame: DEFAULT_VIEW_FRM];
        
        self.screenType = ELScreenType_ASPECT_FULL_SCR;
        
        _shouldUpdateVideoPicture = YES;
    }
    
    return self;
}

- (void)dealloc 
{
    self.mediaPath = nil;
    
    [self.videoView removeFromSuperview];
    self.videoView = nil;
    self.containerView = nil;
    
    [super dealloc];
}

- (void) player_get_message_send_to_ui:(id)obj
{
    int nMessage = ((ELMessageObject *)obj).message;
    int wParam = ((ELMessageObject *)obj).arg1;
    int lParam = ((ELMessageObject *)obj).arg2;

    switch (nMessage)
    {
        case EL_PLAYER_MESSAGE_OPEN_SUCCESS:
        {
            EL_DBG_LOG("msg ui: EL_PLAYER_MESSAGE_OPEN_SUCCESS\n");
            
            g_playerStoped = NO;

            if ([g_defaultMediaPlayer respondsToSelector:@selector(openOk)])
            {
                [g_defaultMediaPlayer openOk];
            }
            
            if ([g_defaultMediaPlayer respondsToSelector: @selector(mediaWidth:height:)])
            {
                [g_defaultMediaPlayer mediaWidth: wParam height: lParam];
            }

            break;
        }
        case EL_PLAYER_MESSAGE_OPEN_FAILED: 
        {
            if ([g_defaultMediaPlayer respondsToSelector:@selector(openFailed)])
            {
                [g_defaultMediaPlayer openFailed];
            }
                
            break;
        }
        case EL_PLAYER_MESSAGE_BUFFERING_PERCENT: {
            EL_DBG_LOG("msg ui: EL_PLAYER_MESSAGE_BUFFERING_PERCENT\n");
            
            size_t percent = (size_t)wParam;
            if ([g_defaultMediaPlayer respondsToSelector:@selector(bufferPercent:)])
            {
                [g_defaultMediaPlayer bufferPercent:percent];
            }
                    
            break;
        }
        case EL_PLAYER_MESSAGE_READY_TO_PLAY:
        {
            EL_DBG_LOG("msg ui: EL_PLAYER_UI_MESSAGE_READY_TO_PLAY\n");
            
            if ([g_defaultMediaPlayer respondsToSelector:@selector(readyToPlay)])
            {
                [g_defaultMediaPlayer readyToPlay];
            }
            
            break;
        }
            
        case EL_PLAYER_MESSAGE_END_OF_FILE: 
        {
            EL_DBG_LOG("msg ui: EL_PLAYER_MESSAGE_END_OF_FILE\n");
            
            if ( g_playerStoped == YES )  
            {
                return;
            }
            
            if ([g_defaultMediaPlayer respondsToSelector:@selector(playToEnd)])
            {
                [g_defaultMediaPlayer playToEnd];
                
                g_playerStoped = YES;
            }
            
            break;
        }
        case EL_PLAYER_MESSAGE_MEDIA_CURRENT_POS: 
        {
            EL_DBG_LOG("msg ui: EL_PLAYER_MESSAGE_MEDIA_CURRENT_POS\n");

            size_t p = (size_t)wParam;
            
            if ([g_defaultMediaPlayer respondsToSelector:@selector(mediaPosition:)])
                [g_defaultMediaPlayer mediaPosition:p];
            
            break;
        }
        case EL_PLAYER_MESSAGE_DISPLAY_FRAME:
        {
            EL_DBG_LOG("msg ui: EL_PLAYER_MESSAGE_DISPLAY_FRAME\n");
            
            // 2012.10.08
            if (g_defaultMediaPlayer.initOk && _shouldUpdateVideoPicture)
            {
                [self.videoView writeSampleBuffer: (void *)wParam];   
            }

            break;
        }
        case EL_PLAYER_MESSAGE_BUFFERING_START:
        {
            EL_DBG_LOG("msg ui: EL_PLAYER_UI_MESSAGE_BUFFERING_START\n");
            if ([g_defaultMediaPlayer respondsToSelector:@selector(bufferingStart)])
                [g_defaultMediaPlayer bufferingStart];
            
            break;
        }
        case EL_PLAYER_MESSAGE_NOTIFICATION:
        {
            break;
        }
        case EL_PLAYER_MESSAGE_PAUSE_RESULT:   //pause True or False
        {
            EL_DBG_LOG("msg ui: EL_PLAYER_MESSAGE_PAUSE_RESULT\n");
            break;  
        }
        default:
        {
            break;
        }
    }
}

//--------------------------------------------------------------------------------------------------
#pragma mark -
#pragma mark IELMediaPlayer protocol
-(void) setDelegate:(id<ELPlayerMessageProtocol>)delegate
{
    _delegate = delegate; 
}

- (void)setAutoPlayAfterOpen:(BOOL) autoPlay
{
    self.autoPlay = autoPlay;
}

- (BOOL)openMedia:(NSString *)path
{
//    [ELMediaUtil getMediaDescription: path];
    
    return [self openMedia:path seekTo: 0];
}

- (BOOL)openMedia:(NSString *)path seekTo:(size_t)time
{
    if( self.initOk )
    {
        return NO;
    }
    
    if (    !path 
        ||  0 == [path length] )
    {
        return NO;
    }
    
    if (    !path 
        ||  [path length] == 0 ) 
    {
        return NO;
    }
    
    const unsigned char* file = (const unsigned char *)[path UTF8String];

    BOOL bRet = el_media_player_open(file);
    if( bRet )
    {
        self.mediaPath = [NSString stringWithString:path];
        
        self.openning = YES;
        self.stopped = NO;
        self.seekToTime = time;
    }
    
    return bRet;
}

//wj
- (BOOL)closeMedia 
{
    BOOL bRet = el_media_player_close();
    if( bRet )
    {
        self.stopped = YES;
        self.paused = NO;
        self.initOk = NO;
    }   
    
    return bRet;
}


- (void)startPlay 
{
    [self resetVideoFrame];

    if ( 0 == self.seekToTime ) 
    {
        [self startToPlay];
    }
    else
    {
        [self seekTo:self.seekToTime];
        self.seekToTime = 0;
    }
}

#warning stopPlayer 和 closePlayer 操作的区别需要修改
- (void)stopPlay
{
   EL_DBG_LOG("stopPlay .... \n");

    if( g_playerStoped )
    {
        return ;
    }

    if(  el_media_player_stop()  )
    {
        self.stopped = YES;
        self.paused = NO;
        
        [self.videoView stopAnimation];
        self.videoView.hidden = YES;
        
        [self.videoView reset]; 
        
        [self closeMedia];   
    }
}

- (void)pausePlay
{
    self.openning = NO;
    self.paused = YES;
    
    [self.videoView stopAnimation];
    
    if (!self.openning) 
    {
        el_media_player_pause();
    }    
}

- (void)resumePlay 
{
    el_media_player_play();
    self.paused = NO;
    
//    if (    surfaceSize.width > 0 
//        &&  surfaceSize.height > 0 ) 
//    {
//        [videoView startAnimation];
//    }
}

- (size_t)seekTo:(size_t)pos 
{
    EL_DBG_LOG("seekTo:%lu .... \n", pos);
    
    if (!self.initOk) 
    {
        return 0;
    }
    
    return el_media_player_seek_to(pos);
}

- (void)setVideoContainerView:(UIView *)containerView
{
    self.containerView = containerView;
    if( self.initOk )
    {
        [self resetVideoFrame];
    }
}

- (void)setPlayerScreenType:(ELScreenType_e ) screenType
{
    self.screenType = screenType;
    if( self.initOk )
    {
        [self resetVideoFrame];
    }
}

- (void)refreshViewFrame
{
    if( self.initOk )
    {
        [self resetVideoFrame];
    }    
}

- (void)startToPlay 
{    
    el_media_player_start();
    
    self.stopped = NO;
    self.paused = NO;
}

//- (void)startVideo 
//{
//    [videoView startAnimation];
//}
//
//- (void)stopVideo 
//{
//    [videoView stopAnimation];
//}

- (void)mediaDuration:(size_t)duration 
{
    @synchronized(self)
    {
        if( [self.delegate respondsToSelector:@selector(mediaDuration:)] )
        {
            [self.delegate mediaDuration:duration];
        }
    }
}

//--------------------------------------------------------------------------------------------------
#pragma mark -
#pragma mark ELPlayerMessageDelegate

- (void)openOk 
{
    self.initOk = YES;
    self.openning = NO;
    
    @synchronized(self) 
    {
        if( [self.delegate respondsToSelector:@selector(openOk)] )
        {
            [self.delegate openOk];
        }       
    }

    if( self.autoPlay )
    {
        [self startPlay];    
    }
}

- (void)openFailed 
{
    self.initOk = NO;
    self.openning = NO;

    @synchronized(self) 
    {
        if( [self.delegate respondsToSelector:@selector(openFailed)] )
        {
            [self.delegate openFailed];
        }
    }
    
    [self closeMedia];
}

- (void)bufferPercent:(int)percentage 
{
    self.buffering = YES;
    
    @synchronized(self) 
    {
        if( [self.delegate respondsToSelector:@selector(bufferPercent:)] )
        {
            [self.delegate bufferPercent:percentage];
        }
    }
}

- (void)readyToPlay 
{
    self.buffering = NO;
    
    if ( !self.paused )  
    {
        el_media_player_play();         // start play 和 play是两码事
    }
    
    @synchronized(self)
    {
        if( [self.delegate respondsToSelector:@selector(readyToPlay)] )
        {
            [self.delegate readyToPlay];
        }
    }
}

- (void)bufferingStart 
{
    @synchronized(self)
    {
        if( [self.delegate respondsToSelector:@selector(bufferingStart)] )
        {
            [self.delegate bufferingStart];
        }
    }
}

- (void)playToEnd 
{
   EL_DBG_LOG("playToEnd....\n");
    
    @synchronized(self)
    {
        if( [self.delegate respondsToSelector:@selector(playToEnd)] )
        {
            [self.delegate playToEnd];
        }
    }
}

- (void)mediaPosition:(size_t)position 
{
    @synchronized(self)
    {
        if( [self.delegate respondsToSelector:@selector(mediaPosition:)] )
        {
            [self.delegate mediaPosition:position];
        }
    }
}

// 
- (void)mediaWidth: (size_t) width height: (size_t) height
{
    @synchronized(self)
    {
        if( [self.delegate respondsToSelector:@selector(mediaWidth:height:)] )
        {
            [self.delegate mediaWidth: width height: height];
        }
    }
}

//--------------------------------------------------------------------------------------------------
#pragma mark -
#pragma 工具方法

- (BOOL)resetVideoFrame
{
    ELMediaMuxerDesc_t desc_t;
    ELVideoDesc_t vdesc_t;
    ELAudioDesc_t adesc_t;
    
    memset(&desc_t,     0, sizeof(ELMediaMuxerDesc_t));
    memset(&vdesc_t,    0, sizeof(ELVideoDesc_t));
    memset(&adesc_t,    0, sizeof(ELAudioDesc_t));
    
    EL_BOOL getSuccess = el_media_player_get_description(&desc_t, &vdesc_t, &adesc_t);
    if (!getSuccess) 
    {
        return NO;
    }
    
    if ( !  (   vdesc_t.nWidth == 0 
             || vdesc_t.nHeight == 0 )) 
    {   
        [self.videoView setSurfaceSize:CGSizeMake(vdesc_t.nWidth, vdesc_t.nHeight)];
    
        if( [self.videoView superview] != self.containerView  )
        {
            [self.videoView removeFromSuperview];
            [self.containerView addSubview: self.videoView];   
        }
    }
    
    if ([self respondsToSelector:@selector(mediaDuration:)])
    {
        [self mediaDuration:desc_t.duration];
    }
    
    int x = 0, y = 0;
    
    // 视频宽高
	int w = (int)vdesc_t.nWidth;
	int h = (int)vdesc_t.nHeight;
    
    // 视频view宽高比
	float aspect = (float)w / (float)h;
    
    // 存放视频view的view
	CGSize size = self.containerView.bounds.size;
    
    // 求宽高比
    float aspect0 = size.width / size.height;
    
	if (_screenType == ELScreenType_ASPECT_FULL_SCR) 
    {
        // 放大或这缩小匹配存放视频view的view
		if (aspect > aspect0)
        {
			w = size.width;
			h = w / aspect;
			y = (size.height - h) / 2;
		}
		else 
        {
			h = size.height;
			w = h * aspect;
			x = (size.width - w) / 2;
		}
	}
	else if (_screenType == ELScreenType_FULL_SCR)
    {
		w = size.width;
		h = size.height;
	}
	else  
    {
		if (    w < size.width 
            &&  h < size.height) 
        {
			x = (size.width - w) / 2;
			y = (size.height - h) / 2;
		} 
        else 
        { // copy from case ASPECT_FULL_SCR
			if (aspect > aspect0)
            {
				w = size.width;
				h = w / aspect;
				y = (size.height - h) / 2;
			}
			else 
            {
				h = size.height;
				w = h * aspect;
				x = (size.width - w) / 2;
			}
		}
	}
    
    CGRect frame = CGRectMake(x, y, w, h);
    [self.videoView updateVideoFrame:frame];
    
    return YES;
}

// 后台播放电影，不能显示画面，仅仅播放声音
-(void)setShouldUpdateVideoPicture:(BOOL)shouldUpdateVideoPicture
{
    _shouldUpdateVideoPicture = shouldUpdateVideoPicture;
    
    if (_shouldUpdateVideoPicture)
    {
        [self.videoView startAnimation];
    }
    else
    {
        [self.videoView stopAnimation];
    }
}


@end

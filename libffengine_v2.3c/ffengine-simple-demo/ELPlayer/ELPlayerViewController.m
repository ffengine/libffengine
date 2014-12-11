//
//  ELPlayerViewController.m
//  ELPlayer
//
//  Created by Steven Jobs on 12-2-25.
//  Copyright (c) 2012年 __MyCompanyName__. All rights reserved.
//

#import <AudioToolbox/AudioToolbox.h>

#import "ELPlayerViewController.h"
#import "FFEngine/FFEngine.h"

@implementation ELPlayerViewController

//! 建立横竖屏不同的控制view，
@synthesize protraitNavView = _protraitNavView;
@synthesize protraitBottomView = _protraitBottomView;

//! 正在播放的视频名称
@synthesize titleName = _titleName;
@synthesize titleForP = _titleForP;

@synthesize videoUrl = _videoUrl;

//! 横竖屏有不同的xib元素
@synthesize pEstablishedTimeLabel = _pEstablishedTimeLabel;
@synthesize pLeftTimeLabel = _pLeftTimeLabel;
@synthesize progressPSlider = _progressPSlider;

@synthesize playerView = _playerView;

- (void)dealloc
{
    self.titleName = nil;
    self.titleForP = nil;
    
    self.videoUrl = nil;
    
    self.pEstablishedTimeLabel = nil;
    self.pLeftTimeLabel = nil;
    self.progressPSlider = nil;
    
    self.playerView = nil;
    
    [super dealloc];
}

- (void) viewDidLoad
{
    [super viewDidLoad];
    
    self.navigationController.navigationBar.hidden = YES;
    
    self.titleForP.text = self.titleName;

    // for thumbnail
//    NSData *data = [ELMediaUtil thumbnailPNGDataWithVideoPath: self.videoUrl];
//    [data writeToFile: @"/tmp/1.png" atomically: NO];

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(appBecomeActive)
                                                 name:UIApplicationDidBecomeActiveNotification object:nil];
    
    
	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(appResignActive)
                                                 name:UIApplicationWillResignActiveNotification object:nil];
}

-(void)initSDK
{
    id<IELMediaPlayer> player = loadELMediaPlayer();
    
    [player setDelegate:self];
    [player setAutoPlayAfterOpen:YES];
    [player setVideoContainerView:self.playerView];
    [player setPlayerScreenType:ELScreenType_ASPECT_FULL_SCR];
}

-(void)viewWillAppear:(BOOL)animated
{
    [self initSDK];
}

-(void)viewWillDisappear:(BOOL)animated
{
    /*
    id<IELMediaPlayer> player = loadELMediaPlayer();

    [player setDelegate:nil];

    releaseELMediaPlayer();
     */
}

- (void)openOk
{
    
}

- (void)openFailed
{
    UIAlertView *alertView = [[[UIAlertView alloc] initWithTitle: nil 
                                                         message: @"打开文件失败" 
                                                        delegate: nil
                                               cancelButtonTitle: @"关闭"
                                               otherButtonTitles: nil] autorelease];
    [alertView show];
}

// 本地缓冲很快,本地不送,网络送
- (void)bufferPercent:(int)percentage
{
    NSLog(@"%s %d, percentage = %d", __FUNCTION__, __LINE__, percentage);
    
    self.pEstablishedTimeLabel.text = [NSString stringWithFormat: @"%d%%", percentage];
}

- (void)readyToPlay
{
    
}

- (void)playToEnd
{
    id<IELMediaPlayer> player = loadELMediaPlayer();
    
    [player stopPlay];
}

// 总长度
- (void)mediaDuration:(size_t)duration
{
    _videoDuration = duration;
}

// player -> UI
// 当前进度
- (void)mediaPosition:(size_t)position
{
    _videoPostion = position;

    self.pEstablishedTimeLabel.text = [NSString stringWithFormat: @"%lu", position / 1000];
    self.pLeftTimeLabel.text = [NSString stringWithFormat: @"-%lu", (_videoDuration - _videoPostion) / 1000];
    
    self.progressPSlider.value = 1.0 * _videoPostion / _videoDuration;
}

// player -> UI
- (void)mediaWidth: (size_t) width height: (size_t) height
{
    NSLog(@"%s %d, width: %ld, height: %ld", __FUNCTION__, __LINE__, width, height);
}

#define kDuration 0.6

#pragma mark - 播放控制，各种播放method
//!----------------------------------------------------------------------------------
- (IBAction)onButtonBackPressed:(id)sender
{
    [self stopPlayVideo: nil];

    [[NSNotificationCenter defaultCenter] removeObserver: self];

    self.navigationController.navigationBar.hidden = NO;
    [self.navigationController popViewControllerAnimated: YES];
}

#
//! 播放视频
-(IBAction) startPlayVideo:(id)sender
{
    id<IELMediaPlayer> player = loadELMediaPlayer();
    
    if( [player openMedia: self.videoUrl] )
    {
        self.pEstablishedTimeLabel.text = @"Opening";    
    }
    
    [UIApplication sharedApplication].idleTimerDisabled = YES;
    
}

//! 暂停播放
-(IBAction) pausePlayVideo:(id)sender
{
    static int i = 0;
    id<IELMediaPlayer> player = loadELMediaPlayer();
    
    if(i)
    {
        // resume
        i = 0;
        [player resumePlay];  
    }
    else
    {
        // Pause
        i = 1;
        [player pausePlay];
        
        self.pEstablishedTimeLabel.text = @"Paused";
    }
}

//! 停止播放
-(IBAction) stopPlayVideo:(id)sender
{   
    id<IELMediaPlayer> player = loadELMediaPlayer();
    
    [player stopPlay];
    
    [UIApplication sharedApplication].idleTimerDisabled = NO;

}

//! 到退播放
-(IBAction) rewindPlayVideo:(id)sender
{
    id<IELMediaPlayer> player = loadELMediaPlayer();

    [player seekTo: _videoPostion - 10 * 1000];
}

//! 快进播放
-(IBAction) forwardPlayVideo:(id)sender
{
    id<IELMediaPlayer> player = loadELMediaPlayer();

    [player seekTo: _videoPostion + 10 * 1000];
}

//! 快照
-(IBAction) takePicture:(id)sender
{
    self.pEstablishedTimeLabel.text = @"Nop";
}

// 手动拉动播放
-(IBAction) sliderValueChanged:(id)sender
{
    UISlider *slider = (UISlider *) sender;
    
    size_t currentPosition = (slider.value * _videoDuration);
    
    id<IELMediaPlayer> player = loadELMediaPlayer();
    
    [player seekTo: currentPosition];
}


//!----------------------------------------------------------------------------------

- (void)appBecomeActive
{
    [self onButtonBackPressed: nil];
}

-(void)appResignActive
{
    id<IELMediaPlayer> player = loadELMediaPlayer();
    
    [player stopPlay];
}

@end

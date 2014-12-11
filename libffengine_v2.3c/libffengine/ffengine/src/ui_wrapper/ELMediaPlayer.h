//
//  To play a media, follow these step:
//  1. register media player message delegates
//  2. if player openMedia == TRUE
//        player showVideoOn:a_container_view
//  3. else
//        show error message
//
//  Created by apple on 11-9-2.
//  Copyright 2011年 EL. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "IELMediaPlayer.h"

@class ELVideoEAGLView;
@protocol ELPlayerMessageProtocol;


@interface ELMediaPlayer : NSObject <IELMediaPlayer>

// 只保留一个delegate
@property (nonatomic, assign) id<ELPlayerMessageProtocol> delegate;

@property (nonatomic, retain) NSString *mediaPath;

@property (nonatomic, retain) UIView *containerView;
@property (nonatomic, retain) ELVideoEAGLView *videoView;

@property (nonatomic, assign) size_t seekToTime;
@property (nonatomic, assign) ELScreenType_e screenType;

@property (nonatomic, assign) BOOL isFullscreen;
@property (nonatomic, assign) BOOL openning;
@property (nonatomic, assign) BOOL stopped;
@property (nonatomic, assign) BOOL buffering;
@property (nonatomic, assign) BOOL paused;
@property (nonatomic, assign) BOOL initOk;

@property (nonatomic, assign) BOOL autoPlay;

@property (nonatomic, assign) BOOL shouldUpdateVideoPicture;

+(ELMediaPlayer *) sharedMediaPlayer;
+(void) destroySharedMediaPlayer;


@end

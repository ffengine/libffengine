//
//  ELPlayerMessageDelegate.h
//  HFC
//
//  Created by apple HFC on 11-9-7.
//  Copyright 2011年 HFC. All rights reserved.
//


#ifndef HFC_ELPlayerMessageDelegate_h
#define HFC_ELPlayerMessageDelegate_h


@protocol ELPlayerMessageProtocol <NSObject>

@optional

- (void)openOk;
- (void)openFailed;
- (void)netFailed;
- (void)bufferPercent:(int)percentage;
- (void)readyToPlay;
- (void)playToEnd;
- (void)mediaDuration:(size_t)duration;
- (void)mediaPosition:(size_t)position;
- (void)mediaWidth: (size_t) width height: (size_t) height;
- (void)playStarted;
- (void)playPaused;
- (void)playResumed;
- (void)seekAck;
- (void)playStop;
- (void)playNextMedia;
- (void)bufferingStart;
- (BOOL)liveMedia;
- (BOOL)playingMusic;
- (void)displaySubtitle:(NSString *)subtitle;

#ifdef APPSTORE_AD_VERSION_CN
- (BOOL)isShowAdTypeFront;
#endif
@end

#endif

//
//  ELMediaPlayerSDK.c
//  ELMediaLib_New
//
//  Created by Wei Xie on 12-6-25.
//  Copyright (c) 2012年 __MyCompanyName__. All rights reserved.
//

#import "IELMediaPlayer.h"
#import "ELMediaPlayerSDK.h"
#import "ELShareContainer.h"
#import "ELMediaPlayer.h"

#import <Foundation/Foundation.h>

//#define TARGET_BUNDLE_ID @"net.fanship.iSafePod", @"com.e-linkway.ELPlayer", nil      // 20120913
//#define TARGET_BUNDLE_ID @"com.panoiu.hitbrowser", @"com.e-linkway.ELPlayer", nil       // 20120913
//#define TARGET_BUNDLE_ID @"net.fanship.iSafePod", @"net.fanship.iSafePad", @"net.fanship.iSafeLite", @"net.fanship.iSafePlay", @"com.e-linkway.ELPlayer", nil       // 20120913
//#define TARGET_BUNDLE_ID @"com.abcdefg.mymediaplayer", @"com.e-linkway.ELPlayer", nil      // 20120927
//#define TARGET_BUNDLE_ID @"com.xenon.ExPlus", @"com.e-linkway.ffengine-simple-demo", nil      // 20120927

#define TARGET_BUNDLE_ID @"net.fanship.iSafePod", @"net.fanship.iSafePad", @"net.fanship.iSafeLite", @"net.fanship.iSafePlay", @"com.e-linkway.ELPlayer", @"com.e-linkway.rtmp-player", nil       // 20120913

NSString *getBundleID()
{
    return [[NSBundle mainBundle] bundleIdentifier];
}

// 按照BundleID进行检查
BOOL checkBundleID()
{
    NSArray *bundleArray = [NSArray arrayWithObjects: TARGET_BUNDLE_ID];
    
//    NSString *currentBundleID = getBundleID();
//    NSLog(@"currentBundleID = %@", currentBundleID);
    
    BOOL bundleOK = [bundleArray containsObject: getBundleID()];
    
    return bundleOK;
}

// 按照过期日期进行检查
BOOL checkValidateDate()
{
    // 通过日期来设置过期日期
    static NSString *validDateString = @"2013-12-30";

    NSDateFormatter *dateFromatter = [[[NSDateFormatter alloc] init] autorelease];
    [dateFromatter setDateFormat:@"yyyy-MM-dd"];
    [dateFromatter setTimeZone:[NSTimeZone timeZoneWithName:@"UTC"]];

    NSDate *validDate = [dateFromatter dateFromString:validDateString];
    NSDate *date = [NSDate date];

    if( [date compare:validDate] == NSOrderedAscending )
    {
        return YES;
    }

    return NO;
}

int registerLib(char * registerCode)
{
    BOOL checkResult = NO;

    // 先检查Bundle是否合法
    checkResult = checkBundleID();

    // 如果Bundle不合法，则检查过期日期，把过期日期设置的大一些，争取用户；哪怕是盗版用户
    if(checkResult)
    {
        NSLog(@"FFEngine OK: valid BundleID, thank you for use!");
    }
    else
    {
        NSLog(@"FFEngine Error: illegal BundleID, please contact xiewei.max@gmail.com !");
        
        checkResult = checkValidateDate();
        
        if (!checkResult)
        {
            NSLog(@"FFEngine Error: Trial expired!");
        }
        else
        {
            NSLog(@"In trial Mode!");
        }
    }

    [ELShareContainer defaultShareConatiner].isRegisterOK = checkResult;

    // register for all ffmpeg formats
    avcodec_register_all();
	av_register_all();
    avformat_network_init();
    
    return 0;
}


id<IELMediaPlayer> loadELMediaPlayer()
{
    if( ![ELShareContainer defaultShareConatiner].isRegisterOK )
    {
        return nil;
    }
    
    return [ELMediaPlayer sharedMediaPlayer];
}

void releaseELMediaPlayer()
{
    if( ![ELShareContainer defaultShareConatiner].isRegisterOK )
    {
        return ;
    }
    
    [ELMediaPlayer destroySharedMediaPlayer];
}


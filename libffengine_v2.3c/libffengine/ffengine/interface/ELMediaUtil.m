//
//  ELMediaUtil.m
//  ELMediaLib
//
//  Created by Wei Xie on 12-7-11.
//  Copyright (c) 2012年 __MyCompanyName__. All rights reserved.
//

#import "el_media_info.h"

#import "ELMediaUtil.h"
#import "ELThumbnail.h"


@implementation ELMediaUtil

+(NSDictionary *) getMediaDescription: (NSString *) mediaPath
{
    ELMediaMuxerDesc_t aMediaMuxerDesc = {0};
    ELVideoDesc_t aVideoDesc = {0};
    ELAudioDesc_t aAudioDesc = {0};

    el_get_media_description([mediaPath UTF8String], &aMediaMuxerDesc, &aVideoDesc, &aAudioDesc);

    AVCodec *videoCodec = avcodec_find_decoder(aVideoDesc.eVideoCodecID);
    AVCodec *audioCodec = avcodec_find_decoder(aAudioDesc.eAudioCodecID);
    
    if(NULL == videoCodec || NULL == audioCodec)
    {
        return nil;
    }

    //
    NSMutableDictionary *mediaDictionary = [NSMutableDictionary dictionaryWithCapacity: 10];

    [mediaDictionary setObject: mediaPath forKey: @"mediaPath"];
    [mediaDictionary setObject: [NSNumber numberWithDouble: aMediaMuxerDesc.duration / 1000.0 / 1000.0] forKey: @"mediaDuration"];

    [mediaDictionary setObject: [NSNumber numberWithInt: aVideoDesc.eVideoCodecID] forKey: @"videoCodecID"];
    [mediaDictionary setObject: [NSString stringWithUTF8String: videoCodec->name] forKey: @"videoCodecName"];
    [mediaDictionary setObject: [NSNumber numberWithInt: aVideoDesc.nWidth] forKey: @"videoWidth"];
    [mediaDictionary setObject: [NSNumber numberWithInt: aVideoDesc.nHeight] forKey: @"videoHeight"];
    [mediaDictionary setObject: [NSNumber numberWithInt: aVideoDesc.nBitRate] forKey: @"videoBitRate"];
    [mediaDictionary setObject: [NSNumber numberWithInt: aVideoDesc.nFrameRate] forKey: @"videoFrameRate"];

    //
    [mediaDictionary setObject: [NSNumber numberWithInt: aAudioDesc.eAudioCodecID] forKey: @"audioCodecID"];
    [mediaDictionary setObject: [NSString stringWithUTF8String: audioCodec->name] forKey: @"audioCodecName"];
    [mediaDictionary setObject: [NSNumber numberWithInt: aAudioDesc.eChannelCount] forKey: @"audioChannelCount"];
    [mediaDictionary setObject: [NSNumber numberWithInt: aAudioDesc.euiFreq] forKey: @"audioFrequency"];
    [mediaDictionary setObject: [NSNumber numberWithInt: aAudioDesc.uiBitCount] forKey: @"audioBitCount"];   

    return mediaDictionary;
}

+(NSData *) thumbnailPNGDataWithVideoPath: (NSString *) videoPath
{
    // 这个方法有内存泄露
    NSData *data = [[[[ELThumbnail alloc] initWithVideoPath: videoPath] autorelease] pngData];

    return data;
}

@end

//
//  ELThumbnail.h
//  ELMediaPlayerLib
//
//  Created by www.e-linkway.com on 12-6-25.
//  Copyright (c) 2012年 www.e-linkway.com. All rights reserved.
//

#import "libavformat/avformat.h"
#import "libswscale/swscale.h"
#import "libavcodec/avcodec.h"

@interface ELThumbnail : NSObject
{
    NSString *_videoPath;       // 文件路径

    uint8_t *_imageData;        // 保存image数据
    uint32_t _dataSize;          // image字节数

    AVFormatContext *_formatContext;
    AVCodecContext *_codecContext;
    AVCodec *_videoCodec;

    int _videoStreamIndex;
}

@property (nonatomic, retain) NSString *videoPath;

-(id) initWithVideoPath: (NSString *) videoPath;
-(NSData *) pngData;

@end


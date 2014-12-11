//
//  ELVideoEAGLView.h
//
// This class wraps the CAEAGLLayer from CoreAnimation into a convenient UIView subclass.
// The view content is basically an EAGL surface you render your OpenGL scene into.
// Note that setting the view non-opaque will only work if the EAGL surface has an alpha channel.

//  Created by apple on 11-9-2.
//  Copyright 2011年 EL. All rights reserved.
//


#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>

#import "ESRendererProtocol.h"


@interface ELVideoEAGLView : UIView
{
@private
    id <ESRendererProtocol> renderer;

    BOOL animating;
    NSInteger animationFrameInterval;

    // Use of the CADisplayLink class is the preferred method for controlling your animation timing.
    // CADisplayLink will link to the main display and fire every vsync when added to a given run-loop.
    id displayLink;
    CGSize surfaceSize;
}

@property (readonly, nonatomic) id<ESRendererProtocol> renderer;
@property (readonly, nonatomic, getter=isAnimating) BOOL animating;
@property (nonatomic) NSInteger animationFrameInterval;


- (void)startAnimation;
- (void)stopAnimation;
- (void)drawView:(id)sender;
- (void)setSurfaceSize:(CGSize)size;
- (void)updateVideoFrame:(CGRect)frm;
- (void)reset;

- (void)writeSampleBuffer:(unsigned char *)buf;
- (UIImage *)screenshot;
- (void)clearImage;

@end

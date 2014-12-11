//
//  ELVideoEAGLView.m
//
//  Created by apple on 11-9-2.
//  Copyright 2011年 EL. All rights reserved.
//

#import "ELVideoEAGLView.h"
#import "ES1Renderer.h"
#import "ELMediaPlayer.h"
#import "commonDef.h"
#import "el_log.h"

@implementation ELVideoEAGLView

@synthesize animating;
@synthesize renderer;
@dynamic animationFrameInterval;


+ (Class)layerClass
{
    return [CAEAGLLayer class];
}

//The EAGL view is stored in the nib file. When it's unarchived it's sent -initWithCoder:
- (id)initWithFrame:(CGRect)frm 
{    
    if ((self = [super initWithFrame:frm])) 
    {
        CAEAGLLayer *eaglLayer = (CAEAGLLayer *)self.layer;
        eaglLayer.opaque = TRUE;
        eaglLayer.drawableProperties = [NSDictionary dictionaryWithObjectsAndKeys:
                                        [NSNumber numberWithBool:FALSE], kEAGLDrawablePropertyRetainedBacking, kEAGLColorFormatRGBA8, kEAGLDrawablePropertyColorFormat, nil];

        if (!renderer) {
            renderer = [[ES1Renderer alloc] init];

            if (!renderer) {
                [self release];
                return nil;
            }
        }

        animating = FALSE;
        animationFrameInterval = 1;
        displayLink = nil;
    }

    return self;
}

- (void)setSurfaceSize:(CGSize)size 
{
    surfaceSize = size;
    [renderer setSurfaceSize:size];
}

- (void)updateVideoFrame:(CGRect)frm 
{
    [renderer updateVideoSize:frm.size];
    self.frame = frm;
}

- (void)drawView:(id)sender {
    [renderer render];
}

- (void)layoutSubviews {
    [renderer resizeFromLayer:(CAEAGLLayer*)self.layer];
}

- (NSInteger)animationFrameInterval {
    return animationFrameInterval;
}

- (void)setAnimationFrameInterval: (NSInteger)frameInterval
{
    // Frame interval defines how many display frames must pass between each time the
    // display link fires. The display link will only fire 30 times a second when the
    // frame internal is two on a display that refreshes 60 times a second. The default
    // frame interval setting of one will fire 60 times a second when the display refreshes
    // at 60 times a second. A frame interval setting of less than one results in undefined
    // behavior.
    if (frameInterval >= 1) {
        animationFrameInterval = frameInterval;

        if (animating)
        {
            [self stopAnimation];
            [self startAnimation];
        }
    }
}


- (void)startAnimation 
{
    if (!animating) 
    {
        
        ELMediaPlayer *player = [ELMediaPlayer sharedMediaPlayer];

//        EL_DBG_LOG(@"player.buffering %d", player.buffering);
//        EL_DBG_LOG(@"player.initOk %d", player.initOk);
//        EL_DBG_LOG(@"player.paused %d", player.paused);

        if (!player.buffering && player.initOk && !player.paused)
        {
            displayLink = [NSClassFromString(@"CADisplayLink") displayLinkWithTarget:self selector:@selector(drawView:)];
            [displayLink setFrameInterval:animationFrameInterval];
            [displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
            animating = TRUE;
           EL_DBG_LOG("VideoEAGLView, startAnimation : display");
            self.hidden = NO;
        }
    }
}

- (void)stopAnimation 
{
    [renderer flushBuffer];

    if (animating) 
    {
		[displayLink invalidate];
		displayLink = nil;

        animating = FALSE;
    }
}

- (void)writeSampleBuffer:(unsigned char *)buf 
{
    [renderer writeSampleBuffer:buf];

    if (!animating) 
    {
        [self performSelector:@selector(startAnimation) withObject:nil afterDelay:0.6f];        
    }
}

- (UIImage *)screenshot 
{
    return [renderer glToUIImage];
}

- (void)clearImage 
{
    [renderer clearUIImage];
}


- (void)reset 
{
    [renderer reset];
}

- (void)dealloc 
{
    [renderer release];
    [super dealloc];
}


@end

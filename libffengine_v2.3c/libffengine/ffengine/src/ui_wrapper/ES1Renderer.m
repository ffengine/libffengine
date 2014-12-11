
#import "ES1Renderer.h"
#import "ELVideoBuffer.h"
#import "ELVideoEAGLView.h"
#import "ELMediaPlayer.h"
#import "commonDef.h"

#import "el_std.h"
#import "el_log.h"

@implementation ES1Renderer

// Create an OpenGL ES 1.1 context
- (id)init {
    if ((self = [super init])) {
        context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES1];

        if (!context || ![EAGLContext setCurrentContext:context])
        {
            [self release];
            return nil;
        }

        // Create default framebuffer object. The backing will be allocated for the current layer in -resizeFromLayer
        glGenFramebuffersOES(1, &defaultFramebuffer);
        glGenRenderbuffersOES(1, &colorRenderbuffer);
        glBindFramebufferOES(GL_FRAMEBUFFER_OES, defaultFramebuffer);
        glBindRenderbufferOES(GL_RENDERBUFFER_OES, colorRenderbuffer);
        glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_RENDERBUFFER_OES, colorRenderbuffer);
    }

    return self;
}

- (void)render
{
    [EAGLContext setCurrentContext:context];

    glBindFramebufferOES(GL_FRAMEBUFFER_OES, defaultFramebuffer);

    glViewport(0, 0, backingWidth, backingHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    if (videoTexture == nil) {
        ELMediaPlayer *player = [ELMediaPlayer sharedMediaPlayer];
        [self resizeFromLayer:(CAEAGLLayer*)player.videoView.layer];
    }

	[videoTexture renderCameraToSprite:videoTexture.CameraTexture];
    
    glBindRenderbufferOES(GL_RENDERBUFFER_OES, colorRenderbuffer);
    [context presentRenderbuffer:GL_RENDERBUFFER_OES];
}

- (void)setSurfaceSize:(CGSize)size {
    surfaceSize = size;
    [videoTexture updateSurfaceSize:size];
}

- (void)updateVideoSize:(CGSize)size {
    [videoTexture updateVideoSize:size];
}

- (BOOL)resizeFromLayer:(CAEAGLLayer *)layer {
    if (surfaceSize.width == 0 || surfaceSize.height == 0) {
        return NO;
    }

    // Allocate color buffer backing based on the current layer size
    glBindRenderbufferOES(GL_RENDERBUFFER_OES, colorRenderbuffer);
    [context renderbufferStorage:GL_RENDERBUFFER_OES fromDrawable:layer];
    glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_WIDTH_OES, &backingWidth);
    glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_HEIGHT_OES, &backingHeight);

    if (glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES) != GL_FRAMEBUFFER_COMPLETE_OES)
    {
        usleep(300000);
    }
	
	if (videoTexture == nil) {
        videoTexture = [[ELVideoBuffer alloc] initWithSize:surfaceSize];
	}
	
    return YES;
}

- (void)writeSampleBuffer:(unsigned char *)buf {
    if (videoTexture == nil) {
        
        ELMediaPlayer *player = [ELMediaPlayer sharedMediaPlayer];

        [self resizeFromLayer:(CAEAGLLayer*)player.videoView.layer];
	}

    [videoTexture didOutputSampleBuffer:buf];
}

- (void)flushBuffer {
    glFlush();
}

- (void)reset {
    [videoTexture reset];
}

- (UIImage *)glToUIImage {

    GLsizei w = backingWidth;
    GLsizei h = backingHeight;
    NSInteger myDataLength = w * h * 4;
    
    // allocate array and read pixels into it.
    GLubyte *buffer = (GLubyte *)malloc(myDataLength);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
    
    // gl renders "upside down" so swap top to bottom into new array.
    // there's gotta be a better way, but this works.
    GLubyte* swapBuffer = (GLubyte *)malloc(w * 4);
    
    int y = 0;
    for (y=0; y<h/2; y++ ) {
        memcpy( swapBuffer, &buffer[(h-1-y)*w*4], w*4 );
        memcpy( &buffer[(h-1-y)*w*4], &buffer[y*w*4], w*4 );
        memcpy( &buffer[y*w*4], swapBuffer, w*4 );
    }
    free(swapBuffer);
    
    //make data provider with data.
    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, buffer, myDataLength, NULL);
    
    //prep the ingredients
    int bitsPerComponent = 8;
    int bitsPerPixel = 32;
    int bytesPerRow = 4 * w;
    CGColorSpaceRef colorSpaceRef = CGColorSpaceCreateDeviceRGB();
    CGBitmapInfo bitmapInfo = kCGBitmapByteOrderDefault;
    CGColorRenderingIntent renderingIntent = kCGRenderingIntentDefault;
    
    //make the cgimage
    CGImageRef imageRef = CGImageCreate(w, h, bitsPerComponent, bitsPerPixel, bytesPerRow, colorSpaceRef, bitmapInfo, provider, NULL, NO, renderingIntent);
    
    CGColorSpaceRelease(colorSpaceRef);
    CGDataProviderRelease(provider);
    
    //then make the uiimage from that
    UIImage *myImage = [UIImage imageWithCGImage:imageRef];
    CGImageRelease(imageRef);
    
    return myImage;
}


- (void)dealloc {	
	[videoTexture release];
	videoTexture = nil;
	
	// Tear down GL
    if (defaultFramebuffer)
    {
        glDeleteFramebuffersOES(1, &defaultFramebuffer);

        defaultFramebuffer = 0;
    }

    if (colorRenderbuffer)
    {
        glDeleteRenderbuffersOES(1, &colorRenderbuffer);

        colorRenderbuffer = 0;
    }

    if ([EAGLContext currentContext] == context)
    {
        [EAGLContext setCurrentContext:nil];
    }

    [context release];
    context = nil;

    [super dealloc];
}

@end

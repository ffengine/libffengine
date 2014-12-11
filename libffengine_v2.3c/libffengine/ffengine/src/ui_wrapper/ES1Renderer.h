//
//  ESRenderer.h
//  EL
//
//  Created by apple on 11-9-2.
//  Copyright 2011年 EL. All rights reserved.
//

#import <OpenGLES/ES1/gl.h>
#import <OpenGLES/ES1/glext.h>

#import "ESRendererProtocol.h"


@class ELVideoBuffer;

@interface ES1Renderer : NSObject <ESRendererProtocol> {

@private
    EAGLContext *context;

    // The pixel dimensions of the CAEAGLLayer
    GLint backingWidth;
    GLint backingHeight;

    // The OpenGL ES names for the framebuffer and renderbuffer used to render to this view
    GLuint defaultFramebuffer, colorRenderbuffer;
	
	ELVideoBuffer* videoTexture;
    CGSize surfaceSize;
}

- (void)render;
- (BOOL)resizeFromLayer:(CAEAGLLayer *)layer;
- (void)writeSampleBuffer:(unsigned char *)buf;

@end

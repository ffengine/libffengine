/*

File: ELVideoBuffer.h

Abstract: Creates an OpenGL texture to hold the video output data.
		  The delegate is called when a sample buffer 
		  containing an uncompressed frame from the video being captured
		  is written. When a frame is written, it is copied to an OpenGL
		  texture for later display.

Created by apple on 11-9-2.
Copyright 2011年 EL. All rights reserved.

*/

#import <Foundation/Foundation.h>
#include <OpenGLES/ES1/glext.h>
#include <CoreGraphics/CGGeometry.h>

@interface ELVideoBuffer : NSObject {
	Float64 videoFrameRate;	
	uint m_textureHandle;
    CGSize surfaceSize;
    CGSize videoSize;
}

@property (readwrite) Float64 videoFrameRate;
@property (readwrite) uint CameraTexture;


- (id)initWithSize:(CGSize)surfazeSize;
- (void)renderCameraToSprite:(uint)text;
- (GLuint)createVideoTextuerUsingWidth:(GLuint)w Height:(GLuint)h;
- (void)updateVideoSize:(CGSize)size;
- (void)updateSurfaceSize:(CGSize)size;
- (void)didOutputSampleBuffer:(unsigned char *)sample;

- (void)reset;

@end

#import <QuartzCore/QuartzCore.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/EAGLDrawable.h>
#import <UIKit/UIImage.h>

@protocol ESRendererProtocol <NSObject>

@optional

- (void)render;
- (BOOL)resizeFromLayer:(CAEAGLLayer *)layer;
- (void)writeSampleBuffer:(unsigned char *)buf;
- (void)setSurfaceSize:(CGSize)size;
- (void)updateVideoSize:(CGSize)size;
- (void)reset;
- (void)flushBuffer;
- (UIImage *)glToUIImage;
- (void)clearUIImage;

@end

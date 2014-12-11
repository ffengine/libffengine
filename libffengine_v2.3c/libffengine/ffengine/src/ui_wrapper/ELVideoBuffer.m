//
//  ELVideoBuffer.m
//
// This class wraps the CAEAGLLayer from CoreAnimation into a convenient UIView subclass.
// The view content is basically an EAGL surface you render your OpenGL scene into.
// Note that setting the view non-opaque will only work if the EAGL surface has an alpha channel.

//  Created by apple liutao on 11-9-2.
//  Copyright 2011年 EL. All rights reserved.
//

#import <MobileCoreServices/MobileCoreServices.h>
#import <OpenGLES/ES1/gl.h>
#import <OpenGLES/ES1/glext.h>
#import <CoreGraphics/CoreGraphics.h>

#import "ELVideoBuffer.h"
#import "el_std.h"
#import "el_log.h"


/* disable these capabilities. */
static GLuint s_disable_caps[] = {
	GL_FOG,
	GL_LIGHTING,
	GL_CULL_FACE,
	GL_ALPHA_TEST,
	GL_BLEND,
	GL_COLOR_LOGIC_OP,
	GL_DITHER,
	GL_STENCIL_TEST,
	GL_DEPTH_TEST,
	GL_COLOR_MATERIAL,
	0
};


@implementation ELVideoBuffer

@synthesize videoFrameRate;
@synthesize CameraTexture=m_textureHandle;


#define DEFAULT_VIEW_SIZE    CGSizeMake(480, 320)


- (id)initWithSize:(CGSize)surfazeSize {
	if ((self = [super init])) {
        surfaceSize = surfazeSize;
        m_textureHandle = [self createVideoTextuerUsingWidth:surfaceSize.width Height:surfaceSize.height];
        videoSize = DEFAULT_VIEW_SIZE;
	}
	return self;
}


- (void)updateVideoSize:(CGSize)size {
    videoSize = size;
}

- (void)updateSurfaceSize:(CGSize)size {
    surfaceSize = size;
}


//#define bytes_per_pixel     4
#define bytes_per_pixel     2

- (GLuint)createVideoTextuerUsingWidth:(GLuint)w Height:(GLuint)h
{
	int dataSize = w * h * bytes_per_pixel;
	uint8_t* textureData = (uint8_t*)malloc(dataSize);
	if(textureData == NULL)
		return 0;
	memset(textureData, 128, dataSize);
    
    GLuint *start = s_disable_caps;
	while (*start) {
        glDisable(*start++);
#if OpenGLLOG        
        EL_DBG_LOG("openGL : glDisable (GLenum cap);\n");
#endif
    }
	
	GLuint handle;
	glGenTextures(1, &handle);
#if OpenGLLOG
    EL_DBG_LOG("openGL : glGenTextures (GLsizei n, GLuint* textures);\n");
#endif
	glBindTexture(GL_TEXTURE_2D, handle);
#if OpenGLLOG
    EL_DBG_LOG("openGL : glBindTexture (GLenum target, GLuint texture);\n");
#endif
	glTexParameterf(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
#if OpenGLLOG
    EL_DBG_LOG("openGL : glTexParameterf (GLenum target, GLenum pname, GLfloat param);\n");
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, textureData);
#if OpenGLLOG
    EL_DBG_LOG("openGL : glTexImage2D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid* pixels);\n");
#endif
    
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#if OpenGLLOG
    EL_DBG_LOG("openGL : glTexParameteri (GLenum target, GLenum pname, GLint param);\n");
#endif
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#if OpenGLLOG
    EL_DBG_LOG("openGL : glTexParameteri (GLenum target, GLenum pname, GLint param);\n");
#endif
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
#if OpenGLLOG
    EL_DBG_LOG("openGL : glTexParameterf (GLenum target, GLenum pname, GLfloat param);\n");
#endif
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#if OpenGLLOG
   EL_DBG_LOG("openGL : glTexParameterf (GLenum target, GLenum pname, GLfloat param);\n");
#endif
	glBindTexture(GL_TEXTURE_2D, 0);
#if OpenGLLOG
   EL_DBG_LOG("openGL : glBindTexture (GLenum target, GLuint texture);\n");
#endif
	free(textureData);
#if OpenGLLOG
   EL_DBG_LOG("openGL : free textureData\n");
#endif
	return handle;
}


- (void)didOutputSampleBuffer:(unsigned char *)sample 
{
    //EL_DBG_LOG("ELVideoBuffer didOutputSampleBuffer");
    //-- If we haven't created the video texture, do so now.
	if(m_textureHandle == 0) {
		m_textureHandle = [self createVideoTextuerUsingWidth:surfaceSize.width Height:surfaceSize.height];
	}
	//处理纹理
	glBindTexture(GL_TEXTURE_2D, m_textureHandle);
#if OpenGLLOG
   EL_DBG_LOG("openGL : glBindTexture (GLenum target, GLuint texture);\n");
#endif
    //sample 必须是2的N次幂 长和宽 不必
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, surfaceSize.width, surfaceSize.height, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, sample);
    //NSLog(@"gl: surfaceSize.width = %f, surfaceSize.height = %f",surfaceSize.width, surfaceSize.height);
//        
#if OpenGLLOG
   EL_DBG_LOG("openGL : glTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid* pixels);\n");
#endif
}


- (void)renderCameraToSprite:(uint)texture {
    //EL_DBG_LOG("ELVideoBuffer renderCameraToSprite:(uint)texture\n");
    //纹理坐标
    GLfloat spriteTexcoords[] = {
		1.0f,   1.0f,   
		1.0f,   0.0f,
		0.0f,   1.0f,   
		0.0f,   0.0f,
    };
    //顶点坐标
//#ifdef EL_PLATFORM_IPAD
//    videoSize.height = 768.0f;
//    videoSize.width = 1024.0f;
//#endif
    GLfloat spriteVertices[] =  {
		videoSize.height,   videoSize.width,    0.0f,
		videoSize.height,   0.0f,               0.0f,
		0.0f,               videoSize.width,    0.0f, 
        0.0f,               0.0f,               0.0f,
    };

    //指定当前矩阵是投影矩阵
	glMatrixMode(GL_PROJECTION);
#if OpenGLLOG
   EL_DBG_LOG("openGL : glMatrixMode (GLenum mode);\n");
#endif
    //将矩阵初始化为单位矩阵
	glLoadIdentity();
#if OpenGLLOG
   EL_DBG_LOG("openGL : glLoadIdentity (void);\n");
#endif
    //指定投影类型是正交投影
    glOrthof(0, videoSize.height, videoSize.width, 0, 0, 1);
#if OpenGLLOG
   EL_DBG_LOG("openGL : glOrthof (GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat zNear, GLfloat zFar);\n");
#endif
	//指定当前矩阵为模型矩阵
    glMatrixMode(GL_MODELVIEW);
#if OpenGLLOG
   EL_DBG_LOG("openGL : glMatrixMode (GLenum mode);\n");
#endif
    //将矩阵初始化为单位矩阵
    glLoadIdentity();
#if OpenGLLOG
   EL_DBG_LOG("openGL : glLoadIdentity (void);\n");
#endif
	//glDisable(GL_DEPTH_TEST);
    //取消颜色数组
	glDisableClientState(GL_COLOR_ARRAY);
#if OpenGLLOG
   EL_DBG_LOG("openGL : glDisableClientState (GLenum array);\n");
#endif
    //启用顶点数组
	glEnableClientState(GL_VERTEX_ARRAY);
#if OpenGLLOG
   EL_DBG_LOG("openGL : glEnableClientState (GLenum array);\n");
#endif
    //定义顶点数组
	glVertexPointer(3, GL_FLOAT, 0, spriteVertices);
#if OpenGLLOG
   EL_DBG_LOG("openGL : glVertexPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);\n");
#endif
    //启用纹理坐标数组
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
#if OpenGLLOG
   EL_DBG_LOG("openGL : glEnableClientState (GLenum array);\n");
#endif
    //定义纹理坐标
	glTexCoordPointer(2, GL_FLOAT, 0, spriteTexcoords);
#if OpenGLLOG
   EL_DBG_LOG("openGL : glTexCoordPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);\n");
#endif
    //绑定纹理
	glBindTexture(GL_TEXTURE_2D, texture);
#if OpenGLLOG
   EL_DBG_LOG("openGL : glBindTexture (GLenum target, GLuint texture);\n");
#endif
    //启用2d纹理
	glEnable(GL_TEXTURE_2D);
#if OpenGLLOG
   EL_DBG_LOG("openGL : glEnable (GLenum cap);\n");
#endif
    //绘制
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
#if OpenGLLOG
   EL_DBG_LOG("openGL : glDrawArrays (GLenum mode, GLint first, GLsizei count);\n");
#endif
    //取消
	glDisableClientState(GL_VERTEX_ARRAY);
#if OpenGLLOG
   EL_DBG_LOG("openGL : glDisableClientState (GLenum array);\n");
#endif
	glDisableClientState(GL_COLOR_ARRAY);
#if OpenGLLOG
   EL_DBG_LOG("openGL : glDisableClientState (GLenum array);\n");
#endif
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
#if OpenGLLOG
   EL_DBG_LOG("openGL : glDisableClientState (GLenum array);\n");
#endif
    //取消绑定的纹理
	glBindTexture(GL_TEXTURE_2D, 0);
#if OpenGLLOG
   EL_DBG_LOG("openGL : glBindTexture (GLenum target, GLuint texture);\n");
#endif
	//glEnable(GL_DEPTH_TEST);
}


- (void)reset {
    if (m_textureHandle) {
        glDeleteTextures(1, &m_textureHandle);
#if OpenGLLOG
       EL_DBG_LOG("openGL : glDeleteTextures (GLsizei n, const GLuint* textures);\n");
#endif
        m_textureHandle = 0;
    }
}



- (void)dealloc {
    [self reset];
	[super dealloc];
}

@end

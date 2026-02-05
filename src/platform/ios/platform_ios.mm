/*
 * iOS platform backend (OpenGL ES)
 *
 * Uses CAEAGLLayer + EAGLContext to render NanoVG via GLES3.
 */

#include <stdbool.h>
#include <string.h>

#import <QuartzCore/CAEAGLLayer.h>
#import <OpenGLES/ES3/gl.h>
#import <OpenGLES/ES3/glext.h>
#import <UIKit/UIKit.h>

#define NANOVG_GLES3_IMPLEMENTATION
#include "nanovg/nanovg.h"
#include "nanovg/nanovg_gl.h"

#include "scenic_renderer.h"
#include "platform_ios.h"

static int g_width = 0;
static int g_height = 0;
static float g_pixel_ratio = 1.0f;
static bool g_should_close = false;

static CAEAGLLayer* g_layer = nil;
static EAGLContext* g_context = nil;
static CADisplayLink* g_display_link = nil;
static NSObject* g_display_target = nil;
static scenic_renderer_t* g_renderer = NULL;
static scenic_platform_callback_t g_callback = NULL;
static void* g_user_data = NULL;
static float g_clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
static GLuint g_framebuffer = 0;
static GLuint g_colorbuffer = 0;
static GLuint g_depthbuffer = 0;
static NVGcontext* g_nvg = NULL;

/* External function to set NanoVG context */
extern void scenic_renderer_set_nvg_context(scenic_renderer_t* r, NVGcontext* ctx);

static bool ensure_context(void) {
    if (g_context) {
        return true;
    }
    g_context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
    if (!g_context) {
        return false;
    }
    return [EAGLContext setCurrentContext:g_context];
}

static void destroy_buffers(void) {
    if (g_depthbuffer) {
        glDeleteRenderbuffers(1, &g_depthbuffer);
        g_depthbuffer = 0;
    }
    if (g_colorbuffer) {
        glDeleteRenderbuffers(1, &g_colorbuffer);
        g_colorbuffer = 0;
    }
    if (g_framebuffer) {
        glDeleteFramebuffers(1, &g_framebuffer);
        g_framebuffer = 0;
    }
}

static bool create_buffers(void) {
    if (!g_layer) {
        return false;
    }
    if (!ensure_context()) {
        return false;
    }

    destroy_buffers();

    glGenFramebuffers(1, &g_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, g_framebuffer);

    glGenRenderbuffers(1, &g_colorbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, g_colorbuffer);
    [g_context renderbufferStorage:GL_RENDERBUFFER fromDrawable:g_layer];
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, g_colorbuffer);

    GLint width = 0;
    GLint height = 0;
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);
    g_width = (int)width;
    g_height = (int)height;

    glGenRenderbuffers(1, &g_depthbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, g_depthbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, g_depthbuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g_depthbuffer);

    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

static void ios_begin_frame(void* user_data, int width, int height, float ratio) {
    (void)user_data;
    (void)ratio;
    if (!ensure_context()) {
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, g_framebuffer);
    glViewport(0, 0, width, height);
    glClearColor(g_clear_color[0], g_clear_color[1], g_clear_color[2], g_clear_color[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

static void ios_end_frame(void* user_data) {
    (void)user_data;
    glBindRenderbuffer(GL_RENDERBUFFER, g_colorbuffer);
    [g_context presentRenderbuffer:GL_RENDERBUFFER];
}

@interface ScenicDisplayLinkTarget : NSObject
@end

@implementation ScenicDisplayLinkTarget
- (void)tick:(CADisplayLink*)link {
    (void)link;
    if (g_should_close) {
        [g_display_link invalidate];
        g_display_link = nil;
        return;
    }
    if (g_renderer && g_callback) {
        g_callback(g_renderer, g_user_data);
    }
}
@end

scenic_platform_t scenic_platform_init(int width, int height, const char* title) {
    (void)title;
    g_width = width;
    g_height = height;
    g_pixel_ratio = (float)[UIScreen mainScreen].scale;
    g_should_close = false;

    if (g_layer) {
        g_layer.opaque = YES;
        g_layer.contentsScale = g_pixel_ratio;
        create_buffers();
    }

    if (!g_nvg) {
        g_nvg = nvgCreateGLES3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    }

    scenic_platform_t platform;
    memset(&platform, 0, sizeof(platform));
    platform.user_data = NULL;
    platform.begin_frame = ios_begin_frame;
    platform.end_frame = ios_end_frame;
    platform.swap_buffers = NULL;
    return platform;
}

void scenic_platform_run(scenic_renderer_t* renderer,
                         scenic_platform_callback_t callback,
                         void* user_data) {
    g_renderer = renderer;
    g_callback = callback;
    g_user_data = user_data;

    if (g_renderer && g_nvg) {
        scenic_renderer_set_nvg_context(g_renderer, g_nvg);
    }
    scenic_platform_ios_start();
}

bool scenic_platform_should_close(void) {
    return g_should_close;
}

void scenic_platform_request_close(void) {
    g_should_close = true;
}

void scenic_platform_shutdown(void) {
    g_should_close = true;
    scenic_platform_ios_stop();
    if (g_nvg) {
        nvgDeleteGLES3(g_nvg);
        g_nvg = NULL;
    }
    destroy_buffers();
}

void scenic_platform_get_size(int* width, int* height) {
    if (width) *width = g_width;
    if (height) *height = g_height;
}

float scenic_platform_get_pixel_ratio(void) {
    return g_pixel_ratio;
}

void scenic_platform_ios_set_layer(void* metal_layer) {
    g_layer = (__bridge CAEAGLLayer*)metal_layer;
    if (g_layer) {
        g_layer.opaque = YES;
        g_layer.contentsScale = g_pixel_ratio;
        create_buffers();
    }
}

void scenic_platform_ios_set_drawable_size(int width, int height, float pixel_ratio) {
    g_width = width;
    g_height = height;
    g_pixel_ratio = pixel_ratio > 0.0f ? pixel_ratio : g_pixel_ratio;

    if (g_layer) {
        g_layer.contentsScale = g_pixel_ratio;
        create_buffers();
    }
}

void scenic_platform_ios_start(void) {
    if (g_display_link) {
        return;
    }
    if (!g_display_target) {
        g_display_target = [ScenicDisplayLinkTarget new];
    }
    g_display_link = [CADisplayLink displayLinkWithTarget:g_display_target selector:@selector(tick:)];
    [g_display_link addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
}

void scenic_platform_ios_stop(void) {
    if (g_display_link) {
        [g_display_link invalidate];
        g_display_link = nil;
    }
    [EAGLContext setCurrentContext:nil];
}

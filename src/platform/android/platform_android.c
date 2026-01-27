/*
 * Android platform backend for Scenic renderer
 *
 * This provides the interface between Android's EGL context and the
 * Scenic renderer. The actual EGL/surface management is handled by
 * the Android app's Java/Kotlin code.
 */

#include <stdlib.h>
#include <android/log.h>
#include <GLES3/gl3.h>

#define NANOVG_GLES3_IMPLEMENTATION
#include "nanovg/nanovg.h"
#include "nanovg/nanovg_gl.h"

#include "scenic_renderer.h"

#define LOG_TAG "ScenicPlatform"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/* External function to set NanoVG context */
extern void scenic_renderer_set_nvg_context(scenic_renderer_t* r, NVGcontext* ctx);

static NVGcontext* g_nvg = NULL;
static float g_clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};

/* Platform callbacks */
static void android_begin_frame(void* user_data, int width, int height, float ratio) {
    (void)user_data;
    (void)ratio;
    glViewport(0, 0, width, height);
    glClearColor(g_clear_color[0], g_clear_color[1], g_clear_color[2], g_clear_color[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

static void android_end_frame(void* user_data) {
    (void)user_data;
    /* Buffer swap is handled by Android's GLSurfaceView */
}

/*
 * Initialize NanoVG context
 * Call this after EGL context is created
 */
NVGcontext* scenic_platform_android_init_nvg(void) {
    if (g_nvg) {
        return g_nvg;
    }

    g_nvg = nvgCreateGLES3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    if (!g_nvg) {
        LOGE("Failed to create NanoVG context");
        return NULL;
    }

    LOGI("NanoVG context created");
    return g_nvg;
}

/*
 * Get platform callbacks for renderer
 */
scenic_platform_t scenic_platform_android_get_platform(void) {
    scenic_platform_t platform = {
        .user_data = NULL,
        .begin_frame = android_begin_frame,
        .end_frame = android_end_frame,
        .swap_buffers = NULL  /* Handled by Android */
    };
    return platform;
}

/*
 * Set up renderer with Android platform
 */
void scenic_platform_android_setup_renderer(scenic_renderer_t* renderer) {
    if (!g_nvg) {
        scenic_platform_android_init_nvg();
    }
    if (renderer && g_nvg) {
        scenic_renderer_set_nvg_context(renderer, g_nvg);
    }
}

/*
 * Cleanup NanoVG context
 * Call before EGL context is destroyed
 */
void scenic_platform_android_shutdown(void) {
    if (g_nvg) {
        nvgDeleteGLES3(g_nvg);
        g_nvg = NULL;
        LOGI("NanoVG context destroyed");
    }
}

/*
 * Set clear color
 */
void scenic_platform_android_set_clear_color(float r, float g, float b, float a) {
    g_clear_color[0] = r;
    g_clear_color[1] = g;
    g_clear_color[2] = b;
    g_clear_color[3] = a;
}

/* Override logging functions for Android */
void send_puts(const char* msg) {
    if (msg) {
        LOGI("%s", msg);
    }
}

void log_info(const char* msg) {
    if (msg) {
        LOGI("%s", msg);
    }
}

void log_warn(const char* msg) {
    if (msg) {
        __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "%s", msg);
    }
}

void log_error(const char* msg) {
    if (msg) {
        LOGE("%s", msg);
    }
}

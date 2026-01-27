/*
 * GLFW platform backend for Scenic renderer
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

#include <GLFW/glfw3.h>

#define NANOVG_GL3_IMPLEMENTATION
#include "nanovg/nanovg.h"
#include "nanovg/nanovg_gl.h"

#include "platform/platform.h"
#include "scenic_renderer.h"

/* External function to set NanoVG context */
extern void scenic_renderer_set_nvg_context(scenic_renderer_t* r, NVGcontext* ctx);

static GLFWwindow* g_window = NULL;
static NVGcontext* g_nvg = NULL;
static scenic_renderer_t* g_renderer = NULL;
static bool g_should_close = false;
static float g_clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};

/* GLFW callbacks */
static void error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    (void)window;
    if (g_renderer) {
        float ratio = scenic_platform_get_pixel_ratio();
        scenic_renderer_resize(g_renderer, width, height, ratio);
        scenic_renderer_send_reshape(g_renderer, width, height);
    }
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)window;
    if (g_renderer) {
        scenic_renderer_send_key(g_renderer, (uint32_t)key, (uint32_t)scancode,
                                  (int32_t)action, (uint32_t)mods);
    }

    /* Allow Escape to close window */
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        g_should_close = true;
    }
}

static void char_callback(GLFWwindow* window, unsigned int codepoint) {
    (void)window;
    if (g_renderer) {
        scenic_renderer_send_codepoint(g_renderer, codepoint, 0);
    }
}

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    (void)window;
    if (g_renderer) {
        scenic_renderer_send_cursor_pos(g_renderer, (float)xpos, (float)ypos);
    }
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    if (g_renderer) {
        scenic_renderer_send_mouse_button(g_renderer, (uint32_t)button, (uint32_t)action,
                                           (uint32_t)mods, (float)xpos, (float)ypos);
    }
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    if (g_renderer) {
        scenic_renderer_send_scroll(g_renderer, (float)xoffset, (float)yoffset,
                                     (float)xpos, (float)ypos);
    }
}

static void cursor_enter_callback(GLFWwindow* window, int entered) {
    (void)window;
    if (g_renderer) {
        scenic_renderer_send_cursor_enter(g_renderer, entered != 0);
    }
}

/* Platform callbacks for renderer */
static void platform_begin_frame(void* user_data, int width, int height, float ratio) {
    (void)user_data;
    (void)ratio;
    glViewport(0, 0, width, height);
    glClearColor(g_clear_color[0], g_clear_color[1], g_clear_color[2], g_clear_color[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

static void platform_end_frame(void* user_data) {
    (void)user_data;
}

static void platform_swap_buffers(void* user_data) {
    (void)user_data;
    if (g_window) {
        glfwSwapBuffers(g_window);
    }
}

scenic_platform_t scenic_platform_init(int width, int height, const char* title) {
    scenic_platform_t platform = {0};

    glfwSetErrorCallback(error_callback);

    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return platform;
    }

    /* Request OpenGL 3.2+ core profile */
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_SAMPLES, 4);  /* MSAA */

    g_window = glfwCreateWindow(width, height, title ? title : "Scenic Renderer", NULL, NULL);
    if (!g_window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return platform;
    }

    glfwMakeContextCurrent(g_window);
    glfwSwapInterval(1);  /* VSync */

    /* Create NanoVG context */
    g_nvg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    if (!g_nvg) {
        fprintf(stderr, "Failed to create NanoVG context\n");
        glfwDestroyWindow(g_window);
        glfwTerminate();
        return platform;
    }

    /* Set up GLFW callbacks */
    glfwSetFramebufferSizeCallback(g_window, framebuffer_size_callback);
    glfwSetKeyCallback(g_window, key_callback);
    glfwSetCharCallback(g_window, char_callback);
    glfwSetCursorPosCallback(g_window, cursor_pos_callback);
    glfwSetMouseButtonCallback(g_window, mouse_button_callback);
    glfwSetScrollCallback(g_window, scroll_callback);
    glfwSetCursorEnterCallback(g_window, cursor_enter_callback);

    platform.user_data = NULL;
    platform.begin_frame = platform_begin_frame;
    platform.end_frame = platform_end_frame;
    platform.swap_buffers = platform_swap_buffers;

    g_should_close = false;

    return platform;
}

void scenic_platform_run(scenic_renderer_t* renderer,
                         scenic_platform_callback_t callback,
                         void* user_data) {
    g_renderer = renderer;

    /* Set NanoVG context on renderer */
    if (renderer && g_nvg) {
        scenic_renderer_set_nvg_context(renderer, g_nvg);
    }

    while (!glfwWindowShouldClose(g_window) && !g_should_close) {
        glfwPollEvents();

        if (callback) {
            callback(renderer, user_data);
        }

        /* Render and swap */
        scenic_renderer_render(renderer);
        glfwSwapBuffers(g_window);
    }
}

bool scenic_platform_should_close(void) {
    return g_should_close || (g_window && glfwWindowShouldClose(g_window));
}

void scenic_platform_request_close(void) {
    g_should_close = true;
}

void scenic_platform_shutdown(void) {
    if (g_nvg) {
        nvgDeleteGL3(g_nvg);
        g_nvg = NULL;
    }
    if (g_window) {
        glfwDestroyWindow(g_window);
        g_window = NULL;
    }
    glfwTerminate();
    g_renderer = NULL;
}

void scenic_platform_get_size(int* width, int* height) {
    if (g_window) {
        glfwGetFramebufferSize(g_window, width, height);
    } else {
        if (width) *width = 0;
        if (height) *height = 0;
    }
}

float scenic_platform_get_pixel_ratio(void) {
    if (!g_window) return 1.0f;

    int win_width, win_height;
    int fb_width, fb_height;
    glfwGetWindowSize(g_window, &win_width, &win_height);
    glfwGetFramebufferSize(g_window, &fb_width, &fb_height);

    if (win_width > 0) {
        return (float)fb_width / (float)win_width;
    }
    return 1.0f;
}

/* Set clear color from renderer */
void scenic_platform_set_clear_color(float r, float g, float b, float a) {
    g_clear_color[0] = r;
    g_clear_color[1] = g;
    g_clear_color[2] = b;
    g_clear_color[3] = a;
}

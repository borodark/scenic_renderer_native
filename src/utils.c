/*
 * Utility functions
 */

#include "utils.h"
#include "comms.h"

#ifdef __ANDROID__
#include <GLES3/gl3.h>
#else
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif
#endif

void check_gl_error(void) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        char msg[64];
        snprintf(msg, sizeof(msg), "GL error: 0x%04x", err);
        log_error(msg);
    }
}

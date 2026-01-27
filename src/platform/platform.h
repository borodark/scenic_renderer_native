/*
 * Platform interface for Scenic renderer
 */

#ifndef SCENIC_PLATFORM_H
#define SCENIC_PLATFORM_H

#include "scenic_renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Platform-specific initialization
 * Creates window and OpenGL context
 * Returns platform callbacks structure
 */
scenic_platform_t scenic_platform_init(int width, int height, const char* title);

/*
 * Platform event loop (blocking)
 * Calls the provided callback each frame
 */
typedef void (*scenic_platform_callback_t)(scenic_renderer_t* renderer, void* user_data);

void scenic_platform_run(scenic_renderer_t* renderer,
                         scenic_platform_callback_t callback,
                         void* user_data);

/*
 * Check if platform should continue running
 */
bool scenic_platform_should_close(void);

/*
 * Request platform shutdown
 */
void scenic_platform_request_close(void);

/*
 * Platform shutdown and cleanup
 */
void scenic_platform_shutdown(void);

/*
 * Get window dimensions
 */
void scenic_platform_get_size(int* width, int* height);

/*
 * Get framebuffer pixel ratio
 */
float scenic_platform_get_pixel_ratio(void);

#ifdef __cplusplus
}
#endif

#endif /* SCENIC_PLATFORM_H */

/*
 * iOS platform backend (Metal) - stubs
 *
 * These are placeholders for the upcoming Metal-based platform adapter.
 * The actual implementation will live in platform_ios.mm.
 */

#ifndef SCENIC_PLATFORM_IOS_H
#define SCENIC_PLATFORM_IOS_H

#include "../platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * iOS-specific hooks (Metal)
 *
 * The host app should provide a CAMetalLayer and update drawable size
 * as the view/layout changes. These are optional stubs for now.
 */

/* Provide the CAMetalLayer from the host app (pass as void* to avoid ObjC headers in C). */
void scenic_platform_ios_set_layer(void* metal_layer);

/* Update drawable size + pixel ratio (typically from UIScreen scale). */
void scenic_platform_ios_set_drawable_size(int width, int height, float pixel_ratio);

/* Explicit start/stop for CADisplayLink render loop (host-controlled). */
void scenic_platform_ios_start(void);
void scenic_platform_ios_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* SCENIC_PLATFORM_IOS_H */

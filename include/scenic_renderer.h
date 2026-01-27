/*
 * Scenic Native Renderer
 *
 * Standalone C library that receives Scenic commands and renders them
 * using NanoVG/OpenGL.
 */

#ifndef SCENIC_RENDERER_H
#define SCENIC_RENDERER_H

#include <stdint.h>
#include <stdbool.h>
#include "scenic_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque renderer handle */
typedef struct scenic_renderer scenic_renderer_t;

/* Platform callbacks (provided by platform backend) */
typedef struct {
    void* user_data;
    void (*begin_frame)(void* user_data, int width, int height, float ratio);
    void (*end_frame)(void* user_data);
    void (*swap_buffers)(void* user_data);
} scenic_platform_t;

/* Configuration */
typedef struct {
    int width;
    int height;
    float pixel_ratio;
    scenic_transport_t* transport;      /* NULL for manual command mode */
    scenic_platform_t platform;
} scenic_renderer_config_t;

/*
 * Lifecycle functions
 */

/* Create a new renderer instance */
scenic_renderer_t* scenic_renderer_create(const scenic_renderer_config_t* config);

/* Destroy renderer and free resources */
void scenic_renderer_destroy(scenic_renderer_t* renderer);

/* Handle window/viewport resize */
void scenic_renderer_resize(scenic_renderer_t* r, int w, int h, float ratio);

/*
 * Command processing
 */

/* Process commands from transport (returns commands processed, -1 on error) */
int scenic_renderer_process_commands(scenic_renderer_t* r, int timeout_ms);

/*
 * Manual command interface (when transport is NULL)
 * These functions process command data directly.
 */

/* Set background clear color */
void scenic_renderer_cmd_clear_color(scenic_renderer_t* r, float red, float green, float blue, float alpha);

/* Store/update a script */
void scenic_renderer_cmd_put_script(scenic_renderer_t* r, const uint8_t* data, uint32_t len);

/* Delete a script */
void scenic_renderer_cmd_del_script(scenic_renderer_t* r, const uint8_t* data, uint32_t len);

/* Clear all scripts, fonts, and images */
void scenic_renderer_cmd_reset(scenic_renderer_t* r);

/* Load a font */
void scenic_renderer_cmd_put_font(scenic_renderer_t* r, const uint8_t* data, uint32_t len);

/* Load an image */
void scenic_renderer_cmd_put_image(scenic_renderer_t* r, const uint8_t* data, uint32_t len);

/* Set global transform */
void scenic_renderer_cmd_global_tx(scenic_renderer_t* r, const float tx[6]);

/*
 * Rendering
 */

/* Render current scene */
void scenic_renderer_render(scenic_renderer_t* r);

/*
 * Event sending (only when transport configured)
 */

/* Send ready event */
void scenic_renderer_send_ready(scenic_renderer_t* r);

/* Send reshape event */
void scenic_renderer_send_reshape(scenic_renderer_t* r, int width, int height);

/* Send touch/pointer event */
void scenic_renderer_send_touch(scenic_renderer_t* r, int action, float x, float y);

/* Send key event */
void scenic_renderer_send_key(scenic_renderer_t* r, uint32_t key, uint32_t scancode,
                               int32_t action, uint32_t mods);

/* Send codepoint (character input) event */
void scenic_renderer_send_codepoint(scenic_renderer_t* r, uint32_t codepoint, uint32_t mods);

/* Send mouse button event */
void scenic_renderer_send_mouse_button(scenic_renderer_t* r, uint32_t button, uint32_t action,
                                        uint32_t mods, float x, float y);

/* Send cursor position event */
void scenic_renderer_send_cursor_pos(scenic_renderer_t* r, float x, float y);

/* Send scroll event */
void scenic_renderer_send_scroll(scenic_renderer_t* r, float xoff, float yoff, float x, float y);

/* Send cursor enter/leave event */
void scenic_renderer_send_cursor_enter(scenic_renderer_t* r, bool entered);

/*
 * Utility functions
 */

/* Get NanoVG context (for advanced use) */
void* scenic_renderer_get_nvg_context(scenic_renderer_t* r);

/* Get current dimensions */
void scenic_renderer_get_size(scenic_renderer_t* r, int* width, int* height, float* ratio);

#ifdef __cplusplus
}
#endif

#endif /* SCENIC_RENDERER_H */

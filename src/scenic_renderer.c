/*
 * Scenic Native Renderer Implementation
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "scenic_renderer.h"
#include "scenic_renderer_internal.h"
#include "scenic_protocol.h"
#include "protocol.h"
#include "comms.h"
#include "script.h"
#include "font.h"
#include "image.h"

/* Forward declarations */
static void process_command(scenic_renderer_t* r, uint8_t type,
                           const uint8_t* payload, uint32_t len);
static int send_event(scenic_renderer_t* r, uint8_t type,
                      const void* payload, uint32_t len);

scenic_renderer_t* scenic_renderer_create(const scenic_renderer_config_t* config) {
    scenic_renderer_t* r = calloc(1, sizeof(scenic_renderer_t));
    if (!r) return NULL;

    r->width = config->width;
    r->height = config->height;
    r->pixel_ratio = config->pixel_ratio > 0 ? config->pixel_ratio : 1.0f;
    r->transport = config->transport;
    r->platform = config->platform;

    /* Initialize clear color to black */
    r->clear_color[0] = 0.0f;
    r->clear_color[1] = 0.0f;
    r->clear_color[2] = 0.0f;
    r->clear_color[3] = 1.0f;

    /* Initialize global transform to identity */
    r->global_tx[0] = 1.0f;
    r->global_tx[1] = 0.0f;
    r->global_tx[2] = 0.0f;
    r->global_tx[3] = 1.0f;
    r->global_tx[4] = 0.0f;
    r->global_tx[5] = 0.0f;

    /* Allocate buffers */
    r->recv_buf_size = DEFAULT_RECV_BUF_SIZE;
    r->recv_buf = malloc(r->recv_buf_size);
    r->recv_buf_len = 0;

    r->send_buf_size = DEFAULT_SEND_BUF_SIZE;
    r->send_buf = malloc(r->send_buf_size);

    if (!r->recv_buf || !r->send_buf) {
        free(r->recv_buf);
        free(r->send_buf);
        free(r);
        return NULL;
    }

    /* Initialize subsystems */
    init_scripts();
    init_fonts();
    init_images();

    /* NanoVG context will be created lazily when GL is ready */
    r->nvg_ctx = NULL;
    r->initialized = false;

    return r;
}

void scenic_renderer_destroy(scenic_renderer_t* r) {
    if (!r) return;

    /* Cleanup subsystems */
    reset_scripts();
    if (r->nvg_ctx) {
        reset_fonts(r->nvg_ctx);
        reset_images(r->nvg_ctx);
        /* NanoVG context cleanup depends on backend, handled by platform */
    }

    free(r->recv_buf);
    free(r->send_buf);
    free(r);
}

void scenic_renderer_resize(scenic_renderer_t* r, int w, int h, float ratio) {
    if (!r) return;
    r->width = w;
    r->height = h;
    if (ratio > 0) {
        r->pixel_ratio = ratio;
    }
}

/* Ensure NanoVG context is initialized */
static void ensure_nvg_context(scenic_renderer_t* r) {
    if (r->nvg_ctx) return;

    /* Platform-specific NanoVG creation will be done by the platform layer
     * For now, we expect it to be set externally or created in platform init */
}

int scenic_renderer_process_commands(scenic_renderer_t* r, int timeout_ms) {
    if (!r || !r->transport) return -1;

    int commands_processed = 0;

    /* Check if data is available */
    if (!scenic_transport_data_available(r->transport, timeout_ms)) {
        return 0;
    }

    /* Read available data into buffer */
    int bytes_read = scenic_transport_recv(
        r->transport,
        r->recv_buf + r->recv_buf_len,
        r->recv_buf_size - r->recv_buf_len,
        0  /* Non-blocking since we know data is available */
    );

    if (bytes_read < 0) {
        return -1;
    }

    r->recv_buf_len += bytes_read;

    /* Process complete commands */
    int offset = 0;
    while (offset < r->recv_buf_len) {
        uint8_t type;
        uint32_t payload_len;

        if (!protocol_parse_header(r->recv_buf + offset, r->recv_buf_len - offset,
                                   &type, &payload_len)) {
            break;  /* Need more data for header */
        }

        int total_msg_len = SCENIC_MSG_HEADER_SIZE + payload_len;
        if (r->recv_buf_len - offset < total_msg_len) {
            break;  /* Need more data for payload */
        }

        /* Process this command */
        process_command(r, type,
                       r->recv_buf + offset + SCENIC_MSG_HEADER_SIZE,
                       payload_len);
        commands_processed++;

        offset += total_msg_len;
    }

    /* Move remaining data to start of buffer */
    if (offset > 0 && offset < r->recv_buf_len) {
        memmove(r->recv_buf, r->recv_buf + offset, r->recv_buf_len - offset);
    }
    r->recv_buf_len -= offset;

    return commands_processed;
}

static void process_command(scenic_renderer_t* r, uint8_t type,
                           const uint8_t* payload, uint32_t len) {
    ensure_nvg_context(r);

    switch (type) {
        case SCENIC_CMD_PUT_SCRIPT:
            scenic_renderer_cmd_put_script(r, payload, len);
            break;

        case SCENIC_CMD_DEL_SCRIPT:
            scenic_renderer_cmd_del_script(r, payload, len);
            break;

        case SCENIC_CMD_RESET:
            scenic_renderer_cmd_reset(r);
            break;

        case SCENIC_CMD_CLEAR_COLOR:
            if (len >= 16) {
                float red = ntoh_f32(*(float*)(payload));
                float green = ntoh_f32(*(float*)(payload + 4));
                float blue = ntoh_f32(*(float*)(payload + 8));
                float alpha = ntoh_f32(*(float*)(payload + 12));
                scenic_renderer_cmd_clear_color(r, red, green, blue, alpha);
            }
            break;

        case SCENIC_CMD_PUT_FONT:
            scenic_renderer_cmd_put_font(r, payload, len);
            break;

        case SCENIC_CMD_PUT_IMAGE:
            scenic_renderer_cmd_put_image(r, payload, len);
            break;

        case SCENIC_CMD_RENDER:
            scenic_renderer_render(r);
            break;

        case SCENIC_CMD_GLOBAL_TX:
            if (len >= 24) {
                float tx[6];
                for (int i = 0; i < 6; i++) {
                    tx[i] = ntoh_f32(*(float*)(payload + i * 4));
                }
                scenic_renderer_cmd_global_tx(r, tx);
            }
            break;

        case SCENIC_CMD_QUIT:
            /* Signal shutdown - handled by platform layer */
            log_info("Received quit command");
            break;

        default:
            {
                char msg[64];
                snprintf(msg, sizeof(msg), "Unknown command type: 0x%02x", type);
                log_warn(msg);
            }
            break;
    }
}

/* Manual command interface */

void scenic_renderer_cmd_clear_color(scenic_renderer_t* r, float red, float green,
                                     float blue, float alpha) {
    if (!r) return;
    r->clear_color[0] = red;
    r->clear_color[1] = green;
    r->clear_color[2] = blue;
    r->clear_color[3] = alpha;
}

void scenic_renderer_cmd_put_script(scenic_renderer_t* r, const uint8_t* data, uint32_t len) {
    if (!r || !data || len == 0) return;
    int remaining = (int)len;
    comms_set_buffer(data, remaining);
    put_script(&remaining);
}

void scenic_renderer_cmd_del_script(scenic_renderer_t* r, const uint8_t* data, uint32_t len) {
    if (!r || !data || len == 0) return;
    int remaining = (int)len;
    comms_set_buffer(data, remaining);
    delete_script(&remaining);
}

void scenic_renderer_cmd_reset(scenic_renderer_t* r) {
    if (!r) return;
    reset_scripts();
    if (r->nvg_ctx) {
        reset_fonts(r->nvg_ctx);
        reset_images(r->nvg_ctx);
    }
}

void scenic_renderer_cmd_put_font(scenic_renderer_t* r, const uint8_t* data, uint32_t len) {
    if (!r || !r->nvg_ctx || !data || len == 0) return;
    int remaining = (int)len;
    comms_set_buffer(data, remaining);
    put_font(&remaining, r->nvg_ctx);
}

void scenic_renderer_cmd_put_image(scenic_renderer_t* r, const uint8_t* data, uint32_t len) {
    if (!r || !r->nvg_ctx || !data || len == 0) return;
    int remaining = (int)len;
    comms_set_buffer(data, remaining);
    put_image(&remaining, r->nvg_ctx);
}

void scenic_renderer_cmd_global_tx(scenic_renderer_t* r, const float tx[6]) {
    if (!r) return;
    memcpy(r->global_tx, tx, sizeof(r->global_tx));
}

void scenic_renderer_render(scenic_renderer_t* r) {
    if (!r || !r->nvg_ctx || r->width <= 0 || r->height <= 0) return;

    /* Begin frame - platform handles GL state */
    if (r->platform.begin_frame) {
        r->platform.begin_frame(r->platform.user_data, r->width, r->height, r->pixel_ratio);
    }

    /* Begin NanoVG frame */
    nvgBeginFrame(r->nvg_ctx, r->width, r->height, r->pixel_ratio);

    /* Apply global transform */
    nvgTransform(r->nvg_ctx,
                 r->global_tx[0], r->global_tx[1],
                 r->global_tx[2], r->global_tx[3],
                 r->global_tx[4], r->global_tx[5]);

    /* Render root script */
    sid_t root_id;
    root_id.p_data = "_root_";
    root_id.size = 6;
    render_script(root_id, r->nvg_ctx);

    /* End NanoVG frame */
    nvgEndFrame(r->nvg_ctx);

    /* End frame - platform handles buffer swap */
    if (r->platform.end_frame) {
        r->platform.end_frame(r->platform.user_data);
    }
}

/* Event sending */

static int send_event(scenic_renderer_t* r, uint8_t type,
                      const void* payload, uint32_t len) {
    if (!r || !r->transport) return -1;

    int msg_len = protocol_encode_event(r->send_buf, r->send_buf_size,
                                        type, payload, len);
    if (msg_len < 0) return -1;

    return scenic_transport_send(r->transport, r->send_buf, msg_len);
}

void scenic_renderer_send_ready(scenic_renderer_t* r) {
    send_event(r, SCENIC_EVT_READY, NULL, 0);
}

void scenic_renderer_send_reshape(scenic_renderer_t* r, int width, int height) {
    uint8_t payload[8];
    uint32_t w = hton_ui32((uint32_t)width);
    uint32_t h = hton_ui32((uint32_t)height);
    memcpy(payload, &w, 4);
    memcpy(payload + 4, &h, 4);
    send_event(r, SCENIC_EVT_RESHAPE, payload, 8);
}

void scenic_renderer_send_touch(scenic_renderer_t* r, int action, float x, float y) {
    uint8_t payload[9];
    payload[0] = (uint8_t)action;
    float fx = hton_f32(x);
    float fy = hton_f32(y);
    memcpy(payload + 1, &fx, 4);
    memcpy(payload + 5, &fy, 4);
    send_event(r, SCENIC_EVT_TOUCH, payload, 9);
}

void scenic_renderer_send_key(scenic_renderer_t* r, uint32_t key, uint32_t scancode,
                              int32_t action, uint32_t mods) {
    uint8_t payload[16];
    uint32_t k = hton_ui32(key);
    uint32_t s = hton_ui32(scancode);
    int32_t a = (int32_t)hton_ui32((uint32_t)action);
    uint32_t m = hton_ui32(mods);
    memcpy(payload, &k, 4);
    memcpy(payload + 4, &s, 4);
    memcpy(payload + 8, &a, 4);
    memcpy(payload + 12, &m, 4);
    send_event(r, SCENIC_EVT_KEY, payload, 16);
}

void scenic_renderer_send_codepoint(scenic_renderer_t* r, uint32_t codepoint, uint32_t mods) {
    uint8_t payload[8];
    uint32_t c = hton_ui32(codepoint);
    uint32_t m = hton_ui32(mods);
    memcpy(payload, &c, 4);
    memcpy(payload + 4, &m, 4);
    send_event(r, SCENIC_EVT_CODEPOINT, payload, 8);
}

void scenic_renderer_send_mouse_button(scenic_renderer_t* r, uint32_t button, uint32_t action,
                                        uint32_t mods, float x, float y) {
    uint8_t payload[20];
    uint32_t b = hton_ui32(button);
    uint32_t a = hton_ui32(action);
    uint32_t m = hton_ui32(mods);
    float fx = hton_f32(x);
    float fy = hton_f32(y);
    memcpy(payload, &b, 4);
    memcpy(payload + 4, &a, 4);
    memcpy(payload + 8, &m, 4);
    memcpy(payload + 12, &fx, 4);
    memcpy(payload + 16, &fy, 4);
    send_event(r, SCENIC_EVT_MOUSE_BUTTON, payload, 20);
}

void scenic_renderer_send_cursor_pos(scenic_renderer_t* r, float x, float y) {
    uint8_t payload[8];
    float fx = hton_f32(x);
    float fy = hton_f32(y);
    memcpy(payload, &fx, 4);
    memcpy(payload + 4, &fy, 4);
    send_event(r, SCENIC_EVT_CURSOR_POS, payload, 8);
}

void scenic_renderer_send_scroll(scenic_renderer_t* r, float xoff, float yoff, float x, float y) {
    uint8_t payload[16];
    float fxoff = hton_f32(xoff);
    float fyoff = hton_f32(yoff);
    float fx = hton_f32(x);
    float fy = hton_f32(y);
    memcpy(payload, &fxoff, 4);
    memcpy(payload + 4, &fyoff, 4);
    memcpy(payload + 8, &fx, 4);
    memcpy(payload + 12, &fy, 4);
    send_event(r, SCENIC_EVT_SCROLL, payload, 16);
}

void scenic_renderer_send_cursor_enter(scenic_renderer_t* r, bool entered) {
    uint8_t payload[1] = { entered ? 1 : 0 };
    send_event(r, SCENIC_EVT_CURSOR_ENTER, payload, 1);
}

/* Utility functions */

void* scenic_renderer_get_nvg_context(scenic_renderer_t* r) {
    return r ? r->nvg_ctx : NULL;
}

void scenic_renderer_get_size(scenic_renderer_t* r, int* width, int* height, float* ratio) {
    if (!r) return;
    if (width) *width = r->width;
    if (height) *height = r->height;
    if (ratio) *ratio = r->pixel_ratio;
}

/* Allow platform to set NanoVG context after GL initialization */
void scenic_renderer_set_nvg_context(scenic_renderer_t* r, NVGcontext* ctx) {
    if (r) {
        r->nvg_ctx = ctx;
        r->initialized = true;
    }
}

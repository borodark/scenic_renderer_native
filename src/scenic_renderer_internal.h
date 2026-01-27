/*
 * Internal declarations for Scenic renderer
 */

#pragma once

#include "scenic_renderer.h"
#include "types.h"
#include "nanovg/nanovg.h"

/* Internal renderer state */
struct scenic_renderer {
    int width;
    int height;
    float pixel_ratio;

    NVGcontext* nvg_ctx;
    scenic_transport_t* transport;
    scenic_platform_t platform;

    float clear_color[4];
    float global_tx[6];

    /* Receive buffer for commands */
    uint8_t* recv_buf;
    int recv_buf_size;
    int recv_buf_len;

    /* Send buffer for events */
    uint8_t* send_buf;
    int send_buf_size;

    bool initialized;
};

/* Default buffer sizes */
#define DEFAULT_RECV_BUF_SIZE (256 * 1024)  /* 256KB */
#define DEFAULT_SEND_BUF_SIZE (4 * 1024)    /* 4KB */

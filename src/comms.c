/*
 * Communications utilities for Scenic renderer
 */

#include <string.h>
#include <stdio.h>
#include "comms.h"

static const unsigned char* g_stream_ptr = NULL;
static int g_stream_remaining = 0;

void comms_set_buffer(const void* data, int len) {
    g_stream_ptr = (const unsigned char*)data;
    g_stream_remaining = len;
}

bool read_bytes_down(void* p_buff, int bytes_to_read, int* p_bytes_remaining) {
    if (bytes_to_read <= 0) return true;
    if (g_stream_ptr == NULL || g_stream_remaining < bytes_to_read) {
        return false;
    }

    memcpy(p_buff, g_stream_ptr, bytes_to_read);
    g_stream_ptr += bytes_to_read;
    g_stream_remaining -= bytes_to_read;
    if (p_bytes_remaining) {
        *p_bytes_remaining -= bytes_to_read;
    }
    return true;
}

/* Default logging implementation - can be overridden by platform */
__attribute__((weak))
void send_puts(const char* msg) {
    if (msg) {
        printf("[Scenic] %s\n", msg);
    }
}

__attribute__((weak))
void log_info(const char* msg) {
    if (msg) {
        printf("[Scenic INFO] %s\n", msg);
    }
}

__attribute__((weak))
void log_warn(const char* msg) {
    if (msg) {
        printf("[Scenic WARN] %s\n", msg);
    }
}

__attribute__((weak))
void log_error(const char* msg) {
    if (msg) {
        fprintf(stderr, "[Scenic ERROR] %s\n", msg);
    }
}

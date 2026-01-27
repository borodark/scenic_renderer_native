/*
 * Scenic Renderer Standalone Test Application
 *
 * This example creates a window and waits for a Scenic driver to connect
 * over TCP on port 4000. It renders whatever the driver sends.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scenic_renderer.h"
#include "scenic_transport.h"
#include "platform/platform.h"

static scenic_renderer_t* g_renderer = NULL;

/* Called each frame to process commands from the driver */
static void frame_callback(scenic_renderer_t* renderer, void* user_data) {
    (void)user_data;

    /* Process any pending commands from the driver */
    int processed = scenic_renderer_process_commands(renderer, 0);
    if (processed < 0) {
        fprintf(stderr, "Error processing commands\n");
        scenic_platform_request_close();
    }
}

static void print_usage(const char* prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -p, --port PORT    TCP port to listen on (default: 4000)\n");
    fprintf(stderr, "  -s, --socket PATH  Unix socket path to listen on\n");
    fprintf(stderr, "  -w, --width WIDTH  Window width (default: 800)\n");
    fprintf(stderr, "  -h, --height H     Window height (default: 600)\n");
    fprintf(stderr, "  --help             Show this help\n");
}

int main(int argc, char** argv) {
    int port = 4000;
    const char* socket_path = NULL;
    int width = 800;
    int height = 600;

    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (i + 1 < argc) {
                port = atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--socket") == 0) {
            if (i + 1 < argc) {
                socket_path = argv[++i];
            }
        } else if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--width") == 0) {
            if (i + 1 < argc) {
                width = atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--height") == 0) {
            if (i + 1 < argc) {
                height = atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    /* Initialize platform (creates window) */
    scenic_platform_t platform = scenic_platform_init(width, height, "Scenic Renderer");
    if (!platform.begin_frame) {
        fprintf(stderr, "Failed to initialize platform\n");
        return 1;
    }

    /* Create transport */
    scenic_transport_t* transport;
    char address[512];

    if (socket_path) {
        printf("Creating Unix socket at %s...\n", socket_path);
        transport = scenic_transport_unix_socket_create();
        snprintf(address, sizeof(address), "%s", socket_path);
    } else {
        printf("Creating TCP server on port %d...\n", port);
        transport = scenic_transport_tcp_server_create();
        snprintf(address, sizeof(address), "0.0.0.0:%d", port);
    }

    if (!transport) {
        fprintf(stderr, "Failed to create transport\n");
        scenic_platform_shutdown();
        return 1;
    }

    printf("Waiting for connection...\n");
    if (scenic_transport_connect(transport, address) < 0) {
        fprintf(stderr, "Failed to start server/connect\n");
        scenic_transport_destroy(transport);
        scenic_platform_shutdown();
        return 1;
    }
    printf("Client connected!\n");

    /* Get actual framebuffer size */
    int fb_width, fb_height;
    scenic_platform_get_size(&fb_width, &fb_height);
    float ratio = scenic_platform_get_pixel_ratio();

    /* Create renderer */
    scenic_renderer_config_t config = {
        .width = fb_width,
        .height = fb_height,
        .pixel_ratio = ratio,
        .transport = transport,
        .platform = platform
    };

    g_renderer = scenic_renderer_create(&config);
    if (!g_renderer) {
        fprintf(stderr, "Failed to create renderer\n");
        scenic_transport_destroy(transport);
        scenic_platform_shutdown();
        return 1;
    }

    /* Send ready event to driver */
    scenic_renderer_send_ready(g_renderer);
    scenic_renderer_send_reshape(g_renderer, fb_width, fb_height);

    /* Run platform event loop */
    printf("Running... Press ESC to quit\n");
    scenic_platform_run(g_renderer, frame_callback, NULL);

    /* Cleanup */
    printf("Shutting down...\n");
    scenic_renderer_destroy(g_renderer);
    scenic_transport_destroy(transport);
    scenic_platform_shutdown();

    return 0;
}

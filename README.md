# Scenic Renderer Native

Standalone C library that receives Scenic commands over a transport and renders them using NanoVG/OpenGL. Zero Erlang/Elixir dependencies.

## Features

- NanoVG-based vector graphics rendering
- Multiple transport options (Unix socket, TCP)
- Platform backends (GLFW for desktop, Android support)
- Full Scenic script rendering support
- Input event handling (touch, keyboard, mouse)

## Building

### Prerequisites

- CMake 3.16+
- C11 compiler
- For GLFW backend: GLFW 3.3+, OpenGL 3.2+

### Build Commands

```bash
mkdir build && cd build
cmake ..
make
```

### Build Options

- `SCENIC_BUILD_SHARED` - Build shared library (default: ON)
- `SCENIC_BUILD_STATIC` - Build static library (default: ON)
- `SCENIC_BUILD_GLFW` - Build GLFW platform backend (default: ON)
- `SCENIC_BUILD_EXAMPLES` - Build example programs (default: ON)
- `SCENIC_BUILD_TESTS` - Build tests (default: ON)

## Usage

### Standalone Renderer

Run the GLFW standalone example:

```bash
./build/examples/glfw_standalone/scenic_standalone --port 4000
```

Then connect a Scenic application using `ScenicDriverRemote` with TCP transport.

### As a Library

```c
#include "scenic_renderer.h"
#include "scenic_transport.h"
#include "platform/platform.h"

int main() {
    // Initialize platform
    scenic_platform_t platform = scenic_platform_init(800, 600, "My App");

    // Create transport (server mode)
    scenic_transport_t* transport = scenic_transport_tcp_server_create();
    scenic_transport_connect(transport, "0.0.0.0:4000");

    // Create renderer
    scenic_renderer_config_t config = {
        .width = 800,
        .height = 600,
        .pixel_ratio = 1.0f,
        .transport = transport,
        .platform = platform
    };
    scenic_renderer_t* renderer = scenic_renderer_create(&config);

    // Send ready event
    scenic_renderer_send_ready(renderer);

    // Run event loop
    scenic_platform_run(renderer, my_callback, NULL);

    // Cleanup
    scenic_renderer_destroy(renderer);
    scenic_transport_destroy(transport);
    scenic_platform_shutdown();

    return 0;
}
```

## Protocol

See `include/scenic_protocol.h` for the binary protocol specification.

## License

Apache-2.0

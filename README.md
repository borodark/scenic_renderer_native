# Scenic Renderer Native

Standalone C library that receives Scenic commands over a transport and renders them using NanoVG/OpenGL. Zero Erlang/Elixir dependencies.

## Architecture

```
+------------------+       binary protocol        +------------------+
|  BEAM / Scenic   | --------------------------> |  This Library     |
|  ViewPort        | <--- events (touch, keys) --|  NanoVG / OpenGL  |
|  + driver        |       TCP / WS / Unix       |  or Metal         |
+------------------+                              +------------------+
```

This library implements the renderer side of the [Scenic Remote Protocol](https://github.com/borodark/scenic_driver_remote/blob/main/SCENIC_REMOTE_PROTOCOL.md). It receives commands from [scenic_driver_remote](https://github.com/borodark/scenic_driver_remote) and renders Scenic scenes using NanoVG.

## Features

- NanoVG-based vector graphics rendering
- Multiple transport options (Unix socket, TCP)
- Platform backends (GLFW for desktop, Android, iOS)
- Full Scenic script rendering support (62+ drawing operations)
- Input event handling (touch, keyboard, mouse, scroll)
- Font and image asset management

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

### Android Build

Used as a library in Android NDK projects. See `src/platform/android/` for JNI integration.

```bash
mkdir build-android && cd build-android
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DSCENIC_BUILD_GLFW=OFF \
  -DSCENIC_BUILD_ANDROID=ON
make
```

### iOS Build (Metal)

```bash
mkdir build-ios && cd build-ios
cmake .. \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/ios.toolchain.cmake \
  -DSCENIC_BUILD_IOS=ON \
  -DSCENIC_BUILD_GLFW=OFF \
  -DSCENIC_BUILD_EXAMPLES=OFF \
  -DSCENIC_BUILD_TESTS=OFF
make
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `SCENIC_BUILD_SHARED` | ON | Build shared library |
| `SCENIC_BUILD_STATIC` | ON | Build static library |
| `SCENIC_BUILD_GLFW` | ON | Build GLFW platform backend |
| `SCENIC_BUILD_ANDROID` | OFF | Build Android platform backend |
| `SCENIC_BUILD_IOS` | OFF | Build iOS platform backend |
| `SCENIC_BUILD_EXAMPLES` | ON | Build example programs |
| `SCENIC_BUILD_TESTS` | ON | Build tests |

## Usage

### Standalone Renderer

Run the GLFW standalone example:

```bash
./build/examples/glfw_standalone/scenic_standalone -p 4000
# Or with Unix socket:
./build/examples/glfw_standalone/scenic_standalone -s /tmp/scenic.sock
# See all options:
./build/examples/glfw_standalone/scenic_standalone --help
```

Then connect a Scenic application using `ScenicDriverRemote` with TCP or Unix socket transport.

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

    // Send ready event to driver
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

### Manual Command Mode

For embedded use without transport:

```c
scenic_renderer_config_t config = {
    .width = 800,
    .height = 600,
    .pixel_ratio = 1.0f,
    .transport = NULL,  // Manual mode
    .platform = platform
};
scenic_renderer_t* renderer = scenic_renderer_create(&config);

// Direct command API
scenic_renderer_cmd_clear_color(renderer, 0.1f, 0.1f, 0.1f, 1.0f);
scenic_renderer_cmd_put_script(renderer, script_data, script_len);
scenic_renderer_render(renderer);
```

## Binary Protocol

See [SCENIC_REMOTE_PROTOCOL.md](https://github.com/borodark/scenic_driver_remote/blob/main/SCENIC_REMOTE_PROTOCOL.md) for the complete protocol specification.

### Quick Reference

#### Frame Format

```
+--------+----------------+------------------+
| Type   | Length         | Payload          |
| 1 byte | 4 bytes BE     | Length bytes     |
+--------+----------------+------------------+
```

#### Commands (Driver -> Renderer)

| Code | Name | Payload |
|------|------|---------|
| 0x01 | PUT_SCRIPT | id_len:u32 id:bytes script:bytes |
| 0x02 | DEL_SCRIPT | id_len:u32 id:bytes |
| 0x03 | RESET | *(empty)* |
| 0x04 | GLOBAL_TX | a:f32 b:f32 c:f32 d:f32 e:f32 f:f32 |
| 0x05 | CURSOR_TX | a:f32 b:f32 c:f32 d:f32 e:f32 f:f32 |
| 0x06 | RENDER | *(empty)* |
| 0x08 | CLEAR_COLOR | r:f32 g:f32 b:f32 a:f32 |
| 0x0A | REQUEST_INPUT | flags:u32 |
| 0x20 | QUIT | *(empty)* |
| 0x40 | PUT_FONT | name_len:u32 data_len:u32 name:bytes data:bytes |
| 0x41 | PUT_IMAGE | id_len:u32 data_len:u32 w:u32 h:u32 fmt:u32 id:bytes data:bytes |

#### Events (Renderer -> Driver)

| Code | Name | Payload |
|------|------|---------|
| 0x01 | STATS | bytes_received:u64 |
| 0x05 | RESHAPE | width:u32 height:u32 |
| 0x06 | READY | *(empty)* |
| 0x08 | TOUCH | action:u8 x:f32 y:f32 |
| 0x0A | KEY | key:u32 scancode:u32 action:i32 mods:u32 |
| 0x0B | CODEPOINT | codepoint:u32 mods:u32 |
| 0x0C | CURSOR_POS | x:f32 y:f32 |
| 0x0D | MOUSE_BUTTON | button:u32 action:u32 mods:u32 x:f32 y:f32 |
| 0x0E | SCROLL | x_off:f32 y_off:f32 x:f32 y:f32 |
| 0x0F | CURSOR_ENTER | entered:u8 |

### Connection Lifecycle

1. Renderer starts, listens for connections
2. Driver connects
3. Renderer sends **READY** event
4. Driver sends fonts, images, scripts, then **RENDER**
5. Renderer sends **RESHAPE** with screen dimensions
6. Driver sends **GLOBAL_TX** for scaling
7. Normal operation: scene updates and input events

## Header Files

| Header | Purpose |
|--------|---------|
| `scenic_renderer.h` | Main renderer API |
| `scenic_transport.h` | Transport abstraction |
| `scenic_protocol.h` | Protocol constants (canonical source) |

The `scenic_protocol.h` header is the **canonical source** for protocol constants. All implementations (Elixir, Java, Swift) must align with these values, which originate from [scenic_driver_local](https://github.com/ScenicFramework/scenic_driver_local).

## Directory Structure

```
scenic_renderer_native/
├── include/                    # Public headers
│   ├── scenic_renderer.h       # Main API
│   ├── scenic_transport.h      # Transport interface
│   └── scenic_protocol.h       # Protocol constants
├── src/
│   ├── scenic_renderer.c       # Renderer implementation
│   ├── protocol.c              # Protocol parsing
│   ├── script.c                # Script storage + rendering
│   ├── script_ops.c            # 62+ drawing operations
│   ├── font.c                  # Font management
│   ├── image.c                 # Image/texture management
│   ├── transport/              # Transport implementations
│   ├── nanovg/                 # Vendored NanoVG
│   ├── tommyds/                # Vendored hash table
│   └── platform/               # Platform backends
│       ├── glfw/               # Desktop (GLFW + OpenGL)
│       ├── android/            # Android (EGL + GLES)
│       └── ios/                # iOS (Metal)
├── examples/
│   └── glfw_standalone/        # Desktop test application
└── test/
    └── test_protocol.c         # Protocol tests
```

## Related Projects

- [scenic_driver_remote](https://github.com/borodark/scenic_driver_remote) - Elixir driver implementation
- [scenic_driver_local](https://github.com/ScenicFramework/scenic_driver_local) - Canonical protocol source
- [Scenic](https://github.com/ScenicFramework/scenic) - Elixir UI framework

## License

Apache-2.0

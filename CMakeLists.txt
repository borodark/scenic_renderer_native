cmake_minimum_required(VERSION 3.16)
project(scenic_renderer_native VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Options
option(SCENIC_BUILD_SHARED "Build shared library" ON)
option(SCENIC_BUILD_STATIC "Build static library" ON)
option(SCENIC_BUILD_GLFW "Build GLFW platform backend" ON)
option(SCENIC_BUILD_ANDROID "Build Android platform backend" OFF)
option(SCENIC_BUILD_EXAMPLES "Build example programs" ON)
option(SCENIC_BUILD_TESTS "Build tests" ON)

# Detect platform
if(ANDROID)
    set(SCENIC_BUILD_GLFW OFF)
    set(SCENIC_BUILD_ANDROID ON)
endif()

# Core source files (platform-independent)
set(SCENIC_CORE_SOURCES
    src/scenic_renderer.c
    src/protocol.c
    src/comms.c
    src/script.c
    src/font.c
    src/image.c
    src/utils.c
    src/transport/transport.c
    src/transport/unix_socket.c
    src/transport/tcp.c
    src/nanovg/nanovg.c
    src/tommyds/tommy.c
    src/tommyds/tommyhash.c
    src/tommyds/tommyhashlin.c
    src/tommyds/tommylist.c
)

# Include directories
set(SCENIC_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nanovg
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tommyds
)

# Compiler flags
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# Static library
if(SCENIC_BUILD_STATIC)
    add_library(scenic_renderer_static STATIC ${SCENIC_CORE_SOURCES})
    target_include_directories(scenic_renderer_static PUBLIC ${SCENIC_INCLUDES})
    set_target_properties(scenic_renderer_static PROPERTIES OUTPUT_NAME scenic_renderer)

    if(NOT ANDROID)
        target_compile_definitions(scenic_renderer_static PRIVATE NANOVG_GL3_IMPLEMENTATION)
    else()
        target_compile_definitions(scenic_renderer_static PRIVATE NANOVG_GLES3_IMPLEMENTATION)
    endif()

    # Math library on Unix
    if(UNIX AND NOT APPLE)
        target_link_libraries(scenic_renderer_static PUBLIC m)
    endif()
endif()

# Shared library
if(SCENIC_BUILD_SHARED)
    add_library(scenic_renderer_shared SHARED ${SCENIC_CORE_SOURCES})
    target_include_directories(scenic_renderer_shared PUBLIC ${SCENIC_INCLUDES})
    set_target_properties(scenic_renderer_shared PROPERTIES OUTPUT_NAME scenic_renderer)

    if(NOT ANDROID)
        target_compile_definitions(scenic_renderer_shared PRIVATE NANOVG_GL3_IMPLEMENTATION)
    else()
        target_compile_definitions(scenic_renderer_shared PRIVATE NANOVG_GLES3_IMPLEMENTATION)
    endif()

    if(UNIX AND NOT APPLE)
        target_link_libraries(scenic_renderer_shared PUBLIC m)
    endif()
endif()

# GLFW Platform backend
if(SCENIC_BUILD_GLFW)
    find_package(glfw3 3.3 REQUIRED)
    find_package(OpenGL REQUIRED)

    add_library(scenic_platform_glfw STATIC src/platform/glfw/platform_glfw.c)
    target_include_directories(scenic_platform_glfw PUBLIC ${SCENIC_INCLUDES})
    target_compile_definitions(scenic_platform_glfw PRIVATE NANOVG_GL3_IMPLEMENTATION)

    if(SCENIC_BUILD_STATIC)
        target_link_libraries(scenic_platform_glfw PUBLIC scenic_renderer_static glfw OpenGL::GL)
    else()
        target_link_libraries(scenic_platform_glfw PUBLIC scenic_renderer_shared glfw OpenGL::GL)
    endif()

    # Platform-specific OpenGL handling
    if(APPLE)
        target_compile_definitions(scenic_platform_glfw PRIVATE GL_SILENCE_DEPRECATION)
    endif()
endif()

# Android Platform backend
if(SCENIC_BUILD_ANDROID)
    add_subdirectory(src/platform/android)
endif()

# Examples
if(SCENIC_BUILD_EXAMPLES AND SCENIC_BUILD_GLFW)
    add_subdirectory(examples/glfw_standalone)
endif()

# Tests
if(SCENIC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# Install
install(DIRECTORY include/ DESTINATION include)
if(SCENIC_BUILD_STATIC)
    install(TARGETS scenic_renderer_static ARCHIVE DESTINATION lib)
endif()
if(SCENIC_BUILD_SHARED)
    install(TARGETS scenic_renderer_shared LIBRARY DESTINATION lib)
endif()
if(SCENIC_BUILD_GLFW)
    install(TARGETS scenic_platform_glfw ARCHIVE DESTINATION lib)
endif()

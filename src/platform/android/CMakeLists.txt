# Android platform backend
add_library(scenic_platform_android STATIC
    platform_android.c
)

target_include_directories(scenic_platform_android PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(scenic_platform_android PUBLIC
    scenic_renderer_static
    GLESv3
    EGL
    android
    log
)

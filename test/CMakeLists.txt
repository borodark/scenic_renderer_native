# Test for protocol encoding/decoding
add_executable(test_protocol test_protocol.c)
target_include_directories(test_protocol PRIVATE ${SCENIC_INCLUDES})
target_link_libraries(test_protocol PRIVATE scenic_renderer_static)
add_test(NAME test_protocol COMMAND test_protocol)

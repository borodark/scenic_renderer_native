/*
 * Protocol encoding/decoding tests
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "protocol.h"
#include "comms.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    static void test_##name(void)

#define RUN_TEST(name) do { \
    printf("  Running %s...", #name); \
    tests_run++; \
    test_##name(); \
    tests_passed++; \
    printf(" OK\n"); \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf(" FAILED at line %d: %s\n", __LINE__, #cond); \
        exit(1); \
    } \
} while(0)

TEST(parse_header_complete) {
    uint8_t buf[] = {0x01, 0x00, 0x00, 0x00, 0x10};  /* type=1, length=16 */
    uint8_t type;
    uint32_t len;

    ASSERT(protocol_parse_header(buf, sizeof(buf), &type, &len) == true);
    ASSERT(type == 0x01);
    ASSERT(len == 16);
}

TEST(parse_header_incomplete) {
    uint8_t buf[] = {0x01, 0x00, 0x00};  /* Only 3 bytes, need 5 */
    uint8_t type;
    uint32_t len;

    ASSERT(protocol_parse_header(buf, sizeof(buf), &type, &len) == false);
}

TEST(parse_header_big_endian) {
    /* Test big-endian length: 0x00010203 = 66051 */
    uint8_t buf[] = {0x05, 0x00, 0x01, 0x02, 0x03};
    uint8_t type;
    uint32_t len;

    ASSERT(protocol_parse_header(buf, sizeof(buf), &type, &len) == true);
    ASSERT(type == 0x05);
    ASSERT(len == 0x00010203);
}

TEST(encode_event_ready) {
    uint8_t buf[32];
    int len = protocol_encode_event(buf, sizeof(buf), SCENIC_EVT_READY, NULL, 0);

    ASSERT(len == 5);  /* Header only */
    ASSERT(buf[0] == SCENIC_EVT_READY);
    ASSERT(buf[1] == 0);
    ASSERT(buf[2] == 0);
    ASSERT(buf[3] == 0);
    ASSERT(buf[4] == 0);
}

TEST(encode_event_reshape) {
    uint8_t buf[32];
    uint8_t payload[] = {0x00, 0x00, 0x03, 0x20,  /* 800 */
                         0x00, 0x00, 0x02, 0x58}; /* 600 */

    int len = protocol_encode_event(buf, sizeof(buf), SCENIC_EVT_RESHAPE, payload, 8);

    ASSERT(len == 13);  /* 5 header + 8 payload */
    ASSERT(buf[0] == SCENIC_EVT_RESHAPE);
    ASSERT(buf[1] == 0);
    ASSERT(buf[2] == 0);
    ASSERT(buf[3] == 0);
    ASSERT(buf[4] == 8);
    ASSERT(memcmp(buf + 5, payload, 8) == 0);
}

TEST(encode_event_buffer_too_small) {
    uint8_t buf[4];  /* Too small */
    int len = protocol_encode_event(buf, sizeof(buf), SCENIC_EVT_READY, NULL, 0);

    ASSERT(len == -1);
}

TEST(byte_order_macros) {
    /* Test ntoh_ui32 */
    uint32_t be_val = 0x01020304;
    uint32_t converted = ntoh_ui32(be_val);
    /* On little-endian: should be 0x04030201 */
    /* On big-endian: should be 0x01020304 */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    ASSERT(converted == 0x04030201);
#else
    ASSERT(converted == 0x01020304);
#endif
}

TEST(comms_buffer) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint8_t buf[3];
    int remaining = sizeof(data);

    comms_set_buffer(data, sizeof(data));

    /* Read first 3 bytes */
    ASSERT(read_bytes_down(buf, 3, &remaining) == true);
    ASSERT(buf[0] == 0x01);
    ASSERT(buf[1] == 0x02);
    ASSERT(buf[2] == 0x03);
    ASSERT(remaining == 2);

    /* Read remaining 2 bytes */
    ASSERT(read_bytes_down(buf, 2, &remaining) == true);
    ASSERT(buf[0] == 0x04);
    ASSERT(buf[1] == 0x05);
    ASSERT(remaining == 0);

    /* Try to read more - should fail */
    ASSERT(read_bytes_down(buf, 1, &remaining) == false);
}

int main(void) {
    printf("Running protocol tests...\n");

    RUN_TEST(parse_header_complete);
    RUN_TEST(parse_header_incomplete);
    RUN_TEST(parse_header_big_endian);
    RUN_TEST(encode_event_ready);
    RUN_TEST(encode_event_reshape);
    RUN_TEST(encode_event_buffer_too_small);
    RUN_TEST(byte_order_macros);
    RUN_TEST(comms_buffer);

    printf("\nAll tests passed! (%d/%d)\n", tests_passed, tests_run);
    return 0;
}

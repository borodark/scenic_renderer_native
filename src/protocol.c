/*
 * Protocol parsing for Scenic renderer
 */

#include <string.h>
#include "protocol.h"
#include "comms.h"

bool protocol_parse_header(const uint8_t* buf, int buf_len,
                           uint8_t* type, uint32_t* payload_len) {
    if (buf_len < SCENIC_MSG_HEADER_SIZE) {
        return false;
    }

    *type = buf[0];

    /* Read length as big-endian uint32 */
    *payload_len = ((uint32_t)buf[1] << 24) |
                   ((uint32_t)buf[2] << 16) |
                   ((uint32_t)buf[3] << 8) |
                   ((uint32_t)buf[4]);

    return true;
}

int protocol_encode_event(uint8_t* buf, int buf_size,
                          uint8_t type, const void* payload, uint32_t payload_len) {
    if (buf_size < (int)(SCENIC_MSG_HEADER_SIZE + payload_len)) {
        return -1;
    }

    /* Write header */
    buf[0] = type;
    buf[1] = (payload_len >> 24) & 0xFF;
    buf[2] = (payload_len >> 16) & 0xFF;
    buf[3] = (payload_len >> 8) & 0xFF;
    buf[4] = payload_len & 0xFF;

    /* Write payload */
    if (payload && payload_len > 0) {
        memcpy(buf + SCENIC_MSG_HEADER_SIZE, payload, payload_len);
    }

    return SCENIC_MSG_HEADER_SIZE + payload_len;
}

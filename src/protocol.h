/*
 * Protocol parsing for Scenic renderer
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "scenic_protocol.h"

/* Parse a command header from buffer
 * Returns true if a complete header was found, false if more data needed
 * Sets type and payload_len on success
 */
bool protocol_parse_header(const uint8_t* buf, int buf_len,
                           uint8_t* type, uint32_t* payload_len);

/* Encode an event message
 * Returns the number of bytes written to buf
 */
int protocol_encode_event(uint8_t* buf, int buf_size,
                          uint8_t type, const void* payload, uint32_t payload_len);

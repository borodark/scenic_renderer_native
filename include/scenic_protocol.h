/*
 * Scenic Remote Rendering Protocol Constants
 *
 * Binary protocol for communication between Scenic driver and native renderer.
 */

#ifndef SCENIC_PROTOCOL_H
#define SCENIC_PROTOCOL_H

#include <stdint.h>

#define SCENIC_PROTOCOL_VERSION 1

/* Message frame: [type:u8][length:u32-be][payload...] */
#define SCENIC_MSG_HEADER_SIZE 5

/* Commands (driver -> renderer) */
#define SCENIC_CMD_PUT_SCRIPT    0x01
#define SCENIC_CMD_DEL_SCRIPT    0x02
#define SCENIC_CMD_RESET         0x03
#define SCENIC_CMD_CLEAR_COLOR   0x04
#define SCENIC_CMD_PUT_FONT      0x05
#define SCENIC_CMD_PUT_IMAGE     0x06
#define SCENIC_CMD_RENDER        0x07
#define SCENIC_CMD_GLOBAL_TX     0x08
#define SCENIC_CMD_CURSOR_TX     0x09
#define SCENIC_CMD_REQUEST_INPUT 0x0A
#define SCENIC_CMD_QUIT          0x20

/* Events (renderer -> driver) */
#define SCENIC_EVT_TOUCH         0x01
#define SCENIC_EVT_KEY           0x02
#define SCENIC_EVT_RESHAPE       0x03
#define SCENIC_EVT_CODEPOINT     0x04
#define SCENIC_EVT_CURSOR_POS    0x05
#define SCENIC_EVT_MOUSE_BUTTON  0x06
#define SCENIC_EVT_SCROLL        0x07
#define SCENIC_EVT_CURSOR_ENTER  0x08
#define SCENIC_EVT_READY         0x10
#define SCENIC_EVT_LOG_INFO      0xA0
#define SCENIC_EVT_LOG_WARN      0xA1
#define SCENIC_EVT_LOG_ERROR     0xA2

/* Image formats */
#define SCENIC_IMG_FMT_ENCODED   0  /* PNG, JPEG, etc. */
#define SCENIC_IMG_FMT_GRAY      1  /* 1 byte/pixel */
#define SCENIC_IMG_FMT_GRAY_A    2  /* 2 bytes/pixel */
#define SCENIC_IMG_FMT_RGB       3  /* 3 bytes/pixel */
#define SCENIC_IMG_FMT_RGBA      4  /* 4 bytes/pixel */

/* Touch actions */
#define SCENIC_TOUCH_DOWN        0
#define SCENIC_TOUCH_UP          1
#define SCENIC_TOUCH_MOVE        2

#endif /* SCENIC_PROTOCOL_H */

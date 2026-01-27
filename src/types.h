/*
 * Common types for Scenic renderer
 */

#pragma once

#ifndef bool
#include <stdbool.h>
#endif

#include <stdint.h>

#ifndef NANOVG_H
#include "nanovg/nanovg.h"
#endif

#ifndef PACK
  #ifdef _MSC_VER
    #define PACK( __Declaration__ ) \
        __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop) )
  #elif defined(__GNUC__)
    #define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
  #endif
#endif

typedef unsigned char byte;

/* Packed 2D vector */
PACK(typedef struct Vector2f
{
  float x;
  float y;
}) Vector2f;

/*
 * Combination of a size and location. Do NOT assume the
 * p_data can be free'd. It is usually in a larger block
 * that was the thing that was allocated.
 */
typedef struct _data_t {
  void* p_data;
  uint32_t size;
} data_t;

/* Script ID type (alias for data_t) */
typedef data_t sid_t;

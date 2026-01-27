/*
 * Utility macros and functions
 */

#pragma once

#ifndef _UTILS_H
#define _UTILS_H

#define ALIGN_UP(n, s) (((n) + (s) - 1) & ~((s) - 1))
#define ALIGN_DOWN(n, s) ((n) & ~((s) - 1))

void check_gl_error(void);

#endif

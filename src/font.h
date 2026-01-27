/*
 * Font loading and management
 */

#pragma once

#include "types.h"
#include "tommyds/tommyhashlin.h"

void init_fonts(void);
void put_font(int* p_msg_length, NVGcontext* p_ctx);
void set_font(sid_t id, NVGcontext* p_ctx);
void reset_fonts(NVGcontext* p_ctx);

/*
 * Script storage and rendering
 */

#pragma once

#include "types.h"
#include "tommyds/tommyhashlin.h"

void init_scripts(void);
void put_script(int* p_msg_length);
void delete_script(int* p_msg_length);
void reset_scripts(void);
void render_script(sid_t id, NVGcontext* p_ctx);

/*
 * Font loading and management
 *
 * Based on original code by Boyd Multerer
 * Copyright 2021 Kry10 Limited. All rights reserved.
 */

#include <string.h>
#include <stdlib.h>

#include "types.h"
#include "utils.h"
#include "comms.h"
#include "font.h"

#define HASH_ID(id) tommy_hash_u32(0, id.p_data, id.size)

typedef struct _font_t {
    int nvg_id;
    sid_t id;
    data_t blob;
    tommy_hashlin_node node;
} font_t;

static tommy_hashlin fonts = {0};

void init_fonts(void) {
    tommy_hashlin_init(&fonts);
}

static int _comparator(const void* p_arg, const void* p_obj) {
    const sid_t* p_id = p_arg;
    const font_t* p_font = p_obj;
    return (p_id->size != p_font->id.size)
        || memcmp(p_id->p_data, p_font->id.p_data, p_id->size);
}

static font_t* get_font_entry(sid_t id) {
    return tommy_hashlin_search(
        &fonts,
        _comparator,
        &id,
        HASH_ID(id)
    );
}

void put_font(int* p_msg_length, NVGcontext* p_ctx) {
    uint32_t id_length;
    read_bytes_down(&id_length, sizeof(uint32_t), p_msg_length);
    id_length = ntoh_ui32(id_length);

    uint32_t blob_size;
    read_bytes_down(&blob_size, sizeof(uint32_t), p_msg_length);
    blob_size = ntoh_ui32(blob_size);

    int struct_size = ALIGN_UP(sizeof(font_t), 8);
    int id_size = ALIGN_UP(id_length + 1, 8);  /* +1 for null terminator */
    int alloc_size = struct_size + id_size + blob_size;
    font_t* p_font = calloc(1, alloc_size);
    if (!p_font) {
        send_puts("Unable to allocate font");
        return;
    }

    p_font->id.size = id_length;
    p_font->id.p_data = ((void*)p_font) + struct_size;
    read_bytes_down(p_font->id.p_data, id_length, p_msg_length);

    p_font->blob.size = blob_size;
    p_font->blob.p_data = ((void*)p_font) + struct_size + id_size;
    read_bytes_down(p_font->blob.p_data, blob_size, p_msg_length);

    /* Check if font already exists */
    if (get_font_entry(p_font->id)) {
        free(p_font);
        return;
    }

    /* Create NanoVG font */
    p_font->nvg_id = nvgCreateFontMem(
        p_ctx, p_font->id.p_data, p_font->blob.p_data, blob_size,
        false  /* Don't free blob data when releasing font */
    );
    if (p_font->nvg_id < 0) {
        send_puts("Unable to create NanoVG font");
        free(p_font);
        return;
    }

    tommy_hashlin_insert(&fonts, &p_font->node, p_font, HASH_ID(p_font->id));
}

void set_font(sid_t id, NVGcontext* p_ctx) {
    font_t* p_font = get_font_entry(id);
    if (p_font) {
        nvgFontFaceId(p_ctx, p_font->nvg_id);
    }
}

static void font_free(void* p_obj) {
    free(p_obj);
}

void reset_fonts(NVGcontext* p_ctx) {
    (void)p_ctx;  /* NanoVG doesn't have a font delete API */
    tommy_hashlin_foreach(&fonts, font_free);
    tommy_hashlin_done(&fonts);
    tommy_hashlin_init(&fonts);
}

/*
 * Image/texture loading and management
 *
 * Based on original code by Boyd Multerer
 * Copyright 2021 Kry10 Limited. All rights reserved.
 */

#include <string.h>
#include <stdlib.h>

#include "types.h"
#include "utils.h"
#include "comms.h"
#include "image.h"
#include "nanovg/stb_image.h"

#define HASH_ID(id) tommy_hash_u32(0, id.p_data, id.size)
#define REPEAT_XY (NVG_IMAGE_REPEATX | NVG_IMAGE_REPEATY)

typedef struct _image_t {
    sid_t id;
    uint32_t nvg_id;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    void* p_pixels;
    tommy_hashlin_node node;
} image_t;

static tommy_hashlin images = {0};

void init_images(void) {
    tommy_hashlin_init(&images);
}

static int _comparator(const void* p_arg, const void* p_obj) {
    const sid_t* p_id = p_arg;
    const image_t* p_img = p_obj;
    return (p_id->size != p_img->id.size)
        || memcmp(p_id->p_data, p_img->id.p_data, p_id->size);
}

static image_t* get_image(sid_t id) {
    return tommy_hashlin_search(
        &images,
        _comparator,
        &id,
        HASH_ID(id)
    );
}

static void image_free(NVGcontext* p_ctx, image_t* p_image) {
    if (p_image) {
        tommy_hashlin_remove_existing(&images, &p_image->node);
        nvgDeleteImage(p_ctx, p_image->nvg_id);
        if (p_image->p_pixels) {
            free(p_image->p_pixels);
        }
        free(p_image);
    }
}

void reset_images(NVGcontext* p_ctx) {
    tommy_hashlin_foreach_arg(&images, (tommy_foreach_arg_func*)image_free, p_ctx);
    tommy_hashlin_done(&images);
    tommy_hashlin_init(&images);
}

static int read_pixels(void* p_pixels, uint32_t width, uint32_t height,
                       uint32_t format_in, int* p_msg_length) {
    int buffer_size = *p_msg_length;
    void* p_buffer = malloc(buffer_size);
    if (!p_buffer) {
        send_puts("Unable to alloc temporary pixel buffer");
        return -1;
    }
    read_bytes_down(p_buffer, buffer_size, p_msg_length);

    unsigned int pixel_count = width * height;
    unsigned int src_i, dst_i;
    int x, y, comp;
    void* p_temp = NULL;

    switch (format_in) {
        case 0:  /* encoded file format */
            p_temp = (void*)stbi_load_from_memory(p_buffer, buffer_size, &x, &y, &comp, 4);
            if (p_temp && (x != (int)width || y != (int)height)) {
                send_puts("Image size mismatch");
                free(p_temp);
                free(p_buffer);
                return -1;
            }
            if (p_temp) {
                memcpy(p_pixels, p_temp, pixel_count * 4);
                free(p_temp);
            }
            break;

        case 1:  /* grayscale */
            for (unsigned int i = 0; i < pixel_count; i++) {
                dst_i = i * 4;
                ((char*)p_pixels)[dst_i] = ((char*)p_buffer)[i];
                ((char*)p_pixels)[dst_i + 1] = ((char*)p_buffer)[i];
                ((char*)p_pixels)[dst_i + 2] = ((char*)p_buffer)[i];
                ((char*)p_pixels)[dst_i + 3] = 0xff;
            }
            break;

        case 2:  /* gray + alpha */
            for (unsigned int i = 0; i < pixel_count; i++) {
                dst_i = i * 4;
                src_i = i * 2;
                ((char*)p_pixels)[dst_i] = ((char*)p_buffer)[src_i];
                ((char*)p_pixels)[dst_i + 1] = ((char*)p_buffer)[src_i];
                ((char*)p_pixels)[dst_i + 2] = ((char*)p_buffer)[src_i];
                ((char*)p_pixels)[dst_i + 3] = ((char*)p_buffer)[src_i + 1];
            }
            break;

        case 3:  /* rgb */
            for (unsigned int i = 0; i < pixel_count; i++) {
                dst_i = i * 4;
                src_i = i * 3;
                ((char*)p_pixels)[dst_i] = ((char*)p_buffer)[src_i];
                ((char*)p_pixels)[dst_i + 1] = ((char*)p_buffer)[src_i + 1];
                ((char*)p_pixels)[dst_i + 2] = ((char*)p_buffer)[src_i + 2];
                ((char*)p_pixels)[dst_i + 3] = 0xff;
            }
            break;

        case 4:  /* rgba */
            memcpy(p_pixels, p_buffer, pixel_count * 4);
            break;
    }

    free(p_buffer);
    return 0;
}

void put_image(int* p_msg_length, NVGcontext* p_ctx) {
    uint32_t id_length, blob_size, width, height, format;
    read_bytes_down(&id_length, sizeof(uint32_t), p_msg_length);
    read_bytes_down(&blob_size, sizeof(uint32_t), p_msg_length);
    read_bytes_down(&width, sizeof(uint32_t), p_msg_length);
    read_bytes_down(&height, sizeof(uint32_t), p_msg_length);
    read_bytes_down(&format, sizeof(uint32_t), p_msg_length);

    /* Convert from big-endian */
    id_length = ntoh_ui32(id_length);
    blob_size = ntoh_ui32(blob_size);
    width = ntoh_ui32(width);
    height = ntoh_ui32(height);
    format = ntoh_ui32(format);

    void* p_temp_id = calloc(1, id_length + 1);
    if (!p_temp_id) {
        send_puts("Unable to allocate image id buffer");
        return;
    }
    read_bytes_down(p_temp_id, id_length, p_msg_length);

    sid_t id;
    id.size = id_length;
    id.p_data = p_temp_id;

    image_t* p_image = get_image(id);

    /* Check if dimensions changed */
    if (p_image && ((width != p_image->width) || (height != p_image->height))) {
        log_error("Cannot change image size");
        free(p_temp_id);
        return;
    }

    if (!p_image) {
        /* Create new image record */
        int struct_size = ALIGN_UP(sizeof(image_t), 8);
        int id_size = ALIGN_UP(id_length + 1, 8);
        int pixel_size = width * height * 4;
        int alloc_size = struct_size + id_size + pixel_size;

        p_image = malloc(alloc_size);
        if (!p_image) {
            send_puts("Unable to allocate image struct");
            free(p_temp_id);
            return;
        }

        memset(p_image, 0, struct_size + id_size);
        p_image->width = width;
        p_image->height = height;
        p_image->format = format;

        p_image->id.size = id_length;
        p_image->id.p_data = ((void*)p_image) + struct_size;
        memcpy(p_image->id.p_data, p_temp_id, id_length);

        p_image->p_pixels = ((void*)p_image) + struct_size + id_size;

        read_pixels(p_image->p_pixels, width, height, format, p_msg_length);

        p_image->nvg_id = nvgCreateImageRGBA(p_ctx, width, height, REPEAT_XY, p_image->p_pixels);

        tommy_hashlin_insert(&images, &p_image->node, p_image, HASH_ID(p_image->id));
    } else {
        /* Update existing image pixels */
        read_pixels(p_image->p_pixels, width, height, format, p_msg_length);
        nvgUpdateImage(p_ctx, p_image->nvg_id, p_image->p_pixels);
    }

    free(p_temp_id);
}

void set_fill_image(NVGcontext* p_ctx, sid_t id) {
    image_t* p_image = get_image(id);
    if (!p_image) return;

    int w, h;
    nvgImageSize(p_ctx, p_image->nvg_id, &w, &h);

    nvgFillPaint(p_ctx,
        nvgImagePattern(p_ctx, 0, 0, w, h, 0, p_image->nvg_id, 1.0));
}

void set_stroke_image(NVGcontext* p_ctx, sid_t id) {
    image_t* p_image = get_image(id);
    if (!p_image) return;

    int w, h;
    nvgImageSize(p_ctx, p_image->nvg_id, &w, &h);

    nvgStrokePaint(p_ctx,
        nvgImagePattern(p_ctx, 0, 0, w, h, 0, p_image->nvg_id, 1.0));
}

void draw_image(NVGcontext* p_ctx, sid_t id,
                float sx, float sy, float sw, float sh,
                float dx, float dy, float dw, float dh) {
    image_t* p_image = get_image(id);
    if (!p_image) return;

    int iw, ih;
    nvgImageSize(p_ctx, p_image->nvg_id, &iw, &ih);

    float ax = dw / sw;
    float ay = dh / sh;

    NVGpaint img_pattern = nvgImagePattern(
        p_ctx,
        dx - sx * ax, dy - sy * ay, (float)iw * ax, (float)ih * ay,
        0, p_image->nvg_id, 1.0
    );

    nvgBeginPath(p_ctx);
    nvgRect(p_ctx, dx, dy, dw, dh);
    nvgFillPaint(p_ctx, img_pattern);
    nvgFill(p_ctx);
}
